/*
 * ivykis, an event handling library
 * Copyright (C) 2011 Lennert Buytenhek
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
#include <port.h>
#include <string.h>
#include <syslog.h>
#include "iv_private.h"

static int iv_port_init(struct iv_state *st, int maxfd)
{
	int fd;

	fd = port_create();
	if (fd < 0)
		return -1;

	st->port.port_fd = fd;

	return 0;
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = 0;
	if (bits & MASKIN)
		mask |= POLLIN;
	if (bits & MASKOUT)
		mask |= POLLOUT;

	return mask;
}

static void iv_port_associate(struct iv_state *st, struct iv_fd_ *fd)
{
	int ret;

	ret = port_associate(st->port.port_fd, PORT_SOURCE_FD,
			     fd->fd, bits_to_poll_mask(fd->wanted_bands), fd);
	if (ret < 0) {
		syslog(LOG_CRIT, "iv_port_associate: got error %d[%s]", errno,
		       strerror(errno));
		abort();
	}
}

static void iv_port_deassociate(struct iv_state *st, struct iv_fd_ *fd)
{
	int ret;

	ret = port_dissociate(st->port.port_fd, PORT_SOURCE_FD, fd->fd);
	if (ret < 0) {
		syslog(LOG_CRIT, "iv_port_associate: got error %d[%s]", errno,
		       strerror(errno));
		abort();
	}
}

static void
iv_port_poll(struct iv_state *st, struct list_head *active, int msec)
{
	port_event_t pe;
	struct timespec to;
	int ret;
	int revents;
	struct iv_fd_ *fd;

	to.tv_sec = msec / 1000;
	to.tv_nsec = 1000000 * (msec % 1000);

	ret = port_get(st->port.port_fd, &pe, &to);
	if (ret < 0) {
		if (errno == EINTR || errno == ETIME)
			return;

		syslog(LOG_CRIT, "iv_port_poll: got error %d[%s]", errno,
		       strerror(errno));
		abort();
	}

	revents = pe.portev_events;
	fd = pe.portev_user;

	if (revents & (POLLIN | POLLERR | POLLHUP))
		iv_fd_make_ready(active, fd, MASKIN);

	if (revents & (POLLOUT | POLLERR | POLLHUP))
		iv_fd_make_ready(active, fd, MASKOUT);

	if (revents & (POLLERR | POLLHUP))
		iv_fd_make_ready(active, fd, MASKERR);

	iv_port_associate(st, fd, fd->registered_bands);
}

static void iv_port_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	if (fd->registered_bands != fd->wanted_bands) {
		if (fd->registered_bands)
			iv_port_deassociate(st, fd);

		if (fd->wanted_bands)
			iv_port_associate(st, fd);

		fd->registered_bands = fd->wanted_bands;
	}
}

static void iv_port_deinit(struct iv_state *st)
{
	close(st->port.port_fd);
}


struct iv_poll_method iv_method_port = {
	.name		= "port",
	.init		= iv_port_init,
	.poll		= iv_port_poll,
	.notify_fd	= iv_port_notify_fd,
	.deinit		= iv_port_deinit,
};
