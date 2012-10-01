/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009, 2012, 2013 Lennert Buytenhek
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
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include "iv_private.h"
#include "config.h"

#if MAX_POLL_FDS == 7
 #define OKAY_MIN	4
 #define OKAY_MAX	6
#elif MAX_POLL_FDS == 31
 #define OKAY_MIN	11
 #define OKAY_MAX	21
#else
 #error invalid MAX_POLL_FDS
#endif

static int iv_mt_group_key_allocated;
static pthread_key_t iv_mt_group_key;

static int iv_fd_poll_mt_init(struct iv_state *st)
{
	struct iv_mt_group *me;

	if (!iv_mt_group_key_allocated) {
		if (pthread_key_create(&iv_mt_group_key, NULL))
			return -1;

		iv_mt_group_key_allocated = 1;
	}

	pthread_mutex_init(&st->u.poll_mt.state_lock, NULL);
	pthread_cond_init(&st->u.poll_mt.groups_aux_empty, NULL);
	INIT_IV_LIST_HEAD(&st->u.poll_mt.groups_aux);
	st->u.poll_mt.notokay = NULL;
	pthread_cond_init(&st->u.poll_mt.exec_lock_free, NULL);
	st->u.poll_mt.exec_lock = 1;
	st->u.poll_mt.event_rx_active = 0;

	me = &st->u.poll_mt.group_main;
	pthread_setspecific(iv_mt_group_key, me);

	INIT_IV_LIST_HEAD(&me->list_aux);
	me->st = st;
	me->upd_pipe[0] = -1;
	me->upd_pipe[1] = -1;
	me->update_occured = 0;
	me->num_fds = 0;

	return 0;
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = 0;
	if (bits & MASKIN)
		mask |= POLLIN | POLLHUP;
	if (bits & MASKOUT)
		mask |= POLLOUT | POLLHUP;
	if (bits & MASKERR)
		mask |= POLLHUP;

	return mask;
}

static void
__iv_fd_poll_mt_update_pollfds(struct iv_mt_group *me, struct pollfd *pfds)
{
	int i;

	for (i = 0; i < me->num_fds; i++) {
		struct iv_fd_ *fd;

		fd = me->fds[i];
		pfds[i].fd = fd->fd;
		pfds[i].events = bits_to_poll_mask(fd->wanted_bands);
	}
}

static void iv_fd_poll_mt_consume_control_event(struct iv_mt_group *me)
{
	char buf[1024];
	int ret;

	ret = read(me->upd_pipe[0], buf, sizeof(buf));

	if (ret == 0) {
		iv_fatal("iv_fd_poll_mt_consume_control_event: "
			 "reading from event fd returned zero");
	}

	if (ret < 0 && errno != EAGAIN && errno != EINTR) {
		iv_fatal("iv_fd_poll_mt_consume_control_event: "
			 "reading from event fd returned error %d[%s]",
			 errno, strerror(errno));
	}
}

static void __iv_fd_poll_mt_activate_fds(struct iv_mt_group *me,
					 struct pollfd *pfds,
					 struct iv_list_head *active)
{
	int i;

