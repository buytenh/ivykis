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
#include <iv_avl.h>
#include <iv_event_raw.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_signal {
	int			signum;
	unsigned int		flags;
	void			*cookie;
	void			(*handler)(void *);

	struct iv_avl_node	an;
	uint8_t			active;
	struct iv_event_raw	ev;
	struct iv_fd		sigfd;
};

static inline void IV_SIGNAL_INIT(struct iv_signal *this)
{
}

#define IV_SIGNAL_FLAG_EXCLUSIVE	1

int iv_signal_register(struct iv_signal *this);
void iv_signal_unregister(struct iv_signal *this);

#ifdef __cplusplus
}
#endif


#endif
