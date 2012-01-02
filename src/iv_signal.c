/*
 * ivykis, an event handling library
 * Copyright (C) 2010, 2011, 2016 Lennert Buytenhek
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
#include <iv_tls.h>
#include <string.h>
#include "spinlock.h"

#ifndef _NSIG
#define _NSIG		64
#endif

static pid_t sig_owner_pid;
static spinlock_t sig_lock;
static int total_num_interests[_NSIG];
static struct iv_avl_tree process_sigs;
static sigset_t sig_mask_fork;

static int
iv_signal_compare(const struct iv_avl_node *_a, const struct iv_avl_node *_b)
{
	const struct iv_signal *a =
		iv_container_of(_a, struct iv_signal, an);
	const struct iv_signal *b =
		iv_container_of(_b, struct iv_signal, an);

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

	spin_lock_sigmask(&sig_lock, &mask);
	sig_mask_fork = mask;
}

static void iv_signal_parent(void)
{
	sigset_t mask;

	mask = sig_mask_fork;
	spin_unlock_sigmask(&sig_lock, &mask);
}

static void iv_signal_child(void)
{
	spin_init(&sig_lock);

	pthr_sigmask(SIG_SETMASK, &sig_mask_fork, NULL);
}

struct iv_signal_thr_info {
	struct iv_avl_tree	thr_sigs;
};

static void iv_signal_tls_init_thread(void *_tinfo)
{
	struct iv_signal_thr_info *tinfo = _tinfo;

	INIT_IV_AVL_TREE(&tinfo->thr_sigs, iv_signal_compare);
}

static struct iv_tls_user iv_signal_tls_user = {
	.sizeof_state	= sizeof(struct iv_signal_thr_info),
	.init_thread	= iv_signal_tls_init_thread,
};

static void iv_signal_init(void) __attribute__((constructor));
static void iv_signal_init(void)
{
	spin_init(&sig_lock);

	INIT_IV_AVL_TREE(&process_sigs, iv_signal_compare);

	pthr_atfork(iv_signal_prepare, iv_signal_parent, iv_signal_child);

	iv_tls_user_register(&iv_signal_tls_user);
}

static struct iv_avl_node *
__iv_signal_find_first(struct iv_avl_tree *tree, int signum)
{
	struct iv_avl_node *iter;
	struct iv_avl_node *best;

	for (iter = tree->root, best = NULL; iter != NULL; ) {
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

static int __iv_signal_do_wake(struct iv_avl_tree *tree, int signum)
{
	int woken;
	struct iv_avl_node *an;

	woken = 0;

	an = __iv_signal_find_first(tree, signum);
	while (an != NULL) {
		struct iv_signal *is;

		is = iv_container_of(an, struct iv_signal, an);
		if (is->signum != signum)
			break;

		is->active = 1;
		iv_event_raw_post(&is->ev);

		woken++;

		if (is->flags & IV_SIGNAL_FLAG_EXCLUSIVE)
			break;

		an = iv_avl_tree_next(an);
	}

	return woken;
}

static void iv_signal_handler(int signum)
{
	struct iv_signal_thr_info *tinfo;

	if (sig_owner_pid == 0 || sig_owner_pid != getpid())
		return;

	tinfo = iv_tls_user_ptr(&iv_signal_tls_user);
	if (tinfo == NULL || !__iv_signal_do_wake(&tinfo->thr_sigs, signum)) {
		spin_lock(&sig_lock);
		__iv_signal_do_wake(&process_sigs, signum);
		spin_unlock(&sig_lock);
	}
}

static void iv_signal_event(void *_this)
{
	struct iv_signal *this = _this;
	sigset_t all;
	sigset_t mask;

	sigfillset(&all);
	pthr_sigmask(SIG_BLOCK, &all, &mask);

	if (!(this->flags & IV_SIGNAL_FLAG_THIS_THREAD)) {
		spin_lock(&sig_lock);
		this->active = 0;
		spin_unlock(&sig_lock);
	} else {
		this->active = 0;
	}

	pthr_sigmask(SIG_SETMASK, &mask, NULL);

	this->handler(this->cookie);
}

void iv_signal_child_reset_postfork(void)
{
	struct sigaction sa;
	int i;
	struct iv_signal_thr_info *tinfo;

	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	for (i = 0; i < _NSIG; i++) {
		if (total_num_interests[i]) {
			sigaction(i, &sa, NULL);
			total_num_interests[i] = 0;
		}
	}

	sig_owner_pid = 0;

	process_sigs.root = NULL;

	tinfo = iv_tls_user_ptr(&iv_signal_tls_user);
	if (tinfo != NULL)
		tinfo->thr_sigs.root = NULL;
}

static struct iv_avl_tree *iv_signal_tree(struct iv_signal *this)
{
	if (this->flags & IV_SIGNAL_FLAG_THIS_THREAD) {
		struct iv_signal_thr_info *tinfo;

		tinfo = iv_tls_user_ptr(&iv_signal_tls_user);
		return &tinfo->thr_sigs;
	} else {
		return &process_sigs;
	}
}

int iv_signal_register(struct iv_signal *this)
{
	pid_t mypid;
	sigset_t mask;

	if (this->signum < 0 || this->signum >= _NSIG)
		return -1;

	spin_lock_sigmask(&sig_lock, &mask);

	mypid = getpid();
	if (sig_owner_pid != 0 && sig_owner_pid != mypid) {
		iv_signal_child_reset_postfork();
		sig_owner_pid = mypid;
	} else if (sig_owner_pid == 0) {
		sig_owner_pid = mypid;
	}

	IV_EVENT_RAW_INIT(&this->ev);
	this->ev.cookie = this;
	this->ev.handler = iv_signal_event;
	iv_event_raw_register(&this->ev);

	this->active = 0;

	if (!total_num_interests[this->signum]++) {
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

	iv_avl_tree_insert(iv_signal_tree(this), &this->an);

	spin_unlock_sigmask(&sig_lock, &mask);

	return 0;
}

void iv_signal_unregister(struct iv_signal *this)
{
	sigset_t mask;

	if (this->signum < 0 || this->signum >= _NSIG)
		iv_fatal("iv_signal_unregister: signal number out of range");

	spin_lock_sigmask(&sig_lock, &mask);

	iv_avl_tree_delete(iv_signal_tree(this), &this->an);

	if (!--total_num_interests[this->signum]) {
		struct sigaction sa;

		sa.sa_handler = SIG_DFL;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(this->signum, &sa, NULL);
	} else if ((this->flags & IV_SIGNAL_FLAG_EXCLUSIVE) && this->active) {
		__iv_signal_do_wake(iv_signal_tree(this), this->signum);
	}

	spin_unlock_sigmask(&sig_lock, &mask);

	iv_event_raw_unregister(&this->ev);
}
