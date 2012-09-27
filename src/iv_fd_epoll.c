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
#include "iv_private.h"
#include "iv_fd_private.h"

static int iv_fd_epoll_init(struct iv_state *st)
{
	int fd;

	INIT_IV_LIST_HEAD(&st->u.epoll.notify);

#ifdef HAVE_EPOLL_CREATE1
	fd = epoll_create1(EPOLL_CLOEXEC);
	if (fd >= 0) {
		st->u.epoll.epoll_fd = fd;
		return 0;
	} else if (errno != ENOSYS) {
		return -1;
	}
#endif

	fd = epoll_create(maxfd);
	if (fd < 0)
		return -1;

	iv_fd_set_cloexec(fd);

	st->u.epoll.epoll_fd = fd;

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

static int __iv_fd_epoll_flush_one(struct iv_state *st, struct iv_fd_ *fd)
{
	int op;
	struct epoll_event event;
	int ret;

	iv_list_del_init(&fd->list_notify);

	if (fd->registered_bands == fd->wanted_bands)
		return 0;

	if (!fd->registered_bands && fd->wanted_bands)
		op = EPOLL_CTL_ADD;
	else if (fd->registered_bands && !fd->wanted_bands)
		op = EPOLL_CTL_DEL;
	else
		op = EPOLL_CTL_MOD;

	event.data.ptr = fd;
	event.events = bits_to_poll_mask(fd->wanted_bands);
	do {
		ret = epoll_ctl(st->u.epoll.epoll_fd, op, fd->fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret == 0)
		fd->registered_bands = fd->wanted_bands;

	return ret;
}

static void iv_fd_epoll_flush_one(struct iv_state *st, struct iv_fd_ *fd)
{
	if (__iv_fd_epoll_flush_one(st, fd) < 0) {
		iv_fatal("iv_fd_epoll_flush_one: got error %d[%s]",
			 errno, strerror(errno));
	}
}

static void iv_fd_epoll_flush_pending(struct iv_state *st)
{
	while (!iv_list_empty(&st->u.epoll.notify)) {
		struct iv_fd_ *fd;

		fd = iv_list_entry(st->u.epoll.notify.next,
				   struct iv_fd_, list_notify);

		iv_fd_epoll_flush_one(st, fd);
	}
}

static void iv_fd_epoll_poll(struct iv_state *st,
			     struct iv_list_head *active, struct timespec *to)
{
	struct epoll_event batch[st->numfds ? : 1];
	int msec;
	int ret;
	int i;

	iv_fd_epoll_flush_pending(st);

	msec = 1000 * to->tv_sec + ((to->tv_nsec + 999999) / 1000000);

	ret = epoll_wait(st->u.epoll.epoll_fd, batch, ARRAY_SIZE(batch), msec);
	if (ret < 0) {
		if (errno == EINTR)
			return;

		iv_fatal("iv_fd_epoll_poll: got error %d[%s]", errno,
			 strerror(errno));
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

static void iv_fd_epoll_unregister_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	if (!iv_list_empty(&fd->list_notify))
		iv_fd_epoll_flush_one(st, fd);
}

static void iv_fd_epoll_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	iv_list_del_init(&fd->list_notify);
	if (fd->registered_bands != fd->wanted_bands)
		iv_list_add_tail(&fd->list_notify, &st->u.epoll.notify);
}

static int iv_fd_epoll_notify_fd_sync(struct iv_state *st, struct iv_fd_ *fd)
{
	return __iv_fd_epoll_flush_one(st, fd);
}

static void iv_fd_epoll_deinit(struct iv_state *st)
{
	close(st->u.epoll.epoll_fd);
}


struct iv_fd_poll_method iv_fd_poll_method_epoll = {
	.name		= "epoll",
	.init		= iv_fd_epoll_init,
	.poll		= iv_fd_epoll_poll,
	.unregister_fd	= iv_fd_epoll_unregister_fd,
	.notify_fd	= iv_fd_epoll_notify_fd,
	.notify_fd_sync	= iv_fd_epoll_notify_fd_sync,
	.deinit		= iv_fd_epoll_deinit,
};
