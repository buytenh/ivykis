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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <syslog.h>
#include <sys/poll.h>
#include "iv_private.h"

static struct pollfd		*pfds;
static struct iv_fd		**fds;
static int			numfds;


static int iv_poll_init(int maxfd)
{
	pfds = malloc(maxfd * sizeof(struct pollfd));
	if (pfds == NULL)
		return -1;

	fds = malloc(maxfd * sizeof(struct iv_fd *));
	if (fds == NULL) {
		free(pfds);
		return -1;
	}

	numfds = 0;

	return 0;
}

static int __poll_mask(struct iv_fd *fd)
{
	int mask;

	mask = 0;
	if (!(fd->flags & (1 << FD_ReadyErr))) {
		mask = POLLERR;
		if (fd->handler_in != NULL && !(fd->flags & 1 << FD_ReadyIn))
			mask |= POLLIN | POLLHUP;
		if (fd->handler_out != NULL && !(fd->flags & 1 << FD_ReadyOut))
			mask |= POLLOUT;
	}

	return mask;
}

static void internal_unregister(struct iv_fd *fd)
{
	int index;

	index = (int)(ptrdiff_t)fd->list_all.next;
	if (index != numfds - 1) {
		struct iv_fd *last = fds[numfds-1];

		pfds[index] = pfds[numfds-1];
		fds[index] = last;
		last->list_all.next = last->list_all.prev = (void *)(ptrdiff_t)index;
	}
	numfds--;

	fd->list_all.next = fd->list_all.prev = (void *)-1;
}

static void iv_poll_poll(int timeout)
{
	int i;
	int ret;

	do {
		ret = poll(pfds, numfds, timeout);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_poll_poll: got error %d[%s]", errno,
		       strerror(errno));
		abort();
	}

	for (i=0;i<numfds;i++) {
		if (pfds[i].revents) {
			struct iv_fd *fd = fds[i];

			if (pfds[i].revents & (POLLIN | POLLERR | POLLHUP))
				iv_fd_make_ready(fd, FD_ReadyIn);
			if (pfds[i].revents & (POLLOUT | POLLERR))
				iv_fd_make_ready(fd, FD_ReadyOut);
			if (pfds[i].revents & POLLERR)
				iv_fd_make_ready(fd, FD_ReadyErr);

			pfds[i].events = __poll_mask(fd);
			if (pfds[i].events == 0) {
				internal_unregister(fd);
				i--;
			}
		}
	}
}

static void iv_poll_register_fd(struct iv_fd *fd)
{
	int index;

	index = numfds++;
	pfds[index].fd = fd->fd;
	pfds[index].events = __poll_mask(fd);
	fds[index] = fd;

	fd->list_all.next = fd->list_all.prev = (void *)(ptrdiff_t)index;
}

static void iv_poll_reregister_fd(struct iv_fd *fd)
{
	int index;

	index = (int)(ptrdiff_t)fd->list_all.next;
	if (index != -1) {
		if (pfds[index].fd != fd->fd || fds[index] != fd) {
			syslog(LOG_CRIT, "iv_poll_reregister_fd: index fuckup");
			abort();
		}

		pfds[index].events = __poll_mask(fd);
	}
}

static void iv_poll_unregister_fd(struct iv_fd *fd)
{
	int index;

	index = (int)(ptrdiff_t)fd->list_all.next;
	if (index != -1) {
		if (pfds[index].fd != fd->fd || fds[index] != fd) {
			syslog(LOG_CRIT, "iv_poll_unregister_fd: index fuckup");
			abort();
		}

		internal_unregister(fd);
	}

	INIT_LIST_HEAD(&(fd->list_all));
}

static void iv_poll_deinit(void)
{
	free(fds);
	free(pfds);
}


struct iv_poll_method iv_method_poll = {
	name:			"poll",
	init:			iv_poll_init,
	poll:			iv_poll_poll,
	register_fd:		iv_poll_register_fd,
	reregister_fd:		iv_poll_reregister_fd,
	unregister_fd:		iv_poll_unregister_fd,
	deinit:			iv_poll_deinit,
};
