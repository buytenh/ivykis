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
#include <iv.h>
#include <iv_event.h>
#include <iv_thread.h>
#ifdef USE_SIGNAL
#include <signal.h>
#endif

static int die;
static int ev_received;
static struct iv_event ev_parent;
static struct iv_event ev_child;
static struct timespec tim_start;
static struct timespec tim_end;

#ifdef USE_SIGNAL
static void got_signal_timeout(int sigh)
{
	die = 1;
}
#else
static struct iv_timer timeout;

static void got_timer_timeout(void *_dummy)
{
	die = 1;
}
#endif

static void got_ev_parent(void *_dummy)
{
	ev_received++;

	if (die == 0) {
		iv_event_post(&ev_child);
	} else if (die == 1) {
		die = 2;
		iv_event_post(&ev_child);
	} else if (die == 2) {
		iv_fatal("iv_event_bench: entered invalid state");
	} else if (die == 3) {
		iv_validate_now();
		tim_end = iv_now;
		iv_event_unregister(&ev_parent);
	}
}

static void got_ev_child(void *_dummy)
{
	if (die == 2) {
		die = 3;
		iv_event_post(&ev_parent);
		iv_event_unregister(&ev_child);
	} else {
		iv_event_post(&ev_parent);
	}
}

static void thr_child(void *_dummy)
{
	iv_init();

	IV_EVENT_INIT(&ev_child);
	ev_child.handler = got_ev_child;
	iv_event_register(&ev_child);

	iv_validate_now();
	tim_start = iv_now;

	iv_event_post(&ev_parent);

	iv_main();

	iv_deinit();
}

int main()
{
	long long nsec;

	iv_init();

#ifdef USE_SIGNAL
	signal(SIGALRM, got_signal_timeout);
	alarm(5);
#else
	IV_TIMER_INIT(&timeout);
	iv_validate_now();
	timeout.expires = iv_now;
	timeout.expires.tv_sec += 5;
	timeout.handler = got_timer_timeout;
	iv_timer_register(&timeout);
#endif

	IV_EVENT_INIT(&ev_parent);
	ev_parent.handler = got_ev_parent;
	iv_event_register(&ev_parent);

	iv_thread_create("child", thr_child, NULL);

	iv_main();

	iv_deinit();

	nsec = 1000000000ULL * (tim_end.tv_sec - tim_start.tv_sec) +
		(tim_end.tv_nsec - tim_start.tv_nsec);

	printf("%s: %d in %ld nsec => %d/sec\n",
	       iv_poll_method_name(), ev_received, (long)nsec,
	       (int)(1000000000ULL * ev_received / nsec));

	return 0;
}
