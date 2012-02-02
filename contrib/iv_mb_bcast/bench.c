/*
 * ivykis, an event handling library
 * Copyright (C) 2012 Lennert Buytenhek
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
#include <iv_mb_bcast.h>
#include <pthread.h>

static int wanted_mb_threads;
static int wanted_dummy_threads;

static pthread_mutex_t lock;
static int running_dummies;
static pthread_cond_t cond;

static void *iv_mb_bcast_dummy(void *dummy)
{
	struct iv_timer foo;

	pthread_mutex_lock(&lock);

	iv_init();

	IV_TIMER_INIT(&foo);
	iv_validate_now();
	foo.expires = iv_now;
	foo.expires.tv_sec += 3600;
	foo.cookie = NULL;
	foo.handler = NULL;
	iv_timer_register(&foo);

	running_dummies++;
	if (running_dummies == wanted_dummy_threads)
		pthread_cond_signal(&cond);

	pthread_mutex_unlock(&lock);

	iv_main();

	iv_deinit();

	return NULL;
}

static void *iv_mb_bcast_hammer(void *dummy)
{
	int i;

	iv_mb_bcast_init_thread();

	for (i = 0; i < 25000; i++)
		iv_mb_bcast();

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t tid[1024];
	int i;

	if (argc < 2 || sscanf(argv[1], "%d", &wanted_mb_threads) != 1)
		wanted_mb_threads = 1;
	else if (wanted_mb_threads > 1024)
		wanted_mb_threads = 1024;

	if (argc < 3 || sscanf(argv[2], "%d", &wanted_dummy_threads) != 1)
		wanted_dummy_threads = 1;
	else if (wanted_dummy_threads > 1024)
		wanted_dummy_threads = 1024;

	for (i = 0; i < wanted_dummy_threads; i++) {
		pthread_t t;

		pthread_create(&t, NULL, iv_mb_bcast_dummy, NULL);
	}

	pthread_mutex_lock(&lock);
	while (running_dummies < wanted_dummy_threads)
		pthread_cond_wait(&cond, &lock);
	pthread_mutex_unlock(&lock);

	for (i = 0; i < wanted_mb_threads; i++)
		pthread_create(&tid[i], NULL, iv_mb_bcast_hammer, NULL);

	for (i = 0; i < wanted_mb_threads; i++)
		pthread_join(tid[i], NULL);

	return 0;
}
