/*
 * ivykis, an event handling library
 * Copyright (C) 2010 Lennert Buytenhek
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
#include <errno.h>
#include <fcntl.h>
#include <iv.h>
#include <iv_event_raw.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "iv_private.h"
#include "iv_fd_private.h"

/* eventfd syscall **********************************************************/
#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif

#if defined(HAVE_EVENTFD) && defined(EFD_NONBLOCK) && defined(EFD_CLOEXEC)
static int grab_eventfd(void)
{
	int fd;

	fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (fd < 0) {
		if (errno != EINVAL && errno != ENOSYS)
			perror("eventfd");
		return -errno;
	}

	return fd;
}
#else
static int grab_eventfd(void)
{
	return -ENOSYS;
}
#endif


/* implementation ***********************************************************/
static int eventfd_unavailable;

static void iv_event_raw_got_event(void *_this)
{
	struct iv_event_raw *this = (struct iv_event_raw *)_this;
	int toread;
	char buf[1024];
	int ret;

	toread = eventfd_unavailable ? sizeof(buf) : 8;

	do {
		ret = read(this->event_rfd.fd, buf, toread);
	} while (ret < 0 && errno == EINTR);

	if (ret <= 0) {
		if (ret == 0) {
			iv_fatal("iv_event_raw: reading from event fd "
				 "returned zero");
		} else if (errno != EAGAIN) {
			iv_fatal("iv_event_raw: reading from event fd "
				 "returned error %d[%s]", errno,
				 strerror(errno));
		}
		return;
	}

	this->handler(this->cookie);
}

int iv_event_raw_register(struct iv_event_raw *this)
{
	int fd[2];

	if (!eventfd_unavailable) {
		int ret;

		ret = grab_eventfd();
		if (ret < 0) {
			if (ret != -EINVAL && ret != -ENOSYS)
				return -1;
			eventfd_unavailable = 1;
		} else {
			fd[0] = ret;
			fd[1] = ret;
		}
	}

	if (eventfd_unavailable) {
		if (pipe(fd) < 0) {
			perror("pipe");
			return -1;
		}
	}

	IV_FD_INIT(&this->event_rfd);
	this->event_rfd.fd = fd[0];
	this->event_rfd.cookie = this;
	this->event_rfd.handler_in = iv_event_raw_got_event;
	iv_fd_register(&this->event_rfd);

	this->event_wfd = fd[1];
	if (eventfd_unavailable) {
		iv_fd_set_cloexec(fd[1]);
		iv_fd_set_nonblock(fd[1]);
	}

	return 0;
}

void iv_event_raw_unregister(struct iv_event_raw *this)
{
	iv_fd_unregister(&this->event_rfd);
	close(this->event_rfd.fd);

	if (eventfd_unavailable)
		close(this->event_wfd);
}

void iv_event_raw_post(struct iv_event_raw *this)
{
	if (eventfd_unavailable) {
		write(this->event_wfd, "", 1);
	} else {
		uint64_t x = 1;
		write(this->event_wfd, &x, sizeof(x));
	}
}
