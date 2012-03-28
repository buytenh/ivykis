/*
 * ivykis, an event handling library
 * Copyright (C) 2011 Lennert Buytenhek
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

#ifndef __IV_GETADDRINFO_H
#define __IV_GETADDRINFO_H

#include <iv_work.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_getaddrinfo {
	const char		*node;
	const char 		*service;
	const struct addrinfo	*hints;
	void			*cookie;
	void			(*handler)(void *cookie, int ret,
					   struct addrinfo *res);

	void			*task;
};

int iv_getaddrinfo_submit(struct iv_getaddrinfo *ig);
void iv_getaddrinfo_cancel(struct iv_getaddrinfo *ig);

#ifdef __cplusplus
}
#endif


#endif
