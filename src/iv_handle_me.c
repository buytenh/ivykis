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

/* handle group tls handling ************************************************/
static DWORD iv_handle_group_index = -1;

static inline struct iv_handle_group *iv_handle_get_group(void)
{
	return TlsGetValue(iv_handle_group_index);
}


/* poll handling ************************************************************/
static void __iv_handle_poll_state_update(struct iv_handle_group *grp)
{
	int i;
	int j;

	for (i = grp->num_handles; i < grp->addition_pointer; i++)
		grp->hnd[i] = grp->h[i]->handle;

	for (i = 0, j = 0; i < grp->addition_pointer; i++) {
		if (grp->h[i] != NULL) {
			if (j != i) {
				grp->h[j] = grp->h[i];
				grp->h[j]->index = j;
				grp->hnd[j] = grp->hnd[i];
			}
			j++;
		}
	}

	grp->addition_pointer = j;
	grp->num_deletions = 0;

	grp->quit = grp->st->quit;
	grp->num_handles = j;
	grp->hnd[j] = grp->thr_signal_handle;
	grp->have_active_handles = !iv_list_empty(&grp->active_handles);
}

static int
__iv_handle_ready(struct iv_handle_group *grp, int off, DWORD ret)
{
	int index;

	if (ret >= WAIT_ABANDONED_0)
		index = off + (ret - WAIT_ABANDONED_0);
	else
		index = off + (ret - WAIT_OBJECT_0);

	if (index < grp->num_handles) {
		struct iv_handle_ *h;

		h = grp->h[index];
		if (h != NULL) {
			if (iv_list_empty(&h->list_active)) {
				iv_list_add_tail(&h->list_active,
						 &grp->active_handles);
			}
			grp->have_active_handles = 1;
		}
	} else {
		__iv_handle_poll_state_update(grp);
	}

	return index + 1;
}

static void iv_handle_poll_handles(struct iv_handle_group *grp, DWORD msec)
{
	int num_handles;
	HANDLE *hnd;
	int off;
	DWORD ret;

	num_handles = grp->num_handles + 1;
	hnd = grp->hnd;

	if (!grp->quit) {
		if (grp->have_active_handles)
			msec = 0;
		off = 0;
	} else {
		off = grp->num_handles;
	}

	ret = WaitForMultipleObjectsEx(num_handles - off,
				       hnd + off, FALSE, msec, TRUE);
	if (ret == WAIT_FAILED)
		iv_fatal("iv_handle_poll_handles: wait failed");

	if (ret == WAIT_IO_COMPLETION || ret == WAIT_TIMEOUT)
		return;

	EnterCriticalSection(&grp->group_lock);

	off = __iv_handle_ready(grp, off, ret);
	while (off < num_handles) {
		ret = WaitForMultipleObjectsEx(num_handles - off,
					       hnd + off, FALSE, 0, FALSE);
		if (ret == WAIT_FAILED)
			iv_fatal("iv_handle_poll_handles: wait failed");

		if (ret == WAIT_TIMEOUT)
			break;

		off = __iv_handle_ready(grp, off, ret);
	}

	LeaveCriticalSection(&grp->group_lock);
}

static void __iv_handle_run_active_list(struct iv_handle_group *grp)
{
	struct iv_list_head active;

	__iv_list_steal_elements(&grp->active_handles, &active);
	grp->have_active_handles = 0;

	while (!iv_list_empty(&active)) {
		struct iv_handle_ *h;

		h = iv_container_of(active.next, struct iv_handle_,
				    list_active);
		iv_list_del_init(&h->list_active);

		h->handler(h->cookie);
	}
}


/* poll thread **************************************************************/
static int timespec_lt(struct timespec *a, struct timespec *b)
{
	if (a->tv_sec < b->tv_sec)
		return 1;
	if (a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec)
		return 1;

	return 0;
}

static int should_wake_zero(struct iv_state *st, struct iv_handle_group *grp,
			    int ts_expired, struct timespec *ts)
{
	if (!st->numobjs)
		return 1;

	if (!ts_expired) {
		int ts2_expired;
		struct timespec ts2;

		ts2_expired = iv_get_soonest_timeout(st, &ts2);
		if (ts2_expired || timespec_lt(&ts2, ts))
			return 1;
	}

	// @@@
	if (iv_pending_tasks(st))
		return 1;

	return 0;
}

