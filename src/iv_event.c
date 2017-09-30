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

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <iv_event.h>
#include <iv_event_raw.h>
#include <iv_tls.h>
#include "iv_private.h"
#include "iv_event_private.h"
#include "mutex.h"

static int iv_event_use_event_raw;

static void __iv_event_run_pending_events(void *_st)
{
	struct iv_state *st = _st;
	struct iv_list_head events;

	___mutex_lock(&st->event_list_mutex);

	if (iv_list_empty(&st->events_pending)) {
		___mutex_unlock(&st->event_list_mutex);
		return;
	}

	__iv_list_steal_elements(&st->events_pending, &events);
	while (1) {
		struct iv_event *ie;
		int empty_now;

		ie = iv_container_of(events.next, struct iv_event, list);
		iv_list_del_init(&ie->list);
		empty_now = !!iv_list_empty(&events);

		___mutex_unlock(&st->event_list_mutex);

		ie->handler(ie->cookie);
		if (empty_now)
			break;

		___mutex_lock(&st->event_list_mutex);

		if (iv_list_empty(&events)) {
			___mutex_unlock(&st->event_list_mutex);
			break;
		}
	}
}

void iv_event_init(struct iv_state *st)
{
	st->event_count = 0;

	IV_TASK_INIT(&st->events_local);
	st->events_local.cookie = st;
	st->events_local.handler = __iv_event_run_pending_events;

	IV_EVENT_RAW_INIT(&st->events_kick);
	st->events_kick.cookie = st;
	st->events_kick.handler = __iv_event_run_pending_events;

	___mutex_init(&st->event_list_mutex);

	INIT_IV_LIST_HEAD(&st->events_pending);
}

void iv_event_deinit(struct iv_state *st)
{
	___mutex_destroy(&st->event_list_mutex);
}

void iv_event_run_pending_events(void)
{
	__iv_event_run_pending_events(iv_get_state());
}

int iv_event_register(struct iv_event *this)
{
	struct iv_state *st = iv_get_state();

	st->numobjs++;

	if (!st->event_count++ && is_mt_app()) {
		if (!iv_event_use_event_raw && event_rx_on(st))
			iv_event_use_event_raw = 1;

		if (iv_event_use_event_raw) {
			int ret;

			ret = iv_event_raw_register(&st->events_kick);
			if (ret) {
				st->event_count--;
				return ret;
			}
		}
	}

	this->owner = st;

	INIT_IV_LIST_HEAD(&this->list);

	return 0;
}

void iv_event_unregister(struct iv_event *this)
{
	struct iv_state *st = this->owner;

	if (!iv_list_empty(&this->list)) {
		___mutex_lock(&st->event_list_mutex);
		iv_list_del(&this->list);
		___mutex_unlock(&st->event_list_mutex);
	}

	if (!--st->event_count && is_mt_app()) {
		if (iv_event_use_event_raw) {
			iv_event_raw_unregister(&st->events_kick);
		} else {
			event_rx_off(st);
		}
	}

	st->numobjs--;
}

void iv_event_post(struct iv_event *this)
{
	struct iv_state *dst = this->owner;
	int post;

	post = 0;

	___mutex_lock(&dst->event_list_mutex);
	if (iv_list_empty(&this->list)) {
		if (iv_list_empty(&dst->events_pending))
			post = 1;
		iv_list_add_tail(&this->list, &dst->events_pending);
	}
	___mutex_unlock(&dst->event_list_mutex);

	if (post) {
		struct iv_state *me = iv_get_state();

		if (dst == me) {
			if (!iv_task_registered(&me->events_local))
				iv_task_register(&me->events_local);
		} else if (iv_event_use_event_raw) {
			iv_event_raw_post(&dst->events_kick);
		} else {
			event_send(dst);
		}
	}
}
