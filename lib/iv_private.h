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

#include "iv.h"
#include "iv_avl.h"
#include "iv_list.h"
#include "config.h"

/*
 * Private versions of the fd/task/timer structures, exposing their
 * internal state.  The user data fields of these structures MUST
 * match the definitions in the public header file iv.h.
 */
struct iv_fd_ {
	/*
	 * User data.
	 */
	int			fd;
	void			*cookie;
	void			(*handler_in)(void *);
	void			(*handler_out)(void *);
	void			(*handler_err)(void *);

	/*
	 * If this fd gathered any events during this polling round,
	 * fd->list_active will be on iv_main()'s active list, and
	 * fd->ready_bands will indicate which bands are currently
	 * active.
	 */
	struct list_head	list_active;
	unsigned		ready_bands:3;

	/*
	 * Reflects whether the fd has been registered with
	 * iv_fd_register().  Will be zero in ->notify_fd() if the
	 * fd is being unregistered.
	 */
	unsigned		registered:1;

	/*
	 * ->registered_bands is maintained by the poll method to
	 * indicate which bands are currently registered with the
	 * kernel, so that the ivykis core knows when to call
	 * the poll method's ->notify_fd() on an fd.
	 */
	unsigned		registered_bands:3;

	/*
	 * This is for state internal to some of the poll methods:
	 * ->avl_node is used by poll methods that maintain an
	 * internal fd tree, and ->index is used by iv_method_poll
	 * to maintain the index of this fd in the list of pollfds.
	 */
	union {
		struct iv_avl_node	avl_node;
		int			index;
	};
};

struct iv_task_ {
	/*
	 * User data.
	 */
	void			*cookie;
	void			(*handler)(void *);

	/*
	 * Private data.
	 */
	struct list_head	list;
};

struct iv_timer_ {
	/*
	 * User data.
	 */
	struct timespec		expires;
	void			*cookie;
	void			(*handler)(void *);

	/*
	 * Private data.
	 */
	int			index;
};


/*
 * Misc internal stuff.
 */
#define MASKIN		1
#define MASKOUT		2
#define MASKERR		4

struct iv_poll_method {
	char	*name;
	int	(*init)(int maxfd);
	void	(*poll)(int numfds, struct list_head *active, int msec);
	void	(*register_fd)(struct iv_fd_ *fd);
	void	(*unregister_fd)(struct iv_fd_ *fd);
	void	(*notify_fd)(struct iv_fd_ *fd, int wanted_bands);
	int     (*pollable)(int fd);
	void	(*deinit)(void);
};

extern struct iv_poll_method iv_method_dev_poll;
extern struct iv_poll_method iv_method_epoll;
extern struct iv_poll_method iv_method_kqueue;
extern struct iv_poll_method iv_method_poll;
extern struct iv_poll_method iv_method_select;


/* iv_main.c */
void iv_fd_make_ready(struct list_head *active, struct iv_fd_ *fd, int bands);

/* iv_task.c */
void iv_task_init(void);
int iv_pending_tasks(void);
void iv_run_tasks(void);

/* iv_timer.c */
void iv_timer_init(void);
int iv_pending_timers(void);
int iv_get_soonest_timeout(struct timespec *to);
void iv_run_timers(void);
void iv_timer_deinit(void);
