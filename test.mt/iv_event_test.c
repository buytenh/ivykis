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
#include <iv_event.h>
#include <iv_thread.h>
#include <unistd.h>

static struct iv_event ev0;
static struct iv_timer tim0;
static struct iv_event ev1;
static struct iv_timer tim1;

static void got_ev0(void *_dummy)
{
	printf("%lu: got ev0, starting tim0\n", iv_get_thread_id());

	iv_validate_now();
	tim0.expires = iv_now;
	tim0.expires.tv_sec++;
	iv_timer_register(&tim0);
}

static void got_tim0(void *_dummy)
{
	printf("%lu: tim0 expired, signaling ev1\n", iv_get_thread_id());

	iv_event_post(&ev1);
}

static void got_ev1(void *_dummy)
{
	printf("%lu: got ev1, starting tim1\n", iv_get_thread_id());

	iv_validate_now();
	tim1.expires = iv_now;
	tim1.expires.tv_sec++;
	iv_timer_register(&tim1);
}

static void got_tim1(void *_dummy)
{
	printf("%lu: tim1 expired, signaling ev0\n", iv_get_thread_id());

	iv_event_post(&ev0);
}

static void thread1(void *_dummy)
{
	iv_init();

	IV_EVENT_INIT(&ev1);
	ev1.handler = got_ev1;
	iv_event_register(&ev1);

	IV_TIMER_INIT(&tim1);
	tim1.handler = got_tim1;

	iv_main();
}

int main()
{
	iv_init();

	IV_EVENT_INIT(&ev0);
	ev0.handler = got_ev0;
	iv_event_register(&ev0);

	IV_TIMER_INIT(&tim0);
	tim0.handler = got_tim0;

	iv_thread_create("thread1", thread1, NULL);

	iv_validate_now();
	tim0.expires = iv_now;
	tim0.expires.tv_sec++;
	iv_timer_register(&tim0);

	iv_main();

	iv_deinit();

	return 0;
}
