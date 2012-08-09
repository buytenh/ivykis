/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <iv.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct handle {
	struct iv_fd fd;
	int port;
};


static int conns;

static void handler(void *_h)
{
	struct handle *h = (struct handle *)_h;
	struct sockaddr_in addr;
	socklen_t addrlen;
	int ret;

	addrlen = sizeof(addr);
	ret = accept(h->fd.fd, (struct sockaddr *)&addr, &addrlen);
	if (ret > 0) {
		char buf[128];
		int len;

		len = snprintf(buf, 128, "this is port %d\n", h->port);
		write(ret, buf, len);
		close(ret);

		if (!(++conns % 10000))
			printf("%i\n", conns);
	}
}

static void create_handle(struct handle *h, int port)
{
	struct sockaddr_in addr;
	int sock;
	int yes;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	yes = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		       &yes, sizeof(yes)) < 0) {
		perror("setsockopt");
		exit(1);
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(1);
	}

	listen(sock, 5);

	IV_FD_INIT(&h->fd);
	h->fd.fd = sock;
	h->fd.handler_in = handler;
	h->fd.handler_out = NULL;
	h->fd.cookie = h;
	h->port = port;
	iv_fd_register(&h->fd);
}

static void create_run_handles(int fp, int numhandles)
{
	struct handle hh[numhandles];
	int i;

	printf("entering main loop for ports %d..%d\n",
	       fp, fp + numhandles - 1);

	for (i = 0; i < numhandles; i++)
		create_handle(hh + i, fp + i);

	iv_main();

	iv_deinit();
}


#ifndef __hpux__
#define NUMPORTS	100
#else
#define NUMPORTS	4
#endif

#ifdef THREAD
#include <pthread.h>

static void *thr(void *_fp)
{
	int fp = (int)(unsigned long)_fp;

	iv_init();

	create_run_handles(fp, NUMPORTS);

	return NULL;
}

int main()
{
	int i;

	iv_init();

	for (i = 1; i < 10; i++) {
		unsigned long fp = 20000 + i * NUMPORTS;
		pthread_t id;

		pthread_create(&id, NULL, thr, (void *)fp);
	}

	create_run_handles(20000, NUMPORTS);

	return 0;
}
#else
int main()
{
	iv_init();

	create_run_handles(20000, 10 * NUMPORTS);

	return 0;
}
#endif
