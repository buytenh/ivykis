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

#ifndef __IV_SIGNAL_H
#define __IV_SIGNAL_H

#include <iv.h>
#include <iv_event_raw.h>
#include <iv_list.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_signal {
	int			signum;
	unsigned		exclusive:1;
	void			*cookie;
	void			(*handler)(void *);

	struct list_head	list;
	struct iv_event_raw	ev;
};

int iv_signal_register(struct iv_signal *this);
void iv_signal_unregister(struct iv_signal *this);

#ifdef __cplusplus
}
#endif


#endif
