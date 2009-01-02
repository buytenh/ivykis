/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009 Lennert Buytenhek
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include "iv_private.h"

/* main loop ****************************************************************/
#define MAX_QUOTUM		(16)

static struct list_head		arrays[2];
struct list_head		*active;
static struct list_head		*expired;
static unsigned int		epoch;
static struct iv_fd_		*handled_fd;
static int			maxfd;
static struct iv_poll_method	*method;
static int			numfds;
static int			quit;

static int sanitise_nofile_rlimit(int euid)
{
	struct rlimit lim;
	int max_files;

	getrlimit(RLIMIT_NOFILE, &lim);
	max_files = lim.rlim_cur;

	if (euid) {
		if (lim.rlim_cur < lim.rlim_max) {
			lim.rlim_cur = (unsigned int)lim.rlim_max & 0x7FFFFFFF;
			if (lim.rlim_cur > 131072)
				lim.rlim_cur = 131072;

			if (setrlimit(RLIMIT_NOFILE, &lim) >= 0)
				max_files = lim.rlim_cur;
		}
	} else {
		lim.rlim_cur = 131072;
		lim.rlim_max = 131072;
		while (lim.rlim_cur > max_files) {
			if (setrlimit(RLIMIT_NOFILE, &lim) >= 0) {
				max_files = lim.rlim_cur;
				break;
			}

			lim.rlim_cur /= 2;
			lim.rlim_max /= 2;
		}
	}

	return max_files;
}

static int method_is_excluded(char *exclude, char *name)
{
	if (exclude != NULL) {
		char method_name[64];
		int len;

		while (sscanf(exclude, "%63s%n", method_name, &len) > 0) {
			if (!strcmp(name, method_name))
				return 1;
			exclude += len;
		}
	}

	return 0;
}

static void consider_poll_method(char *exclude, struct iv_poll_method *m)
{
	if (method == NULL && !method_is_excluded(exclude, m->name)) {
		if (m->init(maxfd) >= 0)
			method = m;
	}
}

void iv_init(void)
{
	char *exclude;
	int euid;

	euid = geteuid();

	signal(SIGPIPE, SIG_IGN);
	signal(SIGURG, SIG_IGN);

	INIT_LIST_HEAD(arrays);
	INIT_LIST_HEAD(arrays + 1);
	active = arrays;
	expired = arrays + 1;
	epoch = 0;
	handled_fd = NULL;
	maxfd = sanitise_nofile_rlimit(euid);
	numfds = 0;
	method = NULL;

	exclude = getenv("IV_EXCLUDE_POLL_METHOD");
	if (exclude != NULL && getuid() != euid)
		exclude = NULL;

#ifdef linux
	consider_poll_method(exclude, &iv_method_epoll);
#endif
#if defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__NetBSD_) || defined(__OpenBSD__)
	consider_poll_method(exclude, &iv_method_kqueue);
#endif
#ifdef sun
	consider_poll_method(exclude, &iv_method_dev_poll);
#endif
	consider_poll_method(exclude, &iv_method_poll);
	consider_poll_method(exclude, &iv_method_select);

	if (method == NULL) {
		syslog(LOG_CRIT, "iv_init: can't find suitable event "
				 "dispatcher");
		abort();
	}

	iv_task_init();
	iv_timer_init();
}

void iv_quit(void)
{
	quit = 1;
}

static void iv_run_active_list(void)
{
	while (!list_empty(active)) {
		struct iv_fd_ *fd;

		fd = list_entry(active->next, struct iv_fd_, list_active);

		if (fd->handler_in == NULL && fd->handler_out == NULL &&
		    fd->handler_err == NULL) {
			syslog(LOG_CRIT, "iv_run_active_list: active fd "
					 "while no handlers set!");
			abort();
		}

		if (fd->quotum--) {
			handled_fd = fd;

			if (fd->flags & (1 << FD_ReadyErr) &&
			    fd->handler_err != NULL)
				fd->handler_err(fd->cookie);

			if (handled_fd != NULL &&
			    fd->flags & (1 << FD_ReadyIn) &&
			    fd->handler_in != NULL)
				fd->handler_in(fd->cookie);

			if (handled_fd != NULL &&
			    fd->flags & (1 << FD_ReadyOut) &&
			    fd->handler_out != NULL)
				fd->handler_out(fd->cookie);

			continue;
		}

		list_del(&fd->list_active);
		list_add_tail(&fd->list_active, expired);
		fd->epoch = epoch + 1;
		fd->quotum = MAX_QUOTUM;
	}
}

