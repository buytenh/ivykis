/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009, 2012 Lennert Buytenhek
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
#include <sys/time.h>
#include <time.h>
#include "iv_private.h"

#ifdef HAVE_CLOCK_GETTIME
static int clock_source;
#endif

void iv_time_get(struct timespec *time)
{
	struct timeval tv;

#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC_FAST)
	if (clock_source < 1) {
		if (clock_gettime(CLOCK_MONOTONIC_FAST, time) >= 0)
			return;
		clock_source = 1;
	}
#endif

#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC)
	if (clock_source < 2) {
		if (clock_gettime(CLOCK_MONOTONIC, time) >= 0)
			return;
		clock_source = 2;
	}
#endif

#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_REALTIME)
	if (clock_source < 3) {
		if (clock_gettime(CLOCK_REALTIME, time) >= 0)
			return;
		clock_source = 3;
	}
#endif

	gettimeofday(&tv, NULL);
	time->tv_sec = tv.tv_sec;
	time->tv_nsec = 1000L * tv.tv_usec;
}
