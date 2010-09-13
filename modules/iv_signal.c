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
#include <iv_signal.h>
#include <pthread.h>
#include <stdint.h>
#include "thr.h"

#define MAX_SIGS	32

static pthread_mutex_t sig_init_lock = PTHREAD_MUTEX_INITIALIZER;
static int sig_initialized;
static pthread_spinlock_t sig_interests_lock;
static struct list_head sig_interests[MAX_SIGS];

static void iv_signal_handler(int signum)
{
	struct list_head *lh;

	if (signum < 0 || signum >= MAX_SIGS)
		return;

	pthr_spin_lock(&sig_interests_lock);

	list_for_each (lh, &sig_interests[signum]) {
		struct iv_signal *is;

		is = container_of(lh, struct iv_signal, list);

		iv_event_raw_post(&is->ev);
		is->active = 1;

		if (is->exclusive)
			break;
	}

	pthr_spin_unlock(&sig_interests_lock);
}

static void iv_signal_event(void *_this)
{
	struct iv_signal *this = _this;
	sigset_t mask;

	sigfillset(&mask);
	pthr_sigmask(SIG_BLOCK, &mask, &mask);
	pthr_spin_lock(&sig_interests_lock);

	this->active = 0;

	pthr_spin_unlock(&sig_interests_lock);
	pthr_sigmask(SIG_SETMASK, &mask, NULL);

	this->handler(this->cookie);
}

int iv_signal_register(struct iv_signal *this)
{
	sigset_t mask;

	if (this->signum < 0 || this->signum >= MAX_SIGS)
		return -EINVAL;

	this->ev.cookie = this;
	this->ev.handler = iv_signal_event;
	iv_event_raw_register(&this->ev);

	this->active = 0;

	pthr_mutex_lock(&sig_init_lock);
	if (!sig_initialized) {
		int i;

		sig_initialized = 1;

		pthr_spin_init(&sig_interests_lock, PTHREAD_PROCESS_PRIVATE);
		for (i = 0; i < MAX_SIGS; i++)
			INIT_LIST_HEAD(&sig_interests[i]);
	}
	pthr_mutex_unlock(&sig_init_lock);

	sigfillset(&mask);
	pthr_sigmask(SIG_BLOCK, &mask, &mask);
	pthr_spin_lock(&sig_interests_lock);

	if (list_empty(&sig_interests[this->signum])) {
		struct sigaction sa;

		sa.sa_handler = iv_signal_handler;
		sigfillset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;
		sigaction(this->signum, &sa, NULL);
	}
	list_add_tail(&this->list, &sig_interests[this->signum]);

	pthr_spin_unlock(&sig_interests_lock);
	pthr_sigmask(SIG_SETMASK, &mask, NULL);

	return 0;
}

void iv_signal_unregister(struct iv_signal *this)
{
	sigset_t mask;

	sigfillset(&mask);
	pthr_sigmask(SIG_BLOCK, &mask, &mask);
	pthr_spin_lock(&sig_interests_lock);

	list_del(&this->list);
	if (list_empty(&sig_interests[this->signum])) {
		struct sigaction sa;

		sa.sa_handler = SIG_DFL;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(this->signum, &sa, NULL);
	} else if (this->exclusive && this->active) {
		struct iv_signal *nxt;

		nxt = container_of(sig_interests[this->signum].next,
				   struct iv_signal, list);
		iv_event_raw_post(&nxt->ev);
	}

	pthr_spin_unlock(&sig_interests_lock);
	pthr_sigmask(SIG_SETMASK, &mask, NULL);

	iv_event_raw_unregister(&this->ev);
}
