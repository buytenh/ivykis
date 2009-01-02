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

#ifdef sun

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

#define HASH_SIZE		(512)
#define UPLOAD_QUEUE_SIZE	(1024)

static struct list_head		*all;
static struct pollfd		*batch;
static int			batch_size;
static int			poll_fd;
static struct pollfd		*upload_queue;
static int			upload_entries;


static unsigned int __fd_hash(unsigned int fd)
{
	return fd % HASH_SIZE;
}

static struct iv_fd *find_fd(int fd)
{
	int hash = __fd_hash(fd);
	struct list_head *lh;
	struct iv_fd *ret = NULL;

	list_for_each(lh, &all[hash]) {
		struct iv_fd *f;

		f = list_entry(lh, struct iv_fd, list_all);
		if (f->fd == fd) {
			ret = f;
			break;
		}
	}

	return ret;
}


/* interface ****************************************************************/
static int iv_dev_poll_init(int maxfd)
{
	int i;

	poll_fd = open("/dev/poll", O_RDWR);
	if (poll_fd < 0)
		return -1;

	all = malloc(HASH_SIZE * sizeof(*all));
	if (all == NULL) {
		close(poll_fd);
		return -1;
	}

	batch = malloc(maxfd * sizeof(*batch));
	if (batch == NULL) {
		free(all);
		close(poll_fd);
		return -1;
	}

	upload_queue = malloc(UPLOAD_QUEUE_SIZE * sizeof(*upload_queue));
	if (upload_queue == NULL) {
		free(batch);
		free(all);
		close(poll_fd);
		return -1;
	}

	for (i = 0; i < HASH_SIZE; i++)
		INIT_LIST_HEAD(&all[i]);
	batch_size = maxfd;
	upload_entries = 0;

	return 0;
}

static void flush_upload_queue(void)
{
	unsigned char *base;
	int left;

	base = (unsigned char *)upload_queue;
	left = upload_entries * sizeof(struct pollfd);

	while (left) {
		int ret;

		do {
			ret = write(poll_fd, base, left);
		} while (ret < 0 && errno == EINTR);

		if (ret < 0) {
			syslog(LOG_CRIT, "flush_upload_queue: got error %d[%s]",
			       errno, strerror(errno));
			abort();
		}

		base += ret;
		left -= ret;
	}

	upload_entries = 0;
}

static int wanted_bits(struct iv_fd *fd, int regd)
{
	int wanted;
	int handler;
	int ready;

	/*
	 * We are being unregistered?
	 */
	if (list_empty(&fd->list_all))
		return 0;

	/*
	 * Error condition raised?
	 */
	if (fd->flags & (1 << FD_ReadyErr))
		return 0;

	wanted = 4;

	handler = !!(fd->handler_in != NULL);
	ready = !!(fd->flags & (1 << FD_ReadyIn));
	if ((handler && !ready) || ((regd & 1) && (handler || !ready)))
		wanted |= 1;

	handler = !!(fd->handler_out != NULL);
	ready = !!(fd->flags & (1 << FD_ReadyOut));
	if ((handler && !ready) || ((regd & 2) && (handler || !ready)))
		wanted |= 2;

	return wanted;
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = 0;
	if (bits & 4) {
		mask = POLLERR;
		if (bits & 1)
			mask |= POLLIN | POLLHUP;
		if (bits & 2)
			mask |= POLLOUT;
	}

	return mask;
}

static void queue(struct iv_fd *fd)
{
	int regd;
	int wanted;

	if (upload_entries > UPLOAD_QUEUE_SIZE - 2)
		flush_upload_queue();

	regd = (fd->flags >> FD_RegisteredIn) & 7;
	wanted = wanted_bits(fd, regd);

	if (regd & ~wanted) {
		upload_queue[upload_entries].fd = fd->fd;
		upload_queue[upload_entries].events = POLLREMOVE;
		upload_entries++;
	}

	if (wanted) {
		upload_queue[upload_entries].fd = fd->fd;
		upload_queue[upload_entries].events = bits_to_poll_mask(wanted);
		upload_entries++;
	}

	fd->flags &= ~(7 << FD_RegisteredIn);
	fd->flags |= wanted << FD_RegisteredIn;
}

static void iv_dev_poll_poll(int msec)
{
	int i;
	int ret;

#if 0
	/*
	 * @@@ Is this necessary?
	 * @@@ This is ugly and dependent on clock tick granularity.
	 */
	if (msec)
		msec += (1000/100) - 1;
#endif

	if (upload_entries)
		flush_upload_queue();

	do {
		struct dvpoll dvp;

		dvp.dp_fds = batch;
		dvp.dp_nfds = batch_size;
		dvp.dp_timeout = msec;
		ret = ioctl(poll_fd, DP_POLL, &dvp);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_dev_poll_poll: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}

	for (i = 0; i < ret; i++) {
		struct iv_fd *fd;

		fd = find_fd(batch[i].fd);
		if (fd == NULL) {
			syslog(LOG_CRIT, "iv_dev_poll_poll: got event for "
					 "unknown fd %d", batch[i].fd);
			abort();
		}

		if (batch[i].revents) {
			int should_queue;

			should_queue = 0;
			if (batch[i].revents & (POLLIN | POLLERR | POLLHUP)) {
				iv_fd_make_ready(fd, FD_ReadyIn);
				if (fd->handler_in == NULL)
					should_queue = 1;
			}
			if (batch[i].revents & (POLLOUT | POLLERR)) {
				iv_fd_make_ready(fd, FD_ReadyOut);
				if (fd->handler_out == NULL)
					should_queue = 1;
			}
			if (batch[i].revents & POLLERR) {
				iv_fd_make_ready(fd, FD_ReadyErr);
				should_queue = 1;
			}

			if (should_queue)
				queue(fd);
		}
	}
}

static void iv_dev_poll_register_fd(struct iv_fd *fd)
{
	list_add_tail(&fd->list_all, &all[__fd_hash(fd->fd)]);
	queue(fd);
}

static void iv_dev_poll_reregister_fd(struct iv_fd *fd)
{
	queue(fd);
}

static void iv_dev_poll_unregister_fd(struct iv_fd *fd)
{
	list_del_init(&fd->list_all);
	queue(fd);
}

static void iv_dev_poll_deinit(void)
{
	free(upload_queue);
	free(batch);
	free(all);
	close(poll_fd);
}


struct iv_poll_method iv_method_dev_poll = {
	.name		= "dev_poll",
	.init		= iv_dev_poll_init,
	.poll		= iv_dev_poll_poll,
	.register_fd	= iv_dev_poll_register_fd,
	.reregister_fd	= iv_dev_poll_reregister_fd,
	.unregister_fd	= iv_dev_poll_unregister_fd,
	.deinit		= iv_dev_poll_deinit,
};
#endif
