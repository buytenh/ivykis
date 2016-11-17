/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009, 2012 Lennert Buytenhek
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

DWORD iv_state_index = -1;

void iv_init(void)
{
	struct iv_state *st;

	if (iv_state_index == -1) {
		iv_state_index = TlsAlloc();
		if (iv_state_index == TLS_OUT_OF_INDEXES)
			iv_fatal("iv_init: failed to allocate TLS key");
	}

	st = calloc(1, iv_tls_total_state_size());
	TlsSetValue(iv_state_index, st);

	iv_handle_init(st);
	iv_task_init(st);
	iv_time_init(st);
	iv_timer_init(st);

	iv_event_init(st);

	iv_tls_thread_init(st);
}

int iv_inited(void)
{
	if (iv_state_index == -1)
		return 0;

	return iv_get_state() != NULL;
}

void iv_quit(void)
{
	struct iv_state *st = iv_get_state();

	st->quit = 1;
}

void iv_main(void)
{
	struct iv_state *st = iv_get_state();

	st->quit = 0;
	while (1) {
		struct timespec _abs;
		const struct timespec *abs;

		iv_run_timers(st);
		iv_run_tasks(st);

		if (st->quit || !st->numobjs)
			break;

		if (iv_pending_tasks(st)) {
			_abs.tv_sec = 0;
			_abs.tv_nsec = 0;
			abs = &_abs;
		} else {
			abs = iv_get_soonest_timeout(st);
		}

		iv_handle_poll_and_run(st, abs);
	}
}

static void __iv_deinit(struct iv_state *st)
{
	iv_tls_thread_deinit(st);

	iv_event_deinit(st);

	iv_handle_deinit(st);
	iv_timer_deinit(st);

	TlsSetValue(iv_state_index, NULL);

	free(st);
}

void iv_deinit(void)
{
	struct iv_state *st = iv_get_state();

	__iv_deinit(st);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (iv_state_index == -1)
		return TRUE;

	if (fdwReason == DLL_PROCESS_DETACH || fdwReason == DLL_THREAD_DETACH) {
		struct iv_state *st;

		st = iv_get_state();
		if (st != NULL)
			__iv_deinit(st);
	}

	if (fdwReason == DLL_PROCESS_DETACH) {
		TlsFree(iv_state_index);
		iv_state_index = -1;
	}

	return TRUE;
}
