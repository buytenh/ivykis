/*
 * ivykis, an event handling library
 * Copyright (C) 2017 Lennert Buytenhek
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
#include <signal.h>

static int count;
static struct iv_timer tim;

static void got_timer_timeout(void *_dummy)
{
	count++;
	if (count < 100)
		iv_timer_register(&tim);
}

int main()
{
	alarm(5);

	iv_init();

	IV_TIMER_INIT(&tim);
	tim.expires.tv_sec = 0;
	tim.expires.tv_nsec = 0;
	tim.handler = got_timer_timeout;
	iv_timer_register(&tim);

	iv_main();

	iv_deinit();

	return 0;
}
