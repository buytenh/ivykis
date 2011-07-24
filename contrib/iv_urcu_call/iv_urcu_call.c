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

#include <iv.h>
#include <urcu-qsbr.h>
#include "iv_urcu_call.h"

static void iv_urcu_call_callback(void *_uc)
{
	struct iv_urcu_call *uc = _uc;

	iv_event_unregister(&uc->ev);

	uc->handler(uc->cookie);
}

static void iv_urcu_call_handler(struct rcu_head *head)
{
	struct iv_urcu_call *uc =
		iv_container_of(head, struct iv_urcu_call, head);

	iv_event_post(&uc->ev);
}

void iv_urcu_call_schedule(struct iv_urcu_call *uc)
{
	IV_EVENT_INIT(&uc->ev);
	uc->ev.cookie = uc;
	uc->ev.handler = iv_urcu_call_callback;
	iv_event_register(&uc->ev);

	call_rcu(&uc->head, iv_urcu_call_handler);
}
