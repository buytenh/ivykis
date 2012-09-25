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

struct iv_event_thr_info {
	int			event_count;
	union {
		struct iv_event_raw	ier;
		struct iv_state		*st;
	} u;
	__mutex_t		list_mutex;
	struct iv_list_head	pending_events;
};

static int iv_event_use_event_raw;

static void __iv_event_run_pending_events(void *_tinfo)
{
	struct iv_event_thr_info *tinfo = _tinfo;
	struct iv_list_head events;

	mutex_lock(&tinfo->list_mutex);

	__iv_list_steal_elements(&tinfo->pending_events, &events);
	while (!iv_list_empty(&events)) {
		struct iv_event *ie;

		ie = iv_container_of(events.next, struct iv_event, list);
		iv_list_del_init(&ie->list);

		mutex_unlock(&tinfo->list_mutex);
		ie->handler(ie->cookie);
		mutex_lock(&tinfo->list_mutex);
	}

	mutex_unlock(&tinfo->list_mutex);
}

static void iv_event_tls_init_thread(void *_tinfo)
{
	struct iv_event_thr_info *tinfo = _tinfo;

	tinfo->event_count = 0;

	IV_EVENT_RAW_INIT(&tinfo->u.ier);
	tinfo->u.ier.cookie = tinfo;
	tinfo->u.ier.handler = __iv_event_run_pending_events;

	mutex_init(&tinfo->list_mutex);

	INIT_IV_LIST_HEAD(&tinfo->pending_events);
}

static void iv_event_tls_deinit_thread(void *_tinfo)
{
	struct iv_event_thr_info *tinfo = _tinfo;

	mutex_destroy(&tinfo->list_mutex);
}

static struct iv_tls_user iv_event_tls_user = {
	.sizeof_state	= sizeof(struct iv_event_thr_info),
	.init_thread	= iv_event_tls_init_thread,
	.deinit_thread	= iv_event_tls_deinit_thread,
};

static void iv_event_tls_init(void) __attribute__((constructor));
static void iv_event_tls_init(void)
{
	iv_tls_user_register(&iv_event_tls_user);
}

void iv_event_run_pending_events(void)
{
	__iv_event_run_pending_events(iv_tls_user_ptr(&iv_event_tls_user));
}

int iv_event_register(struct iv_event *this)
{
	struct iv_event_thr_info *tinfo = iv_tls_user_ptr(&iv_event_tls_user);

	if (!tinfo->event_count++) {
		if (!iv_event_use_event_raw) {
			struct iv_state *st = iv_get_state();

			if (event_rx_on(st) == 0)
				tinfo->u.st = st;
			else
				iv_event_use_event_raw = 1;
		}

		if (iv_event_use_event_raw) {
			int ret;

			ret = iv_event_raw_register(&tinfo->u.ier);
			if (ret) {
				tinfo->event_count--;
				return ret;
			}
		}
	}

	this->tinfo = tinfo;

	INIT_IV_LIST_HEAD(&this->list);

	return 0;
}

void iv_event_unregister(struct iv_event *this)
{
	struct iv_event_thr_info *tinfo = iv_tls_user_ptr(&iv_event_tls_user);

	if (!iv_list_empty(&this->list)) {
		mutex_lock(&tinfo->list_mutex);
		iv_list_del(&this->list);
		mutex_unlock(&tinfo->list_mutex);
	}

	if (!--tinfo->event_count) {
		if (!iv_event_use_event_raw) {
			event_rx_off(tinfo->u.st);
			tinfo->u.st = NULL;
		} else {
			iv_event_raw_unregister(&tinfo->u.ier);
		}
	}
}

void iv_event_post(struct iv_event *this)
{
	struct iv_event_thr_info *tinfo = this->tinfo;
	int post;

	post = 0;

	mutex_lock(&tinfo->list_mutex);
	if (iv_list_empty(&this->list)) {
		if (iv_list_empty(&tinfo->pending_events))
			post = 1;
		iv_list_add_tail(&this->list, &tinfo->pending_events);
	}
	mutex_unlock(&tinfo->list_mutex);

	if (post) {
		if (!iv_event_use_event_raw)
			event_send(tinfo->u.st);
		else
			iv_event_raw_post(&tinfo->u.ier);
	}
}
