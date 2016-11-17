/*
 * ivykis, an event handling library
 * Copyright (C) 2012 Lennert Buytenhek
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

#include <iv_event_raw.h>
#include "mutex.h"

struct iv_state {
	/* iv_main_win32.c  */
	int			quit;
	int			numobjs;

	/* iv_event.c  */
	int			event_count;
	struct iv_task		events_local;
	struct iv_event_raw	events_kick;
	___mutex_t		event_list_mutex;
	struct iv_list_head	events_pending;

	/* iv_handle.c  */
	HANDLE			wait;
	HANDLE			thread_stop;
	struct iv_list_head	handles;
	CRITICAL_SECTION	active_handle_list_lock;
	struct iv_list_head	active_with_handler;
	struct iv_list_head	active_without_handler;
	struct iv_handle_	*handled_handle;

	/* iv_task.c  */
	struct iv_list_head	tasks;
	struct iv_list_head	*tasks_current;
	uint32_t		task_epoch;

	/* iv_timer.c  */
	struct timespec		time;
	int			time_valid;
	int			num_timers;
	int			rat_depth;
	union {
		struct iv_timer_ratnode		*timer_root;
		struct iv_timer_ratnode		first_leaf;
	} ratnode;
};

struct iv_handle_ {
	/*
	 * User data.
	 */
	HANDLE			handle;
	void			*cookie;
	void			(*handler)(void *);

	/*
	 * Private data.
	 */
	struct iv_list_head	list;
	struct iv_list_head	list_active;
	struct iv_state		*st;
	HANDLE			signal_handle;
};

/* iv_handle.c */
void iv_handle_init(struct iv_state *st);
void iv_handle_deinit(struct iv_state *st);
void iv_handle_poll_and_run(struct iv_state *st, const struct timespec *abs);

/* iv_time_win32.c */
void iv_time_init(struct iv_state *st);


extern DWORD iv_state_index;

static inline int is_mt_app(void)
{
	return 1;
}

static inline struct iv_state *iv_get_state(void)
{
	return TlsGetValue(iv_state_index);
}