	for (i = 0; i < me->num_fds; i++) {
		struct iv_fd_ *fd;
		int revents;

		fd = me->fds[i];
		revents = pfds[i].revents;

		if (revents & (POLLIN | POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKIN);

		if (revents & (POLLOUT | POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKOUT);

		if (revents & (POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKERR);
	}
}

static void
iv_fd_poll_mt_poll(struct iv_state *st, struct iv_list_head *active,
		   struct timespec *abs)
{
	struct iv_mt_group *me = &st->u.poll_mt.group_main;
	int num_fds;
	int msec;
	int ret;
	int run_events;

	if (me->update_occured) {
		me->update_occured = 0;
		__iv_fd_poll_mt_update_pollfds(me, st->u.poll_mt.pfds + 1);
	}
	num_fds = me->num_fds;

	msec = to_msec(st, abs);

	pthread_mutex_lock(&st->u.poll_mt.state_lock);

	if (!st->u.poll_mt.exec_lock)
		iv_fatal("iv_fd_poll_mt_poll: releasing exec lock but zero");
	st->u.poll_mt.exec_lock = 0;

	pthread_cond_signal(&st->u.poll_mt.exec_lock_free);

	pthread_mutex_unlock(&st->u.poll_mt.state_lock);

#if _AIX
	/*
	 * AIX sometimes leaves errno uninitialized even if poll
	 * returns -1.
	 */
	errno = EINTR;
#endif

	if (me->upd_pipe[0] != -1) {
		ret = poll(st->u.poll_mt.pfds, num_fds + 1, msec);
	} else {
		st->u.poll_mt.pfds[0].revents = 0;
		ret = poll(st->u.poll_mt.pfds + 1, num_fds, msec);
	}

	if (ret > 0 && st->u.poll_mt.pfds[0].revents & POLLIN)
		iv_fd_poll_mt_consume_control_event(me);

	pthread_mutex_lock(&st->u.poll_mt.state_lock);

	while (st->u.poll_mt.exec_lock) {
		pthread_cond_wait(&st->u.poll_mt.exec_lock_free,
				  &st->u.poll_mt.state_lock);
	}

	if (st->u.poll_mt.exec_lock)
		iv_fatal("iv_fd_poll_mt_poll: taking exec lock but set");
	st->u.poll_mt.exec_lock = 1;

	__iv_invalidate_now(st);

	if (ret < 0) {
		if (errno == EINTR) {
			pthread_mutex_unlock(&st->u.poll_mt.state_lock);
			return;
		}

		iv_fatal("iv_fd_poll_mt_poll: got error %d[%s]", errno,
			 strerror(errno));
	}

	if (!me->update_occured) {
		__iv_fd_poll_mt_activate_fds(me, st->u.poll_mt.pfds + 1,
					     active);
	}

	run_events = st->u.poll_mt.event_rx_active;
	st->u.poll_mt.event_rx_active = 0;

	pthread_mutex_unlock(&st->u.poll_mt.state_lock);

	if (run_events)
		iv_event_run_pending_events();
}

static void iv_fd_poll_mt_register_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	fd->u.poll_mt.grp = NULL;
	fd->u.poll_mt.index = -1;
}

static void iv_fd_poll_mt_destroy_upd_pipe(struct iv_mt_group *grp)
{
	close(grp->upd_pipe[0]);
	close(grp->upd_pipe[1]);
}

static void *iv_fd_poll_mt_poll_thread(void *arg)
{
	struct iv_mt_group *me = arg;
	struct iv_state *st = me->st;
	struct pollfd pfds[MAX_POLL_FDS + 1];
	int msec;

	pthread_setspecific(iv_state_key, st);
#ifdef HAVE_THREAD
	__st = st;
#endif
	pthread_setspecific(iv_mt_group_key, me);

	pfds[0].fd = me->upd_pipe[0];
	pfds[0].events = POLLIN | POLLHUP;
	msec = -1;

	pthread_mutex_lock(&st->u.poll_mt.state_lock);

	while (me->num_fds) {
		int num_fds;
		int ret;
		int run_events;
		struct iv_list_head active;

		if (me->update_occured) {
			me->update_occured = 0;
			__iv_fd_poll_mt_update_pollfds(me, pfds + 1);
		}

		num_fds = me->num_fds;

		pthread_mutex_unlock(&st->u.poll_mt.state_lock);

#if _AIX
		/*
		 * AIX sometimes leaves errno uninitialized even if poll
		 * returns -1.
		 */
		errno = EINTR;
#endif

		ret = poll(pfds, num_fds + 1, msec);

		if (ret > 0 && pfds[0].revents & POLLIN)
			iv_fd_poll_mt_consume_control_event(me);

		pthread_mutex_lock(&st->u.poll_mt.state_lock);

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			iv_fatal("iv_fd_poll_mt_poll_thread: got error "
				 "%d[%s]", errno, strerror(errno));
		}

		while (!me->update_occured && st->u.poll_mt.exec_lock) {
			pthread_cond_wait(&st->u.poll_mt.exec_lock_free,
					  &st->u.poll_mt.state_lock);
		}

		if (me->update_occured) {
			pthread_cond_signal(&st->u.poll_mt.exec_lock_free);
			continue;
		}

		if (st->u.poll_mt.exec_lock) {
			iv_fatal("iv_fd_poll_mt_poll_thread: taking "
				 "exec lock but set");
		}
		st->u.poll_mt.exec_lock = 1;

		run_events = st->u.poll_mt.event_rx_active;
		st->u.poll_mt.event_rx_active = 0;

		pthread_mutex_unlock(&st->u.poll_mt.state_lock);

		__iv_invalidate_now(st);

		INIT_IV_LIST_HEAD(&active);
		__iv_fd_poll_mt_activate_fds(me, pfds + 1, &active);
		if (run_events)
			iv_event_run_pending_events();
		iv_fd_run_active_list(st, &active);
		iv_run_timers(st);
		iv_run_tasks(st);

		if (!iv_pending_tasks(st))
			msec = to_msec(st, iv_get_soonest_timeout(st));
		else
			msec = 0;

		pthread_mutex_lock(&st->u.poll_mt.state_lock);

		if (!st->u.poll_mt.exec_lock) {
			iv_fatal("iv_fd_poll_mt_poll_thread: releasing "
				 "exec lock but zero");
		}
		st->u.poll_mt.exec_lock = 0;

		pthread_cond_signal(&st->u.poll_mt.exec_lock_free);

		if (me->num_fds && (st->quit || !st->numobjs))
			write(st->u.poll_mt.group_main.upd_pipe[1], "", 1);
	}

	iv_list_del(&me->list_aux);
	if (iv_list_empty(&st->u.poll_mt.groups_aux))
		pthread_cond_signal(&st->u.poll_mt.groups_aux_empty);

	write(st->u.poll_mt.group_main.upd_pipe[1], "", 1);

	pthread_mutex_unlock(&st->u.poll_mt.state_lock);

	iv_fd_poll_mt_destroy_upd_pipe(me);
	free(me);

	pthread_setspecific(iv_state_key, NULL);

	return NULL;
}

static void iv_fd_poll_mt_create_upd_pipe(struct iv_mt_group *grp)
{
	int fd[2];

	if (pipe(fd) < 0) {
		iv_fatal("iv_fd_poll_mt_create_upd_pipe: got error %d[%s]",
			 errno, strerror(errno));
	}

	iv_fd_set_cloexec(fd[0]);
	iv_fd_set_nonblock(fd[0]);
	iv_fd_set_cloexec(fd[1]);
	iv_fd_set_nonblock(fd[1]);

	grp->upd_pipe[0] = fd[0];
	grp->upd_pipe[1] = fd[1];
}

static void
__iv_fd_poll_mt_group_update(struct iv_mt_group *me, struct iv_mt_group *grp)
{
	if (!grp->update_occured) {
		if (grp != me)
			write(grp->upd_pipe[1], "", 1);
		grp->update_occured = 1;
	}
}

static void
__iv_fd_poll_mt_move(struct iv_state *st, struct iv_mt_group *me,
		     struct iv_mt_group *to, struct iv_mt_group *from, int num)
{
	struct iv_fd_ *fd;
	int i;

