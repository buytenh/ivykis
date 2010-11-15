/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009 Lennert Buytenhek
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
#include <errno.h>
#include <fcntl.h>
#include <iv.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct connection {
	struct iv_fd sock;
};


static int devnull;
static int pfd[2];

static void conn_kill(struct connection *conn)
{
	iv_unregister_fd(&conn->sock);
	close(conn->sock.fd);
	free(conn);
}

static void conn_pollin(void *_conn)
{
	struct connection *conn = (struct connection *)_conn;
	int ret;

	ret = splice(conn->sock.fd, NULL, pfd[1], NULL,
		     1048576, SPLICE_F_NONBLOCK);
	if (ret <= 0) {
		if (ret == 0 || (errno != EAGAIN && errno != EINTR))
			conn_kill(conn);
		return;
	}

	while (ret) {
		int ret2;

		ret2 = splice(pfd[0], NULL, devnull, NULL, ret, 0);
		if (ret2 <= 0) {
			if (ret2 < 0)
				perror("splice");
			abort();
		}

		ret -= ret2;
	}
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

	INIT_IV_FD(&conn->sock);
	conn->sock.fd = ret;
	conn->sock.cookie = (void *)conn;
	conn->sock.handler_in = conn_pollin;
	iv_register_fd(&conn->sock);
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
	addr.sin_port = htons(10009);
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return 1;
	}

	listen(sock, 5);

	INIT_IV_FD(&listening_socket);
	listening_socket.fd = sock;
	listening_socket.cookie = NULL;
	listening_socket.handler_in = got_connection;
	iv_register_fd(&listening_socket);

	return 0;
}

int main()
{
	devnull = open("/dev/null", O_WRONLY);
	if (devnull < 0) {
		perror("open /dev/null");
		return 1;
	}

	if (pipe(pfd) < 0) {
		perror("pipe");
		return 1;
	}

	iv_init();
	open_listening_socket();
	iv_main();

	return 0;
}
