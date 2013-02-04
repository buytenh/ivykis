/*
 * ivykis, an event handling library
 * Copyright (C) 2010, 2013, 2016 Lennert Buytenhek
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
#include <iv_thread.h>
#include <iv_tls.h>
#include <string.h>
#include "iv_private.h"
#include "iv_thread_private.h"


/* data structures and global data ******************************************/
static pthr_once_t iv_thread_key_allocated = PTHR_ONCE_INIT;
static pthr_key_t iv_thread_key;


/* thread state handling ****************************************************/
static void iv_thread_destructor(void *_me)
{
	struct iv_thread *me = _me;
	struct iv_thread *parent;

	/*
	 * Let the ivykis destructor run first if there still is
	 * child state to clean up.
	 */
	if (!iv_list_empty(&me->children)) {
		pthr_setspecific(&iv_thread_key, me);
		return;
	}

	if (iv_thread_debug)
		fprintf(stderr, "iv_thread: [%s] terminating\n", me->name);

	___mutex_lock(&iv_thread_lock);

	parent = me->parent;
	if (parent != NULL) {
		iv_list_add_tail(&me->list_dead, &parent->children_dead);
		iv_event_post(&parent->child_died);
	} else {
		iv_list_del(&me->list);
		free(me->name);
		free(me);
	}

	___mutex_unlock(&iv_thread_lock);
}

static void iv_thread_allocate_key(void)
{
	if (pthr_key_create(&iv_thread_key, iv_thread_destructor)) {
		iv_fatal("iv_thread_tls_init_thread: failed "
			 "to allocate TLS key");
	}
}

static void
__iv_thread_release_child(struct iv_thread *me, struct iv_thread *thr)
{
	pthr_join(thr->ptid, NULL);

	if (iv_thread_debug) {
		fprintf(stderr, "iv_thread: [%lu:%s] joined [%lu:%s]\n",
			me->tid, me->name, thr->tid, thr->name);
	}

	iv_list_del(&thr->list);
	iv_list_del(&thr->list_dead);
	free(thr->name);
	free(thr);
}

static void iv_thread_child_died(void *_me)
{
	struct iv_thread *me = _me;

	___mutex_lock(&iv_thread_lock);

	while (!iv_list_empty(&me->children_dead)) {
		struct iv_thread *thr;

		thr = iv_container_of(me->children_dead.next,
				      struct iv_thread, list_dead);

		__iv_thread_release_child(me, thr);
	}

	if (iv_list_empty(&me->children))
		iv_event_unregister(&me->child_died);

	___mutex_unlock(&iv_thread_lock);
}

static struct iv_thread *
iv_thread_new(struct iv_thread *parent, const char *name)
{
	struct iv_thread *thr;

	thr = calloc(1, sizeof(struct iv_thread));
	if (thr == NULL)
		return NULL;

	INIT_IV_LIST_HEAD(&thr->list_dead);

	thr->parent = parent;
	thr->name = strdup(name);

	INIT_IV_LIST_HEAD(&thr->children);

	IV_EVENT_INIT(&thr->child_died);
	thr->child_died.cookie = thr;
	thr->child_died.handler = iv_thread_child_died;

	INIT_IV_LIST_HEAD(&thr->children_dead);

	return thr;
}

struct iv_thread *iv_thread_get_self(void)
{
	struct iv_thread *me;

	pthr_once(&iv_thread_key_allocated, iv_thread_allocate_key);

	me = pthr_getspecific(&iv_thread_key);
	if (me != NULL)
		return me;

	me = iv_thread_new(NULL, "root");
	if (me == NULL)
		return NULL;

	pthr_setspecific(&iv_thread_key, me);

	me->tid = iv_get_thread_id();
	me->ptid = pthr_self();

	___mutex_lock(&iv_thread_lock);
	iv_list_add_tail(&me->list, &iv_thread_roots);
	___mutex_unlock(&iv_thread_lock);

	return me;
}


/* initialization and deinitialization **************************************/
static void iv_thread_tls_init_thread(void *_dummy)
{
	iv_thread_get_self();
}

static void iv_thread_tls_deinit_thread(void *_dummy)
{
	struct iv_thread *me;

	me = pthr_getspecific(&iv_thread_key);
	if (me == NULL)
		return;

	___mutex_lock(&iv_thread_lock);

	if (iv_list_empty(&me->children)) {
		___mutex_unlock(&iv_thread_lock);
		return;
	}

	while (!iv_list_empty(&me->children)) {
		struct iv_thread *thr;

		thr = iv_list_entry(me->children.prev, struct iv_thread, list);
		if (iv_list_empty(&thr->list_dead)) {
			iv_list_del(&thr->list);
			iv_list_add(&thr->list, &me->list);

			thr->parent = me->parent;
			if (me->parent == NULL)
				pthr_detach(thr->ptid);
		} else {
			__iv_thread_release_child(me, thr);
		}
	}

	iv_event_unregister(&me->child_died);

	___mutex_unlock(&iv_thread_lock);
}

static struct iv_tls_user iv_thread_tls_user = {
	.init_thread	= iv_thread_tls_init_thread,
	.deinit_thread	= iv_thread_tls_deinit_thread,
};

static void iv_thread_init(void) __attribute__((constructor));
static void iv_thread_init(void)
{
	if (!pthreads_available())
		return;

	___mutex_init(&iv_thread_lock);
	INIT_IV_LIST_HEAD(&iv_thread_roots);

	iv_thread_debug = 0;

	iv_tls_user_register(&iv_thread_tls_user);
}


/* public API ***************************************************************/
static void *iv_thread_handler(void *_me)
{
	struct iv_thread *me = _me;

	pthr_setspecific(&iv_thread_key, me);
	me->tid = iv_get_thread_id();

	me->start_routine(me->arg);

	return NULL;
}

int iv_thread_create(const char *name, void (*start_routine)(void *), void *arg)
{
	struct iv_thread *me;
	struct iv_thread *thr;
	int ret;

	me = iv_thread_get_self();
	if (me == NULL)
		return -1;

	thr = iv_thread_new(me, name);
	if (thr == NULL)
		return -1;

	thr->start_routine = start_routine;
	thr->arg = arg;
	thr->tid = 0;

	___mutex_lock(&iv_thread_lock);

	ret = pthr_create(&thr->ptid, NULL, iv_thread_handler, thr);
	if (!ret) {
		if (iv_thread_debug) {
			fprintf(stderr, "iv_thread: [%lu:%s] started [%s]\n",
				me->tid, me->name, name);
		}

		if (iv_list_empty(&me->children))
			iv_event_register(&me->child_died);
		iv_list_add_tail(&thr->list, &me->children);
	} else {
		if (iv_thread_debug) {
			fprintf(stderr, "iv_thread: [%lu:%s] starting [%s] "
					"failed with error %d[%s]\n",
				me->tid, me->name, name, ret, strerror(ret));
		}

		free(thr->name);
		free(thr);
	}

	___mutex_unlock(&iv_thread_lock);

	return ret;
}
