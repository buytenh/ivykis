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

#if defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__NetBSD_) || defined(__OpenBSD__)

#warning somebody should make POLLERR work

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include "iv_private.h"

#define POLL_BATCH_SIZE		(1024)
#define UPLOAD_QUEUE_SIZE	(1024)

static struct list_head		all;
static int			kqueue_fd;
static struct kevent		*upload_queue;
static int			upload_entries;


static int iv_kqueue_init(int maxfd)
{
	kqueue_fd = kqueue();
	if (kqueue_fd < 0)
		return -1;

	upload_queue = malloc(UPLOAD_QUEUE_SIZE * sizeof(*upload_queue));
	if (upload_queue == NULL) {
		close(kqueue_fd);
		return -1;
	}

	INIT_LIST_HEAD(&all);
	upload_entries = 0;

	return 0;
}

static void queue(u_int ident, short filter, u_short flags,
		  u_int fflags, int data, void *udata)
{
	if (upload_entries == UPLOAD_QUEUE_SIZE) {
		struct timespec to = { 0, 0 };
		int ret;

		do {
			ret = kevent(kqueue_fd, upload_queue,
				     upload_entries, NULL, 0, &to);
		} while (ret < 0 && errno == EINTR);

		if (ret < 0) {
			syslog(LOG_CRIT, "queue: got error %d[%s]",
			       errno, strerror(errno));
			abort();
		}

		upload_entries = 0;
	}

	EV_SET(&upload_queue[upload_entries], ident, filter, flags,
	       fflags, data, udata);
	upload_entries++;
}

static void iv_kqueue_poll(int timeout)
{
	struct kevent batch[POLL_BATCH_SIZE];
	int i;
	int maxevents;
	int ret;
	struct timespec to;

	maxevents = sizeof(batch)/sizeof(batch[0]);

	to.tv_sec = timeout / 1000;
	to.tv_nsec = 1000000 * (timeout % 1000);

do_it_again:
	do {
		ret = kevent(kqueue_fd, upload_queue, upload_entries,
			     batch, maxevents, &to);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_kqueue_poll: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}

	upload_entries = 0;

	for (i=0;i<ret;i++) {
		struct iv_fd *fd;

		fd = batch[i].udata;
		if (batch[i].filter == EVFILT_READ) {
			iv_fd_make_ready(fd, FD_ReadyIn);
		} else if (batch[i].filter == EVFILT_WRITE) {
			iv_fd_make_ready(fd, FD_ReadyOut);
		} else {
			syslog(LOG_CRIT, "iv_kqueue_poll: got message from "
					 "filter %d", batch[i].filter);
			abort();
		}
	}

	if (ret == maxevents) {
		to.tv_sec = 0;
		to.tv_nsec = 0;
		goto do_it_again;
	}
}

static void iv_kqueue_register_fd(struct iv_fd *fd)
{
	list_add_tail(&(fd->list_all), &all);

	queue(fd->fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR,
	      0, 0, (void *)fd);
	queue(fd->fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_CLEAR,
	      0, 0, (void *)fd);
}

static void iv_kqueue_unregister_fd(struct iv_fd *fd)
{
	list_del_init(&(fd->list_all));

	queue(fd->fd, EVFILT_READ, EV_DELETE, 0, 0, (void *)fd);
	queue(fd->fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void *)fd);
}

static void iv_kqueue_deinit(void)
{
	free(upload_queue);
	close(kqueue_fd);
}


struct iv_poll_method iv_method_kqueue = {
	name:			"kqueue",
	init:			iv_kqueue_init,
	poll:			iv_kqueue_poll,
	register_fd:		iv_kqueue_register_fd,
	reregister_fd:		NULL,
	unregister_fd:		iv_kqueue_unregister_fd,
	deinit:			iv_kqueue_deinit,
};
#endif
