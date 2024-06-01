/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009, 2012, 2013 Lennert Buytenhek
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
#include <sys/syscall.h>
#include "eventfd-linux.h"
#include "iv_private.h"
#include "mutex.h"

static int epoll_support = 2;

static int epollfd_grab(void)
{
#if (defined(__NR_epoll_create1) || defined(HAVE_EPOLL_CREATE1)) && \
     defined(EPOLL_CLOEXEC)
	if (epoll_support == 2) {
		int ret;

#ifdef __NR_epoll_create1
		ret = syscall(__NR_epoll_create1, EPOLL_CLOEXEC);
#else
		ret = epoll_create1(EPOLL_CLOEXEC);
#endif
		if (ret >= 0 || errno != ENOSYS)
			return ret;

		epoll_support = 1;
	}
#endif

	if (epoll_support) {
		int ret;

		ret = epoll_create(1);
		if (ret >= 0 || errno != ENOSYS) {
			if (ret >= 0)
				iv_fd_set_cloexec(ret);
			return ret;
		}

		epoll_support = 0;
	}

	return -1;
}

static int iv_fd_epoll_init(struct iv_state *st)
{
	int fd;

	fd = epollfd_grab();
	if (fd < 0)
		return -1;

	INIT_IV_LIST_HEAD(&st->u.epoll.notify);
	st->u.epoll.epoll_fd = fd;
	st->u.epoll.timer_fd = -1;

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

#ifdef HAVE_EPOLL_PWAIT2
static int epoll_pwait2_support = 1;
#endif

static int iv_fd_epoll_wait(struct iv_state *st, struct epoll_event *events,
			    int maxevents, const struct timespec *abs)
{
	int epfd = st->u.epoll.epoll_fd;

#ifdef HAVE_EPOLL_PWAIT2
	if (epoll_pwait2_support) {
		struct timespec rel;
		int ret;

		ret = epoll_pwait2(epfd, events, maxevents,
				   to_relative(st, &rel, abs), NULL);

		/*
		 * Some container technologies (at least podman on CentOS
		 * 7 and docker on Debian Buster, according to reports)
		 * cause epoll_pwait2() to return -EPERM.  It is unclear
		 * what security benefits this provides, but we'll have to
		 * handle this by falling back to epoll_wait() just as if
		 * -ENOSYS had been returned.
		 */
		if (ret == 0 || (errno != EPERM && errno != ENOSYS))
			return ret;

		epoll_pwait2_support = 0;
	}
#endif

	return epoll_wait(epfd, events, maxevents, to_msec(st, abs));
}

static int iv_fd_epoll_poll(struct iv_state *st,
			    struct iv_list_head *active,
			    const struct timespec *abs)
{
	struct epoll_event batch[st->numfds ? : 1];
	int ret;
	int run_events;
	int i;

	iv_fd_epoll_flush_pending(st);

	ret = iv_fd_epoll_wait(st, batch, ARRAY_SIZE(batch), abs);

	__iv_invalidate_now(st);

	if (ret < 0) {
		if (errno == EINTR)
			return 1;

		iv_fatal("iv_fd_epoll_poll: got error %d[%s]", errno,
			 strerror(errno));
	}

	run_events = 0;
	for (i = 0; i < ret; i++) {
		if (batch[i].data.ptr != st) {
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
		} else {
			run_events = 1;
		}
	}

	if (run_events)
		iv_event_run_pending_events();

	return 1;
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
	if (st->u.epoll.timer_fd != -1)
		close(st->u.epoll.timer_fd);
	close(st->u.epoll.epoll_fd);
}

static int iv_fd_epoll_create_active_fd(void)
{
	int fd;
	int ret;

	fd = eventfd_grab();
	if (fd >= 0) {
		uint64_t one = 1;

		ret = write(fd, &one, sizeof(one));
		if (ret < 0) {
			iv_fatal("iv_fd_epoll_create_active_fd: eventfd "
				 "write returned error %d[%s]", errno,
				 strerror(errno));
		} else if (ret != sizeof(one)) {
			iv_fatal("iv_fd_epoll_create_active_fd: eventfd "
				 "write returned %d", ret);
		}
	} else {
		int pfd[2];

		if (pipe(pfd) < 0) {
			iv_fatal("iv_fd_epoll_create_active_fd: pipe() "
				 "returned error %d[%s]", errno,
				 strerror(errno));
		}

		fd = pfd[0];
		close(pfd[1]);
	}

	return fd;
}

static ___mutex_t iv_fd_epoll_active_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
static int iv_active_fd_refcount;
static int iv_active_fd;

static int iv_fd_epoll_event_rx_on(struct iv_state *st)
{
	struct epoll_event event;
	int ret;

	___mutex_lock(&iv_fd_epoll_active_fd_mutex);
	if (!iv_active_fd_refcount++)
		iv_active_fd = iv_fd_epoll_create_active_fd();
	___mutex_unlock(&iv_fd_epoll_active_fd_mutex);

	event.data.ptr = st;
	event.events = 0;
	do {
		ret = epoll_ctl(st->u.epoll.epoll_fd, EPOLL_CTL_ADD,
				iv_active_fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret == 0)
		st->numobjs++;

	return ret;
}

static void iv_fd_epoll_event_rx_off(struct iv_state *st)
{
	struct epoll_event event;
	int ret;

	event.data.ptr = st;
	event.events = 0;
	do {
		ret = epoll_ctl(st->u.epoll.epoll_fd, EPOLL_CTL_DEL,
				iv_active_fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		iv_fatal("iv_fd_epoll_event_rx_off: epoll_ctl returned "
			 "error %d[%s]", errno, strerror(errno));
	}

	___mutex_lock(&iv_fd_epoll_active_fd_mutex);
	if (!--iv_active_fd_refcount)
		close(iv_active_fd);
	___mutex_unlock(&iv_fd_epoll_active_fd_mutex);

	st->numobjs--;
}

static void iv_fd_epoll_event_send(struct iv_state *dest)
{
	struct epoll_event event;
	int ret;

	event.data.ptr = dest;
	event.events = EPOLLIN | EPOLLONESHOT;
	do {
		ret = epoll_ctl(dest->u.epoll.epoll_fd, EPOLL_CTL_MOD,
				iv_active_fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		iv_fatal("iv_fd_epoll_event_send: epoll_ctl returned "
			 "error %d[%s]", errno, strerror(errno));
	}
}

const struct iv_fd_poll_method iv_fd_poll_method_epoll = {
	.name		= "epoll",
	.init		= iv_fd_epoll_init,
	.poll		= iv_fd_epoll_poll,
	.unregister_fd	= iv_fd_epoll_unregister_fd,
	.notify_fd	= iv_fd_epoll_notify_fd,
	.notify_fd_sync	= iv_fd_epoll_notify_fd_sync,
	.deinit		= iv_fd_epoll_deinit,
	.event_rx_on	= iv_fd_epoll_event_rx_on,
	.event_rx_off	= iv_fd_epoll_event_rx_off,
	.event_send	= iv_fd_epoll_event_send,
};


#ifdef HAVE_TIMERFD_CREATE
#include <sys/timerfd.h>

static int iv_fd_epoll_timerfd_create(struct iv_state *st)
{
	int ret;
	struct epoll_event event;

	ret = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	if (ret < 0) {
		if (errno != ENOSYS) {
			iv_fatal("iv_fd_epoll_timerfd_create: "
				 "got error %d[%s]", errno, strerror(errno));
		}

		return 0;
	}

	st->u.epoll.timer_fd = ret;

	event.data.ptr = &st->time;
	event.events = EPOLLIN;
	do {
		ret = epoll_ctl(st->u.epoll.epoll_fd, EPOLL_CTL_ADD,
				st->u.epoll.timer_fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		iv_fatal("iv_fd_epoll_timerfd_create: got error %d[%s]",
			 errno, strerror(errno));
	}

	return 1;
}

static int
iv_fd_epoll_timerfd_set_poll_timeout(struct iv_state *st,
				     const struct timespec *abs)
{
	struct itimerspec val;
	int ret;

	if (st->u.epoll.timer_fd == -1 && !iv_fd_epoll_timerfd_create(st)) {
		method = &iv_fd_poll_method_epoll;
		return 0;
	}

	val.it_interval.tv_sec = 0;
	val.it_interval.tv_nsec = 0;
	val.it_value = *abs;
	if (val.it_value.tv_sec == 0 && val.it_value.tv_nsec == 0)
		val.it_value.tv_nsec = 1;

	ret = timerfd_settime(st->u.epoll.timer_fd,
			      TFD_TIMER_ABSTIME, &val, NULL);
	if (ret < 0) {
		iv_fatal("iv_fd_epoll_timerfd_set_poll_timeout: "
			 "got error %d[%s]", errno, strerror(errno));
	}

	return 1;
}

static void iv_fd_epoll_timerfd_clear_poll_timeout(struct iv_state *st)
{
	struct itimerspec val;
	int ret;

	val.it_interval.tv_sec = 0;
	val.it_interval.tv_nsec = 0;
	val.it_value.tv_sec = 0;
	val.it_value.tv_nsec = 0;

	ret = timerfd_settime(st->u.epoll.timer_fd,
			      TFD_TIMER_ABSTIME, &val, NULL);
	if (ret < 0) {
		iv_fatal("iv_fd_epoll_timerfd_clear_poll_timeout: "
			 "got error %d[%s]", errno, strerror(errno));
	}
}

static int iv_fd_epoll_timerfd_poll(struct iv_state *st,
				    struct iv_list_head *active,
				    const struct timespec *abs)
{
	int run_timers;
	struct epoll_event batch[st->numfds + 1];
	int ret;
	int run_events;
	int i;

	iv_fd_epoll_flush_pending(st);

	run_timers = !!(abs != NULL);

	ret = iv_fd_epoll_wait(st, batch, ARRAY_SIZE(batch), abs);

	__iv_invalidate_now(st);

	if (ret < 0) {
		if (errno == EINTR)
			return run_timers;

		iv_fatal("iv_fd_epoll_timerfd_poll: got error %d[%s]",
			 errno, strerror(errno));
	}

	run_events = 0;
	for (i = 0; i < ret; i++) {
		if (batch[i].data.ptr == st) {
			run_events = 1;
		} else if (batch[i].data.ptr == &st->time) {
			uint64_t cnt;
			int ret;

			ret = read(st->u.epoll.timer_fd, &cnt, sizeof(cnt));
			if (ret < 0) {
				iv_fatal("iv_fd_epoll_timerfd_poll: got "
					 "timerfd read error %d[%s]",
					 errno, strerror(errno));
			}

			run_timers = 1;
		} else {
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

	if (run_events)
		iv_event_run_pending_events();

	return run_timers;
}

const struct iv_fd_poll_method iv_fd_poll_method_epoll_timerfd = {
	.name			= "epoll-timerfd",
	.init			= iv_fd_epoll_init,
	.set_poll_timeout	= iv_fd_epoll_timerfd_set_poll_timeout,
	.clear_poll_timeout	= iv_fd_epoll_timerfd_clear_poll_timeout,
	.poll			= iv_fd_epoll_timerfd_poll,
	.unregister_fd		= iv_fd_epoll_unregister_fd,
	.notify_fd		= iv_fd_epoll_notify_fd,
	.notify_fd_sync		= iv_fd_epoll_notify_fd_sync,
	.deinit			= iv_fd_epoll_deinit,
	.event_rx_on		= iv_fd_epoll_event_rx_on,
	.event_rx_off		= iv_fd_epoll_event_rx_off,
	.event_send		= iv_fd_epoll_event_send,
};
#endif
