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

#ifndef __IV_WSA_SOCKET_H
#define __IV_WSA_SOCKET_H

#include <iv.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_wsa_socket {
	SOCKET		socket;
	void		*cookie;
	void		(*handler[FD_MAX_EVENTS])(void *, int, int);

	struct iv_handle	handle;
	long			event_mask;
};

static inline void IV_WSA_SOCKET_INIT(struct iv_wsa_socket *this)
{
	int i;

	this->socket = INVALID_SOCKET;	
	this->cookie = NULL;
	for (i = 0; i < FD_MAX_EVENTS; i++)
		this->handler[i] = NULL;
}

int iv_wsa_socket_register(struct iv_wsa_socket *this);
void iv_wsa_socket_unregister(struct iv_wsa_socket *this);
void iv_wsa_socket_set_handler(struct iv_wsa_socket *this, int event,
			       void (*handler)(void *, int, int));

#ifdef __cplusplus
}
#endif


#endif
