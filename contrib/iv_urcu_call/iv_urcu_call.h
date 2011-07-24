/*
 * ivykis, an event handling library
 * Copyright (C) 2011 Lennert Buytenhek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version
 * 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 2.1 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License version 2.1 along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <iv_event.h>
#include <urcu-qsbr.h>

struct iv_urcu_call {
	void		*cookie;
	void		(*handler)(void *);

	struct iv_event		ev;
	struct rcu_head		head;
};

void iv_urcu_call_schedule(struct iv_urcu_call *uc);
