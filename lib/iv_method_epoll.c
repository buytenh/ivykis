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

static int iv_epoll_init(struct iv_state *st, int maxfd)
{
	int fd;
	int flags;

	INIT_IV_LIST_HEAD(&st->epoll.notify);

#ifdef HAVE_EPOLL_CREATE1
	fd = epoll_create1(EPOLL_CLOEXEC);
	if (fd >= 0) {
		st->epoll.epoll_fd = fd;
		return 0;
	} else if (errno != ENOSYS) {
		return -1;
	}
#endif

	fd = epoll_create(maxfd);
	if (fd < 0)
		return -1;

	flags = fcntl(fd, F_GETFD);
	if (!(flags & FD_CLOEXEC)) {
		flags |= FD_CLOEXEC;
		fcntl(fd, F_SETFD, flags);
	}

	st->epoll.epoll_fd = fd;

	return 0;
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = 0;
	if (bits & MASKIN)
		mask |= EPOLLIN;
	if (bits & MASKOUT)
		mask |= EPOLLOUT;

	return mask;
}

static void iv_epoll_flush_one(struct iv_state *st, struct iv_fd_ *fd)
{
	int op;
	struct epoll_event event;
	int ret;

	if (!fd->registered_bands && fd->wanted_bands)
		op = EPOLL_CTL_ADD;
	else if (fd->registered_bands && !fd->wanted_bands)
		op = EPOLL_CTL_DEL;
	else
		op = EPOLL_CTL_MOD;

	event.data.ptr = fd;
	event.events = bits_to_poll_mask(fd->wanted_bands);
	do {
		ret = epoll_ctl(st->epoll.epoll_fd, op, fd->fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_epoll_flush_one: got "
		       "error %d[%s]", errno, strerror(errno));
		abort();
	}

	fd->registered_bands = fd->wanted_bands;
	iv_list_del_init(&fd->list_notify);
}

static void iv_epoll_flush_pending(struct iv_state *st)
{
	while (!iv_list_empty(&st->epoll.notify)) {
		struct iv_fd_ *fd;

		fd = iv_list_entry(st->epoll.notify.next,
				   struct iv_fd_, list_notify);

		iv_epoll_flush_one(st, fd);
	}
}

static void
iv_epoll_poll(struct iv_state *st, struct iv_list_head *active, int msec)
{
	struct epoll_event batch[st->numfds ? : 1];
	int ret;
	int i;

	iv_epoll_flush_pending(st);

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
		uint32_t events;

		fd = batch[i].data.ptr;
		events = batch[i].events;

		if (events & (EPOLLIN | EPOLLERR | EPOLLHUP))
			iv_fd_make_ready(active, fd, MASKIN);

		if (events & (EPOLLOUT | EPOLLERR | EPOLLHUP))
			iv_fd_make_ready(active, fd, MASKOUT);

		if (events & (EPOLLERR | EPOLLHUP))
			iv_fd_make_ready(active, fd, MASKERR);
	}
}

static void iv_epoll_unregister_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	if (!iv_list_empty(&fd->list_notify))
		iv_epoll_flush_one(st, fd);
}

static void iv_epoll_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	iv_list_del_init(&fd->list_notify);
	if (fd->registered_bands != fd->wanted_bands)
		iv_list_add_tail(&fd->list_notify, &st->epoll.notify);
}

static void iv_epoll_deinit(struct iv_state *st)
{
	close(st->epoll.epoll_fd);
}


struct iv_poll_method iv_method_epoll = {
	.name		= "epoll",
	.init		= iv_epoll_init,
	.poll		= iv_epoll_poll,
	.unregister_fd	= iv_epoll_unregister_fd,
	.notify_fd	= iv_epoll_notify_fd,
	.deinit		= iv_epoll_deinit,
};
