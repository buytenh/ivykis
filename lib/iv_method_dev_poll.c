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
#include <syslog.h>
#include <sys/devpoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include "iv_private.h"

#define UPLOAD_QUEUE_SIZE	(1024)

static int iv_dev_poll_init(struct iv_state *st, int maxfd)
{
	int poll_fd;
	struct pollfd *upload_queue;

	poll_fd = open("/dev/poll", O_RDWR);
	if (poll_fd < 0)
		return -1;

	upload_queue = malloc(UPLOAD_QUEUE_SIZE * sizeof(*upload_queue));
	if (upload_queue == NULL) {
		close(poll_fd);
		return -1;
	}

	INIT_IV_AVL_TREE(&st->dev_poll.fds, iv_fd_avl_compare);
	st->dev_poll.poll_fd = poll_fd;
	st->dev_poll.upload_queue = upload_queue;
	st->dev_poll.upload_entries = 0;

	return 0;
}

static void flush_upload_queue(struct iv_state *st)
{
	unsigned char *base;
	int left;

	base = (unsigned char *)st->dev_poll.upload_queue;
	left = st->dev_poll.upload_entries * sizeof(struct pollfd);

	while (left) {
		int ret;

		do {
			ret = write(st->dev_poll.poll_fd, base, left);
		} while (ret < 0 && errno == EINTR);

		if (ret < 0) {
			syslog(LOG_CRIT, "flush_upload_queue: got error %d[%s]",
			       errno, strerror(errno));
			abort();
		}

		base += ret;
		left -= ret;
	}

	st->dev_poll.upload_entries = 0;
}

static void
iv_dev_poll_poll(struct iv_state *st, struct list_head *active, int msec)
{
	struct pollfd batch[st->numfds];
	struct dvpoll dvp;
	int ret;
	int i;

#if 0
	/*
	 * @@@ Is this necessary?
	 * @@@ This is ugly and dependent on clock tick granularity.
	 */
	if (msec)
		msec += (1000/100) - 1;
#endif

	if (st->dev_poll.upload_entries)
		flush_upload_queue(st);

	dvp.dp_fds = batch;
	dvp.dp_nfds = st->numfds;
	dvp.dp_timeout = msec;

	ret = ioctl(st->dev_poll.poll_fd, DP_POLL, &dvp);
	if (ret < 0) {
		if (errno == EINTR)
			return;

		syslog(LOG_CRIT, "iv_dev_poll_poll: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}

	for (i = 0; i < ret; i++) {
		struct iv_fd_ *fd;
		int revents;

		fd = iv_fd_avl_find(&st->dev_poll.fds, batch[i].fd);
		if (fd == NULL) {
			syslog(LOG_CRIT, "iv_dev_poll_poll: got event for "
					 "unknown fd %d", batch[i].fd);
			abort();
		}

		revents = batch[i].revents;

		if (revents & (POLLIN | POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKIN);

		if (revents & (POLLOUT | POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKOUT);

		if (revents & (POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKERR);
	}
}

static void iv_dev_poll_register_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	int ret;

	ret = iv_avl_tree_insert(&st->dev_poll.fds, &fd->avl_node);
	if (ret) {
		syslog(LOG_CRIT, "iv_dev_poll_register_fd: got error %d[%s]",
		       ret, strerror(ret));
		abort();
	}
}

static void iv_dev_poll_unregister_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	iv_avl_tree_delete(&st->dev_poll.fds, &fd->avl_node);

	flush_upload_queue(st);
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = 0;
	if (bits & MASKIN)
		mask |= POLLIN;
	if (bits & MASKOUT)
		mask |= POLLOUT;

	return mask;
}

static void iv_dev_poll_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	struct pollfd *upload_queue;
	int upload_entries;

	if (st->dev_poll.upload_entries > UPLOAD_QUEUE_SIZE - 2)
		flush_upload_queue(st);

	upload_queue = st->dev_poll.upload_queue;
	upload_entries = st->dev_poll.upload_entries;

	if (fd->registered_bands & ~fd->wanted_bands) {
		upload_queue[upload_entries].fd = fd->fd;
		upload_queue[upload_entries].events = POLLREMOVE;
		upload_entries++;
	}

	if (fd->wanted_bands) {
		upload_queue[upload_entries].fd = fd->fd;
		upload_queue[upload_entries].events =
			bits_to_poll_mask(fd->wanted_bands);
		upload_entries++;
	}

	st->dev_poll.upload_entries = upload_entries;

	fd->registered_bands = fd->wanted_bands;
}

static void iv_dev_poll_deinit(struct iv_state *st)
{
	free(st->dev_poll.upload_queue);
	close(st->dev_poll.poll_fd);
}


struct iv_poll_method iv_method_dev_poll = {
	.name		= "dev_poll",
	.init		= iv_dev_poll_init,
	.poll		= iv_dev_poll_poll,
	.register_fd	= iv_dev_poll_register_fd,
	.unregister_fd	= iv_dev_poll_unregister_fd,
	.notify_fd	= iv_dev_poll_notify_fd,
	.deinit		= iv_dev_poll_deinit,
};
