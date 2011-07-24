/*
 * ivykis, an event handling library
 * Copyright (C) 2011 Lennert Buytenhek
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
#include <iv_thread.h>
#include <urcu-qsbr.h>
#include "iv_urcu_call.h"

/* globals ******************************************************************/
static int all_die;


/* helper routines **********************************************************/
static void tv_add_msec(struct timespec *ts, int avg, int spread)
{
	int msec;

	msec = avg - spread;
	if (msec < 0)
		msec = 0;
	msec += random() % (avg + spread - msec);

	ts->tv_sec += msec / 1000;
	ts->tv_nsec += (msec % 1000) * 1000000UL;
	if (ts->tv_nsec >= 1000000000) {
		ts->tv_sec++;
		ts->tv_nsec -= 1000000000;
	}
}


/* wait thread **************************************************************/
struct wait_state {
	struct iv_timer timer;
	struct iv_urcu_call uc;
};

static void wait_done(void *_ws)
{
	struct wait_state *ws = _ws;

	printf("%p: waiting done\n", ws);

	if (!all_die)
		iv_timer_register(&ws->timer);
}

static void wait_start(void *_ws)
{
	struct wait_state *ws = _ws;

	printf("%p: waiting\n", ws);
	iv_urcu_call_schedule(&ws->uc);

	iv_validate_now();
	ws->timer.expires = iv_now;
	tv_add_msec(&ws->timer.expires, 500, 250);
}

static void wait_thread(void *dummy)
{
	struct wait_state ws;

	iv_init();

	IV_TIMER_INIT(&ws.timer);
	ws.timer.cookie = &ws;
	ws.timer.handler = wait_start;
	iv_validate_now();
	ws.timer.expires = iv_now;
	tv_add_msec(&ws.timer.expires, 50, 25);
	iv_timer_register(&ws.timer);

	ws.uc.cookie = &ws;
	ws.uc.handler = wait_done;

	iv_main();

	iv_deinit();
}


/* work thread **************************************************************/
static void work_do(void *_work_timer)
{
	struct iv_timer *work_timer = _work_timer;

	printf("%p: work\n", work_timer);
	sleep(2);
	printf("%p: work done\n", work_timer);

	if (!all_die) {
		iv_validate_now();
		work_timer->expires = iv_now;
		tv_add_msec(&work_timer->expires, 1000, 500);
		iv_timer_register(work_timer);
	}
}

static void work_thread(void *dummy)
{
	struct iv_timer work_timer;

	iv_init();

	IV_TIMER_INIT(&work_timer);
	work_timer.cookie = &work_timer;
	work_timer.handler = work_do;
	iv_validate_now();
	work_timer.expires = iv_now;
	tv_add_msec(&work_timer.expires, 100, 50);
	iv_timer_register(&work_timer);

	iv_main();

	iv_deinit();
}


/* main *********************************************************************/
static void die_command(void *_dummy)
{
	printf("going down!\n");
	all_die = 1;
}

int main()
{
	struct iv_timer die_timer;

	iv_init();

	iv_thread_create("wait a", wait_thread, NULL);
	iv_thread_create("wait b", wait_thread, NULL);
	iv_thread_create("work a", work_thread, NULL);
	iv_thread_create("work b", work_thread, NULL);
	iv_thread_create("work c", work_thread, NULL);
	iv_thread_create("work d", work_thread, NULL);

	IV_TIMER_INIT(&die_timer);
	die_timer.handler = die_command;
	iv_validate_now();
	die_timer.expires = iv_now;
	die_timer.expires.tv_sec += 10;
	iv_timer_register(&die_timer);

	iv_main();

	iv_deinit();

	return 0;
}
