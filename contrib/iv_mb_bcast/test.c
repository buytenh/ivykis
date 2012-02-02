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
#include <iv_mb_bcast.h>
#include <iv_thread.h>

static void timer_expires(void *arg)
{
}

static void thr(void *arg)
{
	struct iv_timer tim;

	iv_init();

	IV_TIMER_INIT(&tim);
	iv_validate_now();
	tim.expires = iv_now;
	tim.expires.tv_sec += 3;
	tim.cookie = NULL;
	tim.handler = timer_expires;
	iv_timer_register(&tim);

	iv_main();

	iv_deinit();
}

int main()
{
	int i;

	iv_init();

	for (i = 0; i < 10; i++) {
		char name[32];

		snprintf(name, sizeof(name), "worker %d", i);
		iv_thread_create(name, thr, NULL);
	}

	sleep(1);

	iv_thread_list_children();

	sleep(1);

	iv_mb_bcast_init_thread();
	iv_mb_bcast();

	sleep(1);

	iv_main();

	iv_deinit();

	return 0;
}
