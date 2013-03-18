/*
 * ivykis, an event handling library
 * Copyright (C) 2012, 2013 Lennert Buytenhek
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

#ifndef __MUTEX_H
#define __MUTEX_H

#ifndef _WIN32
#include "pthr.h"

#ifdef HAVE_PRAGMA_WEAK
#pragma weak pthread_mutex_init
#pragma weak pthread_mutex_destroy
#pragma weak pthread_mutex_lock
#pragma weak pthread_mutex_unlock
#endif

typedef pthread_mutex_t __mutex_t;

static inline int mutex_init(__mutex_t *mutex)
{
	if (pthreads_available())
		return pthread_mutex_init(mutex, NULL);

	return 0;
}

static inline void mutex_destroy(__mutex_t *mutex)
{
	if (pthreads_available())
		pthread_mutex_destroy(mutex);
}

static inline void mutex_lock(__mutex_t *mutex)
{
	if (pthreads_available())
		pthread_mutex_lock(mutex);
}

static inline void mutex_unlock(__mutex_t *mutex)
{
	if (pthreads_available())
		pthread_mutex_unlock(mutex);
}
#else
typedef CRITICAL_SECTION __mutex_t;

static inline int mutex_init(__mutex_t *mutex)
{
	InitializeCriticalSection(mutex);

	return 0;
}

static inline void mutex_destroy(__mutex_t *mutex)
{
	DeleteCriticalSection(mutex);
}

static inline void mutex_lock(__mutex_t *mutex)
{
	EnterCriticalSection(mutex);
}

static inline void mutex_unlock(__mutex_t *mutex)
{
	LeaveCriticalSection(mutex);
}
#endif


#endif
