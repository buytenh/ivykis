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
#include <inttypes.h>
#include <iv.h>
#include <iv_event.h>
#include <iv_list.h>
#include <iv_thread.h>
#include <iv_tls.h>
#include <iv_work.h>
#include "iv_private.h"
#include "mutex.h"

/* data structures **********************************************************/
struct work_pool_priv {
	__mutex_t		lock;
	struct iv_event		ev;
	int			shutting_down;
	int			started_threads;
	struct iv_list_head	idle_threads;
	void			*cookie;
	void			(*thread_start)(void *cookie);
	void			(*thread_stop)(void *cookie);
	uint32_t		seq_head;
	uint32_t		seq_tail;
	struct iv_list_head	work_items;
	struct iv_list_head	work_done;
};

struct work_pool_thread {
	struct work_pool_priv	*pool;
	struct iv_list_head	list;
	struct iv_event		kick;
	struct iv_timer		idle_timer;
};


/* worker thread ************************************************************/
static void __iv_work_thread_die(struct work_pool_thread *thr)
{
	struct work_pool_priv *pool = thr->pool;

	if (iv_event_is_active(&thr->kick))
		iv_fatal("__iv_work_thread_die: called on kicked thread");

	if (!iv_list_empty(&thr->list))
		iv_fatal("__iv_work_thread_die: thread still on list");

	iv_event_unregister(&thr->kick);
	free(thr);

	pool->started_threads--;

	if (pool->thread_stop != NULL)
		pool->thread_stop(pool->cookie);

	if (pool->shutting_down && !pool->started_threads)
		iv_event_post(&pool->ev);
}

static void iv_work_thread_got_event(void *_thr)
{
	struct work_pool_thread *thr = _thr;
	struct work_pool_priv *pool = thr->pool;
	uint32_t last_seq;

	mutex_lock(&pool->lock);

	if (!iv_list_empty(&thr->list)) {
		iv_list_del_init(&thr->list);
		iv_timer_unregister(&thr->idle_timer);
	}

	last_seq = pool->seq_tail;
	while ((int32_t)(last_seq - pool->seq_head) > 0) {
		struct iv_work_item *work;

		pool->seq_head++;
		work = iv_container_of(pool->work_items.next,
				       struct iv_work_item, list);
		iv_list_del(&work->list);

		mutex_unlock(&pool->lock);
		work->work(work->cookie);
		iv_invalidate_now();
		mutex_lock(&pool->lock);

		if (iv_list_empty(&pool->work_done))
			iv_event_post(&pool->ev);
		iv_list_add_tail(&work->list, &pool->work_done);
	}

	if (pool->seq_head == pool->seq_tail) {
		if (!pool->shutting_down) {
			iv_list_add(&thr->list, &pool->idle_threads);
			iv_validate_now();
			thr->idle_timer.expires = iv_now;
			thr->idle_timer.expires.tv_sec += 10;
			iv_timer_register(&thr->idle_timer);
		} else {
			__iv_work_thread_die(thr);
		}
	} else {
		/*
		 * If we're already at the maximum number of pool
		 * threads, and none of those threads were idle when
		 * more work arrived, then there may have been no
		 * kick sent for the new work item(s) (and no new
		 * pool thread started either), so if we're leaving
		 * with work items still pending, make sure we get
		 * called again, so that we don't deadlock.
		 */
		iv_event_post(&thr->kick);
	}

	mutex_unlock(&pool->lock);
}

static void iv_work_thread_idle_timeout(void *_thr)
{
	struct work_pool_thread *thr = _thr;
	struct work_pool_priv *pool = thr->pool;

	mutex_lock(&pool->lock);
	
	if (iv_event_is_active(&thr->kick)) {
		thr->idle_timer.expires = iv_now;
		thr->idle_timer.expires.tv_sec += 10;
		iv_timer_register(&thr->idle_timer);
	} else {
		iv_list_del_init(&thr->list);
		__iv_work_thread_die(thr);
	}

	mutex_unlock(&pool->lock);
}

static void iv_work_thread(void *_thr)
{
	struct work_pool_thread *thr = _thr;
	struct work_pool_priv *pool = thr->pool;

	iv_init();

	INIT_IV_LIST_HEAD(&thr->list);

	IV_EVENT_INIT(&thr->kick);
	thr->kick.cookie = thr;
	thr->kick.handler = iv_work_thread_got_event;
	iv_event_register(&thr->kick);

	IV_TIMER_INIT(&thr->idle_timer);
	thr->idle_timer.cookie = thr;
	thr->idle_timer.handler = iv_work_thread_idle_timeout;

	if (pool->thread_start != NULL)
		pool->thread_start(pool->cookie);

	iv_event_post(&thr->kick);

	iv_main();

	iv_deinit();
}


