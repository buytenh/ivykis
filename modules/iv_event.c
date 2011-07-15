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
#include <pthread.h>
#include <signal.h>

static __thread struct iv_event_thr_info {
	int			event_count;
	struct iv_event_raw	ier;
	pthread_mutex_t		list_mutex;
	struct list_head	pending_events;
	int			dead;
} tinfo;

static void iv_event_run_pending_events(void *_dummy)
{
	pthread_mutex_lock(&tinfo.list_mutex);
	while (!list_empty(&tinfo.pending_events)) {
		struct iv_event *ie;

		ie = container_of(tinfo.pending_events.next,
				  struct iv_event, list);

		list_del_init(&ie->list);

		pthread_mutex_unlock(&tinfo.list_mutex);

		ie->handler(ie->cookie);
		if (tinfo.dead)
			return;

		pthread_mutex_lock(&tinfo.list_mutex);
	}
	pthread_mutex_unlock(&tinfo.list_mutex);
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

		pthread_mutex_init(&tinfo.list_mutex, NULL);
		INIT_LIST_HEAD(&tinfo.pending_events);
		tinfo.dead = 0;
	}

	this->tinfo = &tinfo;
	INIT_LIST_HEAD(&this->list);

	return 0;
}

void iv_event_unregister(struct iv_event *this)
{
	if (!list_empty(&this->list)) {
		pthread_mutex_lock(&tinfo.list_mutex);
		list_del(&this->list);
		pthread_mutex_unlock(&tinfo.list_mutex);
	}

	if (!--tinfo.event_count) {
		tinfo.dead = 1;
		pthread_mutex_destroy(&tinfo.list_mutex);
		iv_event_raw_unregister(&tinfo.ier);
	}
}

void iv_event_post(struct iv_event *this)
{
	struct iv_event_thr_info *t = this->tinfo;

	pthread_mutex_lock(&t->list_mutex);
	if (list_empty(&this->list)) {
		list_add_tail(&this->list, &t->pending_events);
		iv_event_raw_post(&t->ier);
	}
	pthread_mutex_unlock(&t->list_mutex);
}
