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

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <iv_signal.h>
#include "kojines.h"

static struct kojines_instance ki;
static struct iv_signal sigusr1;

static void got_sigusr1(void *_dummy)
{
	kojines_instance_detach(&ki);
	iv_signal_unregister(&sigusr1);
}

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
	iv_init();

	ki.listen_port = 63636;
	ki.cookie = NULL;
	ki.get_nexthop = get_nexthop;
	if (kojines_instance_register(&ki)) {
		IV_SIGNAL_INIT(&sigusr1);
		sigusr1.signum = SIGUSR1;
		sigusr1.flags = 0;
		sigusr1.cookie = NULL;
		sigusr1.handler = got_sigusr1;
		iv_signal_register(&sigusr1);

		iv_main();
	}

	iv_deinit();

	return 0;
}
