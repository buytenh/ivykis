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

#ifndef __IV_HANDLE_PRIVATE_H
#define __IV_HANDLE_PRIVATE_H

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
#ifdef WITH_HANDLE_APC
	int			registered;
	struct iv_list_head	list_active;
	struct iv_handle_group	*grp;
	int			index;
#endif
#ifdef WITH_HANDLE_ME
	int			registered;
	struct iv_list_head	list_active;
	struct iv_handle_group	*grp;
	int			index;
#endif
#ifdef WITH_HANDLE_MP
	int			registered;
	struct iv_list_head	list_active;
	struct iv_handle_group	*grp;
	int			index;
#endif
#ifdef WITH_HANDLE_SIMPLE
	struct iv_list_head	list;
	struct iv_list_head	list_active;
	struct iv_state		*st;
	int			polling;
	HANDLE			signal_handle;
	HANDLE			thr_handle;
#endif
};

#define MAX_THREAD_HANDLES	(MAXIMUM_WAIT_OBJECTS - 1)

struct iv_handle_group {
#ifdef WITH_HANDLE_APC
	struct iv_list_head	list;
	struct iv_list_head	list_recent_deleted;

	struct iv_state		*st;

	HANDLE			thr_handle;

	int			num_handles;
	int			active_handles;
	struct iv_handle_	*h[MAXIMUM_WAIT_OBJECTS];
	HANDLE			handle[MAXIMUM_WAIT_OBJECTS];
#endif
#ifdef WITH_HANDLE_ME
	struct iv_list_head	list_all;
	struct iv_list_head	list_recent_deleted;

	struct iv_state		*st;

	HANDLE			thr_handle;
	HANDLE			thr_signal_handle;

	CRITICAL_SECTION	group_lock;
	struct iv_list_head	active_handles;
	struct iv_handle_	*h[MAX_THREAD_HANDLES];
	int			addition_pointer;
	int			num_deletions;

	int			quit;
	int			num_handles;
	HANDLE			hnd[MAXIMUM_WAIT_OBJECTS];
	int			have_active_handles;
#endif
#ifdef WITH_HANDLE_MP
	struct iv_list_head	list;
	struct iv_list_head	list_recent_deleted;

	struct iv_state		*st;

	HANDLE			thr_handle;

	int			num_handles;
	int			active_handles;
	struct iv_handle_	*h[MAXIMUM_WAIT_OBJECTS];
	HANDLE			handle[MAXIMUM_WAIT_OBJECTS];
#endif
};


#endif