	if (from->num_fds < num || to->num_fds + num > MAX_POLL_FDS) {
		iv_fatal("__iv_fd_poll_mt_move: %p(%d) -> %p(%d) (%d fds)",
			 from, from->num_fds, to, to->num_fds, num);
	}

	fd = st->handled_fd;
	if (fd != NULL && fd->u.poll_mt.grp == from) {
		int index;

		index = fd->u.poll_mt.index;
		if (index) {
			struct iv_fd_ *fd2 = from->fds[0];

			from->fds[0] = fd;
			fd->u.poll_mt.index = 0;

			from->fds[index] = fd2;
			fd2->u.poll_mt.index = index;
		}

		if (from->num_fds - 1 < num) {
			iv_fatal("__iv_fd_poll_mt_move: %p(%d) <- %p(%d) %d",
				 to, to->num_fds, from, from->num_fds, num);
		}
	}

	from->num_fds -= num;
	__iv_fd_poll_mt_group_update(me, from);

	for (i = 0; i < num; i++) {
		fd = from->fds[from->num_fds + i];
		from->fds[from->num_fds + i] = NULL;

		to->fds[to->num_fds + i] = fd;

		iv_list_del_init(&fd->list_active);

		fd->u.poll_mt.grp = to;
		fd->u.poll_mt.index = to->num_fds + i;
	}

	to->num_fds += num;
	__iv_fd_poll_mt_group_update(me, to);
}

static void
__iv_fd_poll_mt_split_off(struct iv_state *st, struct iv_mt_group *me,
			  struct iv_mt_group *grp, int num)
{
	struct iv_mt_group *other;
	pthread_t tid;

	other = malloc(sizeof(*other));
	if (other == NULL)
		iv_fatal("__iv_fd_poll_mt_split_off: out of memory");

