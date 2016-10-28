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
#include <iv_signal.h>
#include <signal.h>

static int die;
static int sig_received;
static struct iv_signal is;
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

static void got_sig(void *_dummy)
{
	sig_received++;

	if (!die) {
		raise(SIGUSR1);
	} else {
		iv_validate_now();
		tim_end = iv_now;
		iv_signal_unregister(&is);
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

	IV_SIGNAL_INIT(&is);
	is.signum = SIGUSR1;
	is.handler = got_sig;
	iv_signal_register(&is);

	iv_validate_now();
	tim_start = iv_now;

	raise(SIGUSR1);

	iv_main();

	iv_deinit();

	nsec = 1000000000ULL * (tim_end.tv_sec - tim_start.tv_sec) +
		(tim_end.tv_nsec - tim_start.tv_nsec);

	printf("%s: %d in %ld nsec => %d/sec\n",
	       iv_poll_method_name(), sig_received, (long)nsec,
	       (int)(1000000000ULL * sig_received / nsec));

	return 0;
}
