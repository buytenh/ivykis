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
#include <iv_tls.h>
#include <pthread.h>
#include <signal.h>

struct iv_event_thr_info {
	int			event_count;
	struct iv_event_raw	ier;
	pthread_mutex_t		list_mutex;
	struct list_head	pending_events;
};

static void iv_event_run_pending_events(void *_tinfo)
{
	struct iv_event_thr_info *tinfo = _tinfo;
	struct list_head last;

	pthread_mutex_lock(&tinfo->list_mutex);

	list_add_tail(&last, &tinfo->pending_events);

	while (tinfo->pending_events.next != &last) {
		struct iv_event *ie;

		ie = container_of(tinfo->pending_events.next,
				  struct iv_event, list);

		list_del_init(&ie->list);

		pthread_mutex_unlock(&tinfo->list_mutex);

		ie->handler(ie->cookie);

		pthread_mutex_lock(&tinfo->list_mutex);
	}

	list_del(&last);

	pthread_mutex_unlock(&tinfo->list_mutex);
}

static void iv_event_tls_init_thread(void *_tinfo)
{
	struct iv_event_thr_info *tinfo = _tinfo;

	tinfo->event_count = 0;

	IV_EVENT_RAW_INIT(&tinfo->ier);
	tinfo->ier.cookie = tinfo;
	tinfo->ier.handler = iv_event_run_pending_events;

	pthread_mutex_init(&tinfo->list_mutex, NULL);

	INIT_LIST_HEAD(&tinfo->pending_events);
}

static void iv_event_tls_deinit_thread(void *_tinfo)
{
	struct iv_event_thr_info *tinfo = _tinfo;

	pthread_mutex_destroy(&tinfo->list_mutex);
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

int iv_event_register(struct iv_event *this)
{
	struct iv_event_thr_info *tinfo = iv_tls_user_ptr(&iv_event_tls_user);

	if (!tinfo->event_count++) {
		int ret;

		ret = iv_event_raw_register(&tinfo->ier);
		if (ret)
			return ret;
	}

	this->tinfo = tinfo;
	INIT_LIST_HEAD(&this->list);

	return 0;
}

void iv_event_unregister(struct iv_event *this)
{
	struct iv_event_thr_info *tinfo = iv_tls_user_ptr(&iv_event_tls_user);

	if (!list_empty(&this->list)) {
		pthread_mutex_lock(&tinfo->list_mutex);
		list_del(&this->list);
		pthread_mutex_unlock(&tinfo->list_mutex);
	}

	if (!--tinfo->event_count)
		iv_event_raw_unregister(&tinfo->ier);
}

void iv_event_post(struct iv_event *this)
{
	struct iv_event_thr_info *tinfo = this->tinfo;

	pthread_mutex_lock(&tinfo->list_mutex);
	if (list_empty(&this->list)) {
		list_add_tail(&this->list, &tinfo->pending_events);
		iv_event_raw_post(&tinfo->ier);
	}
	pthread_mutex_unlock(&tinfo->list_mutex);
}