	iv_list_add_tail(&other->list_aux, &st->u.poll_mt.groups_aux);
	other->st = st;
	iv_fd_poll_mt_create_upd_pipe(other);
	other->update_occured = 1;
	other->num_fds = 0;

	__iv_fd_poll_mt_move(st, me, other, grp, num);

	iv_get_thread_id();

	if (pthread_create(&tid, NULL, iv_fd_poll_mt_poll_thread, other))
		iv_fatal("__iv_fd_poll_mt_split_off: error creating thread");
	pthread_detach(tid);
}

static void __iv_fd_poll_mt_assign_to_group(struct iv_mt_group *me,
					    struct iv_mt_group *grp,
					    struct iv_fd_ *fd)
{
	fd->u.poll_mt.grp = grp;
	fd->u.poll_mt.index = grp->num_fds;

	grp->fds[grp->num_fds] = fd;
	grp->num_fds++;
	__iv_fd_poll_mt_group_update(me, grp);
}

static void
__iv_fd_poll_mt_assign_fd(struct iv_state *st, struct iv_mt_group *me,
			  struct iv_fd_ *fd)
{
	struct iv_mt_group *grpmain = &st->u.poll_mt.group_main;
	struct iv_mt_group *notokay = st->u.poll_mt.notokay;

	if (me == grpmain && me->num_fds < OKAY_MIN - 1) {
		__iv_fd_poll_mt_assign_to_group(me, me, fd);
		if (notokay != NULL && notokay->num_fds > OKAY_MAX) {
			__iv_fd_poll_mt_move(st, me, me, notokay,
					(notokay->num_fds - me->num_fds) / 2);
			st->u.poll_mt.notokay = NULL;
		}
	} else if (me == grpmain && me->num_fds == OKAY_MIN - 1) {
		__iv_fd_poll_mt_assign_to_group(me, me, fd);
	} else if (me != grpmain && me->num_fds == 0) {
		if (notokay == NULL) {
			__iv_fd_poll_mt_assign_to_group(me, me, fd);
			st->u.poll_mt.notokay = me;
		} else if (notokay->num_fds < OKAY_MIN) {
			__iv_fd_poll_mt_assign_to_group(me, notokay, fd);
			if (notokay->num_fds == OKAY_MIN)
				st->u.poll_mt.notokay = NULL;
		} else if (notokay->num_fds > OKAY_MAX) {
			__iv_fd_poll_mt_assign_to_group(me, me, fd);
			__iv_fd_poll_mt_move(st, me, me, notokay,
					(notokay->num_fds - me->num_fds) / 2);
			st->u.poll_mt.notokay = NULL;
		} else {
			iv_fatal("__iv_fd_poll_mt_assign_fd: notokay "
				 "has %d", notokay->num_fds);
		}
	} else if (me != grpmain && me->num_fds < OKAY_MIN - 1) {
		__iv_fd_poll_mt_assign_to_group(me, me, fd);
	} else if (me != grpmain && me->num_fds == OKAY_MIN - 1) {
		__iv_fd_poll_mt_assign_to_group(me, me, fd);

		if (notokay != me) {
			iv_fatal("__iv_fd_poll_mt_assign_fd: notokay %p "
				 "vs me %p", notokay, me);
		}
		st->u.poll_mt.notokay = NULL;
	} else if (me->num_fds < OKAY_MAX) {
		__iv_fd_poll_mt_assign_to_group(me, me, fd);
	} else if (me->num_fds == OKAY_MAX) {
		__iv_fd_poll_mt_assign_to_group(me, me, fd);

		if (notokay == NULL) {
			st->u.poll_mt.notokay = me;
		} else if (notokay->num_fds < OKAY_MIN) {
			__iv_fd_poll_mt_move(st, me, notokay, me,
					(me->num_fds - notokay->num_fds) / 2);
			st->u.poll_mt.notokay = NULL;
		} else if (notokay->num_fds > OKAY_MAX) {
			__iv_fd_poll_mt_split_off(st, me, notokay,
					(me->num_fds + notokay->num_fds) / 3);
			__iv_fd_poll_mt_move(st, me, notokay, me,
					(me->num_fds - notokay->num_fds) / 2);
			st->u.poll_mt.notokay = NULL;
		} else {
			iv_fatal("__iv_fd_poll_mt_assign_fd: notokay "
				 "has %d", notokay->num_fds);
		}
	} else if (me->num_fds < MAX_POLL_FDS) {
		__iv_fd_poll_mt_assign_to_group(me, me, fd);
	} else if (me->num_fds == MAX_POLL_FDS) {
		if (me->upd_pipe[0] == -1) {
			iv_fd_poll_mt_create_upd_pipe(me);
			st->u.poll_mt.pfds[0].fd = me->upd_pipe[0];
			st->u.poll_mt.pfds[0].events = POLLIN | POLLHUP;
		}

		__iv_fd_poll_mt_split_off(st, me, me, (MAX_POLL_FDS + 1) / 2);

		__iv_fd_poll_mt_assign_to_group(me, me, fd);

		if (notokay != me) {
			iv_fatal("__iv_fd_poll_mt_assign_fd: notokay %p "
				 "vs me %p", notokay, me);
		}
		st->u.poll_mt.notokay = NULL;
	}
}

static void
__iv_fd_poll_mt_unassign_from_group(struct iv_mt_group *me,
				    struct iv_mt_group *grp, struct iv_fd_ *fd)
{
	int i;

