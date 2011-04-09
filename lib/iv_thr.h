/*
 * ivykis, an event handling library
 * Copyright (C) 2010 Lennert Buytenhek
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

#ifndef __IV_THR_H_INCLUDED
#define __IV_THR_H_INCLUDED

#include <pthread.h>
#include <signal.h>
#include "config.h"

#pragma weak pthread_mutex_init
static inline void
pthr_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *attr)
{
	if (pthread_mutex_init != NULL)
		pthread_mutex_init(m, attr);
}

#pragma weak pthread_mutex_destroy
static inline void pthr_mutex_destroy(pthread_mutex_t *m)
{
	if (pthread_mutex_destroy != NULL)
		pthread_mutex_destroy(m);
}

#pragma weak pthread_mutex_lock
static inline void pthr_mutex_lock(pthread_mutex_t *m)
{
	if (pthread_mutex_lock != NULL)
		pthread_mutex_lock(m);
}

#pragma weak pthread_mutex_unlock
static inline void pthr_mutex_unlock(pthread_mutex_t *m)
{
	if (pthread_mutex_unlock != NULL)
		pthread_mutex_unlock(m);
}

#pragma weak pthread_sigmask
static inline int pthr_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
	if (pthread_sigmask != NULL)
		return pthread_sigmask(how, set, oldset);
	else
		return sigprocmask(how, set, oldset);
}

#if HAVE_PTHREAD_SPIN_LOCK

#pragma weak pthread_spin_init
static inline int pthr_spin_init(pthread_spinlock_t *lock, int pshared)
{
	if (pthread_spin_init != NULL)
		return pthread_spin_init(lock, pshared);

	return 0;
}

#pragma weak pthread_spin_lock
static inline int pthr_spin_lock(pthread_spinlock_t *lock)
{
	if (pthread_spin_lock != NULL)
		return pthread_spin_lock(lock);

	return 0;
}

#pragma weak pthread_spin_unlock
static inline int pthr_spin_unlock(pthread_spinlock_t *lock)
{
	if (pthread_spin_unlock != NULL)
		return pthread_spin_unlock(lock);

	return 0;
}

#else

typedef int pthread_spinlock_t;

#define pthr_spin_init(l, s)
#define pthr_spin_lock(l)
#define pthr_spin_unlock(l)

#endif

#if !HAVE_TLS
static struct __tls_variables *__tls_init_thread(pthread_key_t key, size_t size)
{
	struct __tls_variables *ptr;

	ptr = calloc(1, size);
	if (!ptr)
		abort();
	pthread_setspecific(key, ptr);
	return ptr;
}

static inline struct __tls_variables *__tls_deref_helper(pthread_key_t key, size_t size)
{
	struct __tls_variables *ptr;

	ptr = pthread_getspecific(key);
	if (!ptr)
		ptr = __tls_init_thread(key, size);
	return ptr;
}

#define TLS_BLOCK_START    \
	static pthread_key_t __tls_key;					\
	static void __attribute__((constructor)) __tls_init_key(void)   \
	{								\
		pthread_key_create(&__tls_key, free);			\
	}								\
									\
	struct __tls_variables

#define TLS_BLOCK_END

#define __tls_deref(var)   (*({ struct __tls_variables *__ptr = __tls_deref_helper(__tls_key, sizeof(struct __tls_variables)); &__ptr->var; }))

#define __thread #

#else  /* HAVE_TLS */

#define TLS_BLOCK_START				\
	struct __tls_variables

#define TLS_BLOCK_END				      \
	;					      \
	static __thread struct __tls_variables __tls


#define __tls_deref(var)   (__tls.var)

#endif

#endif /* __THR_H_INCLUDED */
