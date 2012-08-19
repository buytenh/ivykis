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

#ifndef __IV_SOCK_H
#define __IV_SOCK_H

#include <iv.h>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct iv_sock;

struct iv_sock *iv_sock_socket(int domain, int type, int protocol);

int iv_sock_get_errno(void);

void iv_sock_set_cookie(struct iv_sock *sock, void *cookie);
void iv_sock_set_handler_in(struct iv_sock *sock,
			    void (*handler_in)(void *cookie));
void iv_sock_set_handler_out(struct iv_sock *sock,
			     void (*handler_out)(void *cookie));

int iv_sock_close(struct iv_sock *sock);
int iv_sock_bind(struct iv_sock *sock, struct sockaddr *addr, int addrlen);
int iv_sock_connect(struct iv_sock *sock, struct sockaddr *addr, int addrlen);
int iv_sock_get_connect_error(struct iv_sock *sock);
int iv_sock_recv(struct iv_sock *sock, void *buf, size_t len, int flags);
int iv_sock_recvfrom(struct iv_sock *sock, void *buf, size_t len,
		     int flags, struct sockaddr *src_addr, int *addrlen);
int iv_sock_send(struct iv_sock *sock, const void *buf, size_t len, int flags);
int iv_sock_sendto(struct iv_sock *sock, const void *buf, size_t len,
		   int flags, struct sockaddr *src_addr, int addrlen);
int iv_sock_shutdown(struct iv_sock *sock, int how);

#ifdef _WIN32
#define ENETDOWN	WSAENETDOWN
#define EADDRINUSE	WSAEADDRINUSE
#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL
#define EFAULT		WSAEFAULT
#define EINPROGRESS	WSAEINPROGRESS
#define EINVAL		WSAEINVAL
#define ENOBUFS		WSAENOBUFS
#endif

#ifdef __cplusplus
}
#endif


#endif
