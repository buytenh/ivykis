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

static int method;
static LONGLONG freq;
static UINT32 last_sec;

void iv_time_init(struct iv_state *st)
{
	if (!method) {
		LARGE_INTEGER _freq;

		if (QueryPerformanceFrequency(&_freq)) {
			method = 1;
			freq = _freq.QuadPart;
		} else {
			method = 2;
		}
	}
}

void iv_time_get(struct timespec *time)
{
	UINT32 local_last_sec;
	UINT32 msec;

	if (method == 1) {
		LARGE_INTEGER _cnt;

		if (QueryPerformanceCounter(&_cnt)) {
			LONGLONG cnt = _cnt.QuadPart;

			time->tv_sec = cnt / freq;
			time->tv_nsec = (1000000000ULL * (cnt % freq)) / freq;
			return;
		}

		method = 2;
	}

	local_last_sec = last_sec;

	msec = GetTickCount() - 1000 * local_last_sec;
	if (msec >= 1000) {
		local_last_sec += msec / 1000;
		msec %= 1000;

		last_sec = local_last_sec;
	}

	time->tv_sec = local_last_sec;
	time->tv_nsec = 1000000 * msec;
}
