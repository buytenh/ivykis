/*
 * ivykis, an event handling library
 * Copyright (C) 2012 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version
 * 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 2.1 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License version 2.1 along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <iv.h>
#include <iv_sock.h>
#include <string.h>
#include "iv_private.h"

struct iv_sock {
	struct iv_fd		fd;
};

struct iv_sock *iv_sock_socket(int domain, int type, int protocol)
{
	int fd;

	fd = socket(domain, type, protocol);
	if (fd >= 0) {
		struct iv_sock *sock;

		sock = malloc(sizeof(*sock));
		if (sock == NULL) {
			close(fd);
			errno = ENOMEM;
			return NULL;
		}

		IV_FD_INIT(&sock->fd);
		sock->fd.fd = fd;
		iv_fd_register(&sock->fd);

		return sock;
	}

	return NULL;
}

int iv_sock_get_errno(void)
{
	return errno;
}

void iv_sock_set_cookie(struct iv_sock *sock, void *cookie)
{
	sock->fd.cookie = cookie;
}

void iv_sock_set_handler_in(struct iv_sock *sock,
			    void (*handler_in)(void *cookie))
{
	iv_fd_set_handler_in(&sock->fd, handler_in);
}

void iv_sock_set_handler_out(struct iv_sock *sock,
			     void (*handler_out)(void *cookie))
{
	iv_fd_set_handler_out(&sock->fd, handler_out);
}

int iv_sock_close(struct iv_sock *sock)
{
	iv_fd_unregister(&sock->fd);
	close(sock->fd.fd);
	free(sock);

	return 0;
}

int iv_sock_bind(struct iv_sock *sock, struct sockaddr *addr, int addrlen)
{
	return bind(sock->fd.fd, addr, addrlen);
}

int iv_sock_connect(struct iv_sock *sock, struct sockaddr *addr, int addrlen)
{
	return connect(sock->fd.fd, addr, addrlen);
}

int iv_sock_get_connect_error(struct iv_sock *sock)
{
	socklen_t len;
	int ret;

	len = sizeof(ret);
	if (getsockopt(sock->fd.fd, SOL_SOCKET, SO_ERROR, &ret, &len) < 0) {
		iv_fatal("iv_sock_get_connect_error: getsockopt "
			 "got error %d[%s]", errno, strerror(errno));
	}

	return ret;
}

int iv_sock_recv(struct iv_sock *sock, void *buf, size_t len, int flags)
{
	return recv(sock->fd.fd, buf, len, flags);
}

int iv_sock_recvfrom(struct iv_sock *sock, void *buf, size_t len,
		     int flags, struct sockaddr *src_addr, int *addrlen)
{
	return recvfrom(sock->fd.fd, buf, len, flags, src_addr, addrlen);
}

int iv_sock_send(struct iv_sock *sock, const void *buf, size_t len, int flags)
{
	return send(sock->fd.fd, buf, len, flags);
}

int iv_sock_sendto(struct iv_sock *sock, const void *buf, size_t len,
		   int flags, struct sockaddr *src_addr, int addrlen)
{
	return sendto(sock->fd.fd, buf, len, flags, src_addr, addrlen);
}

int iv_sock_shutdown(struct iv_sock *sock, int how)
{
	return shutdown(sock->fd.fd, how);
}
