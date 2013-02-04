/*
 * ivykis, an event handling library
 * Copyright (C) 2010, 2012, 2013 Lennert Buytenhek
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

#include "mutex.h"

#ifndef _WIN32
#define TID_FORMAT		"%lu"

struct iv_thread {
	struct iv_list_head	list;
	struct iv_list_head	list_dead;

	void			(*start_routine)(void *);
	void			*arg;

	struct iv_thread	*parent;
	char			*name;
	unsigned long		tid;
	pthread_t		ptid;

	struct iv_list_head	children;
	struct iv_event		child_died;
	struct iv_list_head	children_dead;
};

static inline int iv_thread_dead(const struct iv_thread *thr)
{
	return !iv_list_empty(&thr->list_dead);
}
#else
#define TID_FORMAT		"%lx"

struct iv_thread {
	struct iv_list_head	list;

	void			(*start_routine)(void *);
	void			*arg;

	struct iv_thread	*parent;
	char			*name;
	DWORD			tid;

	struct iv_list_head	children;

	/* for use by ->parent only  */
	struct iv_handle	thr_handle;
};

static inline int iv_thread_dead(const struct iv_thread *thr)
{
	return 0;
}
#endif

extern ___mutex_t iv_thread_lock;
extern struct iv_list_head iv_thread_roots;
extern int iv_thread_debug;

struct iv_thread *iv_thread_get_self(void);
