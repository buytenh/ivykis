/*
 * kojines - Transparent SOCKS connection forwarder.
 *
 * Copyright (C) 2006, 2007, 2009 Lennert Buytenhek <buytenh@wantstofly.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include "kojines.h"

/*
 * Given src and origdst addresses of a connection attempt, determine
 * which SOCKS proxy (nexthop) to forward this connection to.
 */
static int get_nexthop(void *cookie, struct sockaddr_in *nexthop,
			struct sockaddr_in *src, struct sockaddr_in *origdst)
{
	nexthop->sin_family = AF_INET;
	nexthop->sin_port = htons(1080);
	nexthop->sin_addr.s_addr = htonl(0x7f000001);

	return 1;
}

int main()
{
	struct kojines_instance ki;

	iv_init();

	ki.listen_port = 63636;
	ki.cookie = NULL;
	ki.get_nexthop = get_nexthop;
	if (kojines_instance_register(&ki) > 0)
		iv_main();

	return 0;
}
