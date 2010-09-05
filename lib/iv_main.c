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
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include "iv_private.h"

/* process-global state *****************************************************/
static int			maxfd;
static struct iv_poll_method	*method;

static int sanitise_nofile_rlimit(int euid)
{
	struct rlimit lim;
	int max_files;

	getrlimit(RLIMIT_NOFILE, &lim);
	max_files = lim.rlim_cur;

	if (euid) {
		if (lim.rlim_cur < lim.rlim_max) {
			lim.rlim_cur = (unsigned int)lim.rlim_max & 0x7FFFFFFF;
			if (lim.rlim_cur > 131072)
				lim.rlim_cur = 131072;

			if (setrlimit(RLIMIT_NOFILE, &lim) >= 0)
				max_files = lim.rlim_cur;
		}
	} else {
		lim.rlim_cur = 131072;
		lim.rlim_max = 131072;
		while (lim.rlim_cur > max_files) {
			if (setrlimit(RLIMIT_NOFILE, &lim) >= 0) {
				max_files = lim.rlim_cur;
				break;
			}

			lim.rlim_cur /= 2;
			lim.rlim_max /= 2;
		}
	}

	return max_files;
}

static int method_is_excluded(char *exclude, char *name)
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

static void consider_poll_method(char *exclude, struct iv_poll_method *m)
{
	if (method == NULL && !method_is_excluded(exclude, m->name)) {
		if (m->init(maxfd) >= 0)
			method = m;
	}
}

static void iv_init_first_thread(void)
{
	int euid;
	char *exclude;

	euid = geteuid();

	signal(SIGPIPE, SIG_IGN);
	signal(SIGURG, SIG_IGN);

	maxfd = sanitise_nofile_rlimit(euid);
	method = NULL;

	exclude = getenv("IV_EXCLUDE_POLL_METHOD");
	if (exclude != NULL && getuid() != euid)
		exclude = NULL;

#ifdef linux
	consider_poll_method(exclude, &iv_method_epoll);
#endif
#if defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__NetBSD_) || defined(__OpenBSD__)
	consider_poll_method(exclude, &iv_method_kqueue);
#endif
#ifdef sun
	consider_poll_method(exclude, &iv_method_dev_poll);
#endif
	consider_poll_method(exclude, &iv_method_poll);
	consider_poll_method(exclude, &iv_method_select);

	if (method == NULL) {
		syslog(LOG_CRIT, "iv_init: can't find suitable event "
				 "dispatcher");
		abort();
	}
}


/* main loop ****************************************************************/
static __thread struct iv_fd_	*handled_fd;
static __thread int		numfds;
static __thread int		quit;

void iv_init(void)
{
	if (method == NULL) {
		iv_init_first_thread();
	} else if (method->init(maxfd) < 0) {
		syslog(LOG_CRIT, "iv_init: can't initialize event dispatcher");
		abort();
	}

	handled_fd = NULL;
	numfds = 0;

	iv_task_init();
	iv_timer_init();
}

void iv_quit(void)
{
	quit = 1;
}

static void notify_fd(struct iv_fd_ *fd)
{
	int wanted;

	wanted = 0;
	if (fd->registered) {
		if (fd->handler_in != NULL)
			wanted |= MASKIN | MASKERR;
		if (fd->handler_out != NULL)
			wanted |= MASKOUT | MASKERR;
		if (fd->handler_err != NULL)
			wanted |= MASKERR;
	}

	method->notify_fd(fd, wanted);
}

static void iv_run_active_list(struct list_head *active)
{
	while (!list_empty(active)) {
		struct iv_fd_ *fd;
		int notify;

		fd = list_entry(active->next, struct iv_fd_, list_active);
		list_del_init(&fd->list_active);

		handled_fd = fd;
		notify = 0;

		if (fd->ready_bands & MASKERR) {
			if (fd->handler_err != NULL)
				fd->handler_err(fd->cookie);
			else
				notify = 1;
		}

		if (handled_fd != NULL && fd->ready_bands & MASKIN) {
			if (fd->handler_in != NULL)
				fd->handler_in(fd->cookie);
			else
				notify = 1;
		}

		if (handled_fd != NULL && fd->ready_bands & MASKOUT) {
			if (fd->handler_out != NULL)
				fd->handler_out(fd->cookie);
			else
				notify = 1;
		}

		if (handled_fd != NULL && notify)
			notify_fd(fd);
	}
}

static int should_quit(void)
{
	return quit || (!numfds && !iv_pending_tasks() && !iv_pending_timers());
}

