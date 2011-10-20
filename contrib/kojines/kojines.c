/*
 * kojines - Transparent SOCKS connection forwarder.
 *
 * Copyright (C) 2006, 2007, 2009 Lennert Buytenhek <buytenh@wantstofly.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <iv.h>
#include <limits.h>
#include <linux/netfilter_ipv4.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include "kojines.h"

struct kojine
{
	struct list_head		list;

	int				state;

	struct iv_fd			client_fd;
	struct iv_fd			server_fd;
	struct sockaddr_in		orig_dst;

	struct iv_timer			connect_timeout;

	int				us_buf_length;
	int				us_saw_fin;
	int				su_buf_length;
	int				su_saw_fin;
	unsigned char			us_buf[4096];
	unsigned char			su_buf[4096];
};

#define KOJINE_STATE_CONNECTING		0
#define KOJINE_STATE_SENT_AUTH		1
#define KOJINE_STATE_SENT_CONNECT	2
#define KOJINE_STATE_ESTABLISHED	3

static void got_client_data(void *_k);
static void got_client_write_space(void *_k);
static void got_server_data(void *_k);
static void got_server_write_space(void *_k);


static void kojine_kill(struct kojine *k, int timeout)
{
	list_del(&k->list);

	iv_fd_unregister(&k->client_fd);
	close(k->client_fd.fd);
	iv_fd_unregister(&k->server_fd);
	close(k->server_fd.fd);

	if (k->state < KOJINE_STATE_ESTABLISHED && !timeout)
		iv_timer_unregister(&k->connect_timeout);

	free(k);
}


static void got_client_data(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	unsigned char *ptr;
	int space;
	int ret;

	ptr = k->us_buf + k->us_buf_length;
	space = sizeof(k->us_buf) - k->us_buf_length;
	if (!space) {
		fprintf(stderr, "got_client_data: no space\n");
		abort();
	}

	ret = read(k->client_fd.fd, ptr, space);
	if (ret < 0) {
		if (errno != EAGAIN)
			kojine_kill(k, 0);
		return;
	}

	if (!k->us_buf_length)
		iv_fd_set_handler_out(&k->server_fd, got_server_write_space);

	if (ret == 0) {
		k->us_saw_fin = 1;
		iv_fd_set_handler_in(&k->client_fd, NULL);
		return;
	}

	k->us_buf_length += ret;
	if (k->us_buf_length == sizeof(k->us_buf))
		iv_fd_set_handler_in(&k->client_fd, NULL);
}

static void got_client_write_space(void *_k)
{
	struct kojine *k = (struct kojine *)_k;

	if (!k->su_buf_length && k->su_saw_fin != 1) {
		fprintf(stderr, "got_client_write_space: nothing to do\n");
		abort();
	}

	if (k->su_buf_length) {
		int ret;

		ret = write(k->client_fd.fd, k->su_buf, k->su_buf_length);
		if (ret <= 0) {
			if (ret == 0 || errno != EAGAIN)
				kojine_kill(k, 0);
			return;
		}

		if (k->su_buf_length == sizeof(k->su_buf) && !k->su_saw_fin)
			iv_fd_set_handler_in(&k->server_fd, got_server_data);
		k->su_buf_length -= ret;
		memmove(k->su_buf, k->su_buf + ret, k->su_buf_length);
	}

	if (!k->su_buf_length) {
		iv_fd_set_handler_out(&k->client_fd, NULL);
		switch (k->su_saw_fin) {
		case 0:
			break;
		case 1:
			k->su_saw_fin = 2;
			shutdown(k->client_fd.fd, SHUT_WR);
			if (k->us_saw_fin == 2)
				kojine_kill(k, 0);
			break;
		case 2:
			fprintf(stderr, "got_client_write_space: already "
					"relayed fin\n");
			abort();
		}
	}
}

static void got_client_error(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	socklen_t len;
	int ret;

	len = sizeof(ret);
	if (getsockopt(k->client_fd.fd, SOL_SOCKET, SO_ERROR, &ret, &len) < 0) {
		fprintf(stderr, "got_client_error: error %d while "
				"getsockopt(SO_ERROR)\n", errno);
		abort();
	}

	if (ret == 0) {
		fprintf(stderr, "got_client_error: no error?!\n");
		abort();
	}

	kojine_kill(k, 0);
}


static void got_server_data(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	unsigned char *ptr;
	int space;
	int ret;

	ptr = k->su_buf + k->su_buf_length;
	space = sizeof(k->su_buf) - k->su_buf_length;
	if (!space) {
		fprintf(stderr, "got_server_data: no space\n");
		abort();
	}

	ret = read(k->server_fd.fd, ptr, space);
	if (ret < 0) {
		if (errno != EAGAIN)
			kojine_kill(k, 0);
		return;
	}

	if (!k->su_buf_length)
		iv_fd_set_handler_out(&k->client_fd, got_client_write_space);

	if (ret == 0) {
		k->su_saw_fin = 1;
		iv_fd_set_handler_in(&k->server_fd, NULL);
		return;
	}

	k->su_buf_length += ret;
	if (k->su_buf_length == sizeof(k->su_buf))
		iv_fd_set_handler_in(&k->server_fd, NULL);
}

static void got_server_write_space(void *_k)
{
	struct kojine *k = (struct kojine *)_k;

	if (!k->us_buf_length && k->us_saw_fin != 1) {
		fprintf(stderr, "got_server_write_space: nothing to do\n");
		abort();
	}

	if (k->us_buf_length) {
		int ret;

		ret = write(k->server_fd.fd, k->us_buf, k->us_buf_length);
		if (ret <= 0) {
			if (ret == 0 || errno != EAGAIN)
				kojine_kill(k, 0);
			return;
		}

		if (k->us_buf_length == sizeof(k->us_buf) && !k->us_saw_fin)
			iv_fd_set_handler_in(&k->client_fd, got_client_data);
		k->us_buf_length -= ret;
		memmove(k->us_buf, k->us_buf + ret, k->us_buf_length);
	}

	if (!k->us_buf_length) {
		iv_fd_set_handler_out(&k->server_fd, NULL);
		switch (k->us_saw_fin) {
		case 0:
			break;
		case 1:
			k->us_saw_fin = 2;
			shutdown(k->server_fd.fd, SHUT_WR);
			if (k->su_saw_fin == 2)
				kojine_kill(k, 0);
			break;
		case 2:
			fprintf(stderr, "got_server_write_space: already "
					"relayed fin\n");
			abort();
		}
	}
}

static void got_server_error(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	socklen_t len;
	int ret;

	len = sizeof(ret);
	if (getsockopt(k->server_fd.fd, SOL_SOCKET, SO_ERROR, &ret, &len) < 0) {
		fprintf(stderr, "got_server_error: error %d while "
				"getsockopt(SO_ERROR)\n", errno);
		abort();
	}

	if (ret == 0) {
		fprintf(stderr, "got_server_error: no error?!\n");
		abort();
	}

	kojine_kill(k, 0);
}


/*
 * KOJINE_STATE_SENT_CONNECT.
 */
