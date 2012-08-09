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
#include <iv.h>
#include <string.h>
#include <sys/socket.h>

static struct iv_fd ifd;

static void connect_done(void *_dummy)
{
	socklen_t len;
	int ret;

	len = sizeof(ret);
	if (getsockopt(ifd.fd, SOL_SOCKET, SO_ERROR, &ret, &len) < 0) {
		fprintf(stderr, "connect_done: error %d while "
				"getsockopt(SO_ERROR)\n", errno);
		abort();
	}

	if (ret == EINPROGRESS)
		return;

	if (ret)
		fprintf(stderr, "blah: %s\n", strerror(ret));

	iv_fd_set_handler_out(&ifd, NULL);
}

int main()
{
	int fd;
	struct sockaddr_in addr;
	int ret;

	iv_init();

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(1);
	}

	IV_FD_INIT(&ifd);
	ifd.fd = fd;
	ifd.handler_out = connect_done;
	iv_fd_register(&ifd);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0x7f000001);
	addr.sin_port = htons(6667);

	ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret == 0 || errno != EINPROGRESS)
		connect_done(NULL);

	iv_main();

	iv_deinit();

	return 0;
}
