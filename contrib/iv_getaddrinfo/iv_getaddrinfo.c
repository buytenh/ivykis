/*
 * ivykis, an event handling library
 * Copyright (C) 2011 Lennert Buytenhek
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
#include <iv_tls.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "iv_getaddrinfo.h"

struct iv_getaddrinfo_thr_info {
	int			num_requests;
	struct iv_work_pool	pool;
};

static void iv_getaddrinfo_tls_init_thread(void *_tinfo)
{
	struct iv_getaddrinfo_thr_info *tinfo = _tinfo;

	tinfo->num_requests = 0;

	IV_WORK_POOL_INIT(&tinfo->pool);
	tinfo->pool.max_threads = 100;
	tinfo->pool.cookie = NULL;
	tinfo->pool.thread_start = NULL;
	tinfo->pool.thread_stop = NULL;
}

static struct iv_tls_user iv_getaddrinfo_tls_user = {
	.sizeof_state	= sizeof(struct iv_getaddrinfo_thr_info),
	.init_thread	= iv_getaddrinfo_tls_init_thread,
};

static void iv_getaddrinfo_tls_init(void) __attribute__((constructor));
static void iv_getaddrinfo_tls_init(void)
{
	iv_tls_user_register(&iv_getaddrinfo_tls_user);
}


struct iv_getaddrinfo_task {
	struct iv_getaddrinfo	*ig;

	struct iv_work_item	work;

	char			*node;
	char			*service;
	int			have_hints;
	struct addrinfo		hints;

	int			ret;
	struct addrinfo		*res;
};

static void iv_getaddrinfo_task_work(void *_igt)
{
	struct iv_getaddrinfo_task *igt = _igt;

	igt->ret = getaddrinfo(igt->node, igt->service,
			       igt->have_hints ? &igt->hints : NULL, &igt->res);
}

static void iv_getaddrinfo_task_complete(void *_igt)
{
	struct iv_getaddrinfo_task *igt = _igt;
	struct iv_getaddrinfo *ig;
	struct iv_getaddrinfo_thr_info *tinfo;

	ig = igt->ig;
	if (ig != NULL)
		ig->handler(ig->cookie, igt->ret, igt->res);
	else
		freeaddrinfo(igt->res);

	free(igt->node);
	free(igt->service);
	free(igt);

	tinfo = iv_tls_user_ptr(&iv_getaddrinfo_tls_user);
	if (!--tinfo->num_requests)
		iv_work_pool_put(&tinfo->pool);
}

int iv_getaddrinfo_submit(struct iv_getaddrinfo *ig)
{
	struct iv_getaddrinfo_task *igt;
	struct iv_getaddrinfo_thr_info *tinfo;

	igt = calloc(1, sizeof(*igt));
	if (igt == NULL)
		return -1;

	igt->ig = ig;

	IV_WORK_ITEM_INIT(&igt->work);
	igt->work.cookie = igt;
	igt->work.work = iv_getaddrinfo_task_work;
	igt->work.completion = iv_getaddrinfo_task_complete;

	if (ig->node != NULL) {
		igt->node = strdup(ig->node);
		if (igt->node == NULL) {
			free(igt);
			return -1;
		}
	}

	if (ig->service != NULL) {
		igt->service = strdup(ig->service);
		if (igt->service == NULL) {
			free(igt->node);
			free(igt);
			return -1;
		}
	}

	if (ig->hints != NULL) {
		igt->hints.ai_family = ig->hints->ai_family;
		igt->hints.ai_socktype = ig->hints->ai_socktype;
		igt->hints.ai_protocol = ig->hints->ai_protocol;
		igt->hints.ai_flags = ig->hints->ai_flags;

		igt->have_hints = 1;
	}

	tinfo = iv_tls_user_ptr(&iv_getaddrinfo_tls_user);
	if (!tinfo->num_requests++)
		iv_work_pool_create(&tinfo->pool);

	iv_work_pool_submit_work(&tinfo->pool, &igt->work);

	return 0;
}

void iv_getaddrinfo_cancel(struct iv_getaddrinfo *ig)
{
	struct iv_getaddrinfo_task *t = ig->task;

	ig->task = NULL;
	t->ig = NULL;
}
