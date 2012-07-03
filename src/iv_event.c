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
	struct iv_task		run_locally_queued;
	__mutex_t		list_mutex;
	struct iv_list_head	pending_events;
	struct iv_list_head	pending_events_no_handler;
	char			zero_edge;
	char			one_level;
};

static int iv_event_use_event_raw;

static int iv_event_is_level(struct iv_event *this)
{
	return *((char *)this->tinfo);
}

static struct iv_event_thr_info *iv_event_tinfo(struct iv_event *this)
{
	if (iv_event_is_level(this)) {
		return iv_container_of(this->tinfo, struct iv_event_thr_info,
				       one_level);
	} else {
		return iv_container_of(this->tinfo, struct iv_event_thr_info,
				       zero_edge);
	}
}

static void __iv_event_run_pending_events(void *_tinfo)
{
	struct iv_event_thr_info *tinfo = _tinfo;
	struct iv_list_head events;

	mutex_lock(&tinfo->list_mutex);

	if (iv_list_empty(&tinfo->pending_events)) {
		mutex_unlock(&tinfo->list_mutex);
		return;
	}

	__iv_list_steal_elements(&tinfo->pending_events, &events);
	while (1) {
		struct iv_event *ie;
		int empty_now;

		ie = iv_container_of(events.next, struct iv_event, list);
		if (iv_event_is_level(ie)) {
			iv_list_del(&ie->list);
			iv_list_add_tail(&ie->list, &tinfo->pending_events);
		} else {
			iv_list_del_init(&ie->list);
		}
		empty_now = !!iv_list_empty(&events);

		mutex_unlock(&tinfo->list_mutex);

		ie->handler(ie->cookie);
		if (empty_now)
			break;

		mutex_lock(&tinfo->list_mutex);
	}

	if (!iv_list_empty(&tinfo->pending_events) &&
	    !iv_task_registered(&tinfo->run_locally_queued))
		iv_task_register(&tinfo->run_locally_queued);

	mutex_unlock(&tinfo->list_mutex);
}

static void iv_event_tls_init_thread(void *_tinfo)
{
	struct iv_event_thr_info *tinfo = _tinfo;

	tinfo->event_count = 0;

	IV_EVENT_RAW_INIT(&tinfo->u.ier);
	tinfo->u.ier.cookie = tinfo;
	tinfo->u.ier.handler = __iv_event_run_pending_events;

	IV_TASK_INIT(&tinfo->run_locally_queued);
	tinfo->run_locally_queued.cookie = tinfo;
	tinfo->run_locally_queued.handler = __iv_event_run_pending_events;

	mutex_init(&tinfo->list_mutex);

	INIT_IV_LIST_HEAD(&tinfo->pending_events);

	INIT_IV_LIST_HEAD(&tinfo->pending_events_no_handler);

	tinfo->zero_edge = 0;

	tinfo->one_level = 1;
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
	struct iv_state *st = iv_get_state();
	struct iv_event_thr_info *tinfo =
		__iv_tls_user_ptr(st, &iv_event_tls_user);

	if (!tinfo->event_count++ && is_mt_app()) {
		if (!iv_event_use_event_raw) {
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

	this->tinfo = &tinfo->zero_edge;

	INIT_IV_LIST_HEAD(&this->list);

	return 0;
}

void iv_event_unregister(struct iv_event *this)
{
	struct iv_event_thr_info *tinfo = iv_event_tinfo(this);

	if (!iv_list_empty(&this->list)) {
		mutex_lock(&tinfo->list_mutex);
		iv_list_del(&this->list);
		mutex_unlock(&tinfo->list_mutex);
	}

	if (!--tinfo->event_count && is_mt_app()) {
		if (!iv_event_use_event_raw) {
			event_rx_off(tinfo->u.st);
			tinfo->u.st = NULL;
		} else {
			iv_event_raw_unregister(&tinfo->u.ier);
		}
	}
}

static void iv_event_kick_tinfo(struct iv_event_thr_info *tinfo)
{
	struct iv_event_thr_info *me = iv_tls_user_ptr(&iv_event_tls_user);

	if (tinfo == me) {
		if (!iv_task_registered(&me->run_locally_queued))
			iv_task_register(&me->run_locally_queued);
	} else if (!iv_event_use_event_raw) {
		event_send(tinfo->u.st);
	} else {
		iv_event_raw_post(&tinfo->u.ier);
	}
}

void iv_event_set_handler(struct iv_event *this, void (*handler)(void *))
{
	struct iv_event_thr_info *tinfo = iv_event_tinfo(this);
	int post;

	post = 0;

	mutex_lock(&tinfo->list_mutex);
	if (!iv_list_empty(&this->list)) {
		if (this->handler == NULL && handler != NULL) {
			iv_list_del(&this->list);
			if (iv_list_empty(&tinfo->pending_events))
				post = 1;
			iv_list_add_tail(&this->list, &tinfo->pending_events);
		} else if (this->handler != NULL && handler == NULL) {
			iv_list_del(&this->list);
			iv_list_add_tail(&this->list,
					 &tinfo->pending_events_no_handler);
		}
	}
	this->handler = handler;
	mutex_unlock(&tinfo->list_mutex);

	if (post)
		iv_event_kick_tinfo(tinfo);
}

static void __iv_event_activate(struct iv_event *this, int is_level)
{
	struct iv_event_thr_info *tinfo = iv_event_tinfo(this);
	int post;

	post = 0;

	mutex_lock(&tinfo->list_mutex);

	if (is_level)
		this->tinfo = &tinfo->one_level;
	else
		this->tinfo = &tinfo->zero_edge;

	if (iv_list_empty(&this->list)) {
		struct iv_list_head *list;

		if (this->handler != NULL) {
			list = &tinfo->pending_events;
			if (iv_list_empty(list))
				post = 1;
		} else {
			list = &tinfo->pending_events_no_handler;
		}
		iv_list_add_tail(&this->list, list);
	}

	mutex_unlock(&tinfo->list_mutex);

	if (post)
		iv_event_kick_tinfo(tinfo);
}

void iv_event_post(struct iv_event *this)
{
	__iv_event_activate(this, 0);
}

void iv_event_set_active(struct iv_event *this)
{
	__iv_event_activate(this, 1);
}

void iv_event_set_inactive(struct iv_event *this)
{
	struct iv_event_thr_info *me = iv_tls_user_ptr(&iv_event_tls_user);
	struct iv_event_thr_info *tinfo = iv_event_tinfo(this);

	if (me != tinfo)
		iv_fatal("iv_event_set_inactive: called from non-owner thread");

	mutex_lock(&tinfo->list_mutex);
	if (!iv_list_empty(&this->list))
		iv_list_del_init(&this->list);
	mutex_unlock(&tinfo->list_mutex);
}

int iv_event_is_active(struct iv_event *this)
{
	return !iv_list_empty(&this->list);
}
