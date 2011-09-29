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

static void iv_getaddrinfo_work(void *_ig)
{
	struct iv_getaddrinfo *ig = _ig;

	ig->ret = getaddrinfo(ig->node, ig->service, ig->hints, &ig->res);
}

static void iv_getaddrinfo_complete(void *_ig)
{
	struct iv_getaddrinfo *ig = _ig;
	struct iv_getaddrinfo_thr_info *tinfo =
		iv_tls_user_ptr(&iv_getaddrinfo_tls_user);

	if (!--tinfo->num_requests)
		iv_work_pool_put(&tinfo->pool);

	ig->handler(ig->cookie, ig->ret, ig->res);
}

void iv_getaddrinfo_submit(struct iv_getaddrinfo *ig)
{
	struct iv_getaddrinfo_thr_info *tinfo =
		iv_tls_user_ptr(&iv_getaddrinfo_tls_user);

	if (!tinfo->num_requests++)
		iv_work_pool_create(&tinfo->pool);

	IV_WORK_ITEM_INIT(&ig->work);
	ig->work.cookie = ig;
	ig->work.work = iv_getaddrinfo_work;
	ig->work.completion = iv_getaddrinfo_complete;
	iv_work_pool_submit_work(&tinfo->pool, &ig->work);
}
