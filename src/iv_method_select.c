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

#warning somebody should make POLLERR work

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/select.h>
#include "iv_private.h"

#define HASH_SIZE		(512)

static struct list_head		*all;
static int			setsize;
static int			fd_max;
static fd_set			*readfds_master;
static fd_set			*writefds_master;
static fd_set			*readfds;
static fd_set			*writefds;


static unsigned int __fd_hash(unsigned int fd)
{
	return fd % HASH_SIZE;
}

static struct iv_fd *find_fd(int fd)
{
	int hash = __fd_hash(fd);
	struct list_head *lh;
	struct iv_fd *ret = NULL;

	list_for_each (lh, &all[hash]) {
		struct iv_fd *f;

		f = list_entry(lh, struct iv_fd, list_all);
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

	all = malloc(HASH_SIZE * sizeof(*all));
	if (all == NULL)
		return -1;

	setsize = (maxfd + 7) / 8;

	fdsets = malloc(4 * setsize);
	if (fdsets == NULL) {
		free(all);
		return -1;
	}

	readfds_master = (fd_set *)fdsets;
	writefds_master = (fd_set *)(fdsets + setsize);
	readfds = (fd_set *)(fdsets + 2 * setsize);
	writefds = (fd_set *)(fdsets + 3 * setsize);

	for (i=0;i<HASH_SIZE;i++)
		INIT_LIST_HEAD(&all[i]);
	fd_max = 0;
	memset(readfds_master, 0, 2*setsize);

	return 0;
}

static void iv_select_poll(int timeout)
{
	int i;
	int ret;

	/* @@@ This is ugly and dependent on clock tick granularity.  */
	if (timeout)
		timeout += (1000/100) - 1;

	do {
		struct timeval to;

		to.tv_sec = timeout / 1000;
		to.tv_usec = 1000 * (timeout % 1000);

		memcpy(readfds, readfds_master, (fd_max/8) + 1);
		memcpy(writefds, writefds_master, (fd_max/8) + 1);
		ret = select(fd_max+1, readfds, writefds, NULL, &to);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "iv_select_poll: got error %d[%s]", errno,
		       strerror(errno));
		abort();
	}

	for (i=0;i<=fd_max;i++) {
		int pollin;
		int pollout;

		pollin = !!FD_ISSET(i, readfds);
		pollout = !!FD_ISSET(i, writefds);
		if (pollin || pollout) {
			struct iv_fd *fd;

			fd = find_fd(i);
#if IV_DEBUG
			if (fd == NULL) {
				syslog(LOG_CRIT, "iv_select_poll: just puked "
						 "on myself... eeeeeeeeeeew");
				abort();
			}
#endif

			if (pollin) {
				iv_fd_make_ready(fd, FD_ReadyIn);
				FD_CLR(fd->fd, readfds_master);
			}

			if (pollout) {
				iv_fd_make_ready(fd, FD_ReadyOut);
				FD_CLR(fd->fd, writefds_master);
			}
		}
	}
}

static void iv_select_register_fd(struct iv_fd *fd)
{
	list_add_tail(&(fd->list_all), &all[__fd_hash(fd->fd)]);
	if (fd->handler_in != NULL)
		FD_SET(fd->fd, readfds_master);
	if (fd->handler_out != NULL)
		FD_SET(fd->fd, writefds_master);

	/* @@@ Room for optimisation here.  */
	if (fd->fd > fd_max)
		fd_max = fd->fd;
}

static void iv_select_reregister_fd(struct iv_fd *fd)
{
	if (fd->handler_in == NULL || fd->flags & (1 << FD_ReadyIn))
		FD_CLR(fd->fd, readfds_master);
	else
		FD_SET(fd->fd, readfds_master);

	if (fd->handler_out == NULL || fd->flags & (1 << FD_ReadyOut))
		FD_CLR(fd->fd, writefds_master);
	else
		FD_SET(fd->fd, writefds_master);
}

static void iv_select_unregister_fd(struct iv_fd *fd)
{
	list_del_init(&(fd->list_all));
	FD_CLR(fd->fd, readfds_master);
	FD_CLR(fd->fd, writefds_master);
}

static void iv_select_deinit(void)
{
	free(readfds_master);
	free(all);
}


struct iv_poll_method iv_method_select = {
	name:			"select",
	init:			iv_select_init,
	poll:			iv_select_poll,
	register_fd:		iv_select_register_fd,
	reregister_fd:		iv_select_reregister_fd,
	unregister_fd:		iv_select_unregister_fd,
	deinit:			iv_select_deinit,
};
