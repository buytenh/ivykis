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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <syslog.h>
#include <sys/poll.h>
#include "iv_private.h"

#ifndef POLLMSG
#define POLLMSG		0
#endif

#ifndef POLLRDHUP
#define POLLRDHUP	0
#endif

#define SET_IN		(POLLIN | POLLPRI | POLLRDNORM | \
			 POLLRDBAND | POLLMSG | POLLRDHUP)
#define SET_OUT		(POLLOUT | POLLWRNORM | POLLWRBAND)
#define SET_ERR		(POLLERR | POLLHUP)

static struct pollfd	*pfds;
static struct iv_fd_	**fds;
static int		numfds;


static int iv_poll_init(int maxfd)
{
	pfds = malloc(maxfd * sizeof(struct pollfd));
	if (pfds == NULL)
		return -1;

	fds = malloc(maxfd * sizeof(struct iv_fd_ *));
	if (fds == NULL) {
		free(pfds);
		return -1;
	}

	numfds = 0;

	return 0;
}

static void iv_poll_poll(struct list_head *active, int msec)
{
	int ret;
	int i;

	do {
		ret = poll(pfds, numfds, msec);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_poll_poll: got error %d[%s]", errno,
		       strerror(errno));
		abort();
	}

	for (i = 0; i < numfds; i++) {
		struct iv_fd_ *fd = fds[i];

		if (pfds[i].revents & (SET_IN | SET_ERR))
			iv_fd_make_ready(active, fd, MASKIN);
		if (pfds[i].revents & (SET_OUT | SET_ERR))
			iv_fd_make_ready(active, fd, MASKOUT);
		if (pfds[i].revents & SET_ERR)
			iv_fd_make_ready(active, fd, MASKERR);
	}
}

static void iv_poll_register_fd(struct iv_fd_ *fd)
{
	fd->index = -1;
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = 0;
	if (bits & MASKIN)
		mask |= SET_IN;
	if (bits & MASKOUT)
		mask |= SET_OUT;
	if (bits)
		mask |= SET_ERR;

	return mask;
}

static void iv_poll_notify_fd(struct iv_fd_ *fd, int wanted)
{
	if (fd->registered_bands == wanted)
		return;

	if (fd->index == -1 && wanted) {
		fd->index = numfds++;
		pfds[fd->index].fd = fd->fd;
		pfds[fd->index].events = bits_to_poll_mask(wanted);
		fds[fd->index] = fd;
	} else if (fd->index != -1 && !wanted) {
		if (fd->index != numfds - 1) {
			struct iv_fd_ *last = fds[numfds - 1];

			pfds[fd->index] = pfds[numfds - 1];
			fds[fd->index] = last;
		}
		numfds--;

		fd->index = -1;
	} else {
		pfds[fd->index].events = bits_to_poll_mask(wanted);
	}

	fd->registered_bands = wanted;
}

static void iv_poll_deinit(void)
{
	free(fds);
	free(pfds);
}


struct iv_poll_method iv_method_poll = {
	.name		= "poll",
	.init		= iv_poll_init,
	.poll		= iv_poll_poll,
	.register_fd	= iv_poll_register_fd,
	.notify_fd	= iv_poll_notify_fd,
	.deinit		= iv_poll_deinit,
};
