/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009, 2011 Lennert Buytenhek
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
#include <iv_fd_pump.h>
#include <sys/socket.h>

struct connection {
	struct iv_fd sock;
	struct iv_fd_pump pump;
};


static void conn_pump(void *_conn)
{
	struct connection *conn = (struct connection *)_conn;

	if (iv_fd_pump_pump(&conn->pump) <= 0) {
		iv_fd_pump_destroy(&conn->pump);

		iv_fd_unregister(&conn->sock);
		close(conn->sock.fd);

		free(conn);
	}
}

static void conn_set_bands(void *_conn, int pollin, int pollout)
{
	struct connection *conn = (struct connection *)_conn;

	iv_fd_set_handler_in(&conn->sock, pollin ? conn_pump : NULL);
	iv_fd_set_handler_out(&conn->sock, pollout ? conn_pump : NULL);
}


static struct iv_fd listening_socket;

static void got_connection(void *_dummy)
{
	struct sockaddr_in addr;
	struct connection *conn;
	socklen_t addrlen;
	int ret;

	addrlen = sizeof(addr);
	ret = accept(listening_socket.fd, (struct sockaddr *)&addr, &addrlen);
	if (ret < 0)
		return;

	conn = malloc(sizeof(*conn));
	if (conn == NULL) {
		fprintf(stderr, "memory squeeze\n");
		abort();
	}

	IV_FD_INIT(&conn->sock);
	conn->sock.fd = ret;
	conn->sock.cookie = (void *)conn;
	iv_fd_register(&conn->sock);

	IV_FD_PUMP_INIT(&conn->pump);
	conn->pump.from_fd = ret;
	conn->pump.to_fd = ret;
	conn->pump.cookie = conn;
	conn->pump.set_bands = conn_set_bands;
	conn->pump.flags = 0;
	iv_fd_pump_init(&conn->pump);
}

static int open_listening_socket(void)
{
	struct sockaddr_in addr;
	int sock;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		return 1;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(6969);
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return 1;
	}

	listen(sock, 5);

	IV_FD_INIT(&listening_socket);
	listening_socket.fd = sock;
	listening_socket.cookie = NULL;
	listening_socket.handler_in = got_connection;
	iv_fd_register(&listening_socket);

	return 0;
}

int main()
{
	iv_init();
	open_listening_socket();
	iv_main();

	return 0;
}
