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

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <iv_event.h>
#include <iv_event_raw.h>
#include "iv_thr.h"

TLS_BLOCK_START
{
	struct iv_event_thr_info {
		int			event_count;
		struct iv_event_raw	ier;
		pthread_mutex_t		list_mutex;
		struct list_head	pending_events;
		int			dead;
	} tinfo;
}
TLS_BLOCK_END;

#define tinfo __tls_deref(tinfo)

static void iv_event_run_pending_events(void *_dummy)
{
	pthr_mutex_lock(&tinfo.list_mutex);
	while (!list_empty(&tinfo.pending_events)) {
		struct iv_event *ie;

		ie = container_of(tinfo.pending_events.next,
				  struct iv_event, list);

		list_del_init(&ie->list);

		pthr_mutex_unlock(&tinfo.list_mutex);

		ie->handler(ie->cookie);
		if (tinfo.dead)
			return;

		pthr_mutex_lock(&tinfo.list_mutex);
	}
	pthr_mutex_unlock(&tinfo.list_mutex);
}

int iv_event_register(struct iv_event *this)
{
	if (!tinfo.event_count++) {
		int ret;

		IV_EVENT_RAW_INIT(&tinfo.ier);
		tinfo.ier.handler = iv_event_run_pending_events;

		ret = iv_event_raw_register(&tinfo.ier);
		if (ret)
			return ret;

		pthr_mutex_init(&tinfo.list_mutex, NULL);
		INIT_LIST_HEAD(&tinfo.pending_events);
		tinfo.dead = 0;
	}

	this->thr_info = &tinfo;
	INIT_LIST_HEAD(&this->list);

	return 0;
}

void iv_event_unregister(struct iv_event *this)
{
	if (!list_empty(&this->list)) {
		pthr_mutex_lock(&tinfo.list_mutex);
		list_del(&this->list);
		pthr_mutex_unlock(&tinfo.list_mutex);
	}

	if (!--tinfo.event_count) {
		tinfo.dead = 1;
		pthr_mutex_destroy(&tinfo.list_mutex);
		iv_event_raw_unregister(&tinfo.ier);
	}
}

void iv_event_post(struct iv_event *this)
{
	struct iv_event_thr_info *t = this->thr_info;

	pthr_mutex_lock(&t->list_mutex);
	if (list_empty(&this->list)) {
		list_add_tail(&this->list, &t->pending_events);
		iv_event_raw_post(&t->ier);
	}
	pthr_mutex_unlock(&t->list_mutex);
}
