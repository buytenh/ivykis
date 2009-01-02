/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
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

#ifdef linux

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "iv_private.h"
#include "epoll.h"

#if !defined(__NR_epoll_create) && defined(__i386__)
#include <linux/unistd.h>
#define __NR_epoll_create	254
#define __NR_epoll_ctl		255
#define __NR_epoll_wait		256
_syscall1(int, epoll_create, int, size)
_syscall4(int, epoll_ctl, int, epfd, int, op, int, fd,
	  struct epoll_event *, event)
_syscall4(int, epoll_wait, int, epfd, struct epoll_event *, pevents,
	  int, maxevents, int, timeout)
#endif

#define POLL_BATCH_SIZE		(1024)


static struct list_head		all;
static int			epoll_fd;


static int iv_sys_epoll_init(int maxfd)
{
	epoll_fd = epoll_create(maxfd);
	if (epoll_fd < 0)
		return -1;

	INIT_LIST_HEAD(&all);

	return 0;
}

static void iv_sys_epoll_poll(int timeout)
{
	struct epoll_event batch[POLL_BATCH_SIZE];
	int i;
	int maxevents;
	int ret;

	maxevents = sizeof(batch)/sizeof(batch[0]);

do_it_again:
	do {
		ret = epoll_wait(epoll_fd, batch, maxevents, timeout);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_sys_epoll_poll: got error %d[%s]", errno,
		       strerror(errno));
		abort();
	}

	for (i=0;i<ret;i++) {
		struct iv_fd *fd;

		fd = batch[i].data.ptr;
		if (batch[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP))
			iv_fd_make_ready(fd, FD_ReadyIn);
		if (batch[i].events & (EPOLLOUT | EPOLLERR))
			iv_fd_make_ready(fd, FD_ReadyOut);
		if (batch[i].events & EPOLLERR)
			iv_fd_make_ready(fd, FD_ReadyErr);
	}

	if (ret == maxevents) {
		timeout = 0;
		goto do_it_again;
	}
}

static void iv_sys_epoll_register_fd(struct iv_fd *fd)
{
	struct epoll_event event;
	int ret;

	list_add_tail(&(fd->list_all), &all);

	event.data.ptr = fd;
	event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT | EPOLLET;
	do {
		ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd->fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_sys_epoll_register_fd: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}
}

/* Deleting an fd from an epoll set causes all pending events to be
 * cleared from the queue.  Therefore, we need no barrier operation
 * after the epoll_ctl below.  (Recall that the returned epoll_event
 * structures contain opaque-to-the-kernel userspace pointers, which
 * are dereferenced in the event handler without validation.)  */
static void iv_sys_epoll_unregister_fd(struct iv_fd *fd)
{
	struct epoll_event event;
	int ret;

	event.data.ptr = fd;
	event.events = 0;
	do {
		ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd->fd, &event);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_sys_epoll_unregister_fd: got error %d[%s]",
		       errno, strerror(errno));
		abort();
	}

	list_del_init(&(fd->list_all));
}

static void iv_sys_epoll_deinit(void)
{
	close(epoll_fd);
}


struct iv_poll_method iv_method_sys_epoll = {
	name:			"sys_epoll",
	init:			iv_sys_epoll_init,
	poll:			iv_sys_epoll_poll,
	register_fd:		iv_sys_epoll_register_fd,
	reregister_fd:		NULL,
	unregister_fd:		iv_sys_epoll_unregister_fd,
	deinit:			iv_sys_epoll_deinit,
};
#endif
