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
#include <iv_list.h>
#include <iv_thread.h>
#include <iv_work.h>
#include <pthread.h>

/* data structures **********************************************************/
struct work_pool_priv {
	pthread_mutex_t		lock;
	struct iv_event		ev;
	struct iv_work_pool	*public;
	void			*cookie;
	void			(*thread_start)(void *cookie);
	void			(*thread_stop)(void *cookie);
	int			started_threads;
	struct list_head	idle_threads;
	struct list_head	work_items;
	struct list_head	work_done;
};

struct work_pool_thread {
	struct work_pool_priv	*pool;
	struct list_head	list;
	int			kicked;
	struct iv_event		kick;
	struct iv_timer		idle_timer;
};


/* worker thread ************************************************************/
static void __iv_work_thread_cleanup(struct work_pool_thread *thr, int timeout)
{
	struct work_pool_priv *pool = thr->pool;

	list_del(&thr->list);
	iv_event_unregister(&thr->kick);
	if (!timeout)
		iv_timer_unregister(&thr->idle_timer);
	free(thr);

	pool->started_threads--;
}

static void iv_work_thread_got_event(void *_thr)
{
	struct work_pool_thread *thr = _thr;
	struct work_pool_priv *pool = thr->pool;

	pthread_mutex_lock(&pool->lock);

	thr->kicked = 0;

	while (1) {
		struct iv_work_item *work;

		if (list_empty(&pool->work_items))
			break;

		work = container_of(pool->work_items.next,
				    struct iv_work_item, list);
		list_del(&work->list);

		list_del(&thr->list);
		iv_timer_unregister(&thr->idle_timer);

		pthread_mutex_unlock(&pool->lock);
		work->work(work->cookie);
		iv_invalidate_now();
		pthread_mutex_lock(&pool->lock);

		list_add(&thr->list, &pool->idle_threads);
		iv_validate_now();
		thr->idle_timer.expires = iv_now;
		thr->idle_timer.expires.tv_sec += 10;
		iv_timer_register(&thr->idle_timer);

		list_add_tail(&work->list, &pool->work_done);
		iv_event_post(&pool->ev);
	}

	if (pool->public == NULL) {
		__iv_work_thread_cleanup(thr, 0);
		if (!pool->started_threads)
			iv_event_post(&pool->ev);
	}

	pthread_mutex_unlock(&pool->lock);
}

static void iv_work_thread_idle_timeout(void *_thr)
{
	struct work_pool_thread *thr = _thr;
	struct work_pool_priv *pool = thr->pool;

	pthread_mutex_lock(&pool->lock);
	
	if (thr->kicked) {
		thr->idle_timer.expires = iv_now;
		thr->idle_timer.expires.tv_sec += 10;
		iv_timer_register(&thr->idle_timer);

		pthread_mutex_unlock(&pool->lock);

		return;
	}

	__iv_work_thread_cleanup(thr, 1);

	if (pool->public == NULL && !pool->started_threads)
		iv_event_post(&pool->ev);

	pthread_mutex_unlock(&pool->lock);
}

static void iv_work_thread(void *_thr)
{
	struct work_pool_thread *thr = _thr;
	struct work_pool_priv *pool = thr->pool;

	iv_init();

	thr->kicked = 0;

	IV_EVENT_INIT(&thr->kick);
	thr->kick.cookie = thr;
	thr->kick.handler = iv_work_thread_got_event;
	iv_event_register(&thr->kick);

	IV_TIMER_INIT(&thr->idle_timer);
	iv_validate_now();
	thr->idle_timer.expires = iv_now;
	thr->idle_timer.expires.tv_sec += 10;
	thr->idle_timer.cookie = thr;
	thr->idle_timer.handler = iv_work_thread_idle_timeout;
	iv_timer_register(&thr->idle_timer);

	pthread_mutex_lock(&pool->lock);
	list_add_tail(&thr->list, &pool->idle_threads);
	pthread_mutex_unlock(&pool->lock);

	if (pool->thread_start != NULL)
		pool->thread_start(pool->cookie);

	iv_event_post(&thr->kick);

	iv_main();

	if (pool->thread_stop != NULL)
		pool->thread_stop(pool->cookie);

	iv_deinit();
}


/* main thread **************************************************************/
static void iv_work_event(void *_pool)
{
	struct work_pool_priv *pool = _pool;

	pthread_mutex_lock(&pool->lock);

	while (!list_empty(&pool->work_done)) {
		struct iv_work_item *work;

		work = container_of(pool->work_done.next,
				    struct iv_work_item, list);

		list_del(&work->list);

		pthread_mutex_unlock(&pool->lock);
		work->completion(work->cookie);
		pthread_mutex_lock(&pool->lock);
	}

	if (pool->public == NULL && !pool->started_threads) {
		pthread_mutex_unlock(&pool->lock);
		pthread_mutex_destroy(&pool->lock);
		iv_event_unregister(&pool->ev);
		free(pool);
		return;
	}

	pthread_mutex_unlock(&pool->lock);
}

int iv_work_pool_create(struct iv_work_pool *this)
{
	struct work_pool_priv *pool;
	int ret;

	pool = malloc(sizeof(*pool));
	if (pool == NULL)
		return -1;

	ret = pthread_mutex_init(&pool->lock, NULL);
	if (ret < 0) {
		free(pool);
		return -1;
	}

	IV_EVENT_INIT(&pool->ev);
	pool->ev.cookie = pool;
	pool->ev.handler = iv_work_event;
	iv_event_register(&pool->ev);

	pool->public = this;
	pool->cookie = this->cookie;
	pool->thread_start = this->thread_start;
	pool->thread_stop = this->thread_stop;
	pool->started_threads = 0;
	INIT_LIST_HEAD(&pool->idle_threads);
	INIT_LIST_HEAD(&pool->work_items);
	INIT_LIST_HEAD(&pool->work_done);

	this->priv = pool;

	return 0;
}

void iv_work_pool_put(struct iv_work_pool *this)
{
	struct work_pool_priv *pool = this->priv;
	struct list_head *lh;

	pthread_mutex_lock(&pool->lock);

	this->priv = NULL;
	pool->public = NULL;

	if (!pool->started_threads) {
		pthread_mutex_unlock(&pool->lock);
		iv_event_post(&pool->ev);
		return;
	}

	list_for_each (lh, &pool->idle_threads) {
		struct work_pool_thread *thr;

		thr = container_of(lh, struct work_pool_thread, list);
		iv_event_post(&thr->kick);
	}

	pthread_mutex_unlock(&pool->lock);
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

void
iv_work_pool_submit_work(struct iv_work_pool *this, struct iv_work_item *work)
{
	struct work_pool_priv *pool = this->priv;

	pthread_mutex_lock(&pool->lock);

	list_add_tail(&work->list, &pool->work_items);

	if (!list_empty(&pool->idle_threads)) {
		struct work_pool_thread *thr;

		thr = container_of(pool->idle_threads.next,
				   struct work_pool_thread, list);
		thr->kicked = 1;
		iv_event_post(&thr->kick);
	} else if (pool->started_threads < this->max_threads) {
		iv_work_start_thread(pool);
	}

	pthread_mutex_unlock(&pool->lock);
}
