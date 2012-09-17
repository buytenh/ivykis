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
	int			polling;
	HANDLE			signal_handle;
	HANDLE			thr_handle;
};