static DWORD WINAPI iv_handle_poll_thread(void *_grp)
{
	struct iv_handle_group *grp = _grp;
	struct iv_state *st = grp->st;

	TlsSetValue(iv_state_index, grp->st);
	TlsSetValue(iv_handle_group_index, grp);

	while (grp->num_handles) {
		HANDLE h[2];
		DWORD ret;

		iv_handle_poll_handles(grp, INFINITE);

		if (grp->quit || !grp->have_active_handles)
			continue;

		h[0] = st->execution_mutex;
		h[1] = grp->thr_signal_handle;

		do {
			ret = WaitForMultipleObjectsEx(2, h, FALSE,
						       INFINITE, TRUE);
		} while (ret == WAIT_IO_COMPLETION);

		if (ret == WAIT_OBJECT_0 + 1) {
			EnterCriticalSection(&grp->group_lock);
			__iv_handle_poll_state_update(grp);
			LeaveCriticalSection(&grp->group_lock);
			continue;
		}

		if (ret != WAIT_OBJECT_0) {
			iv_fatal("iv_handle_poll_thread: got "
				 "error %x", (int)ret);
		}

		// @@@ condition?
		if (!grp->quit) {
			int ts_expired;
			struct timespec ts;

			ts_expired = iv_get_soonest_timeout(st, &ts);

			// @@@ determine who runs what!
			__iv_invalidate_now(grp->st);
			iv_run_tasks(st);
			iv_run_timers(st);
			__iv_handle_run_active_list(grp);

			if (should_wake_zero(st, grp, ts_expired, &ts)) {
				struct iv_handle_group *grp;

				grp = &st->group_0;
				SetEvent(grp->thr_signal_handle);
			}
		}

		ReleaseMutex(st->execution_mutex);
	}

	CloseHandle(grp->thr_signal_handle);
	DeleteCriticalSection(&grp->group_lock);
	free(grp);

	TlsSetValue(iv_state_index, NULL);

	return 0;
}


/* internal use *************************************************************/
void iv_handle_init(struct iv_state *st)
{
	struct iv_handle_group *me;

	if (iv_handle_group_index == -1) {
		iv_handle_group_index = TlsAlloc();
		if (iv_handle_group_index == TLS_OUT_OF_INDEXES) {
			iv_fatal("iv_handle_init: failed to allocate TLS key");
		}
	}


	st->execution_mutex = CreateMutex(NULL, TRUE, NULL);
	INIT_IV_LIST_HEAD(&st->active_handles_no_handler);
	INIT_IV_LIST_HEAD(&st->groups);
	INIT_IV_LIST_HEAD(&st->groups_recent_deleted);

	me = &st->group_0;
	TlsSetValue(iv_handle_group_index, me);

	iv_list_add_tail(&me->list_all, &st->groups);
	iv_list_add(&me->list_recent_deleted,
		    &st->groups_recent_deleted);
	me->st = st;
	me->thr_handle = INVALID_HANDLE_VALUE;
	me->thr_signal_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
	InitializeCriticalSection(&me->group_lock);
	INIT_IV_LIST_HEAD(&me->active_handles);
	me->addition_pointer = 0;
	me->num_deletions = 0;
	me->num_handles = 0;
	me->hnd[0] = me->thr_signal_handle;
	me->have_active_handles = 0;
}

void iv_handle_deinit(struct iv_state *st)
{
	struct iv_handle_group *me = iv_handle_get_group();

	if (me != &st->group_0)
		iv_fatal("iv_handle_deinit: not called in subthread zero");

	iv_list_del(&me->list_all);

	while (!iv_list_empty(&st->groups)) {
		struct iv_list_head *lh;
		struct iv_handle_group *grp;
		int i;
		HANDLE signal;
		HANDLE wait;
		DWORD ret;

		lh = st->groups.next;
		grp = iv_container_of(lh, struct iv_handle_group, list_all);

		EnterCriticalSection(&grp->group_lock);

		iv_list_del(&grp->list_all);
		iv_list_del(&grp->list_recent_deleted);

		for (i = 0; i < grp->addition_pointer; i++) {
			if (grp->h[i] != NULL) {
				iv_list_del_init(&grp->h[i]->list_active);
				grp->h[i] = NULL;
			}
		}

		grp->addition_pointer = grp->num_handles;
		grp->num_deletions = grp->addition_pointer;

		signal = grp->thr_signal_handle;
		wait = grp->thr_handle;

		LeaveCriticalSection(&grp->group_lock);

		ret = SignalObjectAndWait(signal, wait, INFINITE, TRUE);
		while (ret == WAIT_IO_COMPLETION)
			ret = WaitForSingleObjectEx(wait, INFINITE, TRUE);

		if (ret == WAIT_FAILED)
			iv_fatal("iv_handle_deinit: wait failed");

		CloseHandle(wait);
	}

	CloseHandle(st->execution_mutex);

	CloseHandle(me->thr_signal_handle);
	DeleteCriticalSection(&me->group_lock);
}

