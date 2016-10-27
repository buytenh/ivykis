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
#include <iv_signal.h>
#include <iv_tls.h>
#include <iv_wait.h>
#include "iv_private.h"
#include "mutex.h"

#ifndef WCONTINUED
#define WCONTINUED		0
#define WIFCONTINUED(x)		0
#endif

#define IV_WAIT_STATUS_DEAD	1

struct wait_event {
	struct iv_list_head	list;
	int			status;
#ifdef HAVE_WAIT4
	struct rusage		rusage;
#endif
};

static int iv_wait_interest_compare(const struct iv_avl_node *_a,
				    const struct iv_avl_node *_b)
{
	const struct iv_wait_interest *a =
		iv_container_of(_a, struct iv_wait_interest, avl_node);
	const struct iv_wait_interest *b =
		iv_container_of(_b, struct iv_wait_interest, avl_node);

	if (a->pid < b->pid)
		return -1;
	if (a->pid > b->pid)
		return 1;

	return 0;
}

static ___mutex_t iv_wait_lock;
static struct iv_avl_tree iv_wait_interests =
	IV_AVL_TREE_INIT(iv_wait_interest_compare);

static struct iv_wait_interest *__iv_wait_interest_find(int pid)
{
	struct iv_avl_node *an;

	an = iv_wait_interests.root;
	while (an != NULL) {
		struct iv_wait_interest *p;

		p = iv_container_of(an, struct iv_wait_interest, avl_node);
		if (pid == p->pid)
			return p;

		if (pid < p->pid)
			an = an->left;
		else
			an = an->right;
	}

	return NULL;
}

static int iv_wait_status_dead(int status)
{
	/*
	 * On FreeBSD, WIFCONTINUED(status) => WIFSIGNALED(status).
	 */
	if (WIFSIGNALED(status) && !WIFCONTINUED(status))
		return 1;

	if (WIFEXITED(status))
		return 1;

	return 0;
}

static void iv_wait_got_sigchld(void *_dummy)
{
	___mutex_lock(&iv_wait_lock);
	while (1) {
		pid_t pid;
		int status;
		struct wait_event *we;
		struct iv_wait_interest *p;

#ifdef HAVE_WAIT4
		struct rusage rusage;

#ifdef __digital__
		union wait w;

		pid = wait4(-1, &w, WNOHANG | WUNTRACED | WCONTINUED, &rusage);
		status = w.w_status;
#else
		pid = wait4(-1, &status,
			    WNOHANG | WUNTRACED | WCONTINUED, &rusage);
#endif
		if (pid <= 0) {
			if (pid < 0 && errno != ECHILD)
				perror("wait4");
			break;
		}
#else
		pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
		if (pid <= 0) {
			if (pid < 0 && errno != ECHILD)
				perror("waitpid");
			break;
		}
#endif

		we = malloc(sizeof(*we));
		if (we == NULL)
			iv_fatal("iv_wait_got_sigchld: out of memory");

		we->status = status;
#ifdef HAVE_WAIT4
		we->rusage = rusage;
#endif

		p = __iv_wait_interest_find(pid);
		if (p != NULL) {
			iv_list_add_tail(&we->list, &p->events_pending);
			iv_event_post(&p->ev);
		} else {
			free(we);
		}

		/*
		 * If this pid is now dead, avoid queueing subsequent
		 * process status change events to this interest, as the
		 * pid might be reused between us queueing this event to
		 * the interest and the interest being unregistered,
		 * and events for the new user of this pid would then end
		 * up at the wrong interest.
		 */
		if (iv_wait_status_dead(status)) {
			iv_avl_tree_delete(&iv_wait_interests, &p->avl_node);
			p->flags = IV_WAIT_STATUS_DEAD;
		}
	}
	___mutex_unlock(&iv_wait_lock);
}

struct iv_wait_thr_info {
	int				wait_count;
	struct iv_signal		sigchld_interest;
	struct iv_wait_interest		*handled_wait_interest;
};

static void iv_wait_tls_init_thread(void *_tinfo)
{
	struct iv_wait_thr_info *tinfo = _tinfo;

	tinfo->wait_count = 0;

	IV_SIGNAL_INIT(&tinfo->sigchld_interest);
	tinfo->sigchld_interest.signum = SIGCHLD;
	tinfo->sigchld_interest.flags = IV_SIGNAL_FLAG_EXCLUSIVE;
	tinfo->sigchld_interest.handler = iv_wait_got_sigchld;

	tinfo->handled_wait_interest = NULL;
}

