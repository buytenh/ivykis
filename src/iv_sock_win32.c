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
#include <iv_wsa_socket.h>
#include "iv_private.h"
#include "mutex.h"

struct iv_sock {
	struct iv_wsa_socket	sk;
	void			*cookie;
	void			(*handler_in)(void *cookie);
	void			(*handler_out)(void *cookie);
	int			connect_error;
};

struct iv_sock *iv_sock_socket(int domain, int type, int protocol)
{
	SOCKET sk;

	sk = socket(domain, type, protocol);
	if (sk != INVALID_SOCKET) {
		struct iv_sock *sock;

		sock = malloc(sizeof(*sock));
		if (sock == NULL) {
			closesocket(sk);
			return NULL;
		}

		IV_WSA_SOCKET_INIT(&sock->sk);
		sock->sk.socket = sk;
		sock->sk.cookie = sock;
		iv_wsa_socket_register(&sock->sk);

		return sock;
	}

	return NULL;
}

int iv_sock_get_errno(void)
{
	return WSAGetLastError();
}

void iv_sock_set_cookie(struct iv_sock *sock, void *cookie)
{
	sock->cookie = cookie;
}

static void iv_sock_handler_in(void *cookie, int event, int error)
{
	struct iv_sock *sock = cookie;

	sock->handler_in(sock->cookie);
}

void iv_sock_set_handler_in(struct iv_sock *sock,
			    void (*handler_in)(void *cookie))
{
	void (*h)(void *, int, int);

	sock->handler_in = handler_in;

	h = (handler_in != NULL) ? iv_sock_handler_in : NULL;
	iv_wsa_socket_set_handler(&sock->sk, FD_READ_BIT, h);
	iv_wsa_socket_set_handler(&sock->sk, FD_OOB_BIT, h);
	iv_wsa_socket_set_handler(&sock->sk, FD_ACCEPT_BIT, h);
	iv_wsa_socket_set_handler(&sock->sk, FD_CLOSE_BIT, h);
}

static void iv_sock_handler_out(void *cookie, int event, int error)
{
	struct iv_sock *sock = cookie;

	if (event == FD_CONNECT_BIT)
		sock->connect_error = error;

	sock->handler_out(sock->cookie);
}

void iv_sock_set_handler_out(struct iv_sock *sock,
			     void (*handler_out)(void *cookie))
{
	void (*h)(void *, int, int);

	sock->handler_out = handler_out;

	h = (handler_out != NULL) ? iv_sock_handler_out : NULL;
	iv_wsa_socket_set_handler(&sock->sk, FD_WRITE_BIT, h);
	iv_wsa_socket_set_handler(&sock->sk, FD_CONNECT_BIT, h);
}

int iv_sock_close(struct iv_sock *sock)
{
	iv_wsa_socket_unregister(&sock->sk);
	closesocket(sock->sk.socket);
	free(sock);

	return 0;
}

int iv_sock_bind(struct iv_sock *sock, struct sockaddr *addr, int addrlen)
{
	return bind(sock->sk.socket, addr, addrlen);
}

int iv_sock_connect(struct iv_sock *sock, struct sockaddr *addr, int addrlen)
{
	sock->connect_error = WSAEINPROGRESS;

	return connect(sock->sk.socket, addr, addrlen);
}

int iv_sock_get_connect_error(struct iv_sock *sock)
{
	return sock->connect_error;
}

int iv_sock_recv(struct iv_sock *sock, void *buf, size_t len, int flags)
{
	return recv(sock->sk.socket, buf, len, flags);
}

int iv_sock_recvfrom(struct iv_sock *sock, void *buf, size_t len,
		     int flags, struct sockaddr *src_addr, int *addrlen)
{
	return recvfrom(sock->sk.socket, buf, len, flags, src_addr, addrlen);
}

int iv_sock_send(struct iv_sock *sock, const void *buf, size_t len, int flags)
{
	return send(sock->sk.socket, buf, len, flags);
}

int iv_sock_sendto(struct iv_sock *sock, const void *buf, size_t len,
		   int flags, struct sockaddr *src_addr, int addrlen)
{
	return sendto(sock->sk.socket, buf, len, flags, src_addr, addrlen);
}

int iv_sock_shutdown(struct iv_sock *sock, int how)
{
	return shutdown(sock->sk.socket, how);
}
