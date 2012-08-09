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

static struct iv_timer tim;

static void handler(void *_t)
{
	struct iv_timer *t = (struct iv_timer *)_t;

	printf("hoihoihoihoihoi!\n");

	iv_validate_now();
	t->expires = iv_now;
	t->expires.tv_sec += 1;
	iv_timer_register(t);
}

int main()
{
	iv_init();

	IV_TIMER_INIT(&tim);
	iv_validate_now();
	tim.expires = iv_now;
	tim.expires.tv_sec += 1;
	tim.cookie = (void *)&tim;
	tim.handler = handler;
	iv_timer_register(&tim);

	iv_main();

	iv_deinit();

	return 0;
}
