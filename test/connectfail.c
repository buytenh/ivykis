/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <netinet/in.h>
#include <string.h>

static struct sockaddr_in addr;
static struct iv_fd ifd;

static void connected(void *_dummy)
{
	int ret;

	ret = connect(ifd.fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret == -1) {
		if (errno == EALREADY || errno == EINPROGRESS)
			return;
		fprintf(stderr, "blah: %s\n", strerror(errno));
	}

	iv_fd_set_handler_in(&ifd, NULL);
}

int main()
{
	int fd;

	iv_init();

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0x7f000001);
	addr.sin_port = htons(6667);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(-1);
	}

	INIT_IV_FD(&ifd);
	ifd.fd = fd;
	ifd.cookie = NULL;
	ifd.handler_in = connected;
	ifd.handler_out = NULL;
	iv_register_fd(&ifd);

	connected(NULL);

	iv_main();

	return 0;
}
