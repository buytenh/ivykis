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
#include <pthread.h>
#include "iv_private.h"

int				iv_state_key_allocated;
pthread_key_t			iv_state_key;
#ifdef HAVE_THREAD
__thread struct iv_state	*__st;
#endif

static void __iv_deinit(struct iv_state *st)
{
	iv_tls_thread_deinit(st);

	iv_fd_deinit(st);
	iv_timer_deinit(st);

	pthread_setspecific(iv_state_key, NULL);
#ifdef HAVE_THREAD
	__st = NULL;
#endif

	free(st);
}

static void iv_state_destructor(void *data)
{
	struct iv_state *st = data;

	pthread_setspecific(iv_state_key, st);
	__iv_deinit(st);
}

void iv_init(void)
{
	struct iv_state *st;

	if (!iv_state_key_allocated) {
		if (pthread_key_create(&iv_state_key, iv_state_destructor))
			iv_fatal("iv_init: failed to allocate TLS key");
		iv_state_key_allocated = 1;
	}

	st = calloc(1, iv_tls_total_state_size());

	pthread_setspecific(iv_state_key, st);
#ifdef HAVE_THREAD
	__st = st;
#endif

	st->numobjs = 0;

	iv_fd_init(st);
	iv_task_init(st);
	iv_timer_init(st);

	iv_tls_thread_init(st);
}

int iv_inited(void)
{
#ifndef HAVE_THREAD
	if (!iv_state_key_allocated)
		return 0;
#endif

	return iv_get_state() != NULL;
}

void iv_quit(void)
{
	struct iv_state *st = iv_get_state();

	st->quit = 1;
}

void iv_main(void)
{
	struct iv_state *st = iv_get_state();

	st->quit = 0;
	while (1) {
		struct timespec to;

		iv_run_tasks(st);
		iv_run_timers(st);

		if (st->quit || !st->numobjs)
			break;

		if (iv_pending_tasks(st) || iv_get_soonest_timeout(st, &to)) {
			to.tv_sec = 0;
			to.tv_nsec = 0;
		}

		iv_fd_poll_and_run(st, &to);
	}
}

void iv_deinit(void)
{
	struct iv_state *st = iv_get_state();

	__iv_deinit(st);
}
