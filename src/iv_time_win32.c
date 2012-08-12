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
static int tc32_cs_initialized;
static CRITICAL_SECTION tc32_cs;
static DWORD tc32_high;
static DWORD tc32_low;

void iv_time_init(struct iv_state *st)
{
	if (!tc32_cs_initialized) {
		tc32_cs_initialized = 1;
		InitializeCriticalSection(&tc32_cs);
	}
}

static void tc64_to_timespec(struct timespec *time, ULONGLONG tc64)
{
	time->tv_sec = tc64 / 1000;
	time->tv_nsec = 1000000L * (tc64 % 1000);
}

void iv_get_time(struct timespec *time)
{
	LARGE_INTEGER _count;
	LARGE_INTEGER _freq;
	DWORD tc;
	DWORD tc_high;

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
		EnterCriticalSection(&tc32_cs);

		tc = GetTickCount();
		if ((unsigned)tc < (unsigned)tc32_low)
			tc32_high++;
		tc32_low = tc;

		tc_high = tc32_high;

		LeaveCriticalSection(&tc32_cs);

		tc64_to_timespec(time, (((ULONGLONG)tc_high) << 32) | tc);

		break;
	}
}