	grp->num_fds--;
	for (i = fd->u.poll_mt.index; i < grp->num_fds; i++) {
		struct iv_fd_ *fd;

		fd = grp->fds[i + 1];
		fd->u.poll_mt.index = i;

		grp->fds[i] = fd;
	}

	fd->u.poll_mt.grp = NULL;
	fd->u.poll_mt.index = -1;

	__iv_fd_poll_mt_group_update(me, grp);
}

static void
__iv_fd_poll_mt_unassign_fd(struct iv_state *st, struct iv_mt_group *me,
			    struct iv_mt_group *grp, struct iv_fd_ *fd)
{
	struct iv_mt_group *grpmain = &st->u.poll_mt.group_main;
	struct iv_mt_group *notokay = st->u.poll_mt.notokay;

	__iv_fd_poll_mt_unassign_from_group(me, grp, fd);

	if (grp == grpmain && grp->num_fds < OKAY_MIN) {
		if (notokay != NULL && notokay != me &&
		    notokay->num_fds < OKAY_MIN) {
			__iv_fd_poll_mt_move(st, me, grp, notokay,
					notokay->num_fds);
			st->u.poll_mt.notokay = NULL;
		} else if (notokay != NULL && notokay->num_fds > OKAY_MAX) {
			__iv_fd_poll_mt_move(st, me, grp, notokay,
					(notokay->num_fds - grp->num_fds) / 2);
			st->u.poll_mt.notokay = NULL;
		}
	} else if (grp != grpmain && grp->num_fds == 0) {
		if (notokay != grp) {
			iv_fatal("__iv_fd_poll_mt_unassign_fd: notokay %p "
				 "vs grp %p", notokay, grp);
		}
		st->u.poll_mt.notokay = NULL;
	} else if (grp != grpmain && grp->num_fds == OKAY_MIN - 1) {
		if (notokay == NULL) {
			st->u.poll_mt.notokay = grp;
		} else if (notokay->num_fds < OKAY_MIN) {
			__iv_fd_poll_mt_move(st, me, grp, notokay,
					notokay->num_fds);
			st->u.poll_mt.notokay = NULL;
		} else if (notokay->num_fds > OKAY_MAX) {
			__iv_fd_poll_mt_move(st, me, grp, notokay,
					(notokay->num_fds - grp->num_fds) / 2);
			st->u.poll_mt.notokay = NULL;
		} else {
			iv_fatal("__iv_fd_poll_mt_unassign_fd: "
				 "notokay has %d", notokay->num_fds);
		}
	} else if (grp->num_fds == OKAY_MAX) {
		if (notokay != grp) {
			iv_fatal("__iv_fd_poll_mt_unassign_fd: notokay %p "
				 "vs grp %p\n", notokay, grp);
		}
		st->u.poll_mt.notokay = NULL;
	}
}

static void iv_fd_poll_mt_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	struct iv_mt_group *me = pthread_getspecific(iv_mt_group_key);
	struct iv_mt_group *grp = fd->u.poll_mt.grp;

	if (fd->registered_bands != fd->wanted_bands) {
		pthread_mutex_lock(&st->u.poll_mt.state_lock);
		if (grp == NULL && fd->wanted_bands) {
			__iv_fd_poll_mt_assign_fd(st, me, fd);
		} else if (grp != NULL && !fd->wanted_bands) {
			__iv_fd_poll_mt_unassign_fd(st, me, grp, fd);
		} else if (grp != NULL && fd->wanted_bands) {
			__iv_fd_poll_mt_group_update(me, grp);
		}
		pthread_mutex_unlock(&st->u.poll_mt.state_lock);

		fd->registered_bands = fd->wanted_bands;
	}
}

