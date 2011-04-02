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
	int pfd[2];
	int pipe_bytes;
	int saw_fin;
};


static void conn_kill(struct connection *conn)
{
	iv_fd_unregister(&conn->sock);
	close(conn->sock.fd);
	close(conn->pfd[0]);
	close(conn->pfd[1]);
	free(conn);
}

static void conn_pollin(void *_conn);
static void conn_pollout(void *_conn);
static void conn_pollerr(void *_conn);

static void conn_pollin(void *_conn)
{
	struct connection *conn = (struct connection *)_conn;
	int ret;

	while (1) {
		ret = splice(conn->sock.fd, NULL, conn->pfd[1], NULL,
			     1048576, SPLICE_F_NONBLOCK);
		if (ret <= 0)
			break;

		if (conn->pipe_bytes == 0)
			iv_fd_set_handler_out(&conn->sock, conn_pollout);
		conn->pipe_bytes += ret;
	}

	if (ret == 0) {
		conn->saw_fin = 1;
		iv_fd_set_handler_in(&conn->sock, NULL);
		if (conn->pipe_bytes == 0) {
			shutdown(conn->sock.fd, SHUT_WR);
			conn_kill(conn);
		}
		return;
	}

	if (errno == EAGAIN && conn->pipe_bytes > 0) {
		int bytes_sock = 1;

		ioctl(conn->sock.fd, FIONREAD, &bytes_sock);
		if (bytes_sock > 0)
			iv_fd_set_handler_in(&conn->sock, NULL);
	} else if (errno != EAGAIN) {
		conn_kill(conn);
	}
}

static void conn_pollout(void *_conn)
{
	struct connection *conn = (struct connection *)_conn;
	int ret;

	if (!conn->pipe_bytes) {
		fprintf(stderr, "conn_pollout: no pipe bytes!\n");
		abort();
	}

	do {
		ret = splice(conn->pfd[0], NULL, conn->sock.fd, NULL,
			     conn->pipe_bytes, 0);
		if (ret <= 0)
			break;

		conn->pipe_bytes -= ret;
		if (!conn->saw_fin)
			iv_fd_set_handler_in(&conn->sock, conn_pollin);
	} while (conn->pipe_bytes);

	if (ret == 0) {
		fprintf(stderr, "pollout: splice returned zero!\n");
		abort();
	}

	if (errno == EAGAIN && conn->pipe_bytes == 0) {
		iv_fd_set_handler_out(&conn->sock, NULL);
		if (conn->saw_fin) {
			shutdown(conn->sock.fd, SHUT_WR);
			conn_kill(conn);
		}
	} else if (errno != EAGAIN) {
		conn_kill(conn);
	}
}

static void conn_pollerr(void *_conn)
{
	struct connection *conn = (struct connection *)_conn;
	socklen_t len;
	int ret;

	len = sizeof(ret);
	if (getsockopt(conn->sock.fd, SOL_SOCKET, SO_ERROR, &ret, &len) < 0) {
		fprintf(stderr, "pollerr: error %d while "
				"getsockopt(SO_ERROR)\n", errno);
		abort();
	}

	if (ret == 0) {
		fprintf(stderr, "pollerr: no error?!\n");
		abort();
	}

	conn_kill(conn);
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

	if (pipe(conn->pfd) < 0) {
		fprintf(stderr, "pipe squeeze\n");
		abort();
	}

	IV_FD_INIT(&conn->sock);
	conn->sock.fd = ret;
	conn->sock.cookie = (void *)conn;
	conn->sock.handler_in = conn_pollin;
	conn->sock.handler_err = conn_pollerr;
	iv_fd_register(&conn->sock);

	conn->pipe_bytes = 0;
	conn->saw_fin = 0;
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
