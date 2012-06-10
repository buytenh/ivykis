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
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/time.h>
#include "iv_private.h"

static fd_set *readfds_master(struct iv_state *st)
{
	return (fd_set *)st->select.sets;
}

static fd_set *writefds_master(struct iv_state *st)
{
	return (fd_set *)(st->select.sets + st->select.setsize);
}

static fd_set *readfds(struct iv_state *st)
{
	return (fd_set *)(st->select.sets + 2 * st->select.setsize);
}

static fd_set *writefds(struct iv_state *st)
{
	return (fd_set *)(st->select.sets + 3 * st->select.setsize);
}

static int iv_select_init(struct iv_state *st)
{
	int setsize;
	void *sets;

	setsize = 8 * ((maxfd + 63) / 64);

	sets = malloc(4 * setsize);
	if (sets == NULL)
		return -1;

	memset(sets, 0, 2 * setsize);

	INIT_IV_AVL_TREE(&st->select.fds, iv_fd_avl_compare);
	st->select.sets = sets;
	st->select.setsize = setsize;
	st->select.fd_max = 0;

	return 0;
}

static void
iv_select_poll(struct iv_state *st, struct iv_list_head *active, int msec)
{
	int bytes;
	struct timeval to;
	int ret;
	int i;

	bytes = ((st->select.fd_max + 1) + 7) / 8;

	memcpy(readfds(st), readfds_master(st), bytes);
	memcpy(writefds(st), writefds_master(st), bytes);

	to.tv_sec = msec / 1000;
	to.tv_usec = 1000 * (msec % 1000);

	ret = select(st->select.fd_max + 1, readfds(st),
		     writefds(st), NULL, &to);
	if (ret < 0) {
		if (errno == EINTR)
			return;

		iv_fatal("iv_select_poll: got error %d[%s]", errno,
			 strerror(errno));
	}

	for (i = 0; i <= st->select.fd_max; i++) {
		int pollin;
		int pollout;

		pollin = !!FD_ISSET(i, readfds(st));
		pollout = !!FD_ISSET(i, writefds(st));
		if (pollin || pollout) {
			struct iv_fd_ *fd;

			fd = iv_fd_avl_find(&st->select.fds, i);
			if (fd == NULL)
				iv_fatal("iv_select_poll: can't find fd");

			if (pollin)
				iv_fd_make_ready(active, fd, MASKIN);

			if (pollout)
				iv_fd_make_ready(active, fd, MASKOUT);
		}
	}
}

static void iv_select_register_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	int ret;

	ret = iv_avl_tree_insert(&st->select.fds, &fd->avl_node);
	if (ret) {
		iv_fatal("iv_select_register_fd: got error %d[%s]", ret,
			 strerror(ret));
	}

	if (fd->fd > st->select.fd_max)
		st->select.fd_max = fd->fd;
}

static void iv_select_unregister_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	iv_avl_tree_delete(&st->select.fds, &fd->avl_node);

	if (fd->fd == st->select.fd_max) {
		struct iv_avl_node *an;

		an = iv_avl_tree_max(&st->select.fds);
		if (an != NULL) {
			struct iv_fd_ *fd;

			fd = iv_container_of(an, struct iv_fd_, avl_node);
			st->select.fd_max = fd->fd;
		} else {
			st->select.fd_max = 0;
		}
	}
}

static void iv_select_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
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

static int iv_select_notify_fd_sync(struct iv_state *st, struct iv_fd_ *fd)
{
	int bytes;
	struct timeval to = { 0, 0 };
	int ret;

	bytes = ((st->select.fd_max + 1) + 7) / 8;

	memset(readfds(st), 0, bytes);
	memset(writefds(st), 0, bytes);

	FD_SET(fd->fd, readfds(st));
	FD_SET(fd->fd, writefds(st));

	do {
		ret = select(fd->fd + 1, readfds(st),
			     writefds(st), NULL, &to);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0)
		return -1;

	iv_select_notify_fd(st, fd);

	return 0;
}

static void iv_select_deinit(struct iv_state *st)
{
	free(st->select.sets);
}


struct iv_poll_method iv_method_select = {
	.name		= "select",
	.init		= iv_select_init,
	.poll		= iv_select_poll,
	.register_fd	= iv_select_register_fd,
	.unregister_fd	= iv_select_unregister_fd,
	.notify_fd	= iv_select_notify_fd,
	.notify_fd_sync	= iv_select_notify_fd_sync,
	.deinit		= iv_select_deinit,
};