static void got_server_connect_reply(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	unsigned char *ptr;
	int space;
	int ret;

	ptr = k->su_buf + k->su_buf_length;
	space = sizeof(k->su_buf) - k->su_buf_length;
	if (!space) {
		kojine_kill(k, 0);
		return;
	}

	ret = read(k->server_fd.fd, ptr, space);
	if (ret <= 0) {
		if (ret == 0 || errno != EAGAIN)
			kojine_kill(k, 0);
		return;
	}

	k->su_buf_length += ret;
	if (k->su_buf_length != 10) {
		if (k->su_buf_length > 10)
			kojine_kill(k, 0);
		return;
	}

	if (memcmp(k->su_buf, "\x05\x00", 2)) {
		kojine_kill(k, 0);
		return;
	}

	k->state = KOJINE_STATE_ESTABLISHED;
	iv_fd_set_handler_in(&k->client_fd, got_client_data);
	iv_fd_set_handler_err(&k->client_fd, got_client_error);
	iv_fd_set_handler_in(&k->server_fd, got_server_data);
	iv_fd_set_handler_err(&k->server_fd, got_server_error);
	iv_timer_unregister(&k->connect_timeout);
	k->us_buf_length = 0;
	k->us_saw_fin = 0;
	k->su_buf_length = 0;
}


/*
 * KOJINE_STATE_SENT_AUTH.
 */
static void got_server_auth_reply(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	unsigned char *ptr;
	int space;
	int ret;

	ptr = k->su_buf + k->su_buf_length;
	space = sizeof(k->su_buf) - k->su_buf_length;
	if (!space) {
		kojine_kill(k, 0);
		return;
	}

	ret = read(k->server_fd.fd, ptr, space);
	if (ret <= 0) {
		if (ret == 0 || errno != EAGAIN)
			kojine_kill(k, 0);
		return;
	}

	k->su_buf_length += ret;
	if (k->su_buf_length != 2) {
		if (k->su_buf_length > 2)
			kojine_kill(k, 0);
		return;
	}

	if (memcmp(k->su_buf, "\x05\x00", 2)) {
		kojine_kill(k, 0);
		return;
	}

	k->us_buf[0] = 0x05;		// SOCKSv5
	k->us_buf[1] = 0x01;		// CONNECT
	k->us_buf[2] = 0x00;		// reserved
	k->us_buf[3] = 0x01;		// IPv4
	k->us_buf[4] = (ntohl(k->orig_dst.sin_addr.s_addr) >> 24) & 0xff;
	k->us_buf[5] = (ntohl(k->orig_dst.sin_addr.s_addr) >> 16) & 0xff;
	k->us_buf[6] = (ntohl(k->orig_dst.sin_addr.s_addr) >> 8) & 0xff;
	k->us_buf[7] = ntohl(k->orig_dst.sin_addr.s_addr) & 0xff;
	k->us_buf[8] = (ntohs(k->orig_dst.sin_port) >> 8) & 0xff;
	k->us_buf[9] = ntohs(k->orig_dst.sin_port) & 0xff;
	if (write(k->server_fd.fd, k->us_buf, 10) != 10) {
		kojine_kill(k, 0);
		return;
	}

	k->state = KOJINE_STATE_SENT_CONNECT;
	iv_fd_set_handler_in(&k->server_fd, got_server_connect_reply);
	k->su_buf_length = 0;
}