static struct iv_tls_user iv_wait_tls_user = {
	.sizeof_state	= sizeof(struct iv_wait_thr_info),
	.init_thread	= iv_wait_tls_init_thread,
};

static void iv_wait_tls_init(void) __attribute__((constructor));
static void iv_wait_tls_init(void)
{
	___mutex_init(&iv_wait_lock);

	iv_tls_user_register(&iv_wait_tls_user);
}

static void iv_wait_completion(void *_this)
{
	struct iv_wait_thr_info *tinfo = iv_tls_user_ptr(&iv_wait_tls_user);
	struct iv_wait_interest *this = _this;
	struct iv_list_head events;

	___mutex_lock(&iv_wait_lock);
	__iv_list_steal_elements(&this->events_pending, &events);
	___mutex_unlock(&iv_wait_lock);

	tinfo->handled_wait_interest = this;

	while (!iv_list_empty(&events)) {
		struct wait_event *we;

		we = iv_container_of(events.next, struct wait_event, list);
		iv_list_del(&we->list);

		if (tinfo->handled_wait_interest != NULL) {
#ifdef HAVE_WAIT4
			this->handler(this->cookie, we->status, &we->rusage);
#else
			this->handler(this->cookie, we->status, NULL);
#endif
		}

		free(we);
	}

	tinfo->handled_wait_interest = NULL;
}

static void __iv_wait_interest_register(struct iv_wait_thr_info *tinfo,
					struct iv_wait_interest *this)
{
	if (!tinfo->wait_count++)
		iv_signal_register(&tinfo->sigchld_interest);

	IV_EVENT_INIT(&this->ev);
	this->ev.handler = iv_wait_completion;
	this->ev.cookie = this;
	iv_event_register(&this->ev);

	INIT_IV_LIST_HEAD(&this->events_pending);

	this->dummy = NULL;

	this->flags = 0;
}

void iv_wait_interest_register(struct iv_wait_interest *this)
{
	struct iv_wait_thr_info *tinfo = iv_tls_user_ptr(&iv_wait_tls_user);

	__iv_wait_interest_register(tinfo, this);

	___mutex_lock(&iv_wait_lock);
	iv_avl_tree_insert(&iv_wait_interests, &this->avl_node);
	___mutex_unlock(&iv_wait_lock);
}

static void __iv_wait_interest_unregister(struct iv_wait_thr_info *tinfo,
					  struct iv_wait_interest *this)
{
	iv_event_unregister(&this->ev);

	while (!iv_list_empty(&this->events_pending)) {
		struct wait_event *we;

		we = iv_container_of(this->events_pending.next,
				     struct wait_event, list);
		iv_list_del(&we->list);
		free(we);
	}

	if (tinfo->handled_wait_interest == this)
		tinfo->handled_wait_interest = NULL;

	if (!--tinfo->wait_count)
		iv_signal_unregister(&tinfo->sigchld_interest);
}

int iv_wait_interest_register_spawn(struct iv_wait_interest *this,
				    void (*fn)(void *cookie), void *cookie)
{
	struct iv_wait_thr_info *tinfo = iv_tls_user_ptr(&iv_wait_tls_user);
	pid_t pid;

	__iv_wait_interest_register(tinfo, this);

	___mutex_lock(&iv_wait_lock);

	pid = fork();
	if (pid < 0) {
		___mutex_unlock(&iv_wait_lock);
		__iv_wait_interest_unregister(tinfo, this);
		return pid;
	}

	if (pid == 0) {
		iv_signal_child_reset_postfork();
		fn(cookie);
		exit(1);
	} else {
		this->pid = pid;
		iv_avl_tree_insert(&iv_wait_interests, &this->avl_node);
	}

	___mutex_unlock(&iv_wait_lock);

	return 0;
}

void iv_wait_interest_unregister(struct iv_wait_interest *this)
{
	struct iv_wait_thr_info *tinfo = iv_tls_user_ptr(&iv_wait_tls_user);

	___mutex_lock(&iv_wait_lock);
	if (!(this->flags & IV_WAIT_STATUS_DEAD))
		iv_avl_tree_delete(&iv_wait_interests, &this->avl_node);
	___mutex_unlock(&iv_wait_lock);

	__iv_wait_interest_unregister(tinfo, this);
}

int iv_wait_interest_kill(const struct iv_wait_interest *this, int sig)
{
	int ret;

	___mutex_lock(&iv_wait_lock);
	if (!(this->flags & IV_WAIT_STATUS_DEAD))
		ret = kill(this->pid, sig);
	else
		ret = -ESRCH;
	___mutex_unlock(&iv_wait_lock);

	return ret;
}