/* main thread **************************************************************/
static void iv_work_event(void *_pool)
{
	struct work_pool_priv *pool = _pool;
	struct iv_list_head items;

	mutex_lock(&pool->lock);
	__iv_list_steal_elements(&pool->work_done, &items);
	mutex_unlock(&pool->lock);

	while (!iv_list_empty(&items)) {
		struct iv_work_item *work;

		work = iv_container_of(items.next, struct iv_work_item, list);
		iv_list_del(&work->list);

		work->completion(work->cookie);
	}

	if (pool->shutting_down) {
		mutex_lock(&pool->lock);
		if (!pool->started_threads && iv_list_empty(&pool->work_done)) {
			mutex_unlock(&pool->lock);
			mutex_destroy(&pool->lock);
			iv_event_unregister(&pool->ev);
			free(pool);
			return;
		}
		mutex_unlock(&pool->lock);
	}
}

int iv_work_pool_create(struct iv_work_pool *this)
{
	struct work_pool_priv *pool;
	int ret;

	pool = malloc(sizeof(*pool));
	if (pool == NULL)
		return -1;

	ret = mutex_init(&pool->lock);
	if (ret) {
		free(pool);
		return -1;
	}

	IV_EVENT_INIT(&pool->ev);
	pool->ev.cookie = pool;
	pool->ev.handler = iv_work_event;
	iv_event_register(&pool->ev);

	pool->shutting_down = 0;
	pool->started_threads = 0;
	INIT_IV_LIST_HEAD(&pool->idle_threads);
	pool->cookie = this->cookie;
	pool->thread_start = this->thread_start;
	pool->thread_stop = this->thread_stop;
	pool->seq_head = 0;
	pool->seq_tail = 0;
	INIT_IV_LIST_HEAD(&pool->work_items);
	INIT_IV_LIST_HEAD(&pool->work_done);

	this->priv = pool;

	return 0;
}

void iv_work_pool_put(struct iv_work_pool *this)
{
	struct work_pool_priv *pool = this->priv;
	struct iv_list_head *ilh;

	mutex_lock(&pool->lock);

	this->priv = NULL;
	pool->shutting_down = 1;

	if (!pool->started_threads) {
		mutex_unlock(&pool->lock);
		iv_event_post(&pool->ev);
		return;
	}

	iv_list_for_each (ilh, &pool->idle_threads) {
		struct work_pool_thread *thr;

		thr = iv_container_of(ilh, struct work_pool_thread, list);
		iv_event_post(&thr->kick);
	}

	mutex_unlock(&pool->lock);
}

static int iv_work_start_thread(struct work_pool_priv *pool)
{
	struct work_pool_thread *thr;
	char name[512];
	int ret;

	thr = malloc(sizeof(*thr));
	if (thr == NULL)
		return -1;

	thr->pool = pool;

	snprintf(name, sizeof(name), "iv_work pool %p thread %p", pool, thr);

	ret = iv_thread_create(name, iv_work_thread, thr);
	if (ret < 0) {
		free(thr);
		return -1;
	}

	pool->started_threads++;

	return 0;
}

static void
iv_work_submit_pool(struct iv_work_pool *this, struct iv_work_item *work)
{
	struct work_pool_priv *pool = this->priv;

	mutex_lock(&pool->lock);

	pool->seq_tail++;
	iv_list_add_tail(&work->list, &pool->work_items);

	if (!iv_list_empty(&pool->idle_threads)) {
		struct work_pool_thread *thr;

		thr = iv_container_of(pool->idle_threads.next,
				      struct work_pool_thread, list);
		iv_event_post(&thr->kick);
	} else if (pool->started_threads < this->max_threads) {
		iv_work_start_thread(pool);
	}

	mutex_unlock(&pool->lock);
}

struct iv_work_thr_info {
	struct iv_task		task;
	struct iv_list_head	work_items;
};

static void iv_work_handle_local(void *_tinfo);

static void iv_work_tls_init_thread(void *_tinfo)
{
	struct iv_work_thr_info *tinfo = _tinfo;

	IV_TASK_INIT(&tinfo->task);
	tinfo->task.cookie = tinfo;
	tinfo->task.handler = iv_work_handle_local;

	INIT_IV_LIST_HEAD(&tinfo->work_items);
}

static struct iv_tls_user iv_work_tls_user = {
	.sizeof_state	= sizeof(struct iv_work_thr_info),
	.init_thread	= iv_work_tls_init_thread,
};

static void iv_work_tls_init(void) __attribute__((constructor));
static void iv_work_tls_init(void)
{
	iv_tls_user_register(&iv_work_tls_user);
}

static void iv_work_handle_local(void *_tinfo)
{
	struct iv_work_thr_info *tinfo = _tinfo;
	struct iv_list_head items;

	__iv_list_steal_elements(&tinfo->work_items, &items);
	while (!iv_list_empty(&items)) {
		struct iv_work_item *work;

		work = iv_container_of(items.next, struct iv_work_item, list);
		iv_list_del(&work->list);

		work->work(work->cookie);
		work->completion(work->cookie);
	}
}

static void iv_work_submit_local(struct iv_work_item *work)
{
	struct iv_work_thr_info *tinfo = iv_tls_user_ptr(&iv_work_tls_user);

	if (iv_list_empty(&tinfo->work_items))
		iv_task_register(&tinfo->task);

	iv_list_add_tail(&work->list, &tinfo->work_items);
}

void
iv_work_pool_submit_work(struct iv_work_pool *this, struct iv_work_item *work)
{
	if (this != NULL)
		iv_work_submit_pool(this, work);
	else
		iv_work_submit_local(work);
}