void iv_main(void)
{
	struct list_head active;

	INIT_LIST_HEAD(&active);

	quit = 0;
	while (1) {
		struct timespec to;
		int msec;

		iv_run_timers();
		iv_run_tasks();

		if (should_quit())
			break;

		if (!iv_get_soonest_timeout(&to)) {
			msec = 1000 * to.tv_sec;
			msec += (to.tv_nsec + 999999) / 1000000;
		} else {
			msec = 0;
		}
		method->poll(numfds, &active, msec);

		iv_invalidate_now();

		iv_run_active_list(&active);
	}
}

void iv_deinit(void)
{
	method->deinit();

	iv_timer_deinit();
}


/* file descriptor handling *************************************************/
void INIT_IV_FD(struct iv_fd *_fd)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	fd->fd = -1;
	fd->handler_in = NULL;
	fd->handler_out = NULL;
	fd->handler_err = NULL;
	fd->registered = 0;
}

void iv_register_fd(struct iv_fd *_fd)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int flags;
	int yes;

	if (fd->registered) {
		syslog(LOG_CRIT, "iv_register_fd: called with fd which "
				 "is still registered");
		abort();
	}

	if (fd->fd < 0 || fd->fd >= maxfd) {
		syslog(LOG_CRIT, "iv_register_fd: called with invalid fd "
				 "%d (maxfd=%d)", fd->fd, maxfd);
		abort();
	}

	flags = fcntl(fd->fd, F_GETFD);
	if (!(flags & FD_CLOEXEC)) {
		flags |= FD_CLOEXEC;
		fcntl(fd->fd, F_SETFD, flags);
	}

	flags = fcntl(fd->fd, F_GETFL);
	if (!(flags & O_NONBLOCK)) {
		flags |= O_NONBLOCK;
		fcntl(fd->fd, F_SETFL, flags);
	}

	yes = 1;
	setsockopt(fd->fd, SOL_SOCKET, SO_OOBINLINE, &yes, sizeof(yes));

	fd->registered = 1;
	INIT_LIST_HEAD(&fd->list_active);
	fd->ready_bands = 0;
	fd->registered_bands = 0;

	numfds++;

	if (method->register_fd != NULL)
		method->register_fd(fd);
	notify_fd(fd);
}

void iv_unregister_fd(struct iv_fd *_fd)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	if (!fd->registered) {
		syslog(LOG_CRIT, "iv_unregister_fd: called with fd which "
				 "is not registered");
		abort();
	}
	fd->registered = 0;

	list_del(&fd->list_active);

	notify_fd(fd);
	if (method->unregister_fd != NULL)
		method->unregister_fd(fd);

	numfds--;

	if (handled_fd == fd)
		handled_fd = NULL;
}

void iv_fd_make_ready(struct list_head *active, struct iv_fd_ *fd, int bands)
{
	if (list_empty(&fd->list_active)) {
		fd->ready_bands = 0;
		list_add_tail(&fd->list_active, active);
	}
	fd->ready_bands |= bands;
}

void iv_fd_set_handler_in(struct iv_fd *_fd, void (*handler_in)(void *))
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int notify;

	if (!fd->registered) {
		syslog(LOG_CRIT, "iv_fd_set_handler_in: called with fd "
				 "which is not registered");
		abort();
	}

	notify = 0;
	if (handler_in != NULL && !(fd->registered_bands & MASKIN)) {
		if (fd->handler_in != NULL) {
			syslog(LOG_CRIT, "iv_fd_set_handler_in: old handler "
					 "is NULL, yet not registered");
			abort();
		}
		notify = 1;
	}

	fd->handler_in = handler_in;
	if (notify)
		notify_fd(fd);
}

void iv_fd_set_handler_out(struct iv_fd *_fd, void (*handler_out)(void *))
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int notify;

	if (!fd->registered) {
		syslog(LOG_CRIT, "iv_fd_set_handler_out: called with fd "
				 "which is not registered");
		abort();
	}

	notify = 0;
	if (handler_out != NULL && !(fd->registered_bands & MASKOUT)) {
		if (fd->handler_out != NULL) {
			syslog(LOG_CRIT, "iv_fd_set_handler_out: old handler "
					 "is NULL, yet not registered");
			abort();
		}
		notify = 1;
	}

	fd->handler_out = handler_out;
	if (notify)
		notify_fd(fd);
}

void iv_fd_set_handler_err(struct iv_fd *_fd, void (*handler_err)(void *))
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int notify;

	if (!fd->registered) {
		syslog(LOG_CRIT, "iv_fd_set_handler_err: called with fd "
				 "which is not registered");
		abort();
	}

	notify = 0;
	if (handler_err != NULL && !(fd->registered_bands & MASKERR)) {
		if (fd->handler_err != NULL) {
			syslog(LOG_CRIT, "iv_fd_set_handler_err: old handler "
					 "is NULL, yet not registered");
			abort();
		}
		notify = 1;
	}

	fd->handler_err = handler_err;
	if (notify)
		notify_fd(fd);
}
