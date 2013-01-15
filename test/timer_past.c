/*
 * ivykis, an event handling library
 * Copyright (C) 2013 Lennert Buytenhek
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
static struct iv_task task;
static struct iv_timer timer;

static void handler_task(void *_t)
{
	iv_timer_register(&timer);
}

static void handler_timer(void *_t)
{
	success = 1;
}

int main()
{
	alarm(5);

	iv_init();

	IV_TASK_INIT(&task);
	task.handler = handler_task;
	iv_task_register(&task);

	IV_TIMER_INIT(&timer);
	iv_validate_now();
	timer.expires = iv_now;
	timer.expires.tv_sec--;
	timer.handler = handler_timer;

	iv_main();

	iv_deinit();

	return !success;
}
