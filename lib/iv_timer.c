/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009 Lennert Buytenhek
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
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "iv_private.h"

/* time handling ************************************************************/
void __iv_invalidate_now(struct iv_state *st)
{
	st->time_valid = 0;
}

void iv_invalidate_now(void)
{
	struct iv_state *st = iv_get_state();

	__iv_invalidate_now(st);
}

#ifdef HAVE_CLOCK_GETTIME
static int		clock_source;
#endif

static void __iv_validate_now(struct iv_state *st)
{
	if (!st->time_valid) {
		struct timeval tv;

		st->time_valid = 1;

#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
		if (clock_source < 1) {
			if (clock_gettime(CLOCK_MONOTONIC, &st->time) >= 0)
				return;
			clock_source = 1;
		}
#endif

#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_REALTIME)
		if (clock_source < 2) {
			if (clock_gettime(CLOCK_REALTIME, &st->time) >= 0)
				return;
			clock_source = 2;
		}
#endif

		gettimeofday(&tv, NULL);
		st->time.tv_sec = tv.tv_sec;
		st->time.tv_nsec = 1000L * tv.tv_usec;
	}
}

void iv_validate_now(void)
{
	struct iv_state *st = iv_get_state();

	__iv_validate_now(st);
}

struct timespec *__iv_now_location(void)
{
	struct iv_state *st = iv_get_state();

	return &st->time;
}


/* timer list handling ******************************************************/
#define SPLIT_BITS		(10)
#define SPLIT_NODES		(1 << SPLIT_BITS)
#define SPLIT_LEVELS		(2)
#define SPLIT_MAX		(1 << (SPLIT_BITS * SPLIT_LEVELS))
struct ratnode { void *child[SPLIT_NODES]; };

void IV_TIMER_INIT(struct iv_timer *_t)
{
	struct iv_timer_ *t = (struct iv_timer_ *)_t;

	t->index = -1;
}

static inline int timespec_gt(struct timespec *a, struct timespec *b)
{
	return !!(a->tv_sec > b->tv_sec ||
		 (a->tv_sec == b->tv_sec && a->tv_nsec > b->tv_nsec));
}

static inline int timer_ptr_gt(struct iv_timer_ *a, struct iv_timer_ *b)
{
	return timespec_gt(&a->expires, &b->expires);
}

static struct iv_timer_ **get_node(struct iv_state *st, int index)
{
	struct ratnode **r;
	int i;

	if (index < 1 || index >= SPLIT_MAX)
		return NULL;

	r = &st->timer_root;
	for (i = SPLIT_LEVELS - 1; i >= 0; i--) {
		int bits;

		if (*r == NULL) {
			*r = calloc(1, sizeof(struct ratnode));
			if (*r == NULL)
				return NULL;
		}

		bits = (index >> i * SPLIT_BITS) & (SPLIT_NODES - 1);
		r = (struct ratnode **)&((*r)->child[bits]);
	}

	return (struct iv_timer_ **)r;
}

void iv_timer_init(struct iv_state *st)
{
	if (get_node(st, 1) == NULL)
		iv_fatal("iv_timer_init: can't alloc memory for root ratnode");
}

int iv_pending_timers(struct iv_state *st)
{
	return !!st->num_timers;
}

int iv_get_soonest_timeout(struct iv_state *st, struct timespec *to)
{
	if (st->num_timers) {
		struct iv_timer_ *t = *get_node(st, 1);

		iv_validate_now();
		to->tv_sec = t->expires.tv_sec - st->time.tv_sec;
		to->tv_nsec = t->expires.tv_nsec - st->time.tv_nsec;
		if (to->tv_nsec < 0) {
			to->tv_sec--;
			to->tv_nsec += 1000000000;
		}

		return !!(to->tv_sec < 0 ||
			  (to->tv_sec == 0 && to->tv_nsec == 0));
	}

	to->tv_sec = 3600;
	to->tv_nsec = 0;

	return 0;
}

