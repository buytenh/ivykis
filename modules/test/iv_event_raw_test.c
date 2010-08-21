/*
 * ivykis, an event handling library
 * Copyright (C) 2010 Lennert Buytenhek
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

static struct iv_event_raw ev0;
static struct iv_timer ev1;
static struct iv_event_raw ev2;

static void gotev2(void *_x)
{
	printf("got ev2\n");

	iv_event_raw_unregister(&ev2);
}

static void gotev1(void *_x)
{
	printf("got ev1\n");

	ev2.handler = gotev2;
	iv_event_raw_register(&ev2);

	iv_event_raw_post(&ev2);
}

static void gotev0(void *_x)
{
	printf("got ev0\n");

	iv_event_raw_unregister(&ev0);

	INIT_IV_TIMER(&ev1);
	iv_validate_now();
	ev1.expires = now;
	ev1.expires.tv_sec++;
	ev1.handler = gotev1;
	iv_register_timer(&ev1);
}

int main()
{
	iv_init();

	ev0.handler = gotev0;
	iv_event_raw_register(&ev0);

	iv_event_raw_post(&ev0);

	iv_main();

	return 0;
}
