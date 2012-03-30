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
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <sys/resource.h>
#include "iv_private.h"

/* process-global state *****************************************************/
int			maxfd;
struct iv_poll_method	*method;

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

static void consider_poll_method(struct iv_state *st, char *exclude,
				 struct iv_poll_method *m)
{
	if (method == NULL && !method_is_excluded(exclude, m->name)) {
		if (m->init(st) >= 0)
			method = m;
	}
}

static void iv_init_first_thread(struct iv_state *st)
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

#ifdef HAVE_PORT_CREATE
	consider_poll_method(st, exclude, &iv_method_port);
#endif
#ifdef HAVE_SYS_DEVPOLL_H
	consider_poll_method(st, exclude, &iv_method_dev_poll);
#endif
#ifdef HAVE_EPOLL_CREATE
	consider_poll_method(st, exclude, &iv_method_epoll);
#endif
#ifdef HAVE_KQUEUE
	consider_poll_method(st, exclude, &iv_method_kqueue);
#endif
#ifdef HAVE_POLL
	consider_poll_method(st, exclude, &iv_method_poll);
#endif
#ifdef NEED_SELECT
	consider_poll_method(st, exclude, &iv_method_select);
#endif

	if (method == NULL) {
		syslog(LOG_CRIT, "iv_init: can't find suitable event "
				 "dispatcher");
		abort();
	}
}


/* main loop ****************************************************************/
pthread_key_t			iv_state_key;
#ifdef HAVE_THREAD
__thread struct iv_state	*__st;
#endif

static void __iv_deinit(struct iv_state *st)
{
	method->deinit(st);

	iv_timer_deinit(st);
	iv_tls_thread_deinit(st);

	pthread_setspecific(iv_state_key, NULL);
#ifdef HAVE_THREAD
	__st = NULL;
#endif

	barrier();

	free(st);
}

static void iv_state_destructor(void *data)
{
	struct iv_state *st = data;

	pthread_setspecific(iv_state_key, st);
	__iv_deinit(st);
}

static struct iv_state *iv_allocate_state(void)
{
	struct iv_state *st;

	st = calloc(1, iv_tls_total_state_size());

	pthread_setspecific(iv_state_key, st);
#ifdef HAVE_THREAD
	__st = st;
#endif

	return st;
}

IV_API void iv_init(void)
{
	struct iv_state *st;

	if (method == NULL) {
		if (pthread_key_create(&iv_state_key, iv_state_destructor)) {
			fprintf(stderr, "iv_state_allocate_key: failed "
					"to allocate TLS key\n");
			abort();
		}
	}

	st = iv_allocate_state();

	if (method == NULL) {
		iv_init_first_thread(st);
	} else if (method->init(st) < 0) {
		syslog(LOG_CRIT, "iv_init: can't initialize event dispatcher");
		abort();
	}

	st->handled_fd = NULL;
	st->numfds = 0;

	iv_task_init(st);
	iv_timer_init(st);
	iv_tls_thread_init(st);
}

IV_API int iv_inited(void)
{
	return iv_get_state() != NULL;
}

IV_API const char *iv_poll_method_name(void)
{
	return method != NULL ? method->name : NULL;
}

IV_API void iv_quit(void)
{
	struct iv_state *st = iv_get_state();

	st->quit = 1;
}

static void iv_run_active_list(struct iv_state *st, struct iv_list_head *active)
{
	while (!iv_list_empty(active)) {
		struct iv_fd_ *fd;

		fd = iv_list_entry(active->next, struct iv_fd_, list_active);
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
}

static int should_quit(struct iv_state *st)
{
	if (st->quit)
		return 1;

	if (!st->numfds && !iv_pending_tasks(st) && !iv_pending_timers(st))
		return 1;

	return 0;
}

IV_API void iv_main(void)
{
	struct iv_state *st = iv_get_state();
	struct iv_list_head active;

	INIT_IV_LIST_HEAD(&active);

	st->quit = 0;
	while (1) {
		struct timespec to;
		int msec;

		iv_run_tasks(st);
		iv_run_timers(st);

		if (should_quit(st))
			break;

		if (!iv_pending_tasks(st) && !iv_get_soonest_timeout(st, &to)) {
			msec = 1000 * to.tv_sec;
			msec += (to.tv_nsec + 999999) / 1000000;
		} else {
			msec = 0;
		}
		method->poll(st, &active, msec);

		__iv_invalidate_now(st);

		iv_run_active_list(st, &active);
	}
}

IV_API void iv_deinit(void)
{
	struct iv_state *st = iv_get_state();

	__iv_deinit(st);
}
