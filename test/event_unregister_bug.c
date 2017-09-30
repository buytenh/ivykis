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
#include <string.h>
#include <iv.h>
#include <iv_event.h>

static struct iv_event event1;
static struct iv_event event2;

static void handler(void *_t)
{
	iv_event_unregister(&event1);
	iv_event_unregister(&event2);
}

int main()
{
	iv_init();

	IV_EVENT_INIT(&event1);
	event1.handler = handler;
	iv_event_register(&event1);
	iv_event_post(&event1);

	IV_EVENT_INIT(&event2);
	event2.handler = handler;
	iv_event_register(&event2);
	iv_event_post(&event2);

	iv_main();

	iv_deinit();

	return 0;
}
