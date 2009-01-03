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

static int wanted_bits(struct iv_fd_ *fd)
{
	int wanted;
	int handler;
	int ready;
	int regd;

	wanted = 0;

	handler = !!(fd->handler_in != NULL);
	ready = !!(fd->ready_bands & MASKIN);
	regd = !!(fd->registered_bands & MASKIN);
	if ((handler && !ready) || (regd && (handler || !ready)))
		wanted |= MASKIN;

	handler = !!(fd->handler_out != NULL);
	ready = !!(fd->ready_bands & MASKOUT);
	regd = !!(fd->registered_bands & MASKOUT);
	if ((handler && !ready) || (regd && (handler || !ready)))
		wanted |= MASKOUT;

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

static void queue(struct iv_fd_ *fd)
{
	int wanted;

	wanted = wanted_bits(fd);

	if (fd->registered_bands != wanted) {
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

		fd->registered_bands = wanted;
	}
}

/*
 * Deleting an fd from an epoll set causes all pending events to be
 * cleared from the queue.  Therefore, we need no barrier operation
 * after the epoll_ctl below.  (Recall that the returned epoll_event
 * structures contain opaque-to-the-kernel userspace pointers, which
 * are dereferenced in the event handler without validation.)
 */
static void internal_unregister(struct iv_fd_ *fd)
{
	struct epoll_event event;
	int ret;

	event.data.ptr = fd;
	event.events = 0;
	do {
		ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd->fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_epoll_unregister_fd: got error "
				 "%d[%s]", errno, strerror(errno));
		abort();
	}
}

static void iv_epoll_poll(int msec)
{
	int i;
	int ret;

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
		if (batch[i].events) {
			int should_queue;

			should_queue = 0;
			if (batch[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
				iv_fd_make_ready(fd, MASKIN);
				if (fd->handler_in == NULL)
					should_queue = 1;
			}
			if (batch[i].events & (EPOLLOUT | EPOLLERR)) {
				iv_fd_make_ready(fd, MASKOUT);
				if (fd->handler_out == NULL)
					should_queue = 1;
			}
			if (batch[i].events & EPOLLERR) {
				iv_fd_make_ready(fd, MASKERR);
				internal_unregister(fd);
			} else if (should_queue) {
				queue(fd);
			}
		}
	}
}

static void iv_epoll_register_fd(struct iv_fd_ *fd)
{
	struct epoll_event event;
	int wanted;
	int ret;

	list_add_tail(&fd->list_all, &all);

	wanted = wanted_bits(fd);

	event.data.ptr = fd;
	event.events = bits_to_poll_mask(wanted);
	do {
		ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd->fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_epoll_register_fd: got error "
				 "%d[%s]", errno, strerror(errno));
		abort();
	}

	fd->registered_bands = wanted;
}

static void iv_epoll_reregister_fd(struct iv_fd_ *fd)
{
	if (!(fd->ready_bands & MASKERR))
		queue(fd);
}

static void iv_epoll_unregister_fd(struct iv_fd_ *fd)
{
	if (!(fd->ready_bands & MASKERR))
		internal_unregister(fd);
	list_del_init(&fd->list_all);
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
	.register_fd	= iv_epoll_register_fd,
	.reregister_fd	= iv_epoll_reregister_fd,
	.unregister_fd	= iv_epoll_unregister_fd,
	.deinit		= iv_epoll_deinit,
};
#endif
