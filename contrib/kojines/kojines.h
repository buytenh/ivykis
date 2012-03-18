/*
 * kojines - Transparent SOCKS connection forwarder.
 *
 * Copyright (C) 2006, 2007, 2009 Lennert Buytenhek <buytenh@wantstofly.org>
 */

#ifndef __KOJINES_H
#define __KOJINES_H

#include <arpa/inet.h>
#include <iv.h>
#include <iv_list.h>

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


#endif
