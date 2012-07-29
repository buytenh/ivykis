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
 * Per-thread state.
 */
#define NEED_SELECT

struct iv_state {
	/* iv_main.c  */
	struct iv_fd_		*handled_fd;
	int			numfds;
	int			quit;

	/* iv_task.c  */
	struct iv_list_head	tasks;

	/* iv_timer.c  */
	struct timespec		time;
	int			time_valid;
	int			num_timers;
	struct ratnode		*timer_root;

	/* poll methods  */
	union {
#ifdef HAVE_SYS_DEVPOLL_H
#undef NEED_SELECT
		struct {
			struct iv_avl_tree	fds;
			int			poll_fd;
			struct iv_list_head	notify;
		} dev_poll;
#endif

#ifdef HAVE_EPOLL_CREATE
#undef NEED_SELECT
		struct {
			int			epoll_fd;
			struct iv_list_head	notify;
		} epoll;
#endif

#ifdef HAVE_KQUEUE
#undef NEED_SELECT
		struct {
			int			kqueue_fd;
			struct iv_list_head	notify;
		} kqueue;
#endif

#ifdef HAVE_POLL
#undef NEED_SELECT
		struct {
			struct pollfd		*pfds;
			struct iv_fd_		**fds;
			int			num_regd_fds;
		} poll;
#endif

#ifdef HAVE_PORT_CREATE
#undef NEED_SELECT
		struct {
			int			port_fd;
			struct iv_list_head	notify;
		} port;
#endif

#ifdef NEED_SELECT
		struct {
			struct iv_avl_tree	fds;
			void			*sets;
			int			setsize;
			int			fd_max;
		} select;
#endif
	} u;
};

#ifdef HAVE_THREAD
extern __thread struct iv_state *__st;

static inline struct iv_state *iv_get_state(void)
{
	return __st;
}
#else
#include <pthread.h>

extern pthread_key_t iv_state_key;

static inline struct iv_state *iv_get_state(void)
{
        return pthread_getspecific(iv_state_key);
}
#endif

static inline void barrier(void)
{
	__asm__ __volatile__("" : : : "memory");
}


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
	 * ->avl_node is used by poll methods that maintain an
	 * internal fd tree, and ->index is used by iv_method_poll
	 * to maintain the index of this fd in the list of pollfds.
	 */
	union {
#if defined(HAVE_SYS_DEVPOLL_H) || defined(NEED_SELECT)
		struct iv_avl_node	avl_node;
#endif
#ifdef HAVE_POLL
		int			index;
#endif
	} u;
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
	struct iv_list_head	list;
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
	int	(*init)(struct iv_state *st);
	void	(*poll)(struct iv_state *st, 
			struct iv_list_head *active, struct timespec *to);
	void	(*register_fd)(struct iv_state *st, struct iv_fd_ *fd);
	void	(*unregister_fd)(struct iv_state *st, struct iv_fd_ *fd);
	void	(*notify_fd)(struct iv_state *st, struct iv_fd_ *fd);
	int	(*notify_fd_sync)(struct iv_state *st, struct iv_fd_ *fd);
	void	(*deinit)(struct iv_state *st);
};

static inline void
__iv_list_steal_elements(struct iv_list_head *oldh, struct iv_list_head *newh)
{
	struct iv_list_head *first = oldh->next;
	struct iv_list_head *last = oldh->prev;

	last->next = newh;
	first->prev = newh;

	newh->next = oldh->next;
	newh->prev = oldh->prev;

	oldh->next = oldh;
	oldh->prev = oldh;
}

/* iv_main.c */
extern int maxfd;
extern struct iv_poll_method *method;

/* poll methods */
extern struct iv_poll_method iv_method_dev_poll;
extern struct iv_poll_method iv_method_epoll;
extern struct iv_poll_method iv_method_kqueue;
extern struct iv_poll_method iv_method_poll;
extern struct iv_poll_method iv_method_port;
extern struct iv_poll_method iv_method_select;


/* iv_fd.c */
struct iv_fd_ *iv_fd_avl_find(struct iv_avl_tree *root, int fd);
int iv_fd_avl_compare(struct iv_avl_node *_a, struct iv_avl_node *_b);
void iv_fd_make_ready(struct iv_list_head *active,
		      struct iv_fd_ *fd, int bands);

/* iv_task.c */
void iv_task_init(struct iv_state *st);
int iv_pending_tasks(struct iv_state *st);
void iv_run_tasks(struct iv_state *st);

/* iv_timer.c */
void __iv_invalidate_now(struct iv_state *st);
void iv_timer_init(struct iv_state *st);
int iv_pending_timers(struct iv_state *st);
int iv_get_soonest_timeout(struct iv_state *st, struct timespec *to);
void iv_run_timers(struct iv_state *st);
void iv_timer_deinit(struct iv_state *st);

/* iv_tls.c */
int iv_tls_total_state_size(void);
void iv_tls_thread_init(struct iv_state *st);
void iv_tls_thread_deinit(struct iv_state *st);
