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

#define PAGE_SIZE (getpagesize())

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include "iv_private.h"
#include "eventpoll.h"

#ifndef POLLREMOVE
#define POLLREMOVE		0x1000
#endif

#define HASH_SIZE		(512)
#define POLL_BATCH_SIZE		(1024)
#define UPLOAD_QUEUE_SIZE	(1024)

static struct list_head		*all;
static int			epoll_fd;
static unsigned char		*epoll_io_desc;
static int			epoll_io_desc_size;
static struct pollfd		*upload_queue;
static int			upload_entries;
static struct list_head		undetermined_queue;


static unsigned int __fd_hash(unsigned int fd)
{
	return fd % HASH_SIZE;
}

static struct iv_fd *find_fd(int fd)
{
	int hash = __fd_hash(fd);
	struct list_head *lh;
	struct iv_fd *ret = NULL;

	list_for_each (lh, &all[hash]) {
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
static int iv_dev_epoll_init(int maxfd)
{
	int i;

	epoll_fd = open("/dev/epoll", O_RDWR);
	if (epoll_fd < 0)
		return -1;

	if (ioctl(epoll_fd, EP_ALLOC, maxfd) < 0) {
		close(epoll_fd);
		return -1;
	}

	epoll_io_desc_size = EP_MAP_SIZE(maxfd);
	epoll_io_desc = mmap(NULL, epoll_io_desc_size,
		PROT_READ | PROT_WRITE, MAP_PRIVATE, epoll_fd, 0);
	if (epoll_io_desc == MAP_FAILED) {
		close(epoll_fd);
		return -1;
	}

	all = malloc(HASH_SIZE * sizeof(*all));
	if (all == NULL) {
		munmap(epoll_io_desc, epoll_io_desc_size);
		close(epoll_fd);
		return -1;
	}

	upload_queue = malloc(UPLOAD_QUEUE_SIZE * sizeof(*upload_queue));
	if (upload_queue == NULL) {
		free(all);
		munmap(epoll_io_desc, epoll_io_desc_size);
		close(epoll_fd);
		return -1;
	}

	for (i=0;i<HASH_SIZE;i++)
		INIT_LIST_HEAD(&all[i]);
	upload_entries = 0;
	INIT_LIST_HEAD(&undetermined_queue);

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
			ret = write(epoll_fd, base, left);
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

static void queue(int fd, short events)
{
	if (upload_entries == UPLOAD_QUEUE_SIZE)
		flush_upload_queue();

	upload_queue[upload_entries].fd = fd;
	upload_queue[upload_entries].events = events;
	upload_entries++;
}

static void flush_undetermined_poll_list(struct pollfd *pfds, int qcount)
{
	int i;
	int ret;

	do {
		ret = poll(pfds, qcount, 0);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "flush_undetermined_poll_list: got error "
				 "%d[%s]", errno, strerror(errno));
		abort();
	}

	for (i=0;i<qcount;i++) {
		struct iv_fd *fd;
		int revents;

		fd = list_entry(undetermined_queue.next, struct iv_fd,
				list_all);

#if IV_DEBUG
		if (fd->fd != pfds[i].fd) {
			syslog(LOG_CRIT, "flush_undetermined_poll_list: just "
					 "puked on myself... eeeeeeeeeeew");
			abort();
		}
#endif

		revents = pfds[i].revents;
		if (revents & (POLLIN | POLLERR | POLLHUP))
			iv_fd_make_ready(fd, FD_ReadyIn);
		if (revents & (POLLOUT | POLLERR))
			iv_fd_make_ready(fd, FD_ReadyOut);
		if (revents & POLLERR)
			iv_fd_make_ready(fd, FD_ReadyErr);

		list_del(&(fd->list_all));
		list_add_tail(&(fd->list_all), &all[__fd_hash(fd->fd)]);
	}
}

static void flush_undetermined_queue(void)
{
	struct pollfd pfds[POLL_BATCH_SIZE];
	struct list_head *lh;
	struct list_head *lh2;
	int qcount;

	qcount = 0;
	list_for_each_safe (lh, lh2, &undetermined_queue) {
		struct iv_fd *fd;

		fd = list_entry(lh, struct iv_fd, list_all);

		pfds[qcount].fd = fd->fd;
		pfds[qcount].events = POLLIN | POLLERR | POLLHUP | POLLOUT;
		if (++qcount == POLL_BATCH_SIZE) {
			flush_undetermined_poll_list(pfds, qcount);
			qcount = 0;
		}
	}

	flush_undetermined_poll_list(pfds, qcount);
}

static void iv_dev_epoll_poll(int timeout)
{
	struct evpoll evp;
	int i;
	int num;
	struct pollfd *pfd;

	if (upload_entries)
		flush_upload_queue();

	if (!list_empty(&undetermined_queue)) {
		flush_undetermined_queue();
		if (!list_empty(active))
			return;
	}

	/* @@@ This is ugly and dependent on clock tick granularity.  */
	if (timeout)
		timeout += (1000/100) - 1;

	do {
		evp.ep_timeout = timeout;
		evp.ep_resoff = 0;
		num = ioctl(epoll_fd, EP_POLL, &evp);
	} while (num < 0 && errno == EINTR);

	if (num < 0) {
		syslog(LOG_CRIT, "iv_dev_epoll_poll: got error %d[%s]", errno,
		       strerror(errno));
		abort();
	}

	pfd = (struct pollfd *)(epoll_io_desc + evp.ep_resoff);
	for (i=0;i<num;i++) {
		struct iv_fd *fd;

		fd = find_fd(pfd[i].fd);
		if (fd == NULL) {
			syslog(LOG_CRIT, "iv_dev_epoll_poll: got event for "
					 "unknown fd %d", pfd[i].fd);
			abort();
		}

		if (pfd[i].revents & (POLLIN | POLLERR | POLLHUP))
			iv_fd_make_ready(fd, FD_ReadyIn);
		if (pfd[i].revents & (POLLOUT | POLLERR))
			iv_fd_make_ready(fd, FD_ReadyOut);
		if (pfd[i].revents & POLLERR)
			iv_fd_make_ready(fd, FD_ReadyErr);
	}
}

static void iv_dev_epoll_register_fd(struct iv_fd *fd)
{
	list_add_tail(&(fd->list_all), &undetermined_queue);
	queue(fd->fd, POLLIN | POLLERR | POLLHUP | POLLOUT);
}

/* Someone might unregister an fd while there is still a queued event for
 * it, but this is not a problem, since the actual deleting from the epoll
 * set causes all pending events to be cleared from the queue.  Therefore,
 * we need no barrier operation after the queue() below.  */
static void iv_dev_epoll_unregister_fd(struct iv_fd *fd)
{
	queue(fd->fd, POLLREMOVE);
	list_del_init(&(fd->list_all));
}

static void iv_dev_epoll_deinit(void)
{
	free(upload_queue);
	free(all);
	munmap(epoll_io_desc, epoll_io_desc_size);
	close(epoll_fd);
}


struct iv_poll_method iv_method_dev_epoll = {
	name:			"dev_epoll",
	init:			iv_dev_epoll_init,
	poll:			iv_dev_epoll_poll,
	register_fd:		iv_dev_epoll_register_fd,
	reregister_fd:		NULL,
	unregister_fd:		iv_dev_epoll_unregister_fd,
	deinit:			iv_dev_epoll_deinit,
};
#endif
