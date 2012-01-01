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
#include <inttypes.h>

#define MAX_SIGS	32

static pthread_spinlock_t sig_interests_lock;
static struct iv_list_head sig_interests[MAX_SIGS];

static void iv_signal_init(void) __attribute__((constructor));
static void iv_signal_init(void)
{
	int i;

	pthread_spin_init(&sig_interests_lock, PTHREAD_PROCESS_PRIVATE);

	for (i = 0; i < MAX_SIGS; i++)
		INIT_IV_LIST_HEAD(&sig_interests[i]);
}

static void __iv_signal_do_wake(int signum)
{
	struct iv_list_head *ilh;

	iv_list_for_each (ilh, &sig_interests[signum]) {
		struct iv_signal *is;

		is = iv_container_of(ilh, struct iv_signal, list);

		/*
		 * Prevent signals delivered to child processes
		 * from causing callbacks to be invoked.
		 */
		if (is->owner != getpid())
			continue;

		iv_event_raw_post(&is->ev);
		is->active = 1;

		if (is->flags & IV_SIGNAL_FLAG_EXCLUSIVE)
			break;
	}
}

static void iv_signal_handler(int signum)
{
	if (signum < 0 || signum >= MAX_SIGS)
		return;

	pthread_spin_lock(&sig_interests_lock);
	__iv_signal_do_wake(signum);
	pthread_spin_unlock(&sig_interests_lock);
}

static void iv_signal_event(void *_this)
{
	struct iv_signal *this = _this;
	sigset_t mask;

	sigfillset(&mask);
	pthread_sigmask(SIG_BLOCK, &mask, &mask);
	pthread_spin_lock(&sig_interests_lock);

	this->active = 0;

	pthread_spin_unlock(&sig_interests_lock);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	this->handler(this->cookie);
}

int iv_signal_register(struct iv_signal *this)
{
	sigset_t mask;

	if (this->signum < 0 || this->signum >= MAX_SIGS)
		return -EINVAL;

	IV_EVENT_RAW_INIT(&this->ev);
	this->ev.cookie = this;
	this->ev.handler = iv_signal_event;
	iv_event_raw_register(&this->ev);

	this->owner = getpid();
	this->active = 0;

	sigfillset(&mask);
	pthread_sigmask(SIG_BLOCK, &mask, &mask);
	pthread_spin_lock(&sig_interests_lock);

	if (iv_list_empty(&sig_interests[this->signum])) {
		struct sigaction sa;

		sa.sa_handler = iv_signal_handler;
		sigfillset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;
		sigaction(this->signum, &sa, NULL);
	}
	iv_list_add_tail(&this->list, &sig_interests[this->signum]);

	pthread_spin_unlock(&sig_interests_lock);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	return 0;
}

void iv_signal_unregister(struct iv_signal *this)
{
	sigset_t mask;

	sigfillset(&mask);
	pthread_sigmask(SIG_BLOCK, &mask, &mask);
	pthread_spin_lock(&sig_interests_lock);

	iv_list_del(&this->list);
	if (iv_list_empty(&sig_interests[this->signum])) {
		struct sigaction sa;

		sa.sa_handler = SIG_DFL;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(this->signum, &sa, NULL);
	} else if ((this->flags & IV_SIGNAL_FLAG_EXCLUSIVE) && this->active) {
		__iv_signal_do_wake(this->signum);
	}

	pthread_spin_unlock(&sig_interests_lock);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	iv_event_raw_unregister(&this->ev);
}