static void free_ratnode(struct ratnode *node, int depth)
{
	int i;

	for (i = 0; i < SPLIT_NODES; i++) {
		if (node->child[i] == NULL)
			break;

		if (depth == 1)
			free(node->child[i]);
		else
			free_ratnode(node->child[i], depth - 1);
	}

	free(node);
}

void iv_timer_deinit(struct iv_state *st)
{
	free_ratnode(st->timer_root, SPLIT_LEVELS - 1);
	st->timer_root = NULL;
}

static void pull_up(struct iv_state *st, int index, struct iv_timer_ **i)
{
	while (index != 1) {
		struct iv_timer_ *et;
		int parent;
		struct iv_timer_ **p;

		parent = index / 2;
		p = get_node(st, parent);

		if (!timer_ptr_gt(*p, *i))
			break;

		et = *i;
		*i = *p;
		*p = et;

		(*i)->index = index;
		(*p)->index = parent;

		index = parent;
		i = p;
	}
}

void iv_timer_register(struct iv_timer *_t)
{
	struct iv_state *st = iv_get_state();
	struct iv_timer_ *t = (struct iv_timer_ *)_t;
	struct iv_timer_ **p;
	int index;

	if (t->index != -1) {
		iv_fatal("iv_timer_register: called with timer still "
			 "on the heap");
	}

	index = ++st->num_timers;
	p = get_node(st, index);
	if (p == NULL)
		iv_fatal("iv_timer_register: timer list overflow");

	*p = t;
	t->index = index;

	pull_up(st, index, p);
}

static void push_down(struct iv_state *st, int index, struct iv_timer_ **i)
{
	while (1) {
		struct iv_timer_ *et;
		struct iv_timer_ **imin;
		int index_min;

		index_min = index;
		imin = i;

		if (2 * index <= st->num_timers) {
			struct iv_timer_ **p;

			p = get_node(st, 2 * index);
			if (timer_ptr_gt(*imin, p[0])) {
				index_min = 2 * index;
				imin = p;
			}
			if (p[1] && timer_ptr_gt(*imin, p[1])) {
				index_min = 2 * index + 1;
				imin = p + 1;
			}
		}

		if (index == index_min)
			break;

		et = *i;
		*i = *imin;
		*imin = et;

		(*i)->index = index;
		(*imin)->index = index_min;

		index = index_min;
		i = imin;
	}
}

void iv_timer_unregister(struct iv_timer *_t)
{
	struct iv_state *st = iv_get_state();
	struct iv_timer_ *t = (struct iv_timer_ *)_t;
	struct iv_timer_ **m;
	struct iv_timer_ **p;

	if (t->index == -1) {
		iv_fatal("iv_timer_unregister: called with timer not "
			 "on the heap");
	}

	if (t->index > st->num_timers) {
		iv_fatal("iv_timer_unregister: timer index %d > %d",
			 t->index, st->num_timers);
	}

	p = get_node(st, t->index);
	if (*p != t) {
		iv_fatal("iv_timer_unregister: unregistered timer "
			 "index belonging to other timer");
	}

	m = get_node(st, st->num_timers);
	st->num_timers--;

	*p = *m;
	(*p)->index = t->index;
	*m = NULL;
	if (p != m) {
		pull_up(st, (*p)->index, p);
		push_down(st, (*p)->index, p);
	}

	t->index = -1;
}

void iv_run_timers(struct iv_state *st)
{
	while (st->num_timers) {
		struct iv_timer_ *t = *get_node(st, 1);

		__iv_validate_now(st);
		if (timespec_gt(&t->expires, &st->time))
			break;
		iv_timer_unregister((struct iv_timer *)t);
		t->handler(t->cookie);
	}
}

int iv_timer_registered(struct iv_timer *_t)
{
	struct iv_timer_ *t = (struct iv_timer_ *)_t;

	return !(t->index == -1);
}
