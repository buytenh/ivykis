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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <netinet/in.h>
#include <string.h>

struct connector {
	struct iv_fd fd;
	struct sockaddr_in addr;
};

static void create_connector(struct connector *conn, struct sockaddr_in *addr);

static void connected(void *c)
{
	struct connector *conn = (struct connector *)c;
	int ret;

	ret = connect(conn->fd.fd, (struct sockaddr *)&conn->addr,
		      sizeof(conn->addr));
	if (ret == -1) {
		if (errno == EALREADY || errno == EINPROGRESS)
			return;
		fprintf(stderr, "blah: %s\n", strerror(errno));
	}

#if 0
	fprintf(stderr, ".");
#endif

	iv_unregister_fd(&conn->fd);
	close(conn->fd.fd);
	create_connector(conn, &conn->addr);
}

static void create_connector(struct connector *conn, struct sockaddr_in *addr)
{
	int fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(-1);
	}

	INIT_IV_FD(&conn->fd);
	conn->fd.fd = fd;
	conn->fd.cookie = (void *)conn;
	conn->fd.handler_in = connected;
	conn->fd.handler_out = NULL;
	iv_register_fd(&conn->fd);

	conn->addr = *addr;

	connected((void *)conn);
}

int main()
{
	struct sockaddr_in addr;
	struct connector c[1000];
	int i;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0x7f000001);

	iv_init();

	for (i = 0; i < sizeof(c) / sizeof(c[0]); i++) {
		addr.sin_port = htons(20000 + i);
		create_connector(&c[i], &addr);
	}
	iv_main();

	return 0;
}
