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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <syslog.h>
#include "iv_private.h"

#if defined(HAVE_SYS_DEVPOLL_H) || defined(NEED_SELECT)
/* file descriptor avl tree handling ****************************************/
struct iv_fd_ *iv_fd_avl_find(struct iv_avl_tree *root, int fd)
{
	struct iv_avl_node *an;

	an = root->root;
	while (an != NULL) {
		struct iv_fd_ *p;

		p = iv_container_of(an, struct iv_fd_, avl_node);
		if (fd == p->fd)
			return p;

		if (fd < p->fd)
			an = an->left;
		else
			an = an->right;
	}

	return NULL;
}

int iv_fd_avl_compare(struct iv_avl_node *_a, struct iv_avl_node *_b)
{
	struct iv_fd_ *a = iv_container_of(_a, struct iv_fd_, avl_node);
	struct iv_fd_ *b = iv_container_of(_b, struct iv_fd_, avl_node);

	if (a->fd < b->fd)
		return -1;

	if (a->fd > b->fd)
		return 1;

	return 0;
}
#endif


/* file descriptor handling *************************************************/
void IV_FD_INIT(struct iv_fd *_fd)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	fd->fd = -1;
	fd->handler_in = NULL;
	fd->handler_out = NULL;
	fd->handler_err = NULL;
	fd->registered = 0;
}

static void recompute_wanted_flags(struct iv_fd_ *fd)
{
	int wanted;

	wanted = 0;
	if (fd->registered) {
		if (fd->handler_in != NULL)
			wanted |= MASKIN;
		if (fd->handler_out != NULL)
			wanted |= MASKOUT;
		if (fd->handler_err != NULL)
			wanted |= MASKERR;
	}

	fd->wanted_bands = wanted;
}

static void notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	recompute_wanted_flags(fd);

	method->notify_fd(st, fd);
}

static void iv_fd_register_prologue(struct iv_state *st, struct iv_fd_ *fd)
{
	if (fd->registered) {
		syslog(LOG_CRIT, "iv_fd_register: called with fd which "
				 "is still registered");
		abort();
	}

	if (fd->fd < 0 || fd->fd >= maxfd) {
		syslog(LOG_CRIT, "iv_fd_register: called with invalid fd "
				 "%d (maxfd=%d)", fd->fd, maxfd);
		abort();
	}

	fd->registered = 1;
	INIT_IV_LIST_HEAD(&fd->list_active);
	fd->ready_bands = 0;
	fd->registered_bands = 0;
#if defined(HAVE_SYS_DEVPOLL_H) || defined(HAVE_EPOLL_CREATE) ||	\
    defined(HAVE_KQUEUE) || defined(HAVE_PORT_CREATE)
	INIT_IV_LIST_HEAD(&fd->list_notify);
#endif

	if (method->register_fd != NULL)
		method->register_fd(st, fd);
}

static void iv_fd_register_epilogue(struct iv_state *st, struct iv_fd_ *fd)
{
	int flags;
	int yes;

	st->numfds++;

	flags = fcntl(fd->fd, F_GETFD);
	if (!(flags & FD_CLOEXEC)) {
		flags |= FD_CLOEXEC;
		fcntl(fd->fd, F_SETFD, flags);
	}

	flags = fcntl(fd->fd, F_GETFL);
	if (!(flags & O_NONBLOCK)) {
		flags |= O_NONBLOCK;
		fcntl(fd->fd, F_SETFL, flags);
	}

	yes = 1;
	setsockopt(fd->fd, SOL_SOCKET, SO_OOBINLINE, &yes, sizeof(yes));
}

void iv_fd_register(struct iv_fd *_fd)
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	iv_fd_register_prologue(st, fd);

	notify_fd(st, fd);

	iv_fd_register_epilogue(st, fd);
}

int iv_fd_register_try(struct iv_fd *_fd)
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	iv_fd_register_prologue(st, fd);

	recompute_wanted_flags(fd);

	ret = method->notify_fd_sync(st, fd);
	if (ret) {
		fd->registered = 0;
		return ret;
	}

	iv_fd_register_epilogue(st, fd);

	return 0;
}

void iv_fd_unregister(struct iv_fd *_fd)
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	if (!fd->registered) {
		syslog(LOG_CRIT, "iv_fd_unregister: called with fd which "
				 "is not registered");
		abort();
	}
	fd->registered = 0;

	iv_list_del(&fd->list_active);

	notify_fd(st, fd);
	if (method->unregister_fd != NULL)
		method->unregister_fd(st, fd);

	st->numfds--;

	if (st->handled_fd == fd)
		st->handled_fd = NULL;
}

int iv_fd_registered(struct iv_fd *_fd)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	return fd->registered;
}

void iv_fd_make_ready(struct iv_list_head *active, struct iv_fd_ *fd, int bands)
{
	if (iv_list_empty(&fd->list_active)) {
		fd->ready_bands = 0;
		iv_list_add_tail(&fd->list_active, active);
	}
	fd->ready_bands |= bands;
}

void iv_fd_set_handler_in(struct iv_fd *_fd, void (*handler_in)(void *))
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	if (!fd->registered) {
		syslog(LOG_CRIT, "iv_fd_set_handler_in: called with fd "
				 "which is not registered");
		abort();
	}

	fd->handler_in = handler_in;
	notify_fd(st, fd);
}

void iv_fd_set_handler_out(struct iv_fd *_fd, void (*handler_out)(void *))
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	if (!fd->registered) {
		syslog(LOG_CRIT, "iv_fd_set_handler_out: called with fd "
				 "which is not registered");
		abort();
	}

	fd->handler_out = handler_out;
	notify_fd(st, fd);
}

void iv_fd_set_handler_err(struct iv_fd *_fd, void (*handler_err)(void *))
{
	struct iv_state *st = iv_get_state();
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	if (!fd->registered) {
		syslog(LOG_CRIT, "iv_fd_set_handler_err: called with fd "
				 "which is not registered");
		abort();
	}

	fd->handler_err = handler_err;
	notify_fd(st, fd);
}
