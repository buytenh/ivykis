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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iv.h>
#include <string.h>
#include <sys/socket.h>

struct connector {
	struct sockaddr_in addr;
	struct iv_fd fd;
};

static void create_connector(struct connector *conn);

static int __connect_done(struct connector *conn)
{
	socklen_t len;
	int ret;

	len = sizeof(ret);
	if (getsockopt(conn->fd.fd, SOL_SOCKET, SO_ERROR, &ret, &len) < 0) {
		fprintf(stderr, "connect_done: error %d while "
				"getsockopt(SO_ERROR)\n", errno);
		abort();
	}

	if (ret == EINPROGRESS)
		return 0;

	if (ret)
		fprintf(stderr, "blah: %s\n", strerror(ret));

#if 0
	fprintf(stderr, ".");
#endif

	iv_fd_unregister(&conn->fd);
	close(conn->fd.fd);

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
	int fd;
	int ret;

again:
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(1);
	}

	IV_FD_INIT(&conn->fd);
	conn->fd.fd = fd;
	conn->fd.cookie = (void *)conn;
	conn->fd.handler_out = connect_done;
	iv_fd_register(&conn->fd);

	ret = connect(fd, (struct sockaddr *)&conn->addr, sizeof(conn->addr));
	if ((ret == 0 || errno != EINPROGRESS) && __connect_done(conn))
		goto again;
}

int main()
{
	struct sockaddr_in addr;
	struct connector c[1000];
	int i;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0x7f000001);

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
