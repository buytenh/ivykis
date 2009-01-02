/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef linux

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "iv_private.h"

#pragma weak pthread_create
int pthread_create(pthread_t *, pthread_attr_t *, void *(*)(void *), void *);


#define HASH_SIZE		(512)
#define POLL_BATCH_SIZE		(1024)

static struct list_head		*all;
static int			my_pid;
static int			sigrtmin;
static struct list_head		undetermined_queue;

static unsigned int __fd_hash(unsigned int fd)
{
	return fd % HASH_SIZE;
}

static struct iv_fd *find_fd(int fd)
{
	int hash = __fd_hash(fd);
	struct list_head *lh;
	struct iv_fd *ret = NULL;

	list_for_each (lh, &all[hash]) {
		struct iv_fd *f;

		f = list_entry(lh, struct iv_fd, list_all);
		if (f->fd == fd) {
			ret = f;
			break;
		}
	}

	return ret;
}


/* interface ****************************************************************/
int __libc_allocate_rtsig(int);

static int iv_rtsig_init(int maxfd)
{
	struct sigaction act;
	sigset_t sigset;
	int i;

	/* Real-time signals combined with threads doesn't behave
	 * in a way we can accommodate.  */
	if (&pthread_create != NULL)
		return -1;

	all = malloc(HASH_SIZE * sizeof(*all));
	if (all == NULL)
		return -1;

	for (i=0;i<HASH_SIZE;i++)
		INIT_LIST_HEAD(&all[i]);
	my_pid = getpid();
	sigrtmin = __libc_allocate_rtsig(1);
	INIT_LIST_HEAD(&undetermined_queue);

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGIO);
	sigaddset(&sigset, sigrtmin);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	act.sa_handler = NULL;
	sigemptyset(&(act.sa_mask));
	act.sa_flags = SA_RESTART | SA_SIGINFO;
	sigaction(SIGIO, &act, NULL);

	act.sa_sigaction = NULL;
	sigemptyset(&(act.sa_mask));
	act.sa_flags = SA_RESTART | SA_SIGINFO;
	sigaction(sigrtmin, &act, NULL);

	return 0;
}

static void flush(struct pollfd *fds, struct iv_fd **fd, int qcount)
{
	if (poll(fds, qcount, 0) > 0) {
		int i;

		for (i=0;i<qcount;i++) {
			if (fds[i].revents & (POLLIN | POLLERR | POLLHUP))
				iv_fd_make_ready(fd[i], FD_ReadyIn);
			if (fds[i].revents & (POLLOUT | POLLERR))
				iv_fd_make_ready(fd[i], FD_ReadyOut);
			if (fds[i].revents & POLLERR)
				iv_fd_make_ready(fd[i], FD_ReadyErr);
		}
	}
}

static int __is_worth_polling(int flags)
{
	if ((flags & FD_ReadyMask) != FD_ReadyMask)
		return 1;
	return 0;
}

static void mass_poll(void)
{
	struct pollfd pfds[POLL_BATCH_SIZE];
	struct iv_fd *fds[POLL_BATCH_SIZE];
	int htable;
	int qcount;

	qcount = 0;
	for (htable=0;htable<HASH_SIZE;htable++) {
		struct list_head *lh;

		list_for_each (lh, &all[htable]) {
			struct iv_fd *fd;

			fd = list_entry(lh, struct iv_fd, list_all);
			if (!__is_worth_polling(fd->flags))
				continue;

			fds[qcount] = fd;
			pfds[qcount].fd = fd->fd;
			pfds[qcount].events = POLLERR;
			if (!(fd->flags & 1 << FD_ReadyIn))
				pfds[qcount].events |= POLLIN | POLLHUP;
			if (!(fd->flags & 1 << FD_ReadyOut))
				pfds[qcount].events |= POLLOUT;

			if (++qcount == POLL_BATCH_SIZE) {
				flush(pfds, fds, POLL_BATCH_SIZE);
				qcount = 0;
			}
		}
	}

	flush(pfds, fds, qcount);
}

static void handle_siginfo(siginfo_t *sig, int *overload)
{
	if (sig->si_signo == SIGIO) {
		*overload = 1;
	} else if (!*overload && sig->si_signo == sigrtmin) {
		struct iv_fd *fd;

		fd = find_fd(sig->si_fd);
		if (fd != NULL) {
			if (sig->si_band & (POLLIN | POLLERR | POLLHUP))
				iv_fd_make_ready(fd, FD_ReadyIn);
			if (sig->si_band & (POLLOUT | POLLERR))
				iv_fd_make_ready(fd, FD_ReadyOut);
			if (sig->si_band & POLLERR)
				iv_fd_make_ready(fd, FD_ReadyErr);
		}
	}
}

