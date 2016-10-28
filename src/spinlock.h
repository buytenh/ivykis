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

#include <unistd.h>
#include "iv_private.h"

typedef struct {
	int	fd[2];
} fallback_spinlock_t;

static inline void fallback_spin_init(fallback_spinlock_t *lock)
{
	int ret;

	if (pipe(lock->fd) < 0) {
		iv_fatal("fallback_spin_init: pipe() returned error %d[%s]",
			 errno, strerror(errno));
	}

	iv_fd_set_cloexec(lock->fd[0]);
	iv_fd_set_cloexec(lock->fd[1]);

	do {
		ret = write(lock->fd[1], "", 1);
	} while (ret < 0 && errno == EINTR);
}

static inline void fallback_spin_lock(fallback_spinlock_t *lock)
{
	char c;
	int ret;

	ret = read(lock->fd[0], &c, 1);
	if (ret == 1)
		return;

	if (ret < 0) {
		iv_fatal("fallback_spin_lock: read() returned error %d[%s]",
			 errno, strerror(errno));
	} else {
		iv_fatal("fallback_spin_lock: read() returned %d", ret);
	}
}

static inline void fallback_spin_unlock(fallback_spinlock_t *lock)
{
	int ret;

	do {
		ret = write(lock->fd[1], "", 1);
	} while (ret < 0 && errno == EINTR);
}


#ifdef HAVE_PTHREAD_SPINLOCK_T
#ifdef HAVE_PRAGMA_WEAK
#pragma weak pthread_spin_trylock
#endif

static inline int pthread_spinlocks_available(void)
{
	return !!(pthread_spin_trylock != NULL);
}


#ifdef HAVE_PRAGMA_WEAK
#pragma weak pthread_spin_init
#pragma weak pthread_spin_lock
#pragma weak pthread_spin_unlock
#endif

typedef union {
	pthread_spinlock_t	ps;
	fallback_spinlock_t	fs;
} spinlock_t;

static inline void spin_init(spinlock_t *lock)
{
	if (pthread_spinlocks_available())
		pthread_spin_init(&lock->ps, PTHREAD_PROCESS_PRIVATE);
	else if (pthreads_available())
		fallback_spin_init(&lock->fs);
}

static inline void spin_lock(spinlock_t *lock)
{
	if (pthread_spinlocks_available())
		pthread_spin_lock(&lock->ps);
	else if (pthreads_available())
		fallback_spin_lock(&lock->fs);
}

static inline void spin_unlock(spinlock_t *lock)
{
	if (pthread_spinlocks_available())
		pthread_spin_unlock(&lock->ps);
	else if (pthreads_available())
		fallback_spin_unlock(&lock->fs);
}
#else
typedef fallback_spinlock_t spinlock_t;

static inline void spin_init(spinlock_t *lock)
{
	if (pthreads_available())
		fallback_spin_init(lock);
}

static inline void spin_lock(spinlock_t *lock)
{
	if (pthreads_available())
		fallback_spin_lock(lock);
}

static inline void spin_unlock(spinlock_t *lock)
{
	if (pthreads_available())
		fallback_spin_unlock(lock);
}
#endif


static inline void spin_lock_sigmask(spinlock_t *lock, sigset_t *mask)
{
	sigset_t all;

	sigfillset(&all);
	pthr_sigmask(SIG_BLOCK, &all, mask);

	spin_lock(lock);
}

static inline void spin_unlock_sigmask(spinlock_t *lock, sigset_t *mask)
{
	spin_unlock(lock);

	pthr_sigmask(SIG_SETMASK, mask, NULL);
}
