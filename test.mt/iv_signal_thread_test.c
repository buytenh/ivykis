/*
 * ivykis, an event handling library
 * Copyright (C) 2011, 2016 Lennert Buytenhek
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
#include <iv_thread.h>
#include <signal.h>

static void got_sigusr1(void *_x)
{
	printf("got SIGUSR1 in thread %ld\n", iv_get_thread_id());
}

static void thr(void *_x)
{
	struct iv_signal is_sigusr1;

	printf("starting thread %ld\n", iv_get_thread_id());

	iv_init();

	IV_SIGNAL_INIT(&is_sigusr1);
	is_sigusr1.signum = SIGUSR1;
	is_sigusr1.flags = IV_SIGNAL_FLAG_THIS_THREAD;
	is_sigusr1.handler = got_sigusr1;
	iv_signal_register(&is_sigusr1);

	iv_main();
}

int main()
{
	iv_init();

	iv_thread_create("1", thr, NULL);
	iv_thread_create("2", thr, NULL);
	iv_thread_create("3", thr, NULL);
	iv_thread_create("4", thr, NULL);

	iv_main();

	iv_deinit();

	return 0;
}
