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
#include <iv.h>
#include <iv_tls.h>
#include <iv_wsa_socket.h>

struct iv_wsa_socket_thr_info {
	struct iv_wsa_socket	*handled_socket;
};

static struct iv_tls_user iv_wsa_socket_tls_user = {
	.sizeof_state	= sizeof(struct iv_wsa_socket_thr_info),
};

static void iv_wsa_socket_tls_init(void) __attribute__((constructor));
static void iv_wsa_socket_tls_init(void)
{
	iv_tls_user_register(&iv_wsa_socket_tls_user);
}

static void iv_wsa_socket_set_event_mask(struct iv_wsa_socket *this)
{
	int ret;

	ret = WSAEventSelect(this->socket, this->handle.handle,
			     this->event_mask);
	if (ret) {
		iv_fatal("iv_wsa_socket_set_event_mask: "
			 "WSAEventSelect() returned %d", ret);
	}
}

static void iv_wsa_socket_got_event(void *_s)
{
	struct iv_wsa_socket_thr_info *tinfo =
		iv_tls_user_ptr(&iv_wsa_socket_tls_user);
	struct iv_wsa_socket *this = (struct iv_wsa_socket *)_s;
	WSANETWORKEVENTS ev;
	int ret;
	int i;

	ret = WSAEnumNetworkEvents(this->socket, this->handle.handle, &ev);
	if (ret) {
		iv_fatal("iv_wsa_socket_got_event: WSAEnumNetworkEvents "
			 "returned %d", ret);
	}

	tinfo->handled_socket = this;
	for (i = 0; i < FD_MAX_EVENTS; i++) {
		if (ev.lNetworkEvents & (1 << i)) {
			this->handler[i](this->cookie, i, ev.iErrorCode[i]);
			if (tinfo->handled_socket == NULL)
				return;
		}
	}
	tinfo->handled_socket = NULL;

	iv_wsa_socket_set_event_mask(this);
}

int iv_wsa_socket_register(struct iv_wsa_socket *this)
{
	HANDLE hnd;
	int i;

	hnd = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hnd == NULL)
		return -1;

	IV_HANDLE_INIT(&this->handle);
	this->handle.handle = hnd;
	this->handle.cookie = this;
	this->handle.handler = iv_wsa_socket_got_event;
	iv_handle_register(&this->handle);

	this->event_mask = 0;
	for (i = 0; i < FD_MAX_EVENTS; i++) {
		if (this->handler[i] != NULL)
			this->event_mask |= 1 << i;
	}

	/*
	 * Call WSAEventSelect() even if the event mask is zero,
	 * as it implicitly sets the socket to nonblocking mode.
	 */
	iv_wsa_socket_set_event_mask(this);

	return 0;
}

void iv_wsa_socket_unregister(struct iv_wsa_socket *this)
{
	struct iv_wsa_socket_thr_info *tinfo =
		iv_tls_user_ptr(&iv_wsa_socket_tls_user);

	if (tinfo->handled_socket == this)
		tinfo->handled_socket = NULL;

	if (this->event_mask) {
		this->event_mask = 0;
		iv_wsa_socket_set_event_mask(this);
	}

	iv_handle_unregister(&this->handle);
	CloseHandle(this->handle.handle);
}

void iv_wsa_socket_set_handler(struct iv_wsa_socket *this, int event,
			       void (*handler)(void *, int, int))
{
	struct iv_wsa_socket_thr_info *tinfo =
		iv_tls_user_ptr(&iv_wsa_socket_tls_user);
	long old_mask;

	if (event >= FD_MAX_EVENTS) {
		iv_fatal("iv_wsa_socket_set_handler: called with "
			 "event == %d", event);
	}

	old_mask = this->event_mask;
	if (this->handler[event] == NULL && handler != NULL)
		this->event_mask |= 1 << event;
	else if (this->handler[event] != NULL && handler == NULL)
		this->event_mask &= ~(1 << event);

	this->handler[event] = handler;

	if (tinfo->handled_socket != this && old_mask != this->event_mask)
		iv_wsa_socket_set_event_mask(this);
}
