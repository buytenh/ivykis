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
#include <pthread.h>
#include <unistd.h>

static struct iv_event ev0;
static struct iv_timer tim0;
static struct iv_event ev1;
static struct iv_timer tim1;

static void got_ev0(void *_dummy)
{
	printf("%p: got ev0, starting tim0\n", (void *)pthread_self());

	iv_validate_now();
	tim0.expires = now;
	tim0.expires.tv_sec++;
	iv_register_timer(&tim0);
}

static void got_tim0(void *_dummy)
{
	printf("%p: tim0 expired, signaling ev1\n", (void *)pthread_self());

	iv_event_post(&ev1);
}

static void got_ev1(void *_dummy)
{
	printf("%p: got ev1, starting tim1\n", (void *)pthread_self());

	iv_validate_now();
	tim1.expires = now;
	tim1.expires.tv_sec++;
	iv_register_timer(&tim1);
}

static void got_tim1(void *_dummy)
{
	printf("%p: tim1 expired, signaling ev0\n", (void *)pthread_self());

	iv_event_post(&ev0);
}

static void *thread1(void *_dummy)
{
	iv_init();

	ev1.handler = got_ev1;
	iv_event_register(&ev1);

	INIT_IV_TIMER(&tim1);
	tim1.handler = got_tim1;

	iv_main();

	return NULL;
}

int main()
{
	pthread_t foo;

	iv_init();

	ev0.handler = got_ev0;
	iv_event_register(&ev0);

	INIT_IV_TIMER(&tim0);
	tim0.handler = got_tim0;

	pthread_create(&foo, NULL, thread1, NULL);

	iv_validate_now();
	tim0.expires = now;
	tim0.expires.tv_sec++;
	iv_register_timer(&tim0);

	iv_main();

	iv_deinit();

	return 0;
}
