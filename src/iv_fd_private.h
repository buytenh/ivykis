/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009, 2012 Lennert Buytenhek
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

#define MASKIN		1
#define MASKOUT		2
#define MASKERR		4

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
	struct iv_list_head	list_active;
	unsigned		ready_bands:3;

	/*
	 * Reflects whether the fd has been registered with
	 * iv_fd_register().  Will be zero in ->notify_fd() if the
	 * fd is being unregistered.
	 */
	unsigned		registered:1;

	/*
	 * ->wanted_bands is set by the ivykis core to indicate
	 * which bands currenty have handlers registered for them.
	 */
	unsigned		wanted_bands:3;

	/*
	 * ->registered_bands is maintained by the poll method to
	 * indicate which bands are currently registered with the
	 * kernel, so that the ivykis core knows when to call
	 * the poll method's ->notify_fd() on an fd.
	 */
	unsigned		registered_bands:3;

#if defined(HAVE_SYS_DEVPOLL_H) || defined(HAVE_EPOLL_CREATE) ||	\
    defined(HAVE_KQUEUE) || defined(HAVE_PORT_CREATE)
	/*
	 * ->list_notify is used by poll methods that defer updating
	 * kernel registrations to ->poll() time.
	 */
	struct iv_list_head	list_notify;
#endif

	/*
	 * This is for state internal to some of the poll methods:
	 * ->avl_node is used by the /dev/poll method to maintain an
	 * internal fd tree, and ->index is used by iv_fd_poll to
	 * maintain the index of this fd in the list of pollfds.
	 */
	union {
#ifdef HAVE_SYS_DEVPOLL_H
		struct iv_avl_node	avl_node;
#endif
		int			index;
	} u;
};

struct iv_fd_poll_method {
	char	*name;
	int	(*init)(struct iv_state *st);
	void	(*poll)(struct iv_state *st,
			struct iv_list_head *active, struct timespec *to);
	void	(*register_fd)(struct iv_state *st, struct iv_fd_ *fd);
	void	(*unregister_fd)(struct iv_state *st, struct iv_fd_ *fd);
	void	(*notify_fd)(struct iv_state *st, struct iv_fd_ *fd);
	int	(*notify_fd_sync)(struct iv_state *st, struct iv_fd_ *fd);
	void	(*deinit)(struct iv_state *st);
	int	(*event_rx_on)(struct iv_state *st);
	void	(*event_rx_off)(struct iv_state *st);
	void	(*event_send)(struct iv_state *dest);
};

extern int maxfd;
extern struct iv_fd_poll_method *method;

extern struct iv_fd_poll_method iv_fd_poll_method_dev_poll;
extern struct iv_fd_poll_method iv_fd_poll_method_epoll;
extern struct iv_fd_poll_method iv_fd_poll_method_kqueue;
extern struct iv_fd_poll_method iv_fd_poll_method_poll;
extern struct iv_fd_poll_method iv_fd_poll_method_port;

/* iv_event_posix.c */
void iv_event_run_pending_events(void);

/* iv_fd.c */
void iv_fd_make_ready(struct iv_list_head *active,
		      struct iv_fd_ *fd, int bands);
void iv_fd_set_cloexec(int fd);
void iv_fd_set_nonblock(int fd);
