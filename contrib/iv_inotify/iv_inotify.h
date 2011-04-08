/*
 * iv_inotify, an ivykis inotify component.
 *
 * Dedicated to Kanna Ishihara.
 *
 * Copyright (C) 2008, 2009 Ronald Huizer
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

#ifndef __IV_INOTIFY_H
#define __IV_INOTIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <sys/inotify.h>
#include <iv.h>
#include <iv_list.h>

#define IV_INOTIFY_HASH_SIZE	512

struct iv_inotify_watch {
	const char		*pathname;
	uint32_t		mask;
	void			(*handler)(struct inotify_event *, void *);
	void			*cookie;

	/* Read-only members */
	int			wd;

	/* Private members; internal use only */
	struct list_head	list_hash;
};

struct iv_inotify {
	struct iv_fd		fd;
	struct list_head	hash_chains[IV_INOTIFY_HASH_SIZE];
	size_t			watches;
};

int iv_inotify_init(struct iv_inotify *);
void iv_inotify_destroy(struct iv_inotify *);
int iv_inotify_add_watch(struct iv_inotify *, struct iv_inotify_watch *);
int iv_inotify_rm_watch(struct iv_inotify *, struct iv_inotify_watch *);

#ifdef __cplusplus
}
#endif


#endif
