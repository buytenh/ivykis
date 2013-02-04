/*
 * ivykis, an event handling library
 * Copyright (C) 2010, 2012, 2013 Lennert Buytenhek
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
#include <iv_thread.h>
#include <iv_tls.h>
#include <string.h>
#include "iv_thread_private.h"


/* data structures and global data ******************************************/
static DWORD iv_thread_index = -1;


/* thread state handling ****************************************************/
static void iv_thread_child_died(void *_thr)
{
	struct iv_thread *thr = _thr;

	___mutex_lock(&iv_thread_lock);

	if (iv_thread_debug) {
		struct iv_thread *me;

		me = thr->parent;
		fprintf(stderr, "iv_thread: [%lx:%s] joined [%lx:%s]\n",
			me->tid, me->name, thr->tid, thr->name);
		fflush(stderr);
	}

	while (!iv_list_empty(&thr->children)) {
		struct iv_thread *child;

		child = iv_container_of(thr->children.prev,
					struct iv_thread, list);

		iv_list_del(&child->list);
		iv_list_add(&child->list, &thr->list);

		child->parent = thr->parent;

		iv_handle_register(&child->thr_handle);
	}
	iv_list_del(&thr->list);

	___mutex_unlock(&iv_thread_lock);

	free(thr->name);

	iv_handle_unregister(&thr->thr_handle);
	CloseHandle(thr->thr_handle.handle);

	free(thr);
}

static struct iv_thread *
iv_thread_new(struct iv_thread *parent, const char *name)
{
	struct iv_thread *thr;

	thr = calloc(1, sizeof(struct iv_thread));
	if (thr == NULL)
		return NULL;

	thr->parent = parent;
	thr->name = strdup(name);

	INIT_IV_LIST_HEAD(&thr->children);

	IV_HANDLE_INIT(&thr->thr_handle);
	thr->thr_handle.cookie = thr;
	thr->thr_handle.handler = iv_thread_child_died;

	return thr;
}

struct iv_thread *iv_thread_get_self(void)
{
	struct iv_thread *me;

	if (iv_thread_index == -1) {
		iv_thread_index = TlsAlloc();
		if (iv_thread_index == TLS_OUT_OF_INDEXES) {
			iv_fatal("iv_thread_tls_init_thread: failed "
				 "to allocate TLS key");
		}
	}

	me = TlsGetValue(iv_thread_index);
	if (me != NULL)
		return me;

	me = iv_thread_new(NULL, "root");
	if (me == NULL)
		return NULL;

	TlsSetValue(iv_thread_index, me);

	me->tid = iv_get_thread_id();

	___mutex_lock(&iv_thread_lock);
	iv_list_add_tail(&me->list, &iv_thread_roots);
	___mutex_unlock(&iv_thread_lock);

	return me;
}


/* initialization and deinitialization **************************************/
static void iv_thread_tls_init_thread(void *_dummy)
{
	struct iv_thread *me = iv_thread_get_self();
	struct iv_list_head *ilh;

	iv_list_for_each (ilh, &me->children) {
		struct iv_thread *thr;

		thr = iv_container_of(ilh, struct iv_thread, list);
		iv_handle_register(&thr->thr_handle);
	}
}

static void iv_thread_tls_deinit_thread(void *_dummy)
{
	struct iv_thread *me = iv_thread_get_self();
	struct iv_list_head *ilh;

	iv_list_for_each (ilh, &me->children) {
		struct iv_thread *thr;

		thr = iv_container_of(ilh, struct iv_thread, list);
		iv_handle_unregister(&thr->thr_handle);
	}
}

static struct iv_tls_user iv_thread_tls_user = {
	.init_thread	= iv_thread_tls_init_thread,
	.deinit_thread	= iv_thread_tls_deinit_thread,
};

static void iv_thread_init(void) __attribute__((constructor));
static void iv_thread_init(void)
{
	___mutex_init(&iv_thread_lock);
	INIT_IV_LIST_HEAD(&iv_thread_roots);

	iv_thread_debug = 0;

	iv_tls_user_register(&iv_thread_tls_user);
}

void iv_thread_deinit(void)
{
	if (iv_thread_index != -1) {
		TlsFree(iv_thread_index);
		iv_thread_index = -1;
	}

	___mutex_destroy(&iv_thread_lock);
}

void iv_thread_deinit_this_thread(void)
{
	struct iv_thread *me;

	if (iv_thread_index == -1)
		return;

	me = TlsGetValue(iv_thread_index);
	if (me == NULL || me->parent != NULL)
		return;

	___mutex_lock(&iv_thread_lock);

	while (!iv_list_empty(&me->children)) {
		struct iv_thread *thr;

		thr = iv_container_of(me->children.prev,
				      struct iv_thread, list);

		iv_list_del(&thr->list);
		iv_list_add(&thr->list, &me->list);

		thr->parent = NULL;

		CloseHandle(thr->thr_handle.handle);
		thr->thr_handle.handle = INVALID_HANDLE_VALUE;
	}

	iv_list_del(&me->list);

	___mutex_unlock(&iv_thread_lock);

	free(me->name);
	free(me);
}


/* public API ***************************************************************/
static DWORD WINAPI iv_thread_handler(void *_me)
{
	struct iv_thread *me = _me;

	TlsSetValue(iv_thread_index, me);

	me->start_routine(me->arg);

	return 0;
}

int iv_thread_create(const char *name, void (*start_routine)(void *), void *arg)
{
	struct iv_thread *me;
	struct iv_thread *thr;
	HANDLE h;

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

	h = CreateThread(NULL, 0, iv_thread_handler, thr, 0, &thr->tid);
	if (h != NULL) {
		if (iv_thread_debug) {
			fprintf(stderr, "iv_thread: [%lx:%s] started "
					"[%lx:%s]\n", me->tid, me->name,
				thr->tid, name);
			fflush(stderr);
		}

		thr->thr_handle.handle = h;
		iv_handle_register(&thr->thr_handle);

		iv_list_add_tail(&thr->list, &me->children);
	} else {
		if (iv_thread_debug) {
			fprintf(stderr, "iv_thread: [%lx:%s] starting [%s] "
					"failed\n", me->tid, me->name, name);
			fflush(stderr);
		}

		free(thr->name);
		free(thr);
	}

	___mutex_unlock(&iv_thread_lock);

	return (h != NULL) ? 0 : -1;
}
