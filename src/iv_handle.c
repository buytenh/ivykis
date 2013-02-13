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

void iv_handle_init(struct iv_state *st)
{
	st->wait = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (st->wait == NULL)
		iv_fatal("iv_handle_init: CreateEvent failed");

	st->thread_stop = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (st->thread_stop == NULL)
		iv_fatal("iv_handle_init: CreateEvent failed");

	INIT_IV_LIST_HEAD(&st->handles);
	InitializeCriticalSection(&st->active_handle_list_lock);
	INIT_IV_LIST_HEAD(&st->active_with_handler);
	INIT_IV_LIST_HEAD(&st->active_without_handler);
	st->handled_handle = NULL;
}

static void
iv_handle_stop_poll_thread(struct iv_state *st, struct iv_handle_ *h)
{
	DWORD ret;

	SetEvent(h->signal_handle);

	do {
		ret = WaitForSingleObjectEx(st->thread_stop, INFINITE, TRUE);
	} while (ret == WAIT_IO_COMPLETION);

	if (ret != WAIT_OBJECT_0) {
		iv_fatal("iv_handle_stop_poll_thread: "
			 "WaitForSingleObjectEx fail");
	}
}

void iv_handle_deinit(struct iv_state *st)
{
	while (!iv_list_empty(&st->handles)) {
		struct iv_handle_ *h;

		h = iv_list_entry(st->handles.next, struct iv_handle_, list);
		iv_list_del_init(&h->list);

		iv_handle_stop_poll_thread(st, h);
	}

	CloseHandle(st->wait);
	CloseHandle(st->thread_stop);
	DeleteCriticalSection(&st->active_handle_list_lock);
}

void iv_handle_poll_and_run(struct iv_state *st, const struct timespec *abs)
{
	struct iv_list_head handles;

	__iv_invalidate_now(st);

	EnterCriticalSection(&st->active_handle_list_lock);
	if (iv_list_empty(&st->active_with_handler)) {
		DWORD ret;

		LeaveCriticalSection(&st->active_handle_list_lock);
		ret = WaitForSingleObjectEx(st->wait, to_msec(st, abs), TRUE);
		EnterCriticalSection(&st->active_handle_list_lock);

		if (ret != WAIT_OBJECT_0 && ret != WAIT_IO_COMPLETION &&
		    ret != WAIT_TIMEOUT) {
			iv_fatal("iv_handle_poll_and_run: "
				 "WaitForSingleObjectEx returned %x",
				 (int)ret);
		}
	}
	__iv_list_steal_elements(&st->active_with_handler, &handles);
	LeaveCriticalSection(&st->active_handle_list_lock);

	while (!iv_list_empty(&handles)) {
		struct iv_handle_ *h;

		h = iv_list_entry(handles.next, struct iv_handle_, list_active);
		iv_list_del_init(&h->list_active);

		st->handled_handle = h;
		h->handler(h->cookie);
		if (st->handled_handle == h) {
			SetEvent(h->signal_handle);
			st->handled_handle = NULL;
		}
	}
}

const char *iv_poll_method_name(void)
{
	return "simple";
}

void IV_HANDLE_INIT(struct iv_handle *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;

	INIT_IV_LIST_HEAD(&h->list);
}

static DWORD WINAPI iv_handle_poll_thread(void *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;
	struct iv_state *st = h->st;
	HANDLE hnd[2];
	int sig;

	hnd[0] = h->signal_handle;
	hnd[1] = h->handle;

	sig = 0;

	EnterCriticalSection(&st->active_handle_list_lock);
	while (!iv_list_empty(&h->list)) {
		DWORD num;
		DWORD ret;
		struct iv_list_head *list;

		num = iv_list_empty(&h->list_active) ? 2 : 1;

		LeaveCriticalSection(&st->active_handle_list_lock);
		if (sig) {
			sig = 0;
			SetEvent(st->wait);
		}
		ret = WaitForMultipleObjectsEx(num, hnd, FALSE,
					       INFINITE, FALSE);
		EnterCriticalSection(&st->active_handle_list_lock);

		if (ret == WAIT_OBJECT_0)
			continue;

		if (ret != WAIT_OBJECT_0 + 1 && ret != WAIT_ABANDONED_0 + 1) {
			iv_fatal("iv_handle_poll_thread(%d): %x",
				 (int)(ULONG_PTR)h->handle, (int)ret);
		}

		if (h->handler != NULL) {
			list = &st->active_with_handler;
			if (iv_list_empty(list))
				sig = 1;
		} else {
			list = &st->active_without_handler;
		}
		iv_list_add_tail(&h->list_active, list);
	}
	LeaveCriticalSection(&st->active_handle_list_lock);

	SetEvent(st->thread_stop);

	return 0;
}

void iv_handle_register(struct iv_handle *_h)
{
	struct iv_state *st = iv_get_state();
	struct iv_handle_ *h = (struct iv_handle_ *)_h;
	HANDLE t;

	if (!iv_list_empty(&h->list)) {
		iv_fatal("iv_handle_register: called with handle "
			 "which is still registered");
	}
	iv_list_add_tail(&h->list, &st->handles);

	INIT_IV_LIST_HEAD(&h->list_active);
	h->st = st;

	h->signal_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (h->signal_handle == NULL)
		iv_fatal("iv_handle_register: CreateEvent failed");

	t = CreateThread(NULL, 0, iv_handle_poll_thread, (void *)h, 0, NULL);
	if (t == NULL)
		iv_fatal("iv_handle_register: CreateThread failed");
	CloseHandle(t);

	st->numobjs++;
}

void iv_handle_unregister(struct iv_handle *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;
	struct iv_state *st = h->st;

	if (iv_list_empty(&h->list)) {
		iv_fatal("iv_handle_unregister: called with handle "
			 "which is not registered");
	}
	iv_list_del_init(&h->list);

	iv_handle_stop_poll_thread(st, h);

	if (!iv_list_empty(&h->list_active)) {
		EnterCriticalSection(&st->active_handle_list_lock);
		iv_list_del_init(&h->list_active);
		LeaveCriticalSection(&st->active_handle_list_lock);
	}
	h->st = NULL;
	CloseHandle(h->signal_handle);

	st->numobjs--;
	if (st->handled_handle == h)
		st->handled_handle = NULL;
}

int iv_handle_registered(const struct iv_handle *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;

	return !iv_list_empty(&h->list);
}

static void
iv_handle_move_to_list(struct iv_handle_ *h, struct iv_list_head *list)
{
	iv_list_del(&h->list_active);
	iv_list_add_tail(&h->list_active, list);
}

void iv_handle_set_handler(struct iv_handle *_h, void (*handler)(void *))
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;
	struct iv_state *st = h->st;

	if (iv_list_empty(&h->list)) {
		iv_fatal("iv_handle_set_handler: called with handle "
			 "which is not registered");
	}

	EnterCriticalSection(&st->active_handle_list_lock);

	if (!iv_list_empty(&h->list_active)) {
		if (h->handler == NULL && handler != NULL)
			iv_handle_move_to_list(h, &st->active_with_handler);
		else if (h->handler != NULL && handler == NULL)
			iv_handle_move_to_list(h, &st->active_without_handler);
	}

	h->handler = handler;

	LeaveCriticalSection(&st->active_handle_list_lock);
}
