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

#undef iv_validate_now


/* time handling ************************************************************/
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

const struct timespec *__iv_now_location(void)
{
	struct iv_state *st = iv_get_state();

	return &st->time;
}

const struct timespec *__iv_now_location_valid(void)
{
	struct iv_state *st = iv_get_state();

	if (!st->time_valid) {
		st->time_valid = 1;
		iv_time_get(&st->time);
	}

	return &st->time;
}


/* internal use *************************************************************/
void iv_timer_init(struct iv_state *st)
{
	st->ratnode.timer_root = &st->ratnode.first_leaf;
}

const struct timespec *iv_get_soonest_timeout(const struct iv_state *st)
{
	if (st->num_timers) {
		struct iv_timer_ *t = st->ratnode.first_leaf.child[1];

		return &t->expires;
	}

	return NULL;
}

void iv_run_timers(struct iv_state *st)
{
	struct iv_list_head timers;

	if (!st->num_timers)
		return;

	INIT_IV_LIST_HEAD(&timers);

	if (!st->time_valid) {
		st->time_valid = 1;
		iv_time_get(&st->time);
	}

	while (st->num_timers) {
		struct iv_timer_ *t = st->ratnode.first_leaf.child[1];

		if (t->index != 1) {
			iv_fatal("iv_run_timers: root timer has heap "
				 "index %d", t->index);
		}

		if (timespec_gt(&t->expires, &st->time))
			break;
		iv_timer_unregister((struct iv_timer *)t);

		iv_list_add_tail(&t->list_expired, &timers);
		t->index = 0;
	}

	while (!iv_list_empty(&timers)) {
		struct iv_timer_ *t;

		t = iv_list_entry(timers.next, struct iv_timer_, list_expired);

		iv_list_del(&t->list_expired);
		t->index = -1;

		t->handler(t->cookie);
	}
}

static void iv_timer_free_ratnode(struct iv_timer_ratnode *node, int depth)
{
	if (depth) {
		int i;

		for (i = 0; i < IV_TIMER_SPLIT_NODES; i++) {
			if (node->child[i] == NULL)
				break;
			iv_timer_free_ratnode(node->child[i], depth - 1);
		}
	}

	free(node);
}

static void iv_timer_radix_tree_remove_level(struct iv_state *st)
{
	struct iv_timer_ratnode *root;
	int i;

	st->rat_depth--;

	root = st->ratnode.timer_root;
	for (i = 1; i < IV_TIMER_SPLIT_NODES; i++) {
		if (root->child[i] == NULL)
			break;
		iv_timer_free_ratnode(root->child[i], st->rat_depth);
	}

	st->ratnode.timer_root = root->child[0];
	free(root);
}

void iv_timer_deinit(struct iv_state *st)
{
	while (st->rat_depth)
		iv_timer_radix_tree_remove_level(st);

	st->ratnode.timer_root = NULL;
}


/* public use ***************************************************************/
void IV_TIMER_INIT(struct iv_timer *_t)
{
	struct iv_timer_ *t = (struct iv_timer_ *)_t;

	t->index = -1;
}

static struct iv_timer_ratnode *iv_timer_allocate_ratnode(void)
{
	struct iv_timer_ratnode *node;

	node = calloc(1, sizeof(struct iv_timer_ratnode));
	if (node == NULL)
		iv_fatal("iv_timer_allocate_ratnode: out of memory");

	return node;
}

static struct iv_timer_ **iv_timer_get_node(struct iv_state *st, int index)
{
	struct iv_timer_ratnode *r;
	int i;

	if (index >> ((st->rat_depth + 1) * IV_TIMER_SPLIT_BITS) != 0) {
		st->rat_depth++;

		r = iv_timer_allocate_ratnode();
		r->child[0] = st->ratnode.timer_root;
		st->ratnode.timer_root = r;
	}

	r = st->ratnode.timer_root;
	for (i = st->rat_depth; i > 0; i--) {
		int bits;

		bits = (index >> (i * IV_TIMER_SPLIT_BITS)) &
					(IV_TIMER_SPLIT_NODES - 1);
		if (r->child[bits] == NULL)
			r->child[bits] = iv_timer_allocate_ratnode();
		r = r->child[bits];
	}

	return (struct iv_timer_ **)
		(r->child + (index & (IV_TIMER_SPLIT_NODES - 1)));
}

static inline int
timer_ptr_gt(const struct iv_timer_ *a, const struct iv_timer_ *b)
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

	if (t->index) {
		if (t->index > st->num_timers) {
			iv_fatal("iv_timer_unregister: timer index %d > %d",
				 t->index, st->num_timers);
		}

		p = iv_timer_get_node(st, t->index);
		if (*p != t) {
			iv_fatal("iv_timer_unregister: unregistered timer "
				 "index belonging to other timer");
		}

		m = iv_timer_get_node(st, st->num_timers);
		*p = *m;
		(*p)->index = t->index;
		*m = NULL;

		if (st->rat_depth > 0 &&
		    st->num_timers == (1 << (st->rat_depth *
					     IV_TIMER_SPLIT_BITS))) {
			iv_timer_radix_tree_remove_level(st);
		}
		st->num_timers--;

		if (p != m) {
			pull_up(st, (*p)->index, p);
			push_down(st, (*p)->index, p);
		}

		st->numobjs--;
	} else {
		iv_list_del(&t->list_expired);
	}

	t->index = -1;
}

int iv_timer_registered(const struct iv_timer *_t)
{
	struct iv_timer_ *t = (struct iv_timer_ *)_t;

	return !(t->index == -1);
}
