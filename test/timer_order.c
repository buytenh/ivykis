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

#ifndef __hppa__
#define NUM		1048570
#else
#define NUM		32768
#endif

static struct iv_timer	tim[NUM];
static int expect;

static void handler(void *_t)
{
	struct iv_timer *t = (struct iv_timer *)_t;
	int index;

	index = (int)(t - tim);
	if (expect != index) {
		fprintf(stderr, "expecting %d, got %d\n", expect, index);
		exit(1);
	}

	expect++;
}

int main()
{
	int i;

	alarm(120);

	iv_init();

	iv_validate_now();

	for (i = 0; i < NUM; i++) {
		IV_TIMER_INIT(tim + i);
		tim[i].expires = iv_now;
		tim[i].expires.tv_nsec += i + 100000000;
		if (tim[i].expires.tv_nsec >= 1000000000) {
			tim[i].expires.tv_sec++;
			tim[i].expires.tv_nsec -= 1000000000;
		}
		tim[i].cookie = (void *)&tim[i];
		tim[i].handler = handler;
		iv_timer_register(&tim[i]);
	}

	iv_main();

	iv_deinit();

	if (expect != NUM) {
		fprintf(stderr, "only ran %d timer handlers (vs %d)\n",
			expect, NUM);
		return 1;
	}

	return 0;
}
