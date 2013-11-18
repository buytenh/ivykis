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

#include "config.h"

#ifdef HAVE_PTHREAD_SPIN_LOCK
#define spinlock_t		pthread_spinlock_t

static inline void spin_init(spinlock_t *lock)
{
	pthread_spin_init(lock, PTHREAD_PROCESS_SHARED);
}

static inline void spin_lock(spinlock_t *lock)
{
	pthread_spin_lock(lock);
}

static inline void spin_unlock(spinlock_t *lock)
{
	pthread_spin_unlock(lock);
}
#else
typedef unsigned long spinlock_t;

static inline void spin_init(spinlock_t *lock)
{
	*lock = 0;
}

static inline void spin_lock(spinlock_t *lock)
{
	while (__sync_lock_test_and_set(lock, 1) == 1)
		;
}

static inline void spin_unlock(spinlock_t *lock)
{
	__sync_lock_release(lock);
}
#endif

static inline void spin_lock_sigmask(spinlock_t *lock, sigset_t *mask)
{
	sigset_t all;

	sigfillset(&all);
	pthread_sigmask(SIG_BLOCK, &all, mask);

	spin_lock(lock);
}

static inline void spin_unlock_sigmask(spinlock_t *lock, sigset_t *mask)
{
	spin_unlock(lock);

	pthread_sigmask(SIG_SETMASK, mask, NULL);
}
