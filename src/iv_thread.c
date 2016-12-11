/*
 * ivykis, an event handling library
 * Copyright (C) 2010, 2012, 2013 Lennert Buytenhek
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
#include <iv_event.h>
#include <iv_thread.h>
#include <iv_tls.h>
#include <string.h>
#include "iv_private.h"
#include "iv_thread_private.h"

___mutex_t iv_thread_lock;
struct iv_list_head iv_thread_roots;
int iv_thread_debug;

void iv_thread_set_debug_state(int state)
{
	iv_thread_debug = !!state;
}

unsigned long iv_thread_get_id(void)
{
	return iv_get_thread_id();
}

void iv_thread_set_name(const char *name)
{
	struct iv_thread *me;

	me = iv_thread_get_self();
	if (me == NULL)
		return;

	mutex_lock(&iv_thread_lock);

	if (iv_thread_debug) {
		fprintf(stderr, "iv_thread: [" TID_FORMAT ":%s] renaming "
				"to [%s]\n", me->tid, me->name, name);
	}

	free(me->name);
	me->name = strdup(name);

	mutex_unlock(&iv_thread_lock);
}

static void
__iv_thread_dump_thread(FILE *fp, const struct iv_thread *thr, int level)
{
	int i;
	struct iv_list_head *ilh;

	fprintf(fp, "| ");
	if (level) {
		for (i = 0; i < level - 1; i++)
			fprintf(fp, "  ");
		fprintf(fp, "- ");
	}
	fprintf(fp, TID_FORMAT ": %s", thr->tid, thr->name);
	if (iv_thread_dead(thr))
		fprintf(fp, " (dead)");
	fprintf(fp, "\n");

	iv_list_for_each (ilh, &thr->children) {
		struct iv_thread *child;

		child = iv_container_of(ilh, struct iv_thread, list);
		__iv_thread_dump_thread(fp, child, level + 1);
	}
}

void iv_thread_list_children(void)
{
	struct iv_list_head *ilh;

	___mutex_lock(&iv_thread_lock);

	fprintf(stderr, "+---- threads\n");

	iv_list_for_each (ilh, &iv_thread_roots) {
		struct iv_thread *thr;

		thr = iv_container_of(ilh, struct iv_thread, list);
		__iv_thread_dump_thread(stderr, thr, 0);
	}

	fprintf(stderr, "+----\n");

	___mutex_unlock(&iv_thread_lock);
}
