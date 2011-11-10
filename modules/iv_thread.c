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
	struct list_head	list;
	struct iv_event		dead;
	char			*name;
	pid_t			tid;
	void			(*start_routine)(void *);
	void			*arg;
};

static __thread struct list_head child_threads;

static int iv_thread_debug;


/* gettid *******************************************************************/
#ifdef __FreeBSD__
/* Older FreeBSDs (6.1) don't include ucontext.h in thr.h.  */
#include <sys/ucontext.h>
#include <sys/thr.h>
#endif

#ifndef __NR_gettid
#if defined(linux) && defined(__x86_64__)
#define __NR_gettid	186
#elif defined(linux) && defined(__i386__)
#define __NR_gettid	224
#endif
#endif

static pid_t gettid(void)
{
	pid_t tid;

	tid = 0;
#ifdef __NR_gettid
	tid = syscall(__NR_gettid);
#elif defined(__FreeBSD__)
	long thr;
	thr_self(&thr);
	tid = (pid_t)thr;
#endif

	return tid;
}


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

	thr->tid = gettid();

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

	list_del(&thr->list);
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

	IV_EVENT_INIT(&thr->dead);
	thr->dead.cookie = thr;
	thr->dead.handler = iv_thread_died;
	iv_event_register(&thr->dead);

	thr->name = strdup(name);
	thr->tid = 0;
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

	if (child_threads.next == NULL)
		INIT_LIST_HEAD(&child_threads);
	list_add_tail(&thr->list, &child_threads);

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

void iv_thread_list_children(void)
{
	struct list_head *lh;

	fprintf(stderr, "tid\tname\n");
	fprintf(stderr, "%d\tself\n", (int)gettid());

	list_for_each (lh, &child_threads) {
		struct iv_thread *thr;

		thr = list_entry(lh, struct iv_thread, list);
		fprintf(stderr, "%d\t%s\n", (int)thr->tid, thr->name);
	}
}
