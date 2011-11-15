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
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "iv_private.h"

#define UPLOAD_BATCH	1024

static int iv_kqueue_init(struct iv_state *st, int maxfd)
{
	int kqueue_fd;
	int flags;

	kqueue_fd = kqueue();
	if (kqueue_fd < 0)
		return -1;

	flags = fcntl(kqueue_fd, F_GETFD);
	if (!(flags & FD_CLOEXEC)) {
		flags |= FD_CLOEXEC;
		fcntl(kqueue_fd, F_SETFD, flags);
	}

	st->kqueue.kqueue_fd = kqueue_fd;
	INIT_LIST_HEAD(&st->kqueue.notify);

	return 0;
}

static void
iv_kqueue_queue_one(struct kevent *kev, int *_num, struct iv_fd_ *fd)
{
	int num;
	int wanted;
	int regd;

	num = *_num;
	wanted = fd->wanted_bands;
	regd = fd->registered_bands;

	if (!(wanted & MASKIN) && (regd & MASKIN)) {
		EV_SET(&kev[num], fd->fd, EVFILT_READ, EV_DELETE,
		       0, 0, (intptr_t)fd);
		num++;
	} else if ((wanted & MASKIN) && !(regd & MASKIN)) {
		EV_SET(&kev[num], fd->fd, EVFILT_READ, EV_ADD | EV_ENABLE,
		       0, 0, (intptr_t)fd);
		num++;
	}

	if (!(wanted & MASKOUT) && (regd & MASKOUT)) {
		EV_SET(&kev[num], fd->fd, EVFILT_WRITE, EV_DELETE,
		       0, 0, (intptr_t)fd);
		num++;
	} else if ((wanted & MASKOUT) && !(regd & MASKOUT)) {
		EV_SET(&kev[num], fd->fd, EVFILT_WRITE, EV_ADD | EV_ENABLE,
		       0, 0, (intptr_t)fd);
		num++;
	}

	fd->registered_bands = fd->wanted_bands;

	*_num = num;
}

static void
iv_kqueue_upload(struct iv_state *st, struct kevent *kev, int size, int *num)
{
	struct timespec to = { 0, 0 };

	*num = 0;

	while (!list_empty(&st->kqueue.notify)) {
		struct list_head *lh;
		struct iv_fd_ *fd;

		if (*num > size - 2) {
			int ret;

			ret = kevent(st->kqueue.kqueue_fd, kev, *num,
				     NULL, 0, &to);
			if (ret < 0) {
				syslog(LOG_CRIT, "iv_kqueue_upload: got error "
				       "%d[%s]", errno, strerror(errno));
				abort();
			}

			*num = 0;
		}

		lh = st->kqueue.notify.next;
		list_del_init(lh);

		fd = list_entry(lh, struct iv_fd_, list_notify);

		iv_kqueue_queue_one(kev, num, fd);
	}
}

static void
iv_kqueue_poll(struct iv_state *st, struct list_head *active, int msec)
{
	struct kevent kev[UPLOAD_BATCH];
	int num;
	struct timespec to;
	struct kevent batch[st->numfds ? : 1];
	int ret;
	int i;

	iv_kqueue_upload(st, kev, UPLOAD_BATCH, &num);

	/*
	 * Valgrind 3.5.0 as supplied with FreeBSD 8.1-RELEASE ports
	 * doesn't understand that kevent(2) fills in kevent udata on
	 * return, and labels our subsequent use of it as "Conditional
	 * jump or move depends on uninitialised value(s)".  Zero the
	 * udata fields here as an ugly workaround.
	 */
	for (i = 0; i < (st->numfds ? : 1); i++)
		batch[i].udata = 0;

	to.tv_sec = msec / 1000;
	to.tv_nsec = 1000000 * (msec % 1000);

	ret = kevent(st->kqueue.kqueue_fd, kev, num,
		     batch, st->numfds ? : 1, &to);
	if (ret < 0) {
		if (errno == EINTR)
			return;

		syslog(LOG_CRIT, "iv_kqueue_poll: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}

	for (i = 0; i < ret; i++) {
		struct iv_fd_ *fd;

		fd = (void *)batch[i].udata;
		if (batch[i].filter == EVFILT_READ) {
			iv_fd_make_ready(active, fd, MASKIN);
		} else if (batch[i].filter == EVFILT_WRITE) {
			iv_fd_make_ready(active, fd, MASKOUT);
		} else {
			syslog(LOG_CRIT, "iv_kqueue_poll: got message from "
					 "filter %d", batch[i].filter);
			abort();
		}
	}
}

static void iv_kqueue_upload_all(struct iv_state *st)
{
	struct kevent kev[UPLOAD_BATCH];
	int num;

	iv_kqueue_upload(st, kev, UPLOAD_BATCH, &num);

	if (num) {
		struct timespec to = { 0, 0 };
		int ret;

		ret = kevent(st->kqueue.kqueue_fd, kev, num, NULL, 0, &to);
		if (ret < 0) {
			syslog(LOG_CRIT, "iv_kqueue_upload_all: got error "
			       "%d[%s]", errno, strerror(errno));
			abort();
		}
	}
}

static void iv_kqueue_unregister_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	if (!list_empty(&fd->list_notify))
		iv_kqueue_upload_all(st);
}

static void iv_kqueue_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	list_del_init(&fd->list_notify);
	if (fd->registered_bands != fd->wanted_bands)
		list_add_tail(&fd->list_notify, &st->kqueue.notify);
}

static void iv_kqueue_deinit(struct iv_state *st)
{
	close(st->kqueue.kqueue_fd);
}


struct iv_poll_method iv_method_kqueue = {
	.name		= "kqueue",
	.init		= iv_kqueue_init,
	.poll		= iv_kqueue_poll,
	.unregister_fd	= iv_kqueue_unregister_fd,
	.notify_fd	= iv_kqueue_notify_fd,
	.deinit		= iv_kqueue_deinit,
};
