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
#include <iv_event.h>
#include <iv_thread.h>
#include <pthread.h>
#include <string.h>

/* data structures and global data ******************************************/
struct iv_thread {
	struct iv_event		dead;
	char			*name;
	void			(*start_routine)(void *);
	void			*arg;
};

static int iv_thread_debug;


/* callee thread ************************************************************/
static void iv_thread_cleanup_handler(void *_thr)
{
	struct iv_thread *thr = _thr;

	if (iv_thread_debug)
		fprintf(stderr, "iv_thread: [%s] was canceled\n", thr->name);

	iv_event_post(&thr->dead);
}

static void *iv_thread_handler(void *_thr)
{
	struct iv_thread *thr = _thr;

	pthread_cleanup_push(iv_thread_cleanup_handler, thr);
	thr->start_routine(thr->arg);
	pthread_cleanup_pop(0);

	if (iv_thread_debug)
		fprintf(stderr, "iv_thread: [%s] terminating normally\n",
			thr->name);

	iv_event_post(&thr->dead);

	return NULL;
}


/* calling thread ***********************************************************/
static void iv_thread_died(void *_thr)
{
	struct iv_thread *thr = _thr;

	if (iv_thread_debug)
		fprintf(stderr, "iv_thread: [%s] joined\n", thr->name);

	iv_event_unregister(&thr->dead);
	free(thr->name);
	free(thr);
}

int iv_thread_create(char *name, void (*start_routine)(void *), void *arg)
{
	struct iv_thread *thr;
	pthread_attr_t attr;
	pthread_t t;
	int ret;

	thr = malloc(sizeof(*thr));
	if (thr == NULL)
		return -1;

	thr->dead.cookie = thr;
	thr->dead.handler = iv_thread_died;
	iv_event_register(&thr->dead);

	thr->name = strdup(name);
	thr->start_routine = start_routine;
	thr->arg = arg;

	ret = pthread_attr_init(&attr);
	if (ret < 0)
		goto out_event;

	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ret < 0)
		goto out_attr;

	ret = pthread_create(&t, &attr, iv_thread_handler, thr);
	if (ret < 0)
		goto out_attr;

	pthread_attr_destroy(&attr);

	if (iv_thread_debug)
		fprintf(stderr, "iv_thread: [%s] started\n", name);

	return 0;

out_attr:
	pthread_attr_destroy(&attr);

out_event:
	iv_event_unregister(&thr->dead);
	free(thr);

	if (iv_thread_debug)
		fprintf(stderr, "iv_thread: [%s] failed to start\n", name);

	return -1;
}

void iv_thread_set_debug_state(int state)
{
	iv_thread_debug = !!state;
}
