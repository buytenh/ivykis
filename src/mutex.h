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

#include <pthread.h>

typedef pthread_mutex_t mutex_t;

static inline int mutex_init(mutex_t *mutex)
{
	return pthread_mutex_init(mutex, NULL);
}

static inline void mutex_destroy(mutex_t *mutex)
{
	pthread_mutex_destroy(mutex);
}

static inline void mutex_lock(mutex_t *mutex)
{
	pthread_mutex_lock(mutex);
}

static inline void mutex_unlock(mutex_t *mutex)
{
	pthread_mutex_unlock(mutex);
}