/*
 * KOJINE_STATE_CONNECTING.
 */
static void got_server_connect(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	socklen_t len;
	int ret;

	len = sizeof(ret);
	if (getsockopt(k->server_fd.fd, SOL_SOCKET, SO_ERROR, &ret, &len) < 0) {
		fprintf(stderr, "got_server_connect: error %d while "
				"getsockopt(SO_ERROR)\n", errno);
		abort();
	}

	if (ret) {
		if (ret != EINPROGRESS)
			kojine_kill(k, 0);
		return;
	}

	/*
	 * SOCKSv5 (0x05),
	 * 1 authentication method (0x01):
	 *   no authentication (0x00).
	 */
	if (write(k->server_fd.fd, "\x05\x01\x00", 3) != 3) {
		kojine_kill(k, 0);
		return;
	}

	k->state = KOJINE_STATE_SENT_AUTH;
	iv_fd_set_handler_in(&k->server_fd, got_server_auth_reply);
	iv_fd_set_handler_out(&k->server_fd, NULL);
	k->su_buf_length = 0;
	k->su_saw_fin = 0;
}


static void server_connect_timeout(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	kojine_kill(k, 1);
}

static void got_kojine(void *_ki)
{
	struct kojines_instance *ki = (struct kojines_instance *)_ki;
	struct sockaddr_in src;
	struct sockaddr_in orig_dst;
	struct sockaddr_in nexthop;
	struct kojine *k;
	socklen_t len;
	int client;
	int server;
	int ret;

	len = sizeof(src);
	client = accept(ki->listen_fd.fd, (struct sockaddr *)&src, &len);
	if (client < 0) {
		if (errno != EAGAIN && errno != ECONNABORTED) {
			fprintf(stderr, "got_kojine: error %d\n", errno);
			abort();
		}
		return;
	}

	len = sizeof(orig_dst);
	if (getsockopt(client, SOL_IP, SO_ORIGINAL_DST, &orig_dst, &len) < 0) {
		close(client);
		return;
	}

	if (!ki->get_nexthop(ki->cookie, &nexthop, &src, &orig_dst)) {
		close(client);
		return;
	}

	server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0) {
		close(client);
		return;
	}

	k = malloc(sizeof(*k));
	if (k == NULL) {
		close(client);
		return;
	}

	list_add_tail(&k->list, &ki->kojines);
	k->state = KOJINE_STATE_CONNECTING;
	IV_FD_INIT(&k->client_fd);
	k->client_fd.fd = client;
	k->client_fd.cookie = (void *)k;
	iv_fd_register(&k->client_fd);
	IV_FD_INIT(&k->server_fd);
	k->server_fd.fd = server;
	k->server_fd.cookie = (void *)k;
	k->server_fd.handler_out = got_server_connect;
	iv_fd_register(&k->server_fd);
	k->orig_dst = orig_dst;
	IV_TIMER_INIT(&k->connect_timeout);
	k->connect_timeout.cookie = (void *)k;
	k->connect_timeout.handler = server_connect_timeout;
	iv_validate_now();
	k->connect_timeout.expires = now;
	k->connect_timeout.expires.tv_sec += 120;
	iv_timer_register(&k->connect_timeout);

	ret = connect(k->server_fd.fd, (struct sockaddr *)&nexthop, sizeof(nexthop));
	if (ret == 0 || errno != EINPROGRESS)
		got_server_connect((void *)k);
}


int kojines_instance_register(struct kojines_instance *ki)
{
	struct sockaddr_in listen_addr;
	int fd;
	int yes;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return 0;

	yes = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		close(fd);
		return 0;
	}

	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons(ki->listen_port);
	listen_addr.sin_addr.s_addr = htonl(0x7f000001);
	if (bind(fd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
		close(fd);
		return 0;
	}

	if (listen(fd, 100) < 0) {
		close(fd);
		return 0;
	}

	IV_FD_INIT(&ki->listen_fd);
	ki->listen_fd.fd = fd;
	ki->listen_fd.cookie = (void *)ki;
	ki->listen_fd.handler_in = got_kojine;
	iv_fd_register(&ki->listen_fd);

	INIT_LIST_HEAD(&ki->kojines);

	return 1;
}

void kojines_instance_unregister(struct kojines_instance *ki)
{
	struct list_head *lh;
	struct list_head *lh2;

	iv_fd_unregister(&ki->listen_fd);
	close(ki->listen_fd.fd);

	list_for_each_safe (lh, lh2, &ki->kojines) {
		struct kojine *k;

		k = list_entry(lh, struct kojine, list);
		kojine_kill(k, 0);
	}
}