static void flush_undetermined_poll_list(struct pollfd *pfds, int qcount)
{
	int i;
	int ret;

	do {
		ret = poll(pfds, qcount, 0);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		syslog(LOG_CRIT, "flush_undetermined_poll_list: got error "
				 "%d[%s]", errno, strerror(errno));
		abort();
	}

	for (i=0;i<qcount;i++) {
		struct iv_fd *fd;
		int revents;

		fd = list_entry(undetermined_queue.next, struct iv_fd,
				list_all);

#if IV_DEBUG
		if (fd->fd != pfds[i].fd) {
			syslog(LOG_CRIT, "flush_undetermined_poll_list: just "
					 "puked on myself... eeeeeeeeeeew");
			abort();
		}
#endif

		revents = pfds[i].revents;
		if (revents & (POLLIN | POLLERR | POLLHUP))
			iv_fd_make_ready(fd, FD_ReadyIn);
		if (revents & (POLLOUT | POLLERR))
			iv_fd_make_ready(fd, FD_ReadyOut);
		if (revents & POLLERR)
			iv_fd_make_ready(fd, FD_ReadyErr);

		list_del(&(fd->list_all));
		list_add_tail(&(fd->list_all), &all[__fd_hash(fd->fd)]);
	}
}

static void flush_undetermined_queue(void)
{
	struct pollfd pfds[POLL_BATCH_SIZE];
	struct list_head *lh;
	struct list_head *lh2;
	int qcount;

	qcount = 0;
	list_for_each_safe (lh, lh2, &undetermined_queue) {
		struct iv_fd *fd;

		fd = list_entry(lh, struct iv_fd, list_all);

		pfds[qcount].fd = fd->fd;
		pfds[qcount].events = POLLIN | POLLERR | POLLHUP | POLLOUT;
		if (++qcount == POLL_BATCH_SIZE) {
			flush_undetermined_poll_list(pfds, qcount);
			qcount = 0;
		}
	}

	flush_undetermined_poll_list(pfds, qcount);
}

static void iv_rtsig_poll(int timeout)
{
	struct timespec to;
	sigset_t sigset;
	siginfo_t info;

	if (!list_empty(&undetermined_queue)) {
		flush_undetermined_queue();
		if (!list_empty(active))
			return;
	}

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGIO);
	sigaddset(&sigset, sigrtmin);

	to.tv_sec = timeout / 1000;
	to.tv_nsec = (timeout % 1000) * 1000000;

	if (sigtimedwait(&sigset, &info, &to) >= 0) {
		int overload = 0;

		handle_siginfo(&info, &overload);

		sigdelset(&sigset, SIGIO);
		to.tv_sec = 0;
		to.tv_nsec = 0;
		while (sigtimedwait(&sigset, &info, &to) >= 0)
			handle_siginfo(&info, &overload);

		if (overload)
			mass_poll();
	}
}

static void iv_rtsig_register_fd(struct iv_fd *fd)
{
	int fl;

	list_add_tail(&(fd->list_all), &undetermined_queue);

	fcntl(fd->fd, F_SETSIG, sigrtmin);
	fcntl(fd->fd, F_SETOWN, my_pid);

	fl = fcntl(fd->fd, F_GETFL);
	if (fl >= 0 && !(fl & O_ASYNC)) {
		fl |= O_ASYNC;
		fcntl(fd->fd, F_SETFL, fl);
	}
}

/* There is a small race window here.  Someone might unregister (or
 * close) an fd while there is still a queued signal for it.  When this
 * signal is eventually processed, it will either hit no registered fd
 * at all, or it will hit a new incarnation of this fd.  Both cases are
 * harmless, so we choose to do nothing at all. */
static void iv_rtsig_unregister_fd(struct iv_fd *fd)
{
	int fl;

	fl = fcntl(fd->fd, F_GETFL);
	if (fl >= 0) {
		fl &= ~O_ASYNC;
		fcntl(fd->fd, F_SETFL, fl);
	}

	list_del_init(&(fd->list_all));
}

static void iv_rtsig_deinit(void)
{
	free(all);
}


struct iv_poll_method iv_method_rtsig = {
	name:			"rtsig",
	init:			iv_rtsig_init,
	poll:			iv_rtsig_poll,
	register_fd:		iv_rtsig_register_fd,
	reregister_fd:		NULL,
	unregister_fd:		iv_rtsig_unregister_fd,
	deinit:			iv_rtsig_deinit,
};
#endif
