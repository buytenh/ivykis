/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include "iv_private.h"

static struct list_head tasks;


void iv_register_task(struct iv_task *t)
{
#if IV_DEBUG
	if (!list_empty(&(t->list))) {
		syslog(LOG_CRIT, "iv_register_task: called with task still "
				 "on a list");
		abort();
	}
#endif

	list_add_tail(&(t->list), &tasks);
}

void iv_unregister_task(struct iv_task *t)
{
#if IV_DEBUG
	if (list_empty(&(t->list))) {
		syslog(LOG_CRIT, "iv_unregister_task: called with task not "
				 "on a list");
		abort();
	}
#endif

	list_del(&(t->list));
#if IV_DEBUG
	INIT_LIST_HEAD(&(t->list));
#endif
}

int iv_pending_tasks(void)
{
	return !list_empty(&tasks);
}

void iv_run_tasks(void)
{
	while (!list_empty(&tasks)) {
		struct list_head *lh = tasks.next;
		struct iv_task *t = list_entry(lh, struct iv_task, list);

		list_del(&(t->list));
#if IV_DEBUG
		INIT_LIST_HEAD(&(t->list));
#endif
		t->handler(t->cookie);
	}
}


void iv_task_init(void)
{
	INIT_LIST_HEAD(&tasks);
}
