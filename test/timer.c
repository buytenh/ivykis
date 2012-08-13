/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version
 * 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 2.1 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License version 2.1 along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>

static int success;

static void handler(void *_t)
{
	success = 1;
}

int main()
{
	struct iv_timer timer;

	alarm(5);

	iv_init();

	IV_TIMER_INIT(&timer);
	iv_validate_now();
	timer.expires = iv_now;
	timer.expires.tv_nsec += 100000000;
	if (timer.expires.tv_nsec >= 1000000000) {
		timer.expires.tv_sec++;
		timer.expires.tv_nsec -= 1000000000;
	}
	timer.handler = handler;
	iv_timer_register(&timer);

	iv_main();

	iv_deinit();

	return !success;
}
