/*
 * ivykis, an event handling library
 * Copyright (C) 2012 Lennert Buytenhek
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
#include <sys/time.h>
#include <time.h>
#include "iv_private.h"

static int qpc_unavailable;

void iv_get_time(struct timespec *time)
{
	ULONGLONG tc;

	if (!qpc_unavailable) {
		LARGE_INTEGER _count;
		LARGE_INTEGER _freq;

		if (QueryPerformanceFrequency(&_freq) &&
		    QueryPerformanceCounter(&_count)) {
			LONGLONG count = _count.QuadPart;
			LONGLONG freq = _freq.QuadPart;

			time->tv_sec = count / freq;
			time->tv_nsec = (1000000000ULL * (count % freq)) / freq;

			return;
		}

		qpc_unavailable = 1;
	}

	tc = GetTickCount64();
	time->tv_sec = tc / 1000;
	time->tv_nsec = 1000000L * (tc % 1000);
}
