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
#include <iv_event_raw.h>
#ifdef USE_SIGNAL
#include <signal.h>
#endif

static int die;
static int ev_received;
static struct iv_event_raw ev;
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

static void got_ev(void *_dummy)
{
	ev_received++;

	if (!die) {
		iv_event_raw_post(&ev);
	} else {
		iv_validate_now();
		tim_end = iv_now;
		iv_event_raw_unregister(&ev);
	}
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

	IV_EVENT_RAW_INIT(&ev);
	ev.handler = got_ev;
	iv_event_raw_register(&ev);

	iv_validate_now();
	tim_start = iv_now;

	iv_event_raw_post(&ev);

	iv_main();

	iv_deinit();

	nsec = 1000000000ULL * (tim_end.tv_sec - tim_start.tv_sec) +
		(tim_end.tv_nsec - tim_start.tv_nsec);

	printf("%s: %d in %ld nsec => %d/sec\n",
	       iv_poll_method_name(), ev_received, (long)nsec,
	       (int)(1000000000ULL * ev_received / nsec));

	return 0;
}
