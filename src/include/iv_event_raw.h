/*
 * ivykis, an event handling library
 * Copyright (C) 2010, 2012 Lennert Buytenhek
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

#ifndef __IV_EVENT_RAW_H
#define __IV_EVENT_RAW_H

#include <iv.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_event_raw {
	void			*cookie;
	void			(*handler)(void *);

#ifndef _WIN32
	struct iv_fd		event_rfd;
	int			event_wfd;
#else
	struct iv_handle	h;
#endif
};

static inline void IV_EVENT_RAW_INIT(struct iv_event_raw *this)
{
}

int iv_event_raw_register(struct iv_event_raw *this);
void iv_event_raw_unregister(struct iv_event_raw *this);
void iv_event_raw_post(const struct iv_event_raw *this);

#ifdef __cplusplus
}
#endif


#endif
