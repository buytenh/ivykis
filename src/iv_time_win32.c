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
static ULONGLONG (WINAPI *gtc64)(void);

static void tc64_to_timespec(struct timespec *time, ULONGLONG tc64)
{
	time->tv_sec = tc64 / 1000;
	time->tv_nsec = 1000000L * (tc64 % 1000);
}

void iv_get_time(struct timespec *time)
{
	LARGE_INTEGER _count;
	LARGE_INTEGER _freq;

	switch (method) {
	case 0:
		if (QueryPerformanceFrequency(&_freq) &&
		    QueryPerformanceCounter(&_count)) {
			LONGLONG count = _count.QuadPart;
			LONGLONG freq = _freq.QuadPart;

			time->tv_sec = count / freq;
			time->tv_nsec = (1000000000ULL * (count % freq)) / freq;

			break;
		}

		method = 1;

		/* fall through */

	case 1:
		if (gtc64 == NULL) {
			HMODULE kernel32;

			kernel32 = GetModuleHandle(TEXT("kernel32.dll"));
			gtc64 = (ULONGLONG (WINAPI *)(void))
				GetProcAddress(kernel32, "GetTickCount64");
		}

		if (gtc64 != NULL) {
			tc64_to_timespec(time, gtc64());
			break;
		}

		method = 2;

		/* fall through */

	case 2:
		iv_fatal("iv_time_get: no methods available!");
	}
}