static int should_quit(void)
{
	return quit || (!numfds && !iv_pending_tasks() && !iv_pending_timers());
}

void iv_main(void)
{
	quit = 0;

	while (!should_quit()) {
		struct timespec to;

		iv_invalidate_now();

		do {
			iv_run_active_list();
			iv_run_timers();
			iv_run_tasks();
		} while (!list_empty(active));

		if (!list_empty(expired)) {
			method->poll(0);
			if (list_empty(active)) {
				struct list_head *temp;
				temp = active;
				active = expired;
				expired = temp;
				epoch = (epoch + 1) & 0xFFFF;
			}

			continue;
		}

		if (!should_quit() && !iv_get_soonest_timeout(&to)) {
			int msec;

			epoch = (epoch + 1) & 0xFFFF;
			msec = 1000 * to.tv_sec;
			msec += (to.tv_nsec + 999999) / 1000000;
			method->poll(msec);
		}
	}
}


/* file descriptor handling *************************************************/
void INIT_IV_FD(struct iv_fd *_fd)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	fd->fd = -1;
	fd->handler_in = NULL;
	fd->handler_out = NULL;
	fd->handler_err = NULL;
	INIT_LIST_HEAD(&fd->list_all);
}

void iv_register_fd(struct iv_fd *_fd)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int flags;
	int yes;

	if (!list_empty(&fd->list_all)) {
		syslog(LOG_CRIT, "iv_register_fd: called with fd which "
				 "is still registered");
		abort();
	}

	if (fd->fd < 0 || fd->fd >= maxfd) {
		syslog(LOG_CRIT, "iv_register_fd: called with invalid fd "
				 "%d (maxfd=%d)", fd->fd, maxfd);
		abort();
	}

	flags = fcntl(fd->fd, F_GETFL);
	if (!(flags & O_NONBLOCK)) {
		flags |= O_NONBLOCK;
		fcntl(fd->fd, F_SETFL, flags);
	}

	yes = 1;
	setsockopt(fd->fd, SOL_SOCKET, SO_OOBINLINE, &yes, sizeof(yes));

	INIT_LIST_HEAD(&fd->list_active);
	fd->flags = 0;
	fd->epoch = epoch - 1;

	numfds++;
	method->register_fd(fd);
}

void iv_unregister_fd(struct iv_fd *_fd)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;

	if (list_empty(&fd->list_all)) {
		syslog(LOG_CRIT, "iv_unregister_fd: called with fd which "
				 "is not registered");
		abort();
	}

	method->unregister_fd(fd);
	numfds--;
	list_del(&fd->list_active);

	if (handled_fd == fd)
		handled_fd = NULL;
}

static int should_be_active(struct iv_fd_ *fd)
{
	if (fd->flags & (1 << FD_ReadyIn) && fd->handler_in != NULL)
		return 1;
	if (fd->flags & (1 << FD_ReadyOut) && fd->handler_out != NULL)
		return 1;
	if (fd->flags & (1 << FD_ReadyErr) && fd->handler_err != NULL)
		return 1;

	return 0;
}

static void make_active(struct iv_fd_ *fd)
{
	if (list_empty(&fd->list_active)) {
		list_add_tail(&fd->list_active, active);
		if (fd->epoch != epoch) {
			fd->epoch = epoch;
			fd->quotum = MAX_QUOTUM;
		}
	}
}

void iv_fd_make_ready(struct iv_fd_ *fd, int band)
{
	fd->flags |= 1 << band;
	if (should_be_active(fd))
		make_active(fd);
}

static void make_unready(struct iv_fd_ *fd, int band)
{
	fd->flags &= ~(1 << band);
	if (!should_be_active(fd))
		list_del_init(&fd->list_active);

	if (method->reregister_fd != NULL)
		method->reregister_fd(fd);
}

void iv_fd_set_handler_in(struct iv_fd *_fd, void (*handler_in)(void *))
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	void (*old_handler_in)(void *);
	int rereg;

	if (list_empty(&fd->list_all)) {
		syslog(LOG_CRIT, "iv_fd_set_handler_in: called with fd "
				 "which is not registered");
		abort();
	}

	old_handler_in = fd->handler_in;
	fd->handler_in = handler_in;
	rereg = 0;

	if (handler_in != NULL) {
		if (old_handler_in == NULL) {
			if (fd->flags & (1 << FD_ReadyIn))
				make_active(fd);
			rereg = 1;
		}
	} else if (old_handler_in != NULL) {
		if (!should_be_active(fd))
			list_del_init(&fd->list_active);
		rereg = 1;
	}

	if (rereg && method->reregister_fd != NULL)
		method->reregister_fd(fd);
}