static int iv_fd_poll_mt_notify_fd_sync(struct iv_state *st, struct iv_fd_ *fd)
{
	struct pollfd pfd;
	int ret;

	pfd.fd = fd->fd;
	pfd.events = POLLIN | POLLOUT | POLLHUP;

	do {
		ret = poll(&pfd, 1, 0);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0 || (pfd.revents & POLLNVAL))
		return -1;

	iv_fd_poll_mt_notify_fd(st, fd);

	return 0;
}

static void iv_fd_poll_mt_deinit(struct iv_state *st)
{
	struct iv_mt_group *me = &st->u.poll_mt.group_main;
	struct iv_list_head *ilh;
	struct iv_mt_group *other;

	pthread_mutex_lock(&st->u.poll_mt.state_lock);

	iv_list_for_each (ilh, &st->u.poll_mt.groups_aux) {
		other = iv_container_of(ilh, struct iv_mt_group, list_aux);

		other->num_fds = 0;
		__iv_fd_poll_mt_group_update(me, other);
	}

	pthread_cond_broadcast(&st->u.poll_mt.exec_lock_free);
	while (!iv_list_empty(&st->u.poll_mt.groups_aux)) {
		pthread_cond_wait(&st->u.poll_mt.groups_aux_empty,
				  &st->u.poll_mt.state_lock);
	}

	pthread_mutex_unlock(&st->u.poll_mt.state_lock);

	pthread_mutex_destroy(&st->u.poll_mt.state_lock);
	pthread_cond_destroy(&st->u.poll_mt.groups_aux_empty);
	pthread_cond_destroy(&st->u.poll_mt.exec_lock_free);

	if (me->upd_pipe[0] != -1)
		iv_fd_poll_mt_destroy_upd_pipe(me);
}

static int iv_fd_poll_mt_event_rx_on(struct iv_state *st)
{
	st->numobjs++;

	pthread_mutex_lock(&st->u.poll_mt.state_lock);
	if (iv_list_empty(&st->u.poll_mt.groups_aux)) {
		struct iv_mt_group *me = pthread_getspecific(iv_mt_group_key);

		if (me->upd_pipe[0] == -1) {
			iv_fd_poll_mt_create_upd_pipe(me);
			st->u.poll_mt.pfds[0].fd = me->upd_pipe[0];
			st->u.poll_mt.pfds[0].events = POLLIN | POLLHUP;
		}
	}
	pthread_mutex_unlock(&st->u.poll_mt.state_lock);

	return 0;
}

static void iv_fd_poll_mt_event_rx_off(struct iv_state *st)
{
	st->numobjs--;

	pthread_mutex_lock(&st->u.poll_mt.state_lock);
	if (iv_list_empty(&st->u.poll_mt.groups_aux)) {
		struct iv_mt_group *me = pthread_getspecific(iv_mt_group_key);

		iv_fd_poll_mt_destroy_upd_pipe(me);
		me->upd_pipe[0] = -1;
		me->upd_pipe[1] = -1;
	}
	pthread_mutex_unlock(&st->u.poll_mt.state_lock);
}

static void iv_fd_poll_mt_event_send(struct iv_state *dest)
{
	pthread_mutex_lock(&dest->u.poll_mt.state_lock);
	dest->u.poll_mt.event_rx_active = 1;
	pthread_mutex_unlock(&dest->u.poll_mt.state_lock);

	write(dest->u.poll_mt.group_main.upd_pipe[1], "", 1);
}


struct iv_fd_poll_method iv_fd_poll_method_poll_mt = {
	.name		= "poll_mt",
	.init		= iv_fd_poll_mt_init,
	.poll		= iv_fd_poll_mt_poll,
	.register_fd	= iv_fd_poll_mt_register_fd,
	.notify_fd	= iv_fd_poll_mt_notify_fd,
	.notify_fd_sync	= iv_fd_poll_mt_notify_fd_sync,
	.deinit		= iv_fd_poll_mt_deinit,
	.event_rx_on	= iv_fd_poll_mt_event_rx_on,
	.event_rx_off	= iv_fd_poll_mt_event_rx_off,
	.event_send	= iv_fd_poll_mt_event_send,
};