void iv_handle_poll_and_run(struct iv_state *st, struct timespec *to)
{
	struct iv_handle_group *me = &st->group_0;
	int msec;
	DWORD ret;

	ReleaseMutex(st->execution_mutex);

	msec = 1000 * to->tv_sec + (to->tv_nsec + 999999) / 1000000;
	iv_handle_poll_handles(me, msec);

	do {
		ret = WaitForSingleObjectEx(st->execution_mutex,
					    INFINITE, TRUE);
	} while (ret == WAIT_IO_COMPLETION);

	if (ret == WAIT_FAILED)
		iv_fatal("iv_handle_poll_and_run: wait failed");

	__iv_invalidate_now(st);
	__iv_handle_run_active_list(me);
}

static void iv_handle_bump_other_threads(struct iv_state *st)
{
	struct iv_handle_group *me = iv_handle_get_group();
	struct iv_list_head *lh;

	iv_list_for_each (lh, &st->groups) {
		struct iv_handle_group *grp;

		grp = iv_container_of(lh, struct iv_handle_group, list_all);
		if (me != grp)
			SetEvent(grp->thr_signal_handle);
	}
}

void iv_handle_quit(struct iv_state *st)
{
	iv_handle_bump_other_threads(st);
}

void iv_handle_unquit(struct iv_state *st)
{
	iv_handle_bump_other_threads(st);
}

void iv_handle_process_detach(void)
{
	TlsFree(iv_handle_group_index);
	iv_handle_group_index = -1;
}


/* public use ***************************************************************/
void IV_HANDLE_INIT(struct iv_handle *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;

	h->registered = 0;
	h->grp = NULL;
	h->index = -1;
	INIT_IV_LIST_HEAD(&h->list_active);
}

static struct iv_handle_group *iv_handle_choose_group(struct iv_state *st)
{
	struct iv_handle_group *grp;

	grp = iv_handle_get_group();
	if (grp->addition_pointer < MAX_THREAD_HANDLES)
		return grp;

	if (grp != &st->group_0) {
		grp = &st->group_0;
		if (grp->addition_pointer < MAX_THREAD_HANDLES)
			return grp;
	}

	if (!iv_list_empty(&st->groups_recent_deleted)) {
		grp = iv_container_of(st->groups_recent_deleted.next,
				      struct iv_handle_group,
				      list_recent_deleted);

		return grp;
	}

	/*
	 * @@@
	 * ORRRRRRR
	 * - add to most recently non-full having executed thread !?!?!??!?!!?!
	 *   - this spreads the handles unfairly, busy threads get busier
	 *   - if load okay, has the danger of never filling any thread up
	 */

	return NULL;
}

static void
__iv_handle_new_poll_thread(struct iv_state *st, struct iv_handle_ *firsth)
{
	struct iv_handle_group *grp;
	HANDLE thr_handle;

	grp = malloc(sizeof(*grp));
	if (grp == NULL)
		iv_fatal("__iv_handle_new_poll_thread: out of memory");

	iv_list_add_tail(&grp->list_all, &st->groups);
	iv_list_add(&grp->list_recent_deleted,
		    &st->groups_recent_deleted);

	grp->st = st;

	grp->thr_handle = INVALID_HANDLE_VALUE;
	grp->thr_signal_handle = CreateEvent(NULL, FALSE, FALSE, NULL);

	InitializeCriticalSection(&grp->group_lock);
	INIT_IV_LIST_HEAD(&grp->active_handles);
	grp->h[0] = firsth;
	grp->addition_pointer = 1;
	grp->num_deletions = 0;
	grp->num_handles = 1;
	grp->hnd[0] = firsth->handle;
	grp->hnd[1] = grp->thr_signal_handle;
	grp->have_active_handles = !iv_list_empty(&firsth->list_active);

	firsth->grp = grp;
	firsth->index = 0;
	if (!iv_list_empty(&firsth->list_active)) {
		iv_list_del(&firsth->list_active);
		iv_list_add_tail(&firsth->list_active, &grp->active_handles);
	}

	thr_handle = CreateThread(NULL, 0, iv_handle_poll_thread,
				  (void *)grp, 0, NULL);
	if (thr_handle == NULL)
		iv_fatal("__iv_handle_new_poll_thread: CreateThread failed");

	grp->thr_handle = thr_handle;
}

