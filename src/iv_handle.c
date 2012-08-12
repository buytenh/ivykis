/*
 * ivykis, an event handling library
 * Copyright (C) 2012 Lennert Buytenhek
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
#include "iv_private.h"
#include "iv_handle_private.h"

void iv_poll_init(struct iv_state *st)
{
	st->wait = CreateEvent(NULL, FALSE, FALSE, NULL);
	InitializeCriticalSection(&st->active_handle_list_lock);
	INIT_IV_LIST_HEAD(&st->active_handle_list);
	st->numhandles = 0;
	st->handled_handle = INVALID_HANDLE_VALUE;
}

void iv_poll_deinit(struct iv_state *st)
{
	CloseHandle(st->wait);
	DeleteCriticalSection(&st->active_handle_list_lock);
}

void iv_poll_and_run(struct iv_state *st, struct timespec *to)
{
	int msec;
	int ret;
	struct iv_list_head handles;

	msec = 1000 * to->tv_sec + (to->tv_nsec + 999999) / 1000000;

	ret = WaitForSingleObjectEx(st->wait, msec, TRUE);
	if (ret != WAIT_OBJECT_0 && ret != WAIT_IO_COMPLETION &&
	    ret != WAIT_TIMEOUT) {
		iv_fatal("iv_handle_poll_and_run: MsgWaitForMultipleObjectsEx "
			 "returned %x", (int)ret);
	}

	__iv_invalidate_now(st);

	EnterCriticalSection(&st->active_handle_list_lock);
	__iv_list_steal_elements(&st->active_handle_list, &handles);
	LeaveCriticalSection(&st->active_handle_list_lock);

	while (!iv_list_empty(&handles)) {
		struct iv_handle_ *h;

		h = iv_list_entry(handles.next, struct iv_handle_, list);
		iv_list_del_init(&h->list);

		st->handled_handle = h;
		h->handler(h->cookie);
		if (st->handled_handle == h) {
			SetEvent(h->rewait_handle);
			st->handled_handle = INVALID_HANDLE_VALUE;
		}
	}
}

void IV_HANDLE_INIT(struct iv_handle *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;

	h->registered = 0;
	h->polling = 0;
	h->st = NULL;
	INIT_IV_LIST_HEAD(&h->list);
	h->rewait_handle = INVALID_HANDLE_VALUE;
	h->thr_handle = INVALID_HANDLE_VALUE;
}

static DWORD WINAPI iv_handle_poll_thread(void *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;
	int waiting;

	waiting = 1;
	while (h->polling) {
		struct iv_state *st = h->st;
		HANDLE hnd;
		DWORD ret;
		int sig;

		hnd = waiting ? h->handle : h->rewait_handle;

		ret = WaitForSingleObjectEx(hnd, INFINITE, TRUE);
		if (ret == WAIT_IO_COMPLETION)
			continue;

		if (ret != WAIT_OBJECT_0 && ret != WAIT_ABANDONED) {
			iv_fatal("iv_handle_poll_thread(%d): %x",
				 (int)(ULONG_PTR)h->handle, (int)ret);
		}

		waiting ^= 1;
		if (waiting)
			continue;

		sig = 0;

		EnterCriticalSection(&st->active_handle_list_lock);
		if (iv_list_empty(&h->list)) {
			if (iv_list_empty(&st->active_handle_list))
				sig = 1;
			iv_list_add_tail(&h->list, &st->active_handle_list);
		}
		LeaveCriticalSection(&st->active_handle_list_lock);

		if (sig)
			SetEvent(st->wait);
	}

	return 0;
}

static void __iv_handle_register(struct iv_handle_ *h)
{
	HANDLE w;

	h->polling = 1;

	w = CreateThread(NULL, 0, iv_handle_poll_thread, (void *)h, 0, NULL);
	if (w == NULL)
		iv_fatal("__iv_handle_register: CreateThread failed");

	h->thr_handle = w;
}

void iv_handle_register(struct iv_handle *_h)
{
	struct iv_state *st = iv_get_state();
	struct iv_handle_ *h = (struct iv_handle_ *)_h;

	if (h->registered) {
		iv_fatal("iv_handle_register: called with handle "
			 "which is still registered");
	}

	h->registered = 1;
	h->st = st;
	INIT_IV_LIST_HEAD(&h->list);
	h->rewait_handle = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (h->handler != NULL)
		__iv_handle_register(h);

	st->numobjs++;
	st->numhandles++;
}

static void WINAPI iv_handle_dummy_apcproc(ULONG_PTR dummy)
{
}

static void __iv_handle_unregister(struct iv_handle_ *h)
{
	DWORD ret;

	h->polling = 0;

	ret = QueueUserAPC(iv_handle_dummy_apcproc, h->thr_handle, 0);
	if (ret == 0)
		iv_fatal("__iv_handle_unregister: QueueUserAPC fail");

	do {
		ret = WaitForSingleObjectEx(h->thr_handle, INFINITE, TRUE);
	} while (ret == WAIT_IO_COMPLETION);

	if (ret != WAIT_OBJECT_0)
		iv_fatal("__iv_handle_unregister: WaitForSingleObjectEx fail");

	CloseHandle(h->thr_handle);
	h->thr_handle = INVALID_HANDLE_VALUE;
}

void iv_handle_unregister(struct iv_handle *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;
	struct iv_state *st = h->st;

	if (!h->registered) {
		iv_fatal("iv_handle_unregister: called with handle "
			 "which is not registered");
	}
	h->registered = 0;

	if (h->handler != NULL)
		__iv_handle_unregister(h);

	h->st = NULL;
	iv_list_del_init(&h->list);
	CloseHandle(&h->rewait_handle);

	st->numobjs--;
	st->numhandles--;
	if (st->handled_handle == h)
		st->handled_handle = INVALID_HANDLE_VALUE;
}

int iv_handle_registered(struct iv_handle *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;

	return h->registered;
}

void iv_handle_set_handler(struct iv_handle *_h, void (*handler)(void *))
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;

	if (!h->registered) {
		iv_fatal("iv_handle_set_handler: called with handle "
			 "which is not registered");
	}

	if (h->handler == NULL && handler != NULL)
		__iv_handle_register(h);
	else if (h->handler != NULL && handler == NULL)
		__iv_handle_unregister(h);

	h->handler = handler;
}
