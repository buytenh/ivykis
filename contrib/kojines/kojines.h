/*
 * kojines - Transparent SOCKS connection forwarder.
 * Copyright (C) 2006, 2007, 2009 Lennert Buytenhek <buytenh@wantstofly.org>
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

#ifndef __KOJINES_H
#define __KOJINES_H

#include <arpa/inet.h>
#include <iv.h>
#include <iv_list.h>

#ifdef __cplusplus
extern "C" {
#endif

struct kojines_instance
{
	int			listen_port;
	void			*cookie;
	int			(*get_nexthop)(void *cookie,
					struct sockaddr_in *nexthop,
					struct sockaddr_in *src,
					struct sockaddr_in *origdst);

	struct iv_fd		listen_fd;
	struct iv_list_head	kojines;
};

int kojines_instance_register(struct kojines_instance *);
void kojines_instance_unregister(struct kojines_instance *);
void kojines_instance_detach(struct kojines_instance *);

#ifdef __cplusplus
}
#endif


#endif
