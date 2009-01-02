/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
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


static int iv_epoll_lt_init(int maxfd)
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

static int wanted_bits(struct iv_fd *fd, int regd)
{
	int wanted;
	int handler;
	int ready;

	wanted = 0;

	handler = !!(fd->handler_in != NULL);
	ready = !!(fd->flags & 1 << FD_ReadyIn);
	if ((handler && !ready) || ((regd & 1) && (handler || !ready)))
		wanted |= 1;

	handler = !!(fd->handler_out != NULL);
	ready = !!(fd->flags & 1 << FD_ReadyOut);
	if ((handler && !ready) || ((regd & 2) && (handler || !ready)))
		wanted |= 2;

	return wanted;
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = EPOLLERR;
	if (bits & 1)
		mask |= EPOLLIN | EPOLLHUP;
	if (bits & 2)
		mask |= EPOLLOUT;

	return mask;
}

static void queue(struct iv_fd *fd)
{
	int regd;
	int wanted;

	regd = (fd->flags >> FD_RegisteredIn) & 3;
	wanted = wanted_bits(fd, regd);

	if (regd != wanted) {
		struct epoll_event event;
		int ret;

		event.data.ptr = fd;
		event.events = bits_to_poll_mask(wanted);
		do {
			ret = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd->fd,
					&event);
		} while (ret < 0 && errno == EINTR);

		if (ret < 0) {
			syslog(LOG_CRIT, "queue: got error %d[%s]", errno,
			       strerror(errno));
			abort();
		}
	}

	fd->flags &= ~(7 << FD_RegisteredIn);
	fd->flags |= wanted << FD_RegisteredIn;
}

/*
 * Deleting an fd from an epoll set causes all pending events to be
 * cleared from the queue.  Therefore, we need no barrier operation
 * after the epoll_ctl below.  (Recall that the returned epoll_event
 * structures contain opaque-to-the-kernel userspace pointers, which
 * are dereferenced in the event handler without validation.)
 */
static void internal_unregister(struct iv_fd *fd)
{
	struct epoll_event event;
	int ret;

	event.data.ptr = fd;
	event.events = 0;
	do {
		ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd->fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_epoll_lt_unregister_fd: got error "
				 "%d[%s]", errno, strerror(errno));
		abort();
	}
}

static void iv_epoll_lt_poll(int msec)
{
	int i;
	int ret;

	do {
		ret = epoll_wait(epoll_fd, batch, batch_size, msec);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_epoll_lt_poll: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}

	for (i = 0; i < ret; i++) {
		struct iv_fd *fd;

		fd = batch[i].data.ptr;
		if (batch[i].events) {
			int should_queue;

			should_queue = 0;
			if (batch[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
				iv_fd_make_ready(fd, FD_ReadyIn);
				if (fd->handler_in == NULL)
					should_queue = 1;
			}
			if (batch[i].events & (EPOLLOUT | EPOLLERR)) {
				iv_fd_make_ready(fd, FD_ReadyOut);
				if (fd->handler_out == NULL)
					should_queue = 1;
			}
			if (batch[i].events & EPOLLERR) {
				iv_fd_make_ready(fd, FD_ReadyErr);
				internal_unregister(fd);
			} else if (should_queue) {
				queue(fd);
			}
		}
	}
}

static void iv_epoll_lt_register_fd(struct iv_fd *fd)
{
	struct epoll_event event;
	int wanted;
	int ret;

	list_add_tail(&fd->list_all, &all);

	wanted = wanted_bits(fd, 0);

	event.data.ptr = fd;
	event.events = bits_to_poll_mask(wanted);
	do {
		ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd->fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_epoll_lt_register_fd: got error "
				 "%d[%s]", errno, strerror(errno));
		abort();
	}

	fd->flags |= wanted << FD_RegisteredIn;
}

static void iv_epoll_lt_reregister_fd(struct iv_fd *fd)
{
	if (!(fd->flags & (1 << FD_ReadyErr)))
		queue(fd);
}

static void iv_epoll_lt_unregister_fd(struct iv_fd *fd)
{
	if (!(fd->flags & (1 << FD_ReadyErr)))
		internal_unregister(fd);
	list_del_init(&fd->list_all);
}

static void iv_epoll_lt_deinit(void)
{
	free(batch);
	close(epoll_fd);
}


struct iv_poll_method iv_method_epoll_lt = {
	.name		= "epoll_lt",
	.init		= iv_epoll_lt_init,
	.poll		= iv_epoll_lt_poll,
	.register_fd	= iv_epoll_lt_register_fd,
	.reregister_fd	= iv_epoll_lt_reregister_fd,
	.unregister_fd	= iv_epoll_lt_unregister_fd,
	.deinit		= iv_epoll_lt_deinit,
};
#endif
