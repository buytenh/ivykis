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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "iv.h"
#include "iv_list.h"

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
	 * Private data.
	 */
	struct list_head	list_all;
	struct list_head	list_active;
	unsigned int		quotum:8,
				flags:8,
				epoch:16;
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
	struct timespec	expires;
	void		*cookie;
	void		(*handler)(void *);

	/*
	 * Private data.
	 */
	int		index;
};


/*
 * Misc internal stuff.
 */
#define FD_ReadyIn		0
#define FD_ReadyOut		1
#define FD_ReadyErr		2
#define FD_ReadyMask		((1 << FD_ReadyIn) | (1 << FD_ReadyOut) | (1 << FD_ReadyErr))
#define FD_RegisteredIn		3
#define FD_RegisteredOut	4
#define FD_RegisteredErr	5

struct iv_poll_method {
	char	*name;
	int	(*init)(int maxfd);
	void	(*poll)(int msec);
	void	(*register_fd)(struct iv_fd_ *fd);
	void	(*reregister_fd)(struct iv_fd_ *fd);
	void	(*unregister_fd)(struct iv_fd_ *fd);
	void	(*deinit)(void);
};

extern struct iv_poll_method iv_method_dev_poll;
extern struct iv_poll_method iv_method_epoll;
extern struct iv_poll_method iv_method_kqueue;
extern struct iv_poll_method iv_method_poll;
extern struct iv_poll_method iv_method_select;

extern struct list_head *active;


/* iv_main.c */
void iv_fd_make_ready(struct iv_fd_ *fd, int band);

/* iv_task.c */
void iv_task_init(void);
int iv_pending_tasks(void);
void iv_run_tasks(void);

/* iv_timer.c */
void iv_timer_init(void);
int iv_pending_timers(void);
int iv_get_soonest_timeout(struct timespec *to);
void iv_run_timers(void);
