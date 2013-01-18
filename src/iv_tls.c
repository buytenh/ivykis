/*
 * ivykis, an event handling library
 * Copyright (C) 2011 Lennert Buytenhek
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
#include <iv_list.h>
#include <iv_tls.h>
#include "iv_private.h"

static int inited;
static int last_offset = (sizeof(struct iv_state) + 15) & ~15;
static struct iv_list_head iv_tls_users = IV_LIST_HEAD_INIT(iv_tls_users);

void iv_tls_user_register(struct iv_tls_user *itu)
{
	if (inited)
		iv_fatal("iv_tls_user_register: called after iv_init");

	itu->state_offset = last_offset;
	last_offset = (last_offset + itu->sizeof_state + 15) & ~15;

	iv_list_add_tail(&itu->list, &iv_tls_users);
}

int iv_tls_total_state_size(void)
{
	return last_offset;
}

void iv_tls_thread_init(struct iv_state *st)
{
	struct iv_list_head *ilh;

	inited = 1;

	iv_list_for_each (ilh, &iv_tls_users) {
		struct iv_tls_user *itu;

		itu = iv_container_of(ilh, struct iv_tls_user, list);
		if (itu->init_thread != NULL)
			itu->init_thread(((void *)st) + itu->state_offset);
	}
}

void iv_tls_thread_deinit(struct iv_state *st)
{
	struct iv_list_head *ilh;

	iv_list_for_each (ilh, &iv_tls_users) {
		struct iv_tls_user *itu;

		itu = iv_container_of(ilh, struct iv_tls_user, list);
		if (itu->deinit_thread != NULL)
			itu->deinit_thread(((void *)st) + itu->state_offset);
	}
}

void *
__iv_tls_user_ptr(const struct iv_state *st, const struct iv_tls_user *itu)
{
	if (itu->state_offset == 0)
		iv_fatal("iv_tls_user_ptr: called on unregistered iv_tls_user");

	if (st != NULL)
		return ((void *)st) + itu->state_offset;

	return NULL;
}

void *iv_tls_user_ptr(const struct iv_tls_user *itu)
{
	struct iv_state *st = iv_get_state();

	return __iv_tls_user_ptr(st, itu);
}
