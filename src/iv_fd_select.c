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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include "iv_private.h"
#include "iv_fd_private.h"

static fd_set *readfds_master(struct iv_state *st)
{
	return (fd_set *)st->u.select.sets;
}

static fd_set *writefds_master(struct iv_state *st)
{
	return (fd_set *)(st->u.select.sets + st->u.select.setsize);
}

static fd_set *readfds(struct iv_state *st)
{
	return (fd_set *)(st->u.select.sets + 2 * st->u.select.setsize);
}

static fd_set *writefds(struct iv_state *st)
{
	return (fd_set *)(st->u.select.sets + 3 * st->u.select.setsize);
}

static int iv_fd_select_init(struct iv_state *st)
{
	int setsize;
	void *sets;

	setsize = 8 * ((maxfd + 63) / 64);

	sets = malloc(4 * setsize);
	if (sets == NULL)
		return -1;

	memset(sets, 0, 2 * setsize);

	INIT_IV_AVL_TREE(&st->u.select.fds, iv_fd_avl_compare);
	st->u.select.sets = sets;
	st->u.select.setsize = setsize;
	st->u.select.fd_max = 0;

	return 0;
}

static void iv_fd_select_poll(struct iv_state *st,
			      struct iv_list_head *active, struct timespec *to)
{
	int bytes;
	struct timeval tv;
	int ret;
	int i;

	bytes = ((st->u.select.fd_max + 1) + 7) / 8;

	memcpy(readfds(st), readfds_master(st), bytes);
	memcpy(writefds(st), writefds_master(st), bytes);

	tv.tv_sec = to->tv_sec;
	tv.tv_usec = (to->tv_nsec + 999) / 1000;

	ret = select(st->u.select.fd_max + 1, readfds(st),
		     writefds(st), NULL, &tv);
	if (ret < 0) {
		if (errno == EINTR)
			return;

		iv_fatal("iv_fd_select_poll: got error %d[%s]", errno,
			 strerror(errno));
	}

	for (i = 0; i <= st->u.select.fd_max; i++) {
		int pollin;
		int pollout;

		pollin = !!FD_ISSET(i, readfds(st));
		pollout = !!FD_ISSET(i, writefds(st));
		if (pollin || pollout) {
			struct iv_fd_ *fd;

			fd = iv_fd_avl_find(&st->u.select.fds, i);
			if (fd == NULL)
				iv_fatal("iv_fd_select_poll: can't find fd");

			if (pollin)
				iv_fd_make_ready(active, fd, MASKIN);

			if (pollout)
				iv_fd_make_ready(active, fd, MASKOUT);
		}
	}
}

static void iv_fd_select_register_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	int ret;

	ret = iv_avl_tree_insert(&st->u.select.fds, &fd->u.avl_node);
	if (ret) {
		iv_fatal("iv_fd_select_register_fd: got error %d[%s]", ret,
			 strerror(ret));
	}

	if (fd->fd > st->u.select.fd_max)
		st->u.select.fd_max = fd->fd;
}

static void iv_fd_select_unregister_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	iv_avl_tree_delete(&st->u.select.fds, &fd->u.avl_node);

	if (fd->fd == st->u.select.fd_max) {
		struct iv_avl_node *an;

		an = iv_avl_tree_max(&st->u.select.fds);
		if (an != NULL) {
			struct iv_fd_ *fd;

			fd = iv_container_of(an, struct iv_fd_, avl_node);
			st->u.select.fd_max = fd->fd;
		} else {
			st->u.select.fd_max = 0;
		}
	}
}

static void iv_fd_select_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	if (fd->wanted_bands & MASKIN)
		FD_SET(fd->fd, readfds_master(st));
	else
		FD_CLR(fd->fd, readfds_master(st));

	if (fd->wanted_bands & MASKOUT)
		FD_SET(fd->fd, writefds_master(st));
	else
		FD_CLR(fd->fd, writefds_master(st));

	fd->registered_bands = fd->wanted_bands;
}

static int iv_fd_select_notify_fd_sync(struct iv_state *st, struct iv_fd_ *fd)
{
	int bytes;
	struct timeval tv = { 0, 0 };
	int ret;

	bytes = ((st->u.select.fd_max + 1) + 7) / 8;

	memset(readfds(st), 0, bytes);
	memset(writefds(st), 0, bytes);

	FD_SET(fd->fd, readfds(st));
	FD_SET(fd->fd, writefds(st));

	do {
		ret = select(fd->fd + 1, readfds(st),
			     writefds(st), NULL, &tv);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0)
		return -1;

	iv_fd_select_notify_fd(st, fd);

	return 0;
}

static void iv_fd_select_deinit(struct iv_state *st)
{
	free(st->u.select.sets);
}


struct iv_fd_poll_method iv_fd_poll_method_select = {
	.name		= "select",
	.init		= iv_fd_select_init,
	.poll		= iv_fd_select_poll,
	.register_fd	= iv_fd_select_register_fd,
	.unregister_fd	= iv_fd_select_unregister_fd,
	.notify_fd	= iv_fd_select_notify_fd,
	.notify_fd_sync	= iv_fd_select_notify_fd_sync,
	.deinit		= iv_fd_select_deinit,
};
