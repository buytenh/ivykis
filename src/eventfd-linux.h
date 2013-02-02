/*
 * ivykis, an event handling library
 * Copyright (C) 2010, 2012 Lennert Buytenhek
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

#include "config.h"
#include <errno.h>
#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif
#include <sys/syscall.h>
#include <unistd.h>

#ifdef __i386__
#ifndef __NR_eventfd
#define __NR_eventfd	323
#endif
#ifndef __NR_eventfd2
#define __NR_eventfd2	328
#endif
#endif

#ifdef __x86_64__
#ifndef __NR_eventfd
#define __NR_eventfd	284
#endif
#ifndef __NR_eventfd2
#define __NR_eventfd2	290
#endif
#endif

#if defined(__i386__) || defined(__x86_64__)
#ifndef EFD_CLOEXEC
#define EFD_CLOEXEC	02000000
#endif
#ifndef EFD_NONBLOCK
#define EFD_NONBLOCK	04000
#endif
#endif

static int eventfd_in_use = 2;

static int eventfd_grab(void)
{
#if (defined(__NR_eventfd2) || defined(HAVE_EVENTFD)) && \
     defined(EFD_CLOEXEC) && defined(EFD_NONBLOCK)
	if (eventfd_in_use == 2) {
		int ret;

#ifdef __NR_eventfd2
		ret = syscall(__NR_eventfd2, 0, EFD_CLOEXEC | EFD_NONBLOCK);
#else
		ret = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#endif
		if (ret >= 0)
			return ret;

		if (errno != EINVAL && errno != ENOSYS)
			return -errno;

		eventfd_in_use = 1;
	}
#endif

#if defined(__NR_eventfd) || defined(HAVE_EVENTFD)
	if (eventfd_in_use) {
		int ret;

#ifdef __NR_eventfd
		ret = syscall(__NR_eventfd, 0);
#else
		ret = eventfd(0, 0);
#endif
		if (ret >= 0)
			return ret;

		if (errno != ENOSYS)
			return -errno;
	}
#endif

	eventfd_in_use = 0;

	return -ENOSYS;
}