void iv_fd_set_handler_out(struct iv_fd *_fd, void (*handler_out)(void *))
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	void (*old_handler_out)(void *);
	int rereg;

	if (list_empty(&fd->list_all)) {
		syslog(LOG_CRIT, "iv_fd_set_handler_out: called with fd "
				 "which is not registered");
		abort();
	}

	old_handler_out = fd->handler_out;
	fd->handler_out = handler_out;
	rereg = 0;

	if (handler_out != NULL) {
		if (old_handler_out == NULL) {
			if (fd->flags & (1 << FD_ReadyOut))
				make_active(fd);
			rereg = 1;
		}
	} else if (old_handler_out != NULL) {
		if (!should_be_active(fd))
			list_del_init(&fd->list_active);
		rereg = 1;
	}

	if (rereg && method->reregister_fd != NULL)
		method->reregister_fd(fd);
}

void iv_fd_set_handler_err(struct iv_fd *_fd, void (*handler_err)(void *))
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	void (*old_handler_err)(void *);
	int rereg;

	if (list_empty(&fd->list_all)) {
		syslog(LOG_CRIT, "iv_fd_set_handler_err: called with fd "
				 "which is not registered");
		abort();
	}

	old_handler_err = fd->handler_err;
	fd->handler_err = handler_err;
	rereg = 0;

	if (handler_err != NULL) {
		if (old_handler_err == NULL) {
			if (fd->flags & (1 << FD_ReadyErr))
				make_active(fd);
			rereg = 1;
		}
	} else if (old_handler_err != NULL) {
		if (!should_be_active(fd))
			list_del_init(&fd->list_active);
		rereg = 1;
	}

	if (rereg && method->reregister_fd != NULL)
		method->reregister_fd(fd);
}


/* wrapping *****************************************************************/
int iv_accept(struct iv_fd *_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = accept(fd->fd, addr, addrlen);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

int iv_connect(struct iv_fd *_fd, struct sockaddr *addr, socklen_t addrlen)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = connect(fd->fd, addr, addrlen);
	if (ret == -1 && (errno == EINPROGRESS || errno == EALREADY)) {
		make_unready(fd, FD_ReadyIn);
		make_unready(fd, FD_ReadyOut);
	}

	return ret;
}

ssize_t iv_read(struct iv_fd *_fd, void *buf, size_t count)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = read(fd->fd, buf, count);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

ssize_t iv_readv(struct iv_fd *_fd, const struct iovec *vector, int count)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = readv(fd->fd, vector, count);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

int iv_recv(struct iv_fd *_fd, void *buf, size_t len, int flags)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = recv(fd->fd, buf, len, flags);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

int iv_recvfrom(struct iv_fd *_fd, void *buf, size_t len, int flags,
		struct sockaddr *from, socklen_t *fromlen)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = recvfrom(fd->fd, buf, len, flags, from, fromlen);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

int iv_recvmsg(struct iv_fd *_fd, struct msghdr *msg, int flags)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = recvmsg(fd->fd, msg, flags);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

int iv_send(struct iv_fd *_fd, const void *msg, size_t len, int flags)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = send(fd->fd, msg, len, flags);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}

#ifdef linux
#include <sys/sendfile.h>

ssize_t iv_sendfile(struct iv_fd *_fd, int in_fd, off_t *offset, size_t count)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = sendfile(fd->fd, in_fd, offset, count);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}
#endif

int iv_sendmsg(struct iv_fd *_fd, const struct msghdr *msg, int flags)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = sendmsg(fd->fd, msg, flags);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}

int iv_sendto(struct iv_fd *_fd, const void *msg, size_t len, int flags,
	      const struct sockaddr *to, socklen_t tolen)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = sendto(fd->fd, msg, len, flags, to, tolen);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}

ssize_t iv_write(struct iv_fd *_fd, const void *buf, size_t count)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = write(fd->fd, buf, count);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}

ssize_t iv_writev(struct iv_fd *_fd, const struct iovec *vector, int count)
{
	struct iv_fd_ *fd = (struct iv_fd_ *)_fd;
	int ret;

	ret = writev(fd->fd, vector, count);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}
