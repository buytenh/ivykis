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
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/select.h>
#include "iv_private.h"

#define HASH_SIZE		(512)

static struct list_head	*htable;
static int		setsize;
static int		fd_max;
static fd_set		*readfds_master;
static fd_set		*writefds_master;
static fd_set		*readfds;
static fd_set		*writefds;


static unsigned int __fd_hash(unsigned int fd)
{
	return fd % HASH_SIZE;
}

static struct iv_fd_ *find_fd(int fd)
{
	int hash = __fd_hash(fd);
	struct list_head *lh;
	struct iv_fd_ *ret = NULL;

	list_for_each(lh, &htable[hash]) {
		struct iv_fd_ *f;

		f = list_entry(lh, struct iv_fd_, list_hash);
		if (f->fd == fd) {
			ret = f;
			break;
		}
	}

	return ret;
}


/* interface ****************************************************************/
static int iv_select_init(int maxfd)
{
	unsigned char *fdsets;
	int i;

	htable = malloc(HASH_SIZE * sizeof(*htable));
	if (htable == NULL)
		return -1;

	setsize = (maxfd + 7) / 8;

	fdsets = malloc(4 * setsize);
	if (fdsets == NULL) {
		free(htable);
		return -1;
	}

	fprintf(stderr, "warning: using select(2), POLLERR delivery broken\n");

	readfds_master = (fd_set *)fdsets;
	writefds_master = (fd_set *)(fdsets + setsize);
	readfds = (fd_set *)(fdsets + 2 * setsize);
	writefds = (fd_set *)(fdsets + 3 * setsize);

	for (i = 0; i < HASH_SIZE; i++)
		INIT_LIST_HEAD(&htable[i]);
	fd_max = 0;
	memset(readfds_master, 0, 2*setsize);

	return 0;
}

static void iv_select_poll(struct list_head *active, int msec)
{
	int i;
	int ret;

	/*
	 * @@@ This is ugly and dependent on clock tick granularity.
	 */
	if (msec)
		msec += (1000/100) - 1;

	do {
		struct timeval to;

		to.tv_sec = msec / 1000;
		to.tv_usec = 1000 * (msec % 1000);

		memcpy(readfds, readfds_master, (fd_max / 8) + 1);
		memcpy(writefds, writefds_master, (fd_max / 8) + 1);
		ret = select(fd_max + 1, readfds, writefds, NULL, &to);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_select_poll: got error %d[%s]", errno,
		       strerror(errno));
		abort();
	}

	for (i = 0; i <= fd_max; i++) {
		int pollin;
		int pollout;

		pollin = !!FD_ISSET(i, readfds);
		pollout = !!FD_ISSET(i, writefds);
		if (pollin || pollout) {
			struct iv_fd_ *fd;

			fd = find_fd(i);
			if (fd == NULL) {
				syslog(LOG_CRIT, "iv_select_poll: just puked "
						 "on myself... eeeeeeeeeeew");
				abort();
			}

			if (pollin)
				iv_fd_make_ready(active, fd, MASKIN);

			if (pollout)
				iv_fd_make_ready(active, fd, MASKOUT);
		}
	}
}

static void iv_select_register_fd(struct iv_fd_ *fd)
{
	list_add_tail(&fd->list_hash, &htable[__fd_hash(fd->fd)]);

	/*
	 * @@@ Room for optimisation here.
	 */
	if (fd->fd > fd_max)
		fd_max = fd->fd;
}

static void iv_select_unregister_fd(struct iv_fd_ *fd)
{
	list_del_init(&fd->list_hash);
}

static void iv_select_notify_fd(struct iv_fd_ *fd, int wanted)
{
	if (wanted & MASKIN)
		FD_SET(fd->fd, readfds_master);
	else
		FD_CLR(fd->fd, readfds_master);

	if (wanted & MASKOUT)
		FD_SET(fd->fd, writefds_master);
	else
		FD_CLR(fd->fd, writefds_master);

	fd->registered_bands = wanted;
}

static void iv_select_deinit(void)
{
	free(readfds_master);
	free(htable);
}


struct iv_poll_method iv_method_select = {
	.name		= "select",
	.init		= iv_select_init,
	.poll		= iv_select_poll,
	.register_fd	= iv_select_register_fd,
	.unregister_fd	= iv_select_unregister_fd,
	.notify_fd	= iv_select_notify_fd,
	.deinit		= iv_select_deinit,
};
