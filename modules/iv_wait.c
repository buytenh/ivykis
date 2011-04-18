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
#include <iv_wait.h>
#include <pthread.h>
#include "config.h"
#include "thr.h"

#ifndef WCONTINUED
#define WCONTINUED		0
#define WIFCONTINUED(x)		0
#endif

struct wait_event {
	struct list_head	list;
	int			status;
#ifdef HAVE_WAIT4
	struct rusage		rusage;
#endif
};

static int
iv_wait_interest_compare(struct iv_avl_node *_a, struct iv_avl_node *_b)
{
	struct iv_wait_interest *a;
	struct iv_wait_interest *b;

	a = container_of(_a, struct iv_wait_interest, avl_node);
	b = container_of(_b, struct iv_wait_interest, avl_node);

	if (a->pid < b->pid)
		return -1;
	if (a->pid > b->pid)
		return 1;
	return 0;
}

static pthread_mutex_t iv_wait_lock = PTHREAD_MUTEX_INITIALIZER;
static struct iv_avl_tree iv_wait_interests =
	IV_AVL_TREE_INIT(iv_wait_interest_compare);

static struct iv_wait_interest *__iv_wait_interest_find(int pid)
{
	struct iv_avl_node *an;

	an = iv_wait_interests.root;
	while (an != NULL) {
		struct iv_wait_interest *p;

		p = container_of(an, struct iv_wait_interest, avl_node);
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
	pthr_mutex_lock(&iv_wait_lock);
	while (1) {
		pid_t pid;
		int status;
		struct wait_event *we;
		struct iv_wait_interest *p;

#ifdef HAVE_WAIT4
		struct rusage rusage;

		pid = wait4(-1, &status,
			    WNOHANG | WUNTRACED | WCONTINUED, &rusage);
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
		if (we == NULL) {
			fprintf(stderr, "iv_wait_got_sigchld: OOM\n");
			exit(1);
		}

		we->status = status;
#ifdef HAVE_WAIT4
		we->rusage = rusage;
#endif

		p = __iv_wait_interest_find(pid);
		if (p != NULL) {
			list_add_tail(&we->list, &p->events);
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
			p->dead = 1;
		}
	}
	pthr_mutex_unlock(&iv_wait_lock);
}

static void iv_wait_completion(void *_this)
{
	struct iv_wait_interest *this = _this;

	this->term = (void **)&this;

	pthr_mutex_lock(&iv_wait_lock);
	while (!list_empty(&this->events)) {
		struct wait_event *we;

		we = container_of(this->events.next, struct wait_event, list);
		list_del(&we->list);

		pthr_mutex_unlock(&iv_wait_lock);
#ifdef HAVE_WAIT4
		this->handler(this->cookie, we->status, &we->rusage);
#else
		this->handler(this->cookie, we->status, NULL);
#endif
		pthr_mutex_lock(&iv_wait_lock);

		free(we);

		if (this == NULL)
			break;
	}
	pthr_mutex_unlock(&iv_wait_lock);

	if (this != NULL)
		this->term = NULL;
}

static __thread struct iv_wait_thr_info {
	int			wait_count;
	struct iv_signal	sigchld_interest;
} tinfo;

static void __iv_wait_interest_register(struct iv_wait_interest *this)
{
	if (!tinfo.wait_count++) {
		IV_SIGNAL_INIT(&tinfo.sigchld_interest);
		tinfo.sigchld_interest.signum = SIGCHLD;
		tinfo.sigchld_interest.exclusive = 1;
		tinfo.sigchld_interest.handler = iv_wait_got_sigchld;
		iv_signal_register(&tinfo.sigchld_interest);
	}

	IV_EVENT_INIT(&this->ev);
	this->ev.handler = iv_wait_completion;
	this->ev.cookie = this;
	iv_event_register(&this->ev);

	INIT_LIST_HEAD(&this->events);

	this->term = NULL;

	this->dead = 0;
}

void iv_wait_interest_register(struct iv_wait_interest *this)
{
	__iv_wait_interest_register(this);

	pthr_mutex_lock(&iv_wait_lock);
	iv_avl_tree_insert(&iv_wait_interests, &this->avl_node);
	pthr_mutex_unlock(&iv_wait_lock);
}

int iv_wait_interest_register_spawn(struct iv_wait_interest *this,
				    void (*fn)(void *cookie), void *cookie)
{
	pid_t pid;

	pthr_mutex_lock(&iv_wait_lock);

	pid = fork();
	if (pid < 0) {
		pthr_mutex_unlock(&iv_wait_lock);
		return pid;
	}

	if (pid == 0) {
		fn(cookie);
		exit(1);
	} else {
		this->pid = pid;
		__iv_wait_interest_register(this);
		iv_avl_tree_insert(&iv_wait_interests, &this->avl_node);
	}

	pthr_mutex_unlock(&iv_wait_lock);

	return 0;
}

void iv_wait_interest_unregister(struct iv_wait_interest *this)
{
	pthr_mutex_lock(&iv_wait_lock);
	if (!this->dead)
		iv_avl_tree_delete(&iv_wait_interests, &this->avl_node);
	pthr_mutex_unlock(&iv_wait_lock);

	iv_event_unregister(&this->ev);

	while (!list_empty(&this->events)) {
		struct wait_event *we;

		we = container_of(this->events.next, struct wait_event, list);
		list_del(&we->list);
		free(we);
	}

	if (this->term != NULL)
		*this->term = NULL;

	if (!--tinfo.wait_count)
		iv_signal_unregister(&tinfo.sigchld_interest);
}

int iv_wait_interest_kill(struct iv_wait_interest *this, int sig)
{
	int ret;

	pthr_mutex_lock(&iv_wait_lock);
	ret = !this->dead ? kill(this->pid, sig) : -ESRCH;
	pthr_mutex_unlock(&iv_wait_lock);

	return ret;
}
