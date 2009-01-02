/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>

struct iv_timer tim;

void handler(void *_t)
{
	struct iv_timer *t = (struct iv_timer *)_t;

	printf("hoihoihoihoihoi!\n");

	iv_validate_now();
	t->expires = now;
	t->expires.tv_sec += 1;
	iv_register_timer(t);
}

int main()
{
	iv_init();

	INIT_IV_TIMER(&tim);
	iv_validate_now();
	tim.expires = now;
	tim.expires.tv_sec += 1;
	tim.cookie = (void *)&tim;
	tim.handler = handler;
	iv_register_timer(&tim);

	iv_main();

	return 0;
}
