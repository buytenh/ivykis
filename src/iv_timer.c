/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009, 2012 Lennert Buytenhek
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

void iv_validate_now(void)
{
	struct iv_state *st = iv_get_state();

	if (!st->time_valid) {
		st->time_valid = 1;
		iv_time_get(&st->time);
	}
}

struct timespec *__iv_now_location(void)
{
	struct iv_state *st = iv_get_state();

	return &st->time;
}


/* internal use *************************************************************/
#define SPLIT_BITS		(8)
#define SPLIT_NODES		(1 << SPLIT_BITS)
struct ratnode { void *child[SPLIT_NODES]; };

static struct ratnode *iv_timer_allocate_ratnode(void)
{
	struct ratnode *node;

	node = calloc(1, sizeof(struct ratnode));
	if (node == NULL)
		iv_fatal("iv_timer_allocate_ratnode: out of memory");

	return node;
}

void iv_timer_init(struct iv_state *st)
{
	st->rat_depth = 0;
	st->timer_root = iv_timer_allocate_ratnode();
}

static struct iv_timer_ **iv_timer_get_node(struct iv_state *st, int index)
{
	struct ratnode *r;
	int i;

	if (index >> ((st->rat_depth + 1) * SPLIT_BITS) != 0) {
		st->rat_depth++;

		r = iv_timer_allocate_ratnode();
		r->child[0] = st->timer_root;
		st->timer_root = r;
	}

	r = st->timer_root;
	for (i = st->rat_depth; i > 0; i--) {
		int bits;

		bits = (index >> (i * SPLIT_BITS)) & (SPLIT_NODES - 1);
		if (r->child[bits] == NULL)
			r->child[bits] = iv_timer_allocate_ratnode();
		r = r->child[bits];
	}

	return (struct iv_timer_ **)(r->child + (index & (SPLIT_NODES - 1)));
}

int iv_get_soonest_timeout(struct iv_state *st, struct timespec *to)
{
	if (st->num_timers) {
		struct iv_timer_ *t = *iv_timer_get_node(st, 1);

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

static inline int timespec_gt(struct timespec *a, struct timespec *b)
{
	return !!(a->tv_sec > b->tv_sec ||
		 (a->tv_sec == b->tv_sec && a->tv_nsec > b->tv_nsec));
}

void iv_run_timers(struct iv_state *st)
{
	while (st->num_timers) {
		struct iv_timer_ *t = *iv_timer_get_node(st, 1);

		if (!st->time_valid) {
			st->time_valid = 1;
			iv_time_get(&st->time);
		}

		if (timespec_gt(&t->expires, &st->time))
			break;
		iv_timer_unregister((struct iv_timer *)t);
		t->handler(t->cookie);
	}
}

static void free_ratnode(struct ratnode *node, int depth)
{
	if (depth) {
		int i;

		for (i = 0; i < SPLIT_NODES; i++) {
			if (node->child[i] == NULL)
				break;
			free_ratnode(node->child[i], depth - 1);
		}
	}

	free(node);
}

void iv_timer_deinit(struct iv_state *st)
{
	free_ratnode(st->timer_root, st->rat_depth);
	st->timer_root = NULL;
}

static void iv_timer_radix_tree_remove_level(struct iv_state *st)
{
	struct ratnode *root;
	int i;

	st->rat_depth--;

	root = st->timer_root;
	for (i = 1; i < SPLIT_NODES; i++) {
		if (root->child[i] == NULL)
			break;
		free_ratnode(root->child[i], st->rat_depth);
	}

	st->timer_root = root->child[0];
	free(root);
}


/* public use ***************************************************************/
void IV_TIMER_INIT(struct iv_timer *_t)
{
	struct iv_timer_ *t = (struct iv_timer_ *)_t;

	t->index = -1;
}

static inline int timer_ptr_gt(struct iv_timer_ *a, struct iv_timer_ *b)
{
	return timespec_gt(&a->expires, &b->expires);
}

static void pull_up(struct iv_state *st, int index, struct iv_timer_ **i)
{
	while (index != 1) {
		struct iv_timer_ *et;
		int parent;
		struct iv_timer_ **p;

		parent = index / 2;
		p = iv_timer_get_node(st, parent);

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

	st->numobjs++;

	index = ++st->num_timers;
	p = iv_timer_get_node(st, index);
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

			p = iv_timer_get_node(st, 2 * index);
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

	p = iv_timer_get_node(st, t->index);
	if (*p != t) {
		iv_fatal("iv_timer_unregister: unregistered timer "
			 "index belonging to other timer");
	}

	st->numobjs--;

	m = iv_timer_get_node(st, st->num_timers);
	*p = *m;
	(*p)->index = t->index;
	*m = NULL;

	if (st->rat_depth > 0 &&
	    st->num_timers == (1 << (st->rat_depth * SPLIT_BITS)))
		iv_timer_radix_tree_remove_level(st);
	st->num_timers--;

	if (p != m) {
		pull_up(st, (*p)->index, p);
		push_down(st, (*p)->index, p);
	}

	t->index = -1;
}

int iv_timer_registered(struct iv_timer *_t)
{
	struct iv_timer_ *t = (struct iv_timer_ *)_t;

	return !(t->index == -1);
}
