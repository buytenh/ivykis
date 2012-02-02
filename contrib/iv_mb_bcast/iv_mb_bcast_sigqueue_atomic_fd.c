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
#include <iv.h>
#include <iv_list.h>
#include <iv_mb_bcast.h>
#include <iv_tls.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>

static pthread_mutex_t iv_mb_bcast_lock;
static struct iv_list_head iv_mb_bcast_threads;
static int iv_mb_bcast_signum;

struct iv_mb_bcast_thr_info {
	struct iv_list_head	list;
	pthread_t		me;
};

static void iv_mb_bcast_tls_init_thread(void *_tinfo)
{
	struct iv_mb_bcast_thr_info *tinfo = _tinfo;

	pthread_mutex_lock(&iv_mb_bcast_lock);
	iv_list_add_tail(&tinfo->list, &iv_mb_bcast_threads);
	tinfo->me = pthread_self();
	pthread_mutex_unlock(&iv_mb_bcast_lock);
}

static void iv_mb_bcast_tls_deinit_thread(void *_tinfo)
{
	struct iv_mb_bcast_thr_info *tinfo = _tinfo;

	pthread_mutex_lock(&iv_mb_bcast_lock);
	iv_list_del(&tinfo->list);
	pthread_mutex_unlock(&iv_mb_bcast_lock);
}

static struct iv_tls_user iv_mb_bcast_tls_user = {
	.sizeof_state	= sizeof(struct iv_mb_bcast_thr_info),
	.init_thread	= iv_mb_bcast_tls_init_thread,
	.deinit_thread	= iv_mb_bcast_tls_deinit_thread,
};

struct iv_mb_bcast_info {
	unsigned long counter;
	int fd[2];
};

static void iv_mb_bcast_signal_handler(int signum, siginfo_t *si, void *uctx)
{
	struct iv_mb_bcast_info *info = si->si_ptr;

	if (si->si_pid != getpid())
		return;

	__sync_synchronize();

	if (__sync_sub_and_fetch(&info->counter, 1) == 0)
		write(info->fd[1], "", 1);
}

extern int __libc_allocate_rtsig(int low);

static void iv_mb_bcast_init(void) __attribute__((constructor));
static void iv_mb_bcast_init(void)
{
	struct sigaction sa;

	pthread_mutex_init(&iv_mb_bcast_lock, NULL);
	INIT_IV_LIST_HEAD(&iv_mb_bcast_threads);
	iv_mb_bcast_signum = __libc_allocate_rtsig(1);

	sa.sa_sigaction = iv_mb_bcast_signal_handler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sigaction(iv_mb_bcast_signum, &sa, NULL);

	iv_tls_user_register(&iv_mb_bcast_tls_user);
}

static __thread struct iv_mb_bcast_info info;

void iv_mb_bcast_init_thread(void)
{
	if (pipe(info.fd) < 0) {
		perror("pipe");
		abort();
	}
}

void iv_mb_bcast(void)
{
	struct iv_mb_bcast_thr_info *my_tinfo;
	union sigval sv;
	struct iv_list_head *ilh;
	int ret;

	my_tinfo = iv_tls_user_ptr(&iv_mb_bcast_tls_user);
	sv.sival_ptr = &info;

//	pthread_mutex_lock(&iv_mb_bcast_lock);

	info.counter = 0;
	iv_list_for_each (ilh, &iv_mb_bcast_threads) {
		struct iv_mb_bcast_thr_info *tinfo;

		tinfo = iv_container_of(ilh,
				struct iv_mb_bcast_thr_info, list);
		if (tinfo == my_tinfo)
			continue;

		info.counter++;
	}

	__sync_synchronize();

	iv_list_for_each (ilh, &iv_mb_bcast_threads) {
		struct iv_mb_bcast_thr_info *tinfo;

		tinfo = iv_container_of(ilh,
				struct iv_mb_bcast_thr_info, list);
		if (tinfo == my_tinfo)
			continue;

		pthread_sigqueue(tinfo->me, iv_mb_bcast_signum, sv);
	}

	do {
		char foo;
		ret = read(info.fd[0], &foo, sizeof(foo));
	} while (ret < 0 && errno == EINTR);

//	pthread_mutex_unlock(&iv_mb_bcast_lock);

	if (ret < 0) {
		perror("read");
		abort();
	}
}
