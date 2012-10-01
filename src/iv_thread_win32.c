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
#include <iv_thread.h>
#include <iv_tls.h>
#include <string.h>

/* data structures and global data ******************************************/
struct iv_thread {
	struct iv_list_head	list;
	DWORD			thread_id;
	struct iv_handle	thread_handle;
	char			*name;
	void			(*start_routine)(void *);
	void			*arg;
};

static int iv_thread_debug;


/* tls **********************************************************************/
struct iv_thread_thr_info {
	struct iv_list_head	child_threads;
};

static void iv_thread_tls_init_thread(void *_tinfo)
{
	struct iv_thread_thr_info *tinfo = _tinfo;

	INIT_IV_LIST_HEAD(&tinfo->child_threads);
}

static void iv_thread_tls_deinit_thread(void *_tinfo)
{
	struct iv_thread_thr_info *tinfo = _tinfo;
	struct iv_list_head *ilh;

	iv_list_for_each (ilh, &tinfo->child_threads) {
		struct iv_thread *thr;

		thr = iv_list_entry(ilh, struct iv_thread, list);

		iv_handle_unregister(&thr->thread_handle);
		CloseHandle(thr->thread_handle.handle);
		thr->thread_handle.handle = INVALID_HANDLE_VALUE;
	}
}

static struct iv_tls_user iv_thread_tls_user = {
	.sizeof_state	= sizeof(struct iv_thread_thr_info),
	.init_thread	= iv_thread_tls_init_thread,
	.deinit_thread	= iv_thread_tls_deinit_thread,
};

static void iv_thread_tls_init(void) __attribute__((constructor));
static void iv_thread_tls_init(void)
{
	iv_tls_user_register(&iv_thread_tls_user);
}


/* callee thread ************************************************************/
static DWORD WINAPI iv_thread_handler(void *_thr)
{
	struct iv_thread *thr = _thr;

	thr->start_routine(thr->arg);

	return 0;
}


/* calling thread ***********************************************************/
static void iv_thread_died(void *_thr)
{
	struct iv_thread *thr = _thr;

	iv_handle_unregister(&thr->thread_handle);
	CloseHandle(thr->thread_handle.handle);

	if (iv_thread_debug)
		fprintf(stderr, "iv_thread: [%s] terminated\n", thr->name);

	iv_list_del(&thr->list);
	free(thr->name);
	free(thr);
}

int iv_thread_create(const char *name, void (*start_routine)(void *), void *arg)
{
	struct iv_thread_thr_info *tinfo = iv_tls_user_ptr(&iv_thread_tls_user);
	struct iv_thread *thr;
	HANDLE h;

	thr = malloc(sizeof(*thr));
	if (thr == NULL)
		return -1;

	IV_HANDLE_INIT(&thr->thread_handle);
	thr->thread_handle.cookie = thr;
	thr->thread_handle.handler = iv_thread_died;
	thr->name = strdup(name);
	thr->start_routine = start_routine;
	thr->arg = arg;

	h = CreateThread(NULL, 0, iv_thread_handler, thr, 0, &thr->thread_id);
	if (h == NULL)
		goto out;

	thr->thread_handle.handle = h;
	iv_handle_register(&thr->thread_handle);
	iv_list_add_tail(&thr->list, &tinfo->child_threads);

	if (iv_thread_debug)
		fprintf(stderr, "iv_thread: [%s] started\n", name);

	return 0;

out:
	free(thr->name);
	free(thr);

	if (iv_thread_debug)
		fprintf(stderr, "iv_thread: [%s] failed to start\n", name);

	return -1;
}


/* misc functionality *******************************************************/
void iv_thread_set_debug_state(int state)
{
	iv_thread_debug = !!state;
}

unsigned long iv_thread_get_id(void)
{
	return iv_get_thread_id();
}

void iv_thread_list_children(void)
{
	struct iv_thread_thr_info *tinfo = iv_tls_user_ptr(&iv_thread_tls_user);
	struct iv_list_head *ilh;

	fprintf(stderr, "tid\tname\n");
	fprintf(stderr, "%d\tself\n", (int)GetCurrentThreadId());

	iv_list_for_each (ilh, &tinfo->child_threads) {
		struct iv_thread *thr;

		thr = iv_list_entry(ilh, struct iv_thread, list);
		fprintf(stderr, "%d\t%s\n", (int)thr->thread_id, thr->name);
	}
}
