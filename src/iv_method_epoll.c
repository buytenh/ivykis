/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef linux

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <syslog.h>
#include "iv_private.h"

static struct list_head		all;
static struct epoll_event	*batch;
static int			batch_size;
static int			epoll_fd;


static int iv_epoll_init(int maxfd)
{
	epoll_fd = epoll_create(maxfd);
	if (epoll_fd < 0)
		return -1;

	batch = malloc(maxfd * sizeof(*batch));
	if (batch == NULL) {
		close(epoll_fd);
		return -1;
	}

	INIT_LIST_HEAD(&all);
	batch_size = maxfd;

	return 0;
}

static void iv_epoll_poll(struct list_head *active, int msec)
{
	int ret;
	int i;

	do {
		ret = epoll_wait(epoll_fd, batch, batch_size, msec);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_epoll_poll: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}

	for (i = 0; i < ret; i++) {
		struct iv_fd_ *fd;

		fd = batch[i].data.ptr;

		if (batch[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP))
			iv_fd_make_ready(active, fd, MASKIN);

		if (batch[i].events & (EPOLLOUT | EPOLLERR))
			iv_fd_make_ready(active, fd, MASKOUT);

		if (batch[i].events & EPOLLERR)
			iv_fd_make_ready(active, fd, MASKERR);
	}
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = 0;
	if (bits & MASKIN)
		mask |= EPOLLIN | EPOLLHUP;
	if (bits & MASKOUT)
		mask |= EPOLLOUT;
	if (bits & MASKERR)
		mask |= EPOLLERR;

	return mask;
}

static void iv_epoll_notify_fd(struct iv_fd_ *fd, int wanted)
{
	struct epoll_event event;
	int op;
	int ret;

	if (fd->registered_bands == wanted)
		return;

	if (!fd->registered_bands && wanted)
		op = EPOLL_CTL_ADD;
	else if (fd->registered_bands && !wanted)
		op = EPOLL_CTL_DEL;
	else
		op = EPOLL_CTL_MOD;

	event.data.ptr = fd;
	event.events = bits_to_poll_mask(wanted);
	do {
		ret = epoll_ctl(epoll_fd, op, fd->fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_epoll_notify_fd: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}

	fd->registered_bands = wanted;
}

static void iv_epoll_deinit(void)
{
	free(batch);
	close(epoll_fd);
}


struct iv_poll_method iv_method_epoll = {
	.name		= "epoll",
	.init		= iv_epoll_init,
	.poll		= iv_epoll_poll,
	.notify_fd	= iv_epoll_notify_fd,
	.deinit		= iv_epoll_deinit,
};
#endif
