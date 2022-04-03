/*
 * ivykis, an event handling library
 * Copyright (C) 2010 Lennert Buytenhek
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

#ifndef __IV_WORK_H
#define __IV_WORK_H

#include <iv.h>
#include <iv_list.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_work_pool {
	int		max_threads;
	void		*cookie;
	void		(*thread_start)(void *cookie);
	void		(*thread_stop)(void *cookie);

	void		*priv;
};

struct iv_work_item {
	void			*cookie;
	void			(*work)(void *cookie);
	void			(*completion)(void *cookie);

	struct iv_list_head	list;
};

static inline void IV_WORK_POOL_INIT(struct iv_work_pool *this)
{
	this->thread_start = NULL;
	this->thread_stop = NULL;
}

static inline void IV_WORK_ITEM_INIT(struct iv_work_item *this)
{
}

int iv_work_pool_create(struct iv_work_pool *this);
void iv_work_pool_put(struct iv_work_pool *this);
void iv_work_pool_submit_work(struct iv_work_pool *this,
			      struct iv_work_item *work);
void iv_work_pool_submit_continuation(struct iv_work_pool *this,
                                      struct iv_work_item *work);

#ifdef __cplusplus
}
#endif


#endif
