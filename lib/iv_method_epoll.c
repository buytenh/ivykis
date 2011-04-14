/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
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
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <syslog.h>
#include "iv_private.h"

/*
 * To accommodate systems with older versions of <sys/epoll.h>.
 */
#ifndef EPOLLRDHUP
#define EPOLLRDHUP	0x2000
#endif

#define SET_IN		(EPOLLIN | EPOLLPRI | EPOLLHUP | EPOLLRDNORM | \
			 EPOLLRDBAND | EPOLLMSG | EPOLLRDHUP)
#define SET_OUT		(EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND)
#define SET_ERR		(EPOLLERR)

static int iv_epoll_init(struct iv_state *st, int maxfd)
{
	int flags;

	st->epoll.epoll_fd = epoll_create(maxfd);
	if (st->epoll.epoll_fd < 0)
		return -1;

	flags = fcntl(st->epoll.epoll_fd, F_GETFD);
	if (!(flags & FD_CLOEXEC)) {
		flags |= FD_CLOEXEC;
		fcntl(st->epoll.epoll_fd, F_SETFD, flags);
	}

	return 0;
}

static void
iv_epoll_poll(struct iv_state *st, struct list_head *active, int msec)
{
	struct epoll_event batch[st->numfds ? : 1];
	int ret;
	int i;

	ret = epoll_wait(st->epoll.epoll_fd, batch, st->numfds ? : 1, msec);
	if (ret < 0) {
		if (errno == EINTR)
			return;

		syslog(LOG_CRIT, "iv_epoll_poll: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}

	for (i = 0; i < ret; i++) {
		struct iv_fd_ *fd;

		fd = batch[i].data.ptr;

		if (batch[i].events & (SET_IN | SET_ERR))
			iv_fd_make_ready(active, fd, MASKIN);

		if (batch[i].events & (SET_OUT | SET_ERR))
			iv_fd_make_ready(active, fd, MASKOUT);

		if (batch[i].events & SET_ERR)
			iv_fd_make_ready(active, fd, MASKERR);
	}
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = 0;
	if (bits & MASKIN)
		mask |= SET_IN;
	if (bits & MASKOUT)
		mask |= SET_OUT;
	if (bits)
		mask |= SET_ERR;

	return mask;
}

static void
iv_epoll_notify_fd(struct iv_state *st, struct iv_fd_ *fd, int wanted)
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
		ret = epoll_ctl(st->epoll.epoll_fd, op, fd->fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_epoll_notify_fd: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}

	fd->registered_bands = wanted;
}

static void iv_epoll_deinit(struct iv_state *st)
{
	close(st->epoll.epoll_fd);
}


struct iv_poll_method iv_method_epoll = {
	.name		= "epoll",
	.init		= iv_epoll_init,
	.poll		= iv_epoll_poll,
	.notify_fd	= iv_epoll_notify_fd,
	.deinit		= iv_epoll_deinit,
};
