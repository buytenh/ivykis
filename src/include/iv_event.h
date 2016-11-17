/*
 * ivykis, an event handling library
 * Copyright (C) 2010 Lennert Buytenhek
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

#ifndef __IV_EVENT_H
#define __IV_EVENT_H

#include <iv.h>
#include <iv_list.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_event {
	void			*cookie;
	void			(*handler)(void *);

	void			*owner;
	struct iv_list_head	list;
};

static inline void IV_EVENT_INIT(struct iv_event *this)
{
}

int iv_event_register(struct iv_event *this);
void iv_event_unregister(struct iv_event *this);
void iv_event_post(struct iv_event *this);

#ifdef __cplusplus
}
#endif


#endif
