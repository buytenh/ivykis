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

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iv.h>
#include <iv_getaddrinfo.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG	0
#endif

#ifndef AI_V4MAPPED
#define AI_V4MAPPED	0
#endif

static void got_results(void *_ig, int ret, struct addrinfo *res)
{
	struct iv_getaddrinfo *ig = _ig;
	struct addrinfo *a;

	if (ret) {
		printf("%s: return code is %d\n", ig->node, ret);
		free(ig);
		return;
	}

	a = res;
	while (a != NULL) {
#ifdef VERBOSE
		int flags;

		printf("\n");
		printf("address record:\n");

		printf("ai_flags =");
		flags = a->ai_flags;
		if (flags & AI_PASSIVE) {
			printf(" AI_PASSIVE");
			flags &= ~AI_PASSIVE;
		}
		if (flags & AI_CANONNAME) {
			printf(" AI_CANONNAME");
			flags &= ~AI_CANONNAME;
		}
		if (flags & AI_NUMERICHOST) {
			printf(" AI_NUMERICHOST");
			flags &= ~AI_NUMERICHOST;
		}
		if (flags & AI_V4MAPPED) {
			printf(" AI_V4MAPPED");
			flags &= ~AI_V4MAPPED;
		}
		if (flags & AI_ALL) {
			printf(" AI_ALL");
			flags &= ~AI_ALL;
		}
		if (flags & AI_ADDRCONFIG) {
			printf(" AI_ADDRCONFIG");
			flags &= ~AI_ADDRCONFIG;
		}
		if (flags & AI_IDN) {
			printf(" AI_IDN");
			flags &= ~AI_IDN;
		}
		if (flags & AI_CANONIDN) {
			printf(" AI_CANONIDN");
			flags &= ~AI_CANONIDN;
		}
		if (flags & AI_IDN_ALLOW_UNASSIGNED) {
			printf(" AI_IDN_ALLOW_UNASSIGNED");
			flags &= ~AI_IDN_ALLOW_UNASSIGNED;
		}
		if (flags & AI_IDN_USE_STD3_ASCII_RULES) {
			printf(" AI_IDN_USE_STD3_ASCII_RULES");
			flags &= ~AI_IDN_USE_STD3_ASCII_RULES;
		}
		if (flags & AI_NUMERICSERV) {
			printf(" AI_NUMERICSERV");
			flags &= ~AI_NUMERICSERV;
		}
		if (flags)
			printf(" %x", flags);
		printf("\n");

		printf("ai_family = ");
		if (a->ai_family == PF_INET)
			printf("PF_INET\n");
		else if (a->ai_family == PF_INET6)
			printf("PF_INET6\n");
		else
			printf("%d\n", a->ai_family);

		printf("ai_socktype = ");
		if (a->ai_socktype == SOCK_STREAM)
			printf("SOCK_STREAM\n");
		else if (a->ai_socktype == SOCK_DGRAM)
			printf("SOCK_DGRAM\n");
		else
			printf("%d\n", a->ai_socktype);

		printf("ai_protocol = ");
		if (a->ai_protocol == IPPROTO_TCP)
			printf("IPPROTO_TCP\n");
		else if (a->ai_protocol == IPPROTO_UDP)
			printf("IPPROTO_UDP\n");
		else
			printf("%d\n", a->ai_protocol);

		printf("ai_addr = ");
#endif

		printf("%s: ", ig->node);

		if (a->ai_addr->sa_family == AF_INET) {
			struct sockaddr_in *in;
			char name[64];

			in = (struct sockaddr_in *)a->ai_addr;
			inet_ntop(AF_INET, &in->sin_addr, name, sizeof(name));
			printf("%s\n", name);
#ifndef __hpux__
		} else if (a->ai_addr->sa_family == AF_INET6) {
			struct sockaddr_in6 *in;
			char name[64];

			in = (struct sockaddr_in6 *)a->ai_addr;
			inet_ntop(AF_INET6, &in->sin6_addr, name, sizeof(name));
			printf("%s\n", name);
#endif
		} else {
			printf("unknown address family\n");
		}

#ifdef VERBOSE
		printf("ai_canonname = %s\n", a->ai_canonname);
#endif

		a = a->ai_next;
	}

	freeaddrinfo(res);
	free(ig);
}

int main(int argc, char *argv[])
{
	struct addrinfo hints;
	int i;

	iv_init();

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;

	for (i = 1; i < argc; i++) {
		struct iv_getaddrinfo *ig;

		ig = malloc(sizeof(*ig));
		if (ig == NULL)
			break;

		memset(ig, 0, sizeof(*ig));
		ig->node = argv[i];
		ig->service = NULL;
		ig->hints = &hints;
		ig->cookie = ig;
		ig->handler = got_results;
		iv_getaddrinfo_submit(ig);
	}

	iv_main();

	iv_deinit();

	return 0;
}
