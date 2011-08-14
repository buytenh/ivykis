/*
 * ivykis, an event handling library
 * Copyright (C) 2008, 2009 Ronald Huizer
 * Dedicated to Kanna Ishihara.
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

#include <inttypes.h>
#include <iv.h>
#include <iv_avl.h>
#include <sys/inotify.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_inotify {
	/* No public members.  */

	struct iv_fd		fd;
	struct iv_avl_tree	watches;
	void			**term;
};

static inline void IV_INOTIFY_INIT(struct iv_inotify *this)
{
}

int iv_inotify_register(struct iv_inotify *);
void iv_inotify_unregister(struct iv_inotify *);


struct iv_inotify_watch {
	struct iv_inotify	*inotify;
	const char		*pathname;
	uint32_t		mask;
	void			*cookie;
	void			(*handler)(void *, struct inotify_event *);

	int			wd;
	struct iv_avl_node	an;
};

static inline void IV_INOTIFY_WATCH_INIT(struct iv_inotify_watch *this)
{
}

int iv_inotify_watch_register(struct iv_inotify_watch *);
void iv_inotify_watch_unregister(struct iv_inotify_watch *);

#ifdef __cplusplus
}
#endif


#endif
