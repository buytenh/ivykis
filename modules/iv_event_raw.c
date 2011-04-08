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
#include <sys/syscall.h>
#include <unistd.h>

/* eventfd syscall **********************************************************/
#ifndef __NR_eventfd2
#if defined(linux) && defined(__x86_64__)
#define __NR_eventfd2	290
#elif defined(linux) && defined(__i386__)
#define __NR_eventfd2	328
#endif
#endif

#ifndef EFD_NONBLOCK
#define EFD_NONBLOCK	04000
#endif
#ifndef EFD_CLOEXEC
#define EFD_CLOEXEC	02000000
#endif

int eventfd2(unsigned int count, int flags)
{
#ifdef __NR_eventfd2
	return syscall(__NR_eventfd2, count, flags);
#else
	errno = ENOSYS;
	return -1;
#endif
}


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
		if (ret == 0 || errno != EAGAIN) {
			perror("read");
			abort();
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

		ret = eventfd2(0, EFD_NONBLOCK | EFD_CLOEXEC);
		if (ret < 0) {
			if (errno != ENOSYS) {
				perror("eventfd2");
				return -1;
			}

			eventfd_unavailable = 1;
		} else {
			fd[0] = ret;
			fd[1] = ret;
		}
	}

	if (eventfd_unavailable) {
		if (pipe(fd) < 0) {
			perror("pipe");
			abort();
		}
	}

	IV_FD_INIT(&this->event_rfd);
	this->event_rfd.fd = fd[0];
	this->event_rfd.cookie = this;
	this->event_rfd.handler_in = iv_event_raw_got_event;
	iv_fd_register(&this->event_rfd);

	this->event_wfd = fd[1];
	if (eventfd_unavailable) {
		int flags;

		flags = fcntl(fd[1], F_GETFD);
		if (!(flags & FD_CLOEXEC)) {
			flags |= FD_CLOEXEC;
			fcntl(fd[1], F_SETFD, flags);
		}

		flags = fcntl(fd[1], F_GETFL);
		if (!(flags & O_NONBLOCK)) {
			flags |= O_NONBLOCK;
			fcntl(fd[1], F_SETFL, flags);
		}
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
