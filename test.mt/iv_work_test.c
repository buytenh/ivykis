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
#include <iv_thread.h>
#include <iv_work.h>

static struct iv_work_pool pool;
static struct iv_work_item item_a;
static struct iv_work_item item_b;
static struct iv_work_item item_c;
static struct iv_work_item item_d;
static int item_count;

static void work(void *cookie)
{
	char *task = cookie;

	printf("performing work item %s in thread %lu\n",
	       task, iv_get_thread_id());

#ifndef _WIN32
	sleep(1);
#else
	Sleep(1000);
#endif
}

static void work_complete(void *cookie)
{
	char *task = cookie;

	printf("notification that work item %s is complete\n", task);

	item_count--;
	if (item_count == 3) {
		printf("putting pool\n");
		iv_work_pool_put(&pool);
	}
}

int main()
{
	iv_init();

	iv_thread_set_debug_state(1);

	IV_WORK_POOL_INIT(&pool);
	pool.max_threads = 8;
	iv_work_pool_create(&pool);

	IV_WORK_ITEM_INIT(&item_a);
	item_a.cookie = "a";
	item_a.work = work;
	item_a.completion = work_complete;
	iv_work_pool_submit_work(&pool, &item_a);

	IV_WORK_ITEM_INIT(&item_b);
	item_b.cookie = "b";
	item_b.work = work;
	item_b.completion = work_complete;
	iv_work_pool_submit_work(&pool, &item_b);

	IV_WORK_ITEM_INIT(&item_c);
	item_c.cookie = "c";
	item_c.work = work;
	item_c.completion = work_complete;
	iv_work_pool_submit_work(&pool, &item_c);

	IV_WORK_ITEM_INIT(&item_d);
	item_d.cookie = "d";
	item_d.work = work;
	item_d.completion = work_complete;
	iv_work_pool_submit_work(&pool, &item_d);

	item_count = 4;

	iv_main();

	iv_deinit();

	return 0;
}