static void __iv_handle_register(struct iv_state *st, struct iv_handle_ *h)
{
	struct iv_handle_group *grp;

	grp = iv_handle_choose_group(st);
	if (grp == NULL) {
		__iv_handle_new_poll_thread(st, h);
		return;
	}

	EnterCriticalSection(&grp->group_lock);

	if (grp->addition_pointer == MAX_THREAD_HANDLES)
		iv_fatal("__iv_handle_register: group full");

	// @@@ optimise for registering to self!!!

	h->grp = grp;
	h->index = grp->addition_pointer;

	if (!iv_list_empty(&h->list_active)) {
		iv_list_del(&h->list_active);
		iv_list_add_tail(&h->list_active, &grp->active_handles);
	}

	grp->h[grp->addition_pointer] = h;
	grp->addition_pointer++;
	if (grp->addition_pointer == MAX_THREAD_HANDLES)
		iv_list_del(&grp->list_recent_deleted);

	LeaveCriticalSection(&grp->group_lock);

	SetEvent(grp->thr_signal_handle);
}

void iv_handle_register(struct iv_handle *_h)
{
	struct iv_state *st = iv_get_state();
	struct iv_handle_ *h = (struct iv_handle_ *)_h;

	if (h->registered) {
		iv_fatal("iv_handle_register: called with handle "
			 "which is already registered");
	}
	h->registered = 1;

	if (h->handler != NULL)
		__iv_handle_register(st, h);

	st->numobjs++;
}

static void __iv_handle_unregister(struct iv_handle_ *h)
{
	struct iv_handle_group *me = iv_handle_get_group();
	struct iv_handle_group *grp = h->grp;
	struct iv_state *st = grp->st;

	EnterCriticalSection(&grp->group_lock);

	if (!iv_list_empty(&h->list_active)) {
		iv_list_del(&h->list_active);
		iv_list_add_tail(&h->list_active,
				 &st->active_handles_no_handler);
	}

	if (grp->addition_pointer < MAX_THREAD_HANDLES)
		iv_list_del(&grp->list_recent_deleted);
	iv_list_add(&grp->list_recent_deleted, &st->groups_recent_deleted);

	if (me == grp) {
		if (grp->addition_pointer != grp->num_handles ||
		    grp->num_deletions)
			ResetEvent(grp->thr_signal_handle);

		grp->h[h->index] = NULL;
		h->grp = NULL;
		h->index = -1;

		__iv_handle_poll_state_update(grp);
		if (grp->num_handles == 0 && grp != &st->group_0) {
			iv_list_del(&grp->list_all);
			iv_list_del(&grp->list_recent_deleted);
			CloseHandle(grp->thr_handle);
		}

		LeaveCriticalSection(&grp->group_lock);

		return;
	}

	if (h->index >= grp->addition_pointer) {
		int i;

		grp->addition_pointer--;
		for (i = h->index; i < grp->addition_pointer; i++)
			grp->h[i] = grp->h[i + 1];
	} else {
		grp->h[h->index] = NULL;
		grp->num_deletions++;
	}
	h->grp = NULL;
	h->index = -1;

	if (grp->addition_pointer == grp->num_deletions) {
		HANDLE signal;
		HANDLE wait;
		DWORD ret;

		iv_list_del(&grp->list_all);
		iv_list_del(&grp->list_recent_deleted);

		signal = grp->thr_signal_handle;
		wait = grp->thr_handle;

		LeaveCriticalSection(&grp->group_lock);

		ret = SignalObjectAndWait(signal, wait, INFINITE, TRUE);
		while (ret == WAIT_IO_COMPLETION)
			ret = WaitForSingleObjectEx(wait, INFINITE, TRUE);

		if (ret == WAIT_FAILED)
			iv_fatal("__iv_handle_unregister: wait failed");

		CloseHandle(wait);
	}

	LeaveCriticalSection(&grp->group_lock);
}

void iv_handle_unregister(struct iv_handle *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;
	struct iv_handle_group *grp = h->grp;
	struct iv_state *st = grp->st;

	if (!h->registered) {
		iv_fatal("iv_handle_unregister: called with handle "
			 "which is not registered");
	}
	h->registered = 0;

	if (h->handler != NULL)
		__iv_handle_unregister(h);

	if (!iv_list_empty(&h->list_active))
		iv_list_del(&h->list_active);

	st->numobjs--;
}

int iv_handle_registered(struct iv_handle *_h)
{
	struct iv_handle_ *h = (struct iv_handle_ *)_h;

	return h->registered;
}

void iv_handle_set_handler(struct iv_handle *_h, void (*handler)(void *))
{
	struct iv_state *st = iv_get_state();
	struct iv_handle_ *h = (struct iv_handle_ *)_h;

	if (!h->registered) {
		iv_fatal("iv_handle_set_handler: called with handle "
			 "which is not registered");
	}

	if (h->handler == NULL && handler != NULL)
		__iv_handle_register(st, h);
	else if (h->handler != NULL && handler == NULL)
		__iv_handle_unregister(h);

	h->handler = handler;
}
