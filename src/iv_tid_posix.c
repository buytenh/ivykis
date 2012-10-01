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

#include <iv.h>
#include "iv_private.h"

#ifdef HAVE_PROCESS_H
#include <process.h>
#endif
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif
#ifdef HAVE_SYS_THR_H
/* Older FreeBSDs (6.1) don't include sys/ucontext.h in sys/thr.h.  */
#include <sys/ucontext.h>
#include <sys/thr.h>
#endif
#ifdef HAVE_THREAD_H
#include <thread.h>
#endif

unsigned long iv_get_thread_id(void)
{
	unsigned long thread_id;

#if defined(__NR_gettid)
	thread_id = syscall(__NR_gettid);
#elif defined(HAVE_GETTID) && defined(HAVE_PROCESS_H)
	thread_id = gettid();
#elif defined(HAVE_LWP_GETTID)
	thread_id = lwp_gettid();
#elif defined(HAVE_THR_SELF) && defined(HAVE_SYS_THR_H)
	long thr;
	thr_self(&thr);
	thread_id = (unsigned long)thr;
#elif defined(HAVE_THR_SELF) && defined(HAVE_THREAD_H)
	thread_id = thr_self();
#else
#warning using pthread_self for iv_get_thread_id
	thread_id = pthr_self();
#endif

	return thread_id;
}
