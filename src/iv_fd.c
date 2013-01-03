/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009, 2012 Lennert Buytenhek
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
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include "iv_private.h"

/* internal use *************************************************************/
int				maxfd;
const struct iv_fd_poll_method	*method;

static void sanitise_nofile_rlimit(int euid)
{
	struct rlimit lim;

	getrlimit(RLIMIT_NOFILE, &lim);
	maxfd = lim.rlim_cur;

	if (euid) {
		if (lim.rlim_cur < lim.rlim_max) {
			lim.rlim_cur = (unsigned int)lim.rlim_max & 0x7FFFFFFF;
			if (lim.rlim_cur > 131072)
				lim.rlim_cur = 131072;

			if (setrlimit(RLIMIT_NOFILE, &lim) >= 0)
				maxfd = lim.rlim_cur;
		}
	} else {
		lim.rlim_cur = 131072;
		lim.rlim_max = 131072;
		while (lim.rlim_cur > maxfd) {
			if (setrlimit(RLIMIT_NOFILE, &lim) >= 0) {
				maxfd = lim.rlim_cur;
				break;
			}

			lim.rlim_cur /= 2;
			lim.rlim_max /= 2;
		}
	}
}

static int method_is_excluded(const char *exclude, const char *name)
{
	if (exclude != NULL) {
		char method_name[64];
		int len;

		while (sscanf(exclude, "%63s%n", method_name, &len) > 0) {
			if (!strcmp(name, method_name))
				return 1;
			exclude += len;
		}
	}

	return 0;
}

static void consider_poll_method(struct iv_state *st, const char *exclude,
				 const struct iv_fd_poll_method *m)
{
	if (method == NULL && !method_is_excluded(exclude, m->name)) {
		if (m->init(st) >= 0)
			method = m;
	}
}

static void iv_fd_init_first_thread(struct iv_state *st)
{
	int euid;
	char *exclude;

	euid = geteuid();

	signal(SIGPIPE, SIG_IGN);
	signal(SIGURG, SIG_IGN);

	sanitise_nofile_rlimit(euid);

	exclude = getenv("IV_EXCLUDE_POLL_METHOD");
	if (exclude != NULL && getuid() != euid)
		exclude = NULL;

#ifdef HAVE_PORT_CREATE
	consider_poll_method(st, exclude, &iv_fd_poll_method_port);
#endif
#ifdef HAVE_SYS_DEVPOLL_H
	consider_poll_method(st, exclude, &iv_fd_poll_method_dev_poll);
#endif
#if defined(HAVE_EPOLL_CREATE) && defined(HAVE_TIMERFD_CREATE)
	consider_poll_method(st, exclude, &iv_fd_poll_method_epoll_timerfd);
#endif
#ifdef HAVE_EPOLL_CREATE
	consider_poll_method(st, exclude, &iv_fd_poll_method_epoll);
#endif
#ifdef HAVE_KQUEUE
	consider_poll_method(st, exclude, &iv_fd_poll_method_kqueue);
#endif
#ifdef HAVE_PPOLL
	consider_poll_method(st, exclude, &iv_fd_poll_method_ppoll);
#endif
	consider_poll_method(st, exclude, &iv_fd_poll_method_poll);

	if (method == NULL)
		iv_fatal("iv_init: can't find suitable event dispatcher");
}

void iv_fd_init(struct iv_state *st)
{
	if (method == NULL)
		iv_fd_init_first_thread(st);
	else if (method->init(st) < 0)
		iv_fatal("iv_init: can't initialize event dispatcher");

	st->handled_fd = NULL;
}

void iv_fd_deinit(struct iv_state *st)
{
	method->deinit(st);
}

static int timespec_cmp(const struct timespec *a, const struct timespec *b)
{
	if (a != NULL) {
		if (a->tv_sec < b->tv_sec)
			return -1;

		if (a->tv_sec > b->tv_sec)
			return 1;

		if (a->tv_nsec < b->tv_nsec)
			return -1;

		if (a->tv_nsec > b->tv_nsec)
			return 1;

		return 0;
	}

	return 1;
}

static int iv_fd_timeout_check(struct iv_state *st, const struct timespec *abs)
{
	int cmp;

	cmp = timespec_cmp(abs, &st->last_abs);

	if (st->last_abs_count == 5) {
		if (cmp >= 0)
			return 1;

		method->clear_poll_timeout(st);
	}

	if (cmp == 0) {
		if (st->last_abs_count < 5)
			st->last_abs_count++;

		if (st->last_abs_count == 5)
			return method->set_poll_timeout(st, abs);
	} else if (abs != NULL) {
		st->last_abs_count = 1;
		st->last_abs = *abs;
	} else {
		st->last_abs_count = 0;
	}

	return 0;
}

int iv_fd_poll_and_run(struct iv_state *st, const struct timespec *abs)
{
	struct iv_list_head active;
	int run_timers;

	INIT_IV_LIST_HEAD(&active);
	if (method->set_poll_timeout != NULL && iv_fd_timeout_check(st, abs)) {
		run_timers = method->poll(st, &active, NULL);
		if (run_timers)
			st->last_abs_count = 0;
	} else {
		run_timers = method->poll(st, &active, abs);
	}

	while (!iv_list_empty(&active)) {
		struct iv_fd_ *fd;

		fd = iv_list_entry(active.next, struct iv_fd_, list_active);
		iv_list_del_init(&fd->list_active);

		st->handled_fd = fd;

		if (fd->ready_bands & MASKERR)
			if (fd->handler_err != NULL)
				fd->handler_err(fd->cookie);

		if (st->handled_fd != NULL && fd->ready_bands & MASKIN)
			if (fd->handler_in != NULL)
				fd->handler_in(fd->cookie);

		if (st->handled_fd != NULL && fd->ready_bands & MASKOUT)
			if (fd->handler_out != NULL)
				fd->handler_out(fd->cookie);
	}

	return run_timers;
}

