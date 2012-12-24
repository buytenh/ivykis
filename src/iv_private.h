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

#include "iv.h"
#include "iv_avl.h"
#include "iv_list.h"
#include "iv_tls.h"
#include "config.h"

#ifndef _WIN32
#include "iv_private_posix.h"
#else
#include "iv_private_win32.h"
#endif


/*
 * Private versions of the task/timer structures, exposing their
 * internal state.  The user data fields of these structures MUST
 * match the definitions in the public header file iv.h.
 */
struct iv_task_ {
	/*
	 * User data.
	 */
	void			*cookie;
	void			(*handler)(void *);

	/*
	 * Private data.
	 */
	struct iv_list_head	list;
};

struct iv_timer_ {
	/*
	 * User data.
	 */
	struct timespec		expires;
	void			*cookie;
	void			(*handler)(void *);

	/*
	 * Private data.
	 */
	int			index;
};


/*
 * Misc internal stuff.
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/* iv_task.c */
void iv_task_init(struct iv_state *st);
void iv_run_tasks(struct iv_state *st);

/* iv_time_{posix,win32}.c */
void iv_time_get(struct timespec *time);

/* iv_timer.c */
void __iv_invalidate_now(struct iv_state *st);
void iv_timer_init(struct iv_state *st);
int iv_get_soonest_timeout(struct iv_state *st, struct timespec *to);
void iv_run_timers(struct iv_state *st);
void iv_timer_deinit(struct iv_state *st);

/* iv_tls.c */
int iv_tls_total_state_size(void);
void iv_tls_thread_init(struct iv_state *st);
void iv_tls_thread_deinit(struct iv_state *st);
void *__iv_tls_user_ptr(struct iv_state *st, struct iv_tls_user *itu);


static inline void
__iv_list_steal_elements(struct iv_list_head *oldh, struct iv_list_head *newh)
{
	struct iv_list_head *first = oldh->next;
	struct iv_list_head *last = oldh->prev;

	last->next = newh;
	first->prev = newh;

	newh->next = oldh->next;
	newh->prev = oldh->prev;

	oldh->next = oldh;
	oldh->prev = oldh;
}

static inline int iv_pending_tasks(struct iv_state *st)
{
	return !iv_list_empty(&st->tasks);
}

static inline int to_msec(struct timespec *to)
{
	return 1000 * to->tv_sec + ((to->tv_nsec + 999999) / 1000000);
}
