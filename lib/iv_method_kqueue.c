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
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "iv_private.h"

#define UPLOAD_BATCH	1024

static int iv_kqueue_init(struct iv_state *st)
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
	INIT_IV_LIST_HEAD(&st->kqueue.notify);

	return 0;
}

static void
iv_kqueue_queue_one(struct kevent *kev, int *_num, struct iv_fd_ *fd)
{
	int num;
	int wanted;
	int regd;

	iv_list_del_init(&fd->list_notify);

	num = *_num;
	wanted = fd->wanted_bands;
	regd = fd->registered_bands;

	if (!(wanted & MASKIN) && (regd & MASKIN)) {
		EV_SET(&kev[num], fd->fd, EVFILT_READ, EV_DELETE,
		       0, 0, (void *)(intptr_t)fd);
		num++;
	} else if ((wanted & MASKIN) && !(regd & MASKIN)) {
		EV_SET(&kev[num], fd->fd, EVFILT_READ, EV_ADD | EV_ENABLE,
		       0, 0, (void *)(intptr_t)fd);
		num++;
	}

	if (!(wanted & MASKOUT) && (regd & MASKOUT)) {
		EV_SET(&kev[num], fd->fd, EVFILT_WRITE, EV_DELETE,
		       0, 0, (void *)(intptr_t)fd);
		num++;
	} else if ((wanted & MASKOUT) && !(regd & MASKOUT)) {
		EV_SET(&kev[num], fd->fd, EVFILT_WRITE, EV_ADD | EV_ENABLE,
		       0, 0, (void *)(intptr_t)fd);
		num++;
	}

	*_num = num;
}

static int kevent_retry(int kq, const struct kevent *changelist, int nchanges)
{
	struct timespec to = { 0, 0 };
	int ret;

	do {
		ret = kevent(kq, changelist, nchanges, NULL, 0, &to);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

static void
iv_kqueue_upload(struct iv_state *st, struct kevent *kev, int size, int *num)
{
	*num = 0;

	while (!iv_list_empty(&st->kqueue.notify)) {
		struct iv_fd_ *fd;

		if (*num > size - 2) {
			int ret;

			ret = kevent_retry(st->kqueue.kqueue_fd, kev, *num);
			if (ret < 0) {
				iv_fatal("iv_kqueue_upload: got error %d[%s]",
					 errno, strerror(errno));
			}

			*num = 0;
		}

		fd = iv_list_entry(st->kqueue.notify.next,
				   struct iv_fd_, list_notify);

		iv_kqueue_queue_one(kev, num, fd);
		fd->registered_bands = fd->wanted_bands;
	}
}

static void iv_kqueue_poll(struct iv_state *st,
			   struct iv_list_head *active, struct timespec *to)
{
	struct kevent kev[UPLOAD_BATCH];
	int num;
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

	ret = kevent(st->kqueue.kqueue_fd, kev, num,
		     batch, st->numfds ? : 1, to);
	if (ret < 0) {
		if (errno == EINTR)
			return;

		iv_fatal("iv_kqueue_poll: got error %d[%s]", errno,
			 strerror(errno));
	}

	for (i = 0; i < ret; i++) {
		struct iv_fd_ *fd;

		fd = (void *)batch[i].udata;
		if (batch[i].filter == EVFILT_READ) {
			iv_fd_make_ready(active, fd, MASKIN);
		} else if (batch[i].filter == EVFILT_WRITE) {
			iv_fd_make_ready(active, fd, MASKOUT);
		} else {
			iv_fatal("iv_kqueue_poll: got message from filter %d",
				 batch[i].filter);
		}
	}
}

static void iv_kqueue_upload_all(struct iv_state *st)
{
	struct kevent kev[UPLOAD_BATCH];
	int num;

	iv_kqueue_upload(st, kev, UPLOAD_BATCH, &num);

	if (num) {
		int ret;

		ret = kevent_retry(st->kqueue.kqueue_fd, kev, num);
		if (ret < 0) {
			iv_fatal("iv_kqueue_upload_all: got error %d[%s]",
				 errno, strerror(errno));
		}
	}
}

static void iv_kqueue_unregister_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	if (!iv_list_empty(&fd->list_notify))
		iv_kqueue_upload_all(st);
}

static void iv_kqueue_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	iv_list_del_init(&fd->list_notify);
	if (fd->registered_bands != fd->wanted_bands)
		iv_list_add_tail(&fd->list_notify, &st->kqueue.notify);
}

static int iv_kqueue_notify_fd_sync(struct iv_state *st, struct iv_fd_ *fd)
{
	struct kevent kev[2];
	int num = 0;
	int ret;

	iv_kqueue_queue_one(kev, &num, fd);
	if (num == 0)
		return 0;

	ret = kevent_retry(st->kqueue.kqueue_fd, kev, num);
	if (ret == 0)
		fd->registered_bands = fd->wanted_bands;

	return ret;
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
	.notify_fd_sync	= iv_kqueue_notify_fd_sync,
	.deinit		= iv_kqueue_deinit,
};
