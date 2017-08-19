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
#include "iv_private.h"

void iv_task_init(struct iv_state *st)
{
	INIT_IV_LIST_HEAD(&st->tasks);
}

void iv_run_tasks(struct iv_state *st)
{
	struct iv_list_head tasks;
	uint32_t epoch;

	__iv_list_steal_elements(&st->tasks, &tasks);
	epoch = ++st->task_epoch;

	st->tasks_current = &tasks;
	while (!iv_list_empty(&tasks)) {
		struct iv_task_ *t;

		t = iv_list_entry(tasks.next, struct iv_task_, list);
		iv_list_del_init(&t->list);

		st->numobjs--;

		t->epoch = epoch;
		t->handler(t->cookie);
	}
	st->tasks_current = NULL;
}

void IV_TASK_INIT(struct iv_task *_t)
{
	struct iv_state *st = iv_get_state();
	struct iv_task_ *t = (struct iv_task_ *)_t;

	INIT_IV_LIST_HEAD(&t->list);
	t->epoch = (st != NULL) ? st->task_epoch : 0;
}

void iv_task_register(struct iv_task *_t)
{
	struct iv_state *st = iv_get_state();
	struct iv_task_ *t = (struct iv_task_ *)_t;

	if (!iv_list_empty(&t->list))
		iv_fatal("iv_task_register: called with task still on a list");

	st->numobjs++;
	if (st->tasks_current == NULL || t->epoch == st->task_epoch)
		iv_list_add_tail(&t->list, &st->tasks);
	else
		iv_list_add_tail(&t->list, st->tasks_current);
}

void iv_task_unregister(struct iv_task *_t)
{
	struct iv_state *st = iv_get_state();
	struct iv_task_ *t = (struct iv_task_ *)_t;

	if (iv_list_empty(&t->list))
		iv_fatal("iv_task_unregister: called with task not on a list");

	st->numobjs--;
	iv_list_del_init(&t->list);
}

int iv_task_registered(const struct iv_task *_t)
{
	struct iv_task_ *t = (struct iv_task_ *)_t;

	return !iv_list_empty(&t->list);
}
