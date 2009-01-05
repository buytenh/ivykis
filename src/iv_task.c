/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include "iv_private.h"

static struct list_head tasks;

void iv_task_init(void)
{
	INIT_LIST_HEAD(&tasks);
}

int iv_pending_tasks(void)
{
	return !list_empty(&tasks);
}

void iv_run_tasks(void)
{
	while (!list_empty(&tasks)) {
		struct iv_task_ *t;

		t = list_entry(tasks.next, struct iv_task_, list);
		list_del_init(&t->list);

		t->handler(t->cookie);
	}
}

void INIT_IV_TASK(struct iv_task *_t)
{
	struct iv_task_ *t = (struct iv_task_ *)_t;

	INIT_LIST_HEAD(&t->list);
}

void iv_register_task(struct iv_task *_t)
{
	struct iv_task_ *t = (struct iv_task_ *)_t;

	if (!list_empty(&t->list)) {
		syslog(LOG_CRIT, "iv_register_task: called with task still "
				 "on a list");
		abort();
	}

	list_add_tail(&t->list, &tasks);
}

void iv_unregister_task(struct iv_task *_t)
{
	struct iv_task_ *t = (struct iv_task_ *)_t;

	if (list_empty(&t->list)) {
		syslog(LOG_CRIT, "iv_unregister_task: called with task not "
				 "on a list");
		abort();
	}

	list_del_init(&t->list);
}
