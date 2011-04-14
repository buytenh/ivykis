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

#define SET_IN		(POLLIN | POLLPRI | POLLHUP | POLLRDNORM | \
			 POLLRDBAND | POLLMSG | POLLRDHUP)
#define SET_OUT		(POLLOUT | POLLWRNORM | POLLWRBAND)
#define SET_ERR		(POLLERR)

static int iv_poll_init(struct iv_state *st, int maxfd)
{
	st->poll.pfds = malloc(maxfd * sizeof(struct pollfd));
	if (st->poll.pfds == NULL)
		return -1;

	st->poll.fds = malloc(maxfd * sizeof(struct iv_fd_ *));
	if (st->poll.fds == NULL) {
		free(st->poll.pfds);
		return -1;
	}

	st->poll.num_registered_fds = 0;

	return 0;
}

static void
iv_poll_poll(struct iv_state *st, struct list_head *active, int msec)
{
	int ret;
	int i;

#if _AIX
	/*
	 * AIX sometimes leaves errno uninitialized even if poll
	 * returns -1.
	 */
	errno = EINTR;
#endif

	ret = poll(st->poll.pfds, st->poll.num_registered_fds, msec);
	if (ret < 0) {
		if (errno == EINTR)
			return;

		syslog(LOG_CRIT, "iv_poll_poll: got error %d[%s]", errno,
		       strerror(errno));
		abort();
	}

	for (i = 0; i < st->poll.num_registered_fds; i++) {
		struct iv_fd_ *fd = st->poll.fds[i];

		if (st->poll.pfds[i].revents & (SET_IN | SET_ERR))
			iv_fd_make_ready(active, fd, MASKIN);
		if (st->poll.pfds[i].revents & (SET_OUT | SET_ERR))
			iv_fd_make_ready(active, fd, MASKOUT);
		if (st->poll.pfds[i].revents & SET_ERR)
			iv_fd_make_ready(active, fd, MASKERR);
	}
}

static void iv_poll_register_fd(struct iv_state *st, struct iv_fd_ *fd)
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

static void
iv_poll_notify_fd(struct iv_state *st, struct iv_fd_ *fd, int wanted)
{
	if (fd->registered_bands == wanted)
		return;

	if (fd->index == -1 && wanted) {
		fd->index = st->poll.num_registered_fds++;
		st->poll.pfds[fd->index].fd = fd->fd;
		st->poll.pfds[fd->index].events = bits_to_poll_mask(wanted);
		st->poll.fds[fd->index] = fd;
	} else if (fd->index != -1 && !wanted) {
		if (fd->index != st->poll.num_registered_fds - 1) {
			struct iv_fd_ *last;

			st->poll.pfds[fd->index] =
				st->poll.pfds[st->poll.num_registered_fds - 1];

			last = st->poll.fds[st->poll.num_registered_fds - 1];
			last->index = fd->index;
			st->poll.fds[fd->index] = last;
		}
		st->poll.num_registered_fds--;

		fd->index = -1;
	} else {
		st->poll.pfds[fd->index].events = bits_to_poll_mask(wanted);
	}

	fd->registered_bands = wanted;
}

static void iv_poll_deinit(struct iv_state *st)
{
	free(st->poll.fds);
	free(st->poll.pfds);
}


struct iv_poll_method iv_method_poll = {
	.name		= "poll",
	.init		= iv_poll_init,
	.poll		= iv_poll_poll,
	.register_fd	= iv_poll_register_fd,
	.notify_fd	= iv_poll_notify_fd,
	.deinit		= iv_poll_deinit,
};
