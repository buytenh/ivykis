/*
 * kojines - Transparent SOCKS connection forwarder.
 * Copyright (C) 2006, 2007, 2009, 2011 Lennert Buytenhek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version
 * 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 2.1 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License version 2.1 along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <iv.h>
#include <iv_fd_pump.h>
#include <linux/netfilter_ipv4.h>
#include <string.h>
#include <sys/socket.h>
#include "kojines.h"

struct kojine
{
	struct iv_list_head		list;

	struct iv_fd			client_fd;
	struct iv_fd			server_fd;
	struct sockaddr_in		orig_dst;

	int				connected;
	union {
		struct {
			struct iv_timer		connect_timeout;
			int			buf_length;
			uint8_t			buf[10];
		};
		struct {
			struct iv_fd_pump	cs;
			struct iv_fd_pump	sc;
		};
	};
};

static void __kojine_kill(struct kojine *k)
{
	iv_list_del(&k->list);

	iv_fd_unregister(&k->client_fd);
	close(k->client_fd.fd);

	iv_fd_unregister(&k->server_fd);
	close(k->server_fd.fd);

	free(k);
}

static void kojine_kill(struct kojine *k)
{
	if (!k->connected) {
		iv_timer_unregister(&k->connect_timeout);
	} else {
		iv_fd_pump_destroy(&k->cs);
		iv_fd_pump_destroy(&k->sc);
	}

	__kojine_kill(k);
}

static void cs_pump(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	int ret;

	ret = iv_fd_pump_pump(&k->cs);

	if (ret < 0) {
		kojine_kill(k);
		return;
	}

	if (ret == 0) {
		if (!iv_fd_pump_is_done(&k->sc))
			shutdown(k->server_fd.fd, SHUT_WR);
		else
			kojine_kill(k);
	}
}

static void cs_set_bands(void *_k, int pollin, int pollout)
{
	struct kojine *k = (struct kojine *)_k;

	iv_fd_set_handler_in(&k->client_fd, pollin ? cs_pump : NULL);
	iv_fd_set_handler_out(&k->server_fd, pollout ? cs_pump : NULL);
}

static void sc_pump(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	int ret;

	ret = iv_fd_pump_pump(&k->sc);

	if (ret < 0) {
		kojine_kill(k);
		return;
	}

	if (ret == 0) {
		if (!iv_fd_pump_is_done(&k->cs))
			shutdown(k->client_fd.fd, SHUT_WR);
		else
			kojine_kill(k);
	}
}

static void sc_set_bands(void *_k, int pollin, int pollout)
{
	struct kojine *k = (struct kojine *)_k;

	iv_fd_set_handler_in(&k->server_fd, pollin ? sc_pump : NULL);
	iv_fd_set_handler_out(&k->client_fd, pollout ? sc_pump : NULL);
}

static void got_server_connect_reply(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	int space;
	int ret;

	space = 10 - k->buf_length;
	if (!space) {
		kojine_kill(k);
		return;
	}

	ret = read(k->server_fd.fd, k->buf + k->buf_length, space);
	if (ret <= 0) {
		if (ret == 0 || errno != EAGAIN)
			kojine_kill(k);
		return;
	}

	k->buf_length += ret;
	if (k->buf_length < 10)
		return;

	iv_fd_set_handler_in(&k->server_fd, NULL);

	if (memcmp(k->buf, "\x05\x00", 2)) {
		kojine_kill(k);
		return;
	}

	iv_timer_unregister(&k->connect_timeout);

	k->connected = 1;

	IV_FD_PUMP_INIT(&k->cs);
	k->cs.from_fd = k->client_fd.fd;
	k->cs.to_fd = k->server_fd.fd;
	k->cs.cookie = k;
	k->cs.set_bands = cs_set_bands;
	k->cs.flags = 0;
	iv_fd_pump_init(&k->cs);

	IV_FD_PUMP_INIT(&k->sc);
	k->sc.from_fd = k->server_fd.fd;
	k->sc.to_fd = k->client_fd.fd;
	k->sc.cookie = k;
	k->sc.set_bands = sc_set_bands;
	k->sc.flags = 0;
	iv_fd_pump_init(&k->sc);
}

static void got_server_auth_reply(void *_k)
{
	struct kojine *k = (struct kojine *)_k;
	int space;
	int ret;
	uint8_t buf[10];

	space = 2 - k->buf_length;
	if (!space) {
		kojine_kill(k);
		return;
	}

	ret = read(k->server_fd.fd, k->buf + k->buf_length, space);
	if (ret <= 0) {
		if (ret == 0 || errno != EAGAIN)
			kojine_kill(k);
		return;
	}

	k->buf_length += ret;
	if (k->buf_length < 2)
		return;

	if (memcmp(k->buf, "\x05\x00", 2)) {
		kojine_kill(k);
		return;
	}

	buf[0] = 0x05;			// SOCKSv5
	buf[1] = 0x01;			// CONNECT
	buf[2] = 0x00;			// reserved
	buf[3] = 0x01;			// IPv4
	buf[4] = (ntohl(k->orig_dst.sin_addr.s_addr) >> 24) & 0xff;
	buf[5] = (ntohl(k->orig_dst.sin_addr.s_addr) >> 16) & 0xff;
	buf[6] = (ntohl(k->orig_dst.sin_addr.s_addr) >> 8) & 0xff;
	buf[7] = ntohl(k->orig_dst.sin_addr.s_addr) & 0xff;
	buf[8] = (ntohs(k->orig_dst.sin_port) >> 8) & 0xff;
	buf[9] = ntohs(k->orig_dst.sin_port) & 0xff;
	if (write(k->server_fd.fd, buf, 10) != 10) {
		kojine_kill(k);
		return;
	}

	iv_fd_set_handler_in(&k->server_fd, got_server_connect_reply);
	k->buf_length = 0;
}

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
			kojine_kill(k);
		return;
	}

	/*
	 * SOCKSv5 (0x05),
	 * 1 authentication method (0x01):
	 *   no authentication (0x00).
	 */
	if (write(k->server_fd.fd, "\x05\x01\x00", 3) != 3) {
		kojine_kill(k);
		return;
	}

	iv_fd_set_handler_in(&k->server_fd, got_server_auth_reply);
	iv_fd_set_handler_out(&k->server_fd, NULL);
	k->buf_length = 0;
}

