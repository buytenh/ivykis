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

#ifndef __IV_WAIT_H
#define __IV_WAIT_H

#include <iv_avl.h>
#include <iv_event.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_wait_interest {
	pid_t		pid;
	void		*cookie;
	void		(*handler)(void *cookie, int status,
				   const struct rusage *rusage);

	struct iv_avl_node	avl_node;
	struct iv_event		ev;
	struct iv_list_head	events_pending;
	void			*dummy;
	unsigned int		flags;
};

static inline void IV_WAIT_INTEREST_INIT(struct iv_wait_interest *this)
{
}

void iv_wait_interest_register(struct iv_wait_interest *this);
int iv_wait_interest_register_spawn(struct iv_wait_interest *this,
				    void (*fn)(void *cookie), void *cookie);
void iv_wait_interest_unregister(struct iv_wait_interest *this);
int iv_wait_interest_kill(const struct iv_wait_interest *this, int sig);

#ifdef __cplusplus
}
#endif


#endif
