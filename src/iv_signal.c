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
#include <inttypes.h>
#include <iv_list.h>
#include <iv_signal.h>
#include <pthread.h>
#include <string.h>
#include "spinlock.h"

static spinlock_t sig_interests_lock;
static struct iv_avl_tree sig_interests;
static sigset_t sig_mask_fork;

static int iv_signal_compare(struct iv_avl_node *_a, struct iv_avl_node *_b)
{
	struct iv_signal *a = iv_container_of(_a, struct iv_signal, an);
	struct iv_signal *b = iv_container_of(_b, struct iv_signal, an);

	if (a->signum < b->signum)
		return -1;
	if (a->signum > b->signum)
		return 1;

	if ((a->flags & IV_SIGNAL_FLAG_EXCLUSIVE) &&
	    !(b->flags & IV_SIGNAL_FLAG_EXCLUSIVE))
		return -1;
	if (!(a->flags & IV_SIGNAL_FLAG_EXCLUSIVE) &&
	    (b->flags & IV_SIGNAL_FLAG_EXCLUSIVE))
		return 1;

	if (a < b)
		return -1;
	if (a > b)
		return 1;

	return 0;
}

static void iv_signal_prepare(void)
{
	sigset_t mask;

	spin_lock_sigmask(&sig_interests_lock, &mask);
	sig_mask_fork = mask;
}

static void iv_signal_parent(void)
{
	sigset_t mask;

	mask = sig_mask_fork;
	spin_unlock_sigmask(&sig_interests_lock, &mask);
}

static void iv_signal_child(void)
{
	struct sigaction sa;
	int last_signum;
	struct iv_avl_node *an;

	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	last_signum = -1;
	iv_avl_tree_for_each (an, &sig_interests) {
		struct iv_signal *is;

		is = iv_container_of(an, struct iv_signal, an);
		if (is->signum != last_signum) {
			sigaction(is->signum, &sa, NULL);
			last_signum = is->signum;
		}
	}

	sig_interests.root = NULL;

	iv_signal_parent();
}

static void iv_signal_init(void) __attribute__((constructor));
static void iv_signal_init(void)
{
	spin_init(&sig_interests_lock);

	INIT_IV_AVL_TREE(&sig_interests, iv_signal_compare);

	pthread_atfork(iv_signal_prepare, iv_signal_parent, iv_signal_child);
}

static struct iv_avl_node *__iv_signal_find_first(int signum)
{
	struct iv_avl_node *iter;
	struct iv_avl_node *best;

	for (iter = sig_interests.root, best = NULL; iter != NULL; ) {
		struct iv_signal *is;

		is = iv_container_of(iter, struct iv_signal, an);
		if (signum == is->signum)
			best = iter;

		if (signum <= is->signum)
			iter = iter->left;
		else
			iter = iter->right;
	}

	return best;
}

static void __iv_signal_do_wake(int signum)
{
	struct iv_avl_node *an;

	an = __iv_signal_find_first(signum);
	while (an != NULL) {
		struct iv_signal *is;

		is = iv_container_of(an, struct iv_signal, an);
		if (is->signum != signum)
			break;

		is->active = 1;
		iv_event_raw_post(&is->ev);

		if (is->flags & IV_SIGNAL_FLAG_EXCLUSIVE)
			break;

		an = iv_avl_tree_next(an);
	}
}

static void iv_signal_handler(int signum)
{
	spin_lock(&sig_interests_lock);
	__iv_signal_do_wake(signum);
	spin_unlock(&sig_interests_lock);
}

static void iv_signal_event(void *_this)
{
	struct iv_signal *this = _this;
	sigset_t mask;

	spin_lock_sigmask(&sig_interests_lock, &mask);
	this->active = 0;
	spin_unlock_sigmask(&sig_interests_lock, &mask);

	this->handler(this->cookie);
}

int iv_signal_register(struct iv_signal *this)
{
	sigset_t mask;

	IV_EVENT_RAW_INIT(&this->ev);
	this->ev.cookie = this;
	this->ev.handler = iv_signal_event;
	iv_event_raw_register(&this->ev);

	this->active = 0;

	spin_lock_sigmask(&sig_interests_lock, &mask);

	if (__iv_signal_find_first(this->signum) == NULL) {
		struct sigaction sa;

		sa.sa_handler = iv_signal_handler;
		sigfillset(&sa.sa_mask);
#ifndef __QNX__
		sa.sa_flags = SA_RESTART;
#else
		sa.sa_flags = 0;
#endif
		if (sigaction(this->signum, &sa, NULL) < 0) {
			iv_fatal("iv_signal_register: sigaction got "
				 "error %d[%s]", errno, strerror(errno));
		}
	}
	iv_avl_tree_insert(&sig_interests, &this->an);

	spin_unlock_sigmask(&sig_interests_lock, &mask);

	return 0;
}

void iv_signal_unregister(struct iv_signal *this)
{
	sigset_t mask;

	spin_lock_sigmask(&sig_interests_lock, &mask);

	iv_avl_tree_delete(&sig_interests, &this->an);
	if (__iv_signal_find_first(this->signum) == NULL) {
		struct sigaction sa;

		sa.sa_handler = SIG_DFL;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(this->signum, &sa, NULL);
	} else if ((this->flags & IV_SIGNAL_FLAG_EXCLUSIVE) && this->active) {
		__iv_signal_do_wake(this->signum);
	}

	spin_unlock_sigmask(&sig_interests_lock, &mask);

	iv_event_raw_unregister(&this->ev);
}
