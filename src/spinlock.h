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

#include <unistd.h>
#include "config.h"
#include "iv_private.h"

#ifdef HAVE_PTHREAD_SPIN_LOCK
#define spinlock_t		pthread_spinlock_t

static inline void spin_init(spinlock_t *lock)
{
	pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE);
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
typedef struct {
	int	fd[2];
} spinlock_t;

static inline void spin_init(spinlock_t *lock)
{
	if (pipe(lock->fd) < 0) {
		iv_fatal("spin_init: pipe() returned error %d[%s]",
			 errno, strerror(errno));
	}

	iv_fd_set_cloexec(lock->fd[0]);
	iv_fd_set_cloexec(lock->fd[1]);

	write(lock->fd[1], "", 1);
}

static inline void spin_lock(spinlock_t *lock)
{
	char c;
	int ret;

	ret = read(lock->fd[0], &c, 1);
	if (ret == 1)
		return;

	if (ret < 0) {
		iv_fatal("spin_lock: read() returned error %d[%s]",
			 errno, strerror(errno));
	} else {
		iv_fatal("spin_lock: read() returned %d", ret);
	}
}

static inline void spin_unlock(spinlock_t *lock)
{
	write(lock->fd[1], "", 1);
}
#endif

static inline void spin_lock_sigmask(spinlock_t *lock, sigset_t *mask)
{
	sigfillset(mask);
	pthread_sigmask(SIG_BLOCK, mask, mask);

	spin_lock(lock);
}

static inline void spin_unlock_sigmask(spinlock_t *lock, sigset_t *mask)
{
	spin_unlock(lock);

	pthread_sigmask(SIG_SETMASK, mask, NULL);
}