void iv_fd_make_ready(struct iv_list_head *active, struct iv_fd_ *fd, int bands)
{
	if (iv_list_empty(&fd->list_active)) {
		fd->ready_bands = 0;
		iv_list_add_tail(&fd->list_active, active);
	}
	fd->ready_bands |= bands;
}

void iv_fd_set_cloexec(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFD);
	if (!(flags & FD_CLOEXEC)) {
		flags |= FD_CLOEXEC;
		fcntl(fd, F_SETFD, flags);
	}
}

void iv_fd_set_nonblock(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (!(flags & O_NONBLOCK)) {
		flags |= O_NONBLOCK;
		fcntl(fd, F_SETFL, flags);
	}
}


/* public use ***************************************************************/
const char *iv_poll_method_name(void)
{
	return method != NULL ? method->name : NULL;
}

void IV_FD_INIT(struct iv_fd *_fd)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	fd->fd = -1;
	fd->handler_in = NULL;
	fd->handler_out = NULL;
	fd->handler_err = NULL;
	fd->registered = 0;
}

static void recompute_wanted_flags(struct iv_fd_ *fd)
{
	int wanted;

	wanted = 0;
	if (fd->registered) {
		if (fd->handler_in != NULL)
			wanted |= MASKIN;
		if (fd->handler_out != NULL)
			wanted |= MASKOUT;
		if (fd->handler_err != NULL)
			wanted |= MASKERR;
	}

	fd->wanted_bands = wanted;
}

static void notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	recompute_wanted_flags(fd);

	method->notify_fd(st, fd);
}

static void iv_fd_register_prologue(struct iv_state *st, struct iv_fd_ *fd)
{
	if (fd->registered) {
		iv_fatal("iv_fd_register: called with fd which is "
			 "still registered");
	}

	if (fd->fd < 0 || fd->fd >= maxfd) {
		iv_fatal("iv_fd_register: called with invalid fd %d "
			 "(maxfd=%d)", fd->fd, maxfd);
	}

	fd->registered = 1;
	INIT_IV_LIST_HEAD(&fd->list_active);
	fd->ready_bands = 0;
	fd->registered_bands = 0;
#if defined(HAVE_SYS_DEVPOLL_H) || defined(HAVE_EPOLL_CREATE) ||	\
    defined(HAVE_KQUEUE) || defined(HAVE_PORT_CREATE)
	INIT_IV_LIST_HEAD(&fd->list_notify);
#endif

	if (method->register_fd != NULL)
		method->register_fd(st, fd);
}

static void iv_fd_register_epilogue(struct iv_state *st, struct iv_fd_ *fd)
{
	int yes;

	st->numobjs++;
	st->numfds++;

	iv_fd_set_cloexec(fd->fd);
	iv_fd_set_nonblock(fd->fd);

	yes = 1;
	setsockopt(fd->fd, SOL_SOCKET, SO_OOBINLINE, &yes, sizeof(yes));
}

void iv_fd_register(struct iv_fd *_fd)
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	iv_fd_register_prologue(st, fd);

	notify_fd(st, fd);

	iv_fd_register_epilogue(st, fd);
}

int iv_fd_register_try(struct iv_fd *_fd)
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int orig_wanted_bands;
	int ret;

	iv_fd_register_prologue(st, fd);

	recompute_wanted_flags(fd);

	orig_wanted_bands = fd->wanted_bands;
	if (!fd->wanted_bands)
		fd->wanted_bands = MASKIN | MASKOUT;

	ret = method->notify_fd_sync(st, fd);
	if (ret) {
		fd->registered = 0;
		if (method->unregister_fd != NULL)
			method->unregister_fd(st, fd);
		return ret;
	}

	if (!orig_wanted_bands) {
		fd->wanted_bands = 0;
		method->notify_fd(st, fd);
	}

	iv_fd_register_epilogue(st, fd);

	return 0;
}

void iv_fd_unregister(struct iv_fd *_fd)
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	if (!fd->registered) {
		iv_fatal("iv_fd_unregister: called with fd which is "
			 "not registered");
	}
	fd->registered = 0;

	iv_list_del(&fd->list_active);

	notify_fd(st, fd);
	if (method->unregister_fd != NULL)
		method->unregister_fd(st, fd);

	st->numobjs--;
	st->numfds--;

	if (st->handled_fd == fd)
		st->handled_fd = NULL;
}

int iv_fd_registered(const struct iv_fd *_fd)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	return fd->registered;
}

void iv_fd_set_handler_in(struct iv_fd *_fd, void (*handler_in)(void *))
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	if (!fd->registered) {
		iv_fatal("iv_fd_set_handler_in: called with fd which "
			 "is not registered");
	}

	fd->handler_in = handler_in;
	notify_fd(st, fd);
}

void iv_fd_set_handler_out(struct iv_fd *_fd, void (*handler_out)(void *))
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	if (!fd->registered) {
		iv_fatal("iv_fd_set_handler_out: called with fd which "
			 "is not registered");
	}

	fd->handler_out = handler_out;
	notify_fd(st, fd);
}

void iv_fd_set_handler_err(struct iv_fd *_fd, void (*handler_err)(void *))
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	if (!fd->registered) {
		iv_fatal("iv_fd_set_handler_err: called with fd which "
			 "is not registered");
	}

	fd->handler_err = handler_err;
	notify_fd(st, fd);
}