static void server_connect_timeout(void *_k)
{
	struct kojine *k = (struct kojine *)_k;

	__kojine_kill(k);
}

static void set_keepalive(int fd)
{
	int yes;

	yes = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes)) < 0) {
		fprintf(stderr, "set_keepalive: error %d while "
				"setsockopt(SO_KEEPALIVE)\n", errno);
		abort();
	}
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

	set_keepalive(client);

	server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0) {
		close(client);
		return;
	}

	k = malloc(sizeof(*k));
	if (k == NULL) {
		close(server);
		close(client);
		return;
	}

	set_keepalive(server);

	iv_list_add_tail(&k->list, &ki->kojines);

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

	k->connected = 0;

	IV_TIMER_INIT(&k->connect_timeout);
	k->connect_timeout.cookie = (void *)k;
	k->connect_timeout.handler = server_connect_timeout;
	iv_validate_now();
	k->connect_timeout.expires = iv_now;
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

	INIT_IV_LIST_HEAD(&ki->kojines);

	return 1;
}

void kojines_instance_unregister(struct kojines_instance *ki)
{
	struct iv_list_head *ilh;
	struct iv_list_head *ilh2;

	iv_fd_unregister(&ki->listen_fd);
	close(ki->listen_fd.fd);

	iv_list_for_each_safe (ilh, ilh2, &ki->kojines) {
		struct kojine *k;

		k = iv_list_entry(ilh, struct kojine, list);
		kojine_kill(k);
	}
}

void kojines_instance_detach(struct kojines_instance *ki)
{
	struct iv_list_head *ilh;
	struct iv_list_head *ilh2;

	iv_fd_unregister(&ki->listen_fd);
	close(ki->listen_fd.fd);

	iv_list_for_each_safe (ilh, ilh2, &ki->kojines)
		iv_list_del_init(ilh);
}
