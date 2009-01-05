/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <netinet/in.h>
#include <string.h>

/* SERVER ********************************************************************/
struct client_connection {
	struct iv_fd fd;
	struct iv_timer tim;
};

static struct iv_fd server_socket;

static void timeout(void *_c)
{
	struct client_connection *c = (struct client_connection *)_c;
	struct linger l;

	iv_unregister_fd(&c->fd);

	/* Force a TCP RST on close.  */
	l.l_onoff = 1;
	l.l_linger = 0;
	setsockopt(c->fd.fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));

	close(c->fd.fd);
	free(c);
}

static void got_connection(void *_dummy)
{
	struct client_connection *c;
	struct sockaddr_in addr;
	socklen_t addrlen;
	int ret;

	addrlen = sizeof(addr);
	ret = accept(server_socket.fd, (struct sockaddr *)&addr, &addrlen);
	if (ret <= 0) {
		if (ret == 0 || errno != EAGAIN) {
			perror("accept");
			abort();
		}
		return;
	}

	c = malloc(sizeof(*c));
	if (c == NULL) {
		close(ret);
		return;
	}

	INIT_IV_FD(&c->fd);
	c->fd.fd = ret;
	c->fd.cookie = (void *)c;
	iv_register_fd(&c->fd);

	INIT_IV_TIMER(&c->tim);
	c->tim.cookie = (void *)c;
	c->tim.handler = timeout;
	iv_validate_now();
	c->tim.expires = now;
	c->tim.expires.tv_sec += 1;
	iv_register_timer(&c->tim);
}

static void server_init(void)
{
	struct sockaddr_in addr;
	int sock;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		abort();
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(6667);
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		abort();
	}

	listen(sock, 5);

	INIT_IV_FD(&server_socket);
	server_socket.fd = sock;
	server_socket.handler_in = got_connection;
	iv_register_fd(&server_socket);
}


/* CLIENT ********************************************************************/
static struct sockaddr_in addr;
static struct iv_fd ifd;

static void got_reset(void *_dummy)
{
	printf("reset caught\n");
	iv_fd_set_handler_err(&ifd, NULL);
}

static void connected(void *_dummy)
{
	int ret;

	ret = connect(ifd.fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		if (errno == EALREADY || errno == EINPROGRESS)
			return;
		perror("connect");
		iv_fd_set_handler_in(&ifd, NULL);
		iv_fd_set_handler_out(&ifd, NULL);
		iv_quit();
		return;
	}

	iv_fd_set_handler_in(&ifd, NULL);
	iv_fd_set_handler_out(&ifd, NULL);
	iv_fd_set_handler_err(&ifd, got_reset);
}

int main()
{
	int fd;

	iv_init();

	server_init();

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0x7f000001);
	addr.sin_port = htons(6667);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(-1);
	}

	INIT_IV_FD(&ifd);
	ifd.fd = fd;
	ifd.cookie = NULL;
	ifd.handler_in = connected;
	ifd.handler_out = connected;
	iv_register_fd(&ifd);

	connected(NULL);

	iv_main();

	return 0;
}
