/*
 * ivykis, an event handling library
 * Copyright (C) 2017 Lennert Buytenhek
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

static struct iv_timer timer1;
static struct iv_timer timer2;

static void handler1(void *_t)
{
	if (iv_timer_registered(&timer2))
		iv_timer_unregister(&timer2);
}

static void handler2(void *_t)
{
	if (iv_timer_registered(&timer1))
		iv_timer_unregister(&timer1);
}

int main()
{
	alarm(5);

	iv_init();

	iv_validate_now();

	IV_TIMER_INIT(&timer1);
	timer1.expires = iv_now;
	timer1.handler = handler1;
	iv_timer_register(&timer1);

	IV_TIMER_INIT(&timer2);
	timer2.expires = iv_now;
	timer2.handler = handler2;
	iv_timer_register(&timer2);

	iv_main();

	iv_deinit();

	return 0;
}
