/*
 * ivykis, an event handling library
 * Copyright (C) 2010, 2013 Lennert Buytenhek
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

static void dummy(void *cookie)
{
}

static void delay(int msec)
{
	struct iv_timer tim;

	iv_init();

	IV_TIMER_INIT(&tim);
	iv_validate_now();
	tim.expires = iv_now;
	tim.expires.tv_sec += msec / 1000;
	tim.expires.tv_nsec += 1000000 * (msec % 1000);
	tim.handler = dummy;
	if (tim.expires.tv_nsec >= 1000000000) {
		tim.expires.tv_sec++;
		tim.expires.tv_nsec -= 1000000000;
	}
	iv_timer_register(&tim);

	iv_main();
}

static void thr_grandchild(void *cookie)
{
	delay(100);

	iv_thread_list_children();
}

static void thr_child(void *cookie)
{
	iv_init();

	iv_thread_create("grandchild1", thr_grandchild, NULL);
	iv_thread_create("grandchild2", thr_grandchild, NULL);
	iv_thread_create("grandchild3", thr_grandchild, NULL);

	iv_thread_list_children();

	iv_quit();
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

static void thr_testing(void *cookie)
{
	delay(100);

	iv_thread_list_children();
}

#ifndef _WIN32
static void *thr_noparent(void *cookie)
#else
static DWORD WINAPI thr_noparent(void *_thr)
#endif
{
	iv_thread_set_name("noparent");

	iv_init();

	iv_thread_create("testing", thr_testing, NULL);

	iv_thread_list_children();

	iv_main();

#ifndef _WIN32
	return NULL;
#else
	return 0;
#endif
}

int main()
{
#ifndef _WIN32
	pthread_t thr;
#endif

	iv_thread_set_name("main");

	iv_init();

	iv_thread_set_debug_state(1);

	iv_thread_create("return", thr_return, NULL);
	iv_thread_create("child", thr_child, NULL);
#ifndef _WIN32
	iv_thread_create("selfcancel", thr_selfcancel, NULL);
	iv_thread_create("exit", thr_exit, NULL);
	pthread_create(&thr, NULL, thr_noparent, NULL);
#else
	CreateThread(NULL, 0, thr_noparent, NULL, 0, NULL);
#endif

	iv_thread_list_children();

	iv_main();

	iv_deinit();

	return 0;
}
