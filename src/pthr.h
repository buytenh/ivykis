/*
 * ivykis, an event handling library
 * Copyright (C) 2013, 2016 Lennert Buytenhek
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

#ifndef __PTHR_H
#define __PTHR_H

#include <pthread.h>
#include <signal.h>

#ifdef HAVE_PRAGMA_WEAK
#pragma weak pthread_create
#endif

static inline int pthreads_available(void)
{
	return !!(pthread_create != NULL);
}


#ifdef HAVE_PRAGMA_WEAK

/*
 * On Linux, pthread_atfork() is defined in libpthread_nonshared.a,
 * a static library, and we want to avoid "#pragma weak" for that
 * symbol because that causes it to be undefined even if you link
 * libpthread_nonshared.a in explicitly.
 */
#ifndef HAVE_LIBPTHREAD_NONSHARED
#pragma weak pthread_atfork
#endif

#pragma weak pthread_create
#pragma weak pthread_detach
#pragma weak pthread_getspecific
#pragma weak pthread_join
#pragma weak pthread_key_create
#pragma weak pthread_once
#pragma weak pthread_self
#pragma weak pthread_setspecific
#pragma weak pthread_sigmask
#endif

typedef union {
	pthread_key_t	pk;
	const void	*ptr;
} pthr_key_t;

typedef struct {
	int		called;
	pthread_once_t	po;
} pthr_once_t;

#define PTHR_ONCE_INIT	{ 0, PTHREAD_ONCE_INIT, }

static inline int
pthr_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	if (pthreads_available())
		return pthread_atfork(prepare, parent, child);

	return ENOSYS;
}

static inline int pthr_create(pthread_t *thread, const pthread_attr_t *attr,
			      void *(*start_routine)(void *), void *arg)
{
	if (pthreads_available())
		return pthread_create(thread, attr, start_routine, arg);

	return ENOSYS;
}

static inline int pthr_detach(pthread_t thread)
{
	if (pthreads_available())
		return pthread_detach(thread);

	iv_fatal("pthr_detach: called while pthreads isn't available");

	return ENOSYS;
}

static inline void *pthr_getspecific(pthr_key_t *key)
{
	if (pthreads_available())
		return pthread_getspecific(key->pk);

	return (void *)key->ptr;
}

static inline int pthr_join(pthread_t thread, void **retval)
{
	if (pthreads_available())
		return pthread_join(thread, retval);

	iv_fatal("pthr_join: called while pthreads isn't available");

	return ENOSYS;
}

static inline int pthr_key_create(pthr_key_t *key, void (*destructor)(void*))
{
	if (pthreads_available())
		return pthread_key_create(&key->pk, destructor);

	return 0;
}

static inline int pthr_once(pthr_once_t *once, void (*fn)(void))
{
	if (pthreads_available())
		return pthread_once(&once->po, fn);

	if (once->called == 0) {
		fn();
		once->called = 1;
	}

	return 0;
}

static inline unsigned long pthr_self(void)
{
	if (pthreads_available())
		return (unsigned long)pthread_self();

	return getpid();
}

static inline int pthr_setspecific(pthr_key_t *key, const void *value)
{
	if (pthreads_available())
		return pthread_setspecific(key->pk, value);

	key->ptr = value;

	return 0;
}

static inline int pthr_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
	if (pthreads_available())
		return pthread_sigmask(how, set, oldset);

	return sigprocmask(how, set, oldset);
}


#endif
