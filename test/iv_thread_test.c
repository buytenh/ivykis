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
#include <iv_thread.h>
#ifndef _WIN32
#include <pthread.h>
#endif

static void thr_return(void *cookie)
{
}

#ifndef _WIN32
static void thr_selfcancel(void *cookie)
{
	pthread_cancel(pthread_self());
	pthread_testcancel();
}

static void thr_exit(void *cookie)
{
	pthread_exit(NULL);
}
#endif

int main()
{
	iv_init();

	iv_thread_set_debug_state(1);

	iv_thread_create("return", thr_return, NULL);
#ifndef _WIN32
	iv_thread_create("selfcancel", thr_selfcancel, NULL);
	iv_thread_create("exit", thr_exit, NULL);
#endif

	iv_thread_list_children();

	iv_main();

	iv_deinit();

	return 0;
}
