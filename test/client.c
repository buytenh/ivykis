/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
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
#include <iv.h>
#include <iv_sock.h>
#include <string.h>

struct connector {
	struct sockaddr_in addr;
	struct iv_sock *sock;
};

static void create_connector(struct connector *conn);

static int __connect_done(struct connector *conn)
{
	int ret;

	ret = iv_sock_get_connect_error(conn->sock);
	if (ret == EINPROGRESS)
		return 0;

	if (ret) {
		fprintf(stderr, "blah: %s\n", strerror(ret));
		iv_sock_close(conn->sock);
		return 0;
	}

#if 0
	fprintf(stderr, ".");
#endif

	iv_sock_close(conn->sock);

	return 1;
}

static void connect_done(void *c)
{
	struct connector *conn = (struct connector *)c;

	if (__connect_done(conn))
		create_connector(conn);
}

static void create_connector(struct connector *conn)
{
	static int connections;
	int ret;

again:
	if (connections++ >= 10000)
		return;

	conn->sock = iv_sock_socket(AF_INET, SOCK_STREAM, 0);
	if (conn->sock == NULL) {
		perror("socket");
		exit(1);
	}

	iv_sock_set_cookie(conn->sock, conn);
	iv_sock_set_handler_out(conn->sock, connect_done);

	ret = iv_sock_connect(conn->sock, (struct sockaddr *)&conn->addr,
			      sizeof(conn->addr));

	if ((ret == 0 || iv_sock_get_errno() != EINPROGRESS) &&
	    __connect_done(conn)) {
		goto again;
	}
}

int main()
{
	struct sockaddr_in addr;
	struct connector c[100];
	int i;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0xc0a8010a);

	iv_init();

	for (i = 0; i < sizeof(c) / sizeof(c[0]); i++) {
		struct connector *conn = c + i;

		conn->addr = addr;
		conn->addr.sin_port = htons(20000 + i);
		create_connector(conn);
	}

	iv_main();

	iv_deinit();

	return 0;
}
