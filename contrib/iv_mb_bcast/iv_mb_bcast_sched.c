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
#include <errno.h>
#include <iv_mb_bcast.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>

static int max_set;
static pthread_mutex_t lock;
static int thr_running;
static pthread_cond_t seq_in_cond;
static uint32_t seq_in;
static pthread_cond_t seq_out_cond;
static uint32_t seq_out;

static void iv_mb_bcast_init(void) __attribute__((constructor));
static void iv_mb_bcast_init(void)
{
	pthread_t self;
	int i;

	self = pthread_self();
	for (i = sizeof(long); i <= sizeof(cpu_set_t); i += sizeof(long)) {
		cpu_set_t set;

		if (pthread_getaffinity_np(self, i, &set) == 0)
			break;
	}

	if (i > sizeof(cpu_set_t)) {
		fprintf(stderr, "iv_mb_bcast_init: can't determine "
				"proper CPU bitmask size\n");
		abort();
	}

	max_set = i;
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&seq_in_cond, NULL);
	pthread_cond_init(&seq_out_cond, NULL);
}

static int wait_for_event(void)
{
	while (seq_in == seq_out) {
		struct timespec t;
		int ret;

		clock_gettime(CLOCK_REALTIME, &t);

		t.tv_nsec += 1000000000ULL / 10;
		if (t.tv_nsec >= 1000000000ULL) {
			t.tv_sec++;
			t.tv_nsec -= 1000000000ULL;
		}

		ret = pthread_cond_timedwait(&seq_in_cond, &lock, &t);
		if (ret == ETIMEDOUT)
			return 0;
	}

	return 1;
}

static void *mb_thread(void *_dummy)
{
	pthread_t self;
	int max_cpu;
	cpu_set_t set;

	self = pthread_self();
	max_cpu = 8 * max_set;
	CPU_ZERO(&set);

	pthread_mutex_lock(&lock);

	while (wait_for_event()) {
		uint32_t seq;
		int i;

		seq = seq_in;
		pthread_mutex_unlock(&lock);

		for (i = 0; i < max_cpu; i++) {
			CPU_SET(i, &set);
			pthread_setaffinity_np(self, max_set, &set);
			CPU_CLR(i, &set);
		}

		pthread_mutex_lock(&lock);
		seq_out = seq;
		pthread_cond_broadcast(&seq_out_cond);
	}

	thr_running = 0;

	pthread_mutex_unlock(&lock);

	return NULL;
}

void iv_mb_bcast_init_thread(void)
{
}

void iv_mb_bcast(void)
{
	uint32_t seq;

	pthread_mutex_lock(&lock);

	seq = ++seq_in;

	if (thr_running) {
		pthread_cond_signal(&seq_in_cond);
	} else {
		pthread_t tid;

		if (pthread_create(&tid, NULL, mb_thread, NULL)) {
			fprintf(stderr, "iv_mb_bcast: error "
					"creating sched thread\n");
			abort();
		}

		thr_running = 1;
	}

	do {
		pthread_cond_wait(&seq_out_cond, &lock);
	} while (((int32_t)(seq - seq_out)) > 0);

	pthread_mutex_unlock(&lock);
}
