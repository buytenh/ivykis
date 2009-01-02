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

#define MAX_QUOTUM		(16)

#if ULONG_MAX == 4294967295UL
#define LOG_BITS_PER_LONG	(5)
#define LOG_BIT_MASK		(0x1f)
#else
#if ULONG_MAX == 18446744073709551615ULL
#define LOG_BITS_PER_LONG	(6)
#define LOG_BIT_MASK		(0x3f)
#else
#error unknown sizeof(unsigned long)
#endif
#endif



/****************************************************************************/
#if IV_DEBUG
static int test_bit(int nr, unsigned long *_addr)
{
	unsigned long *addr;
	unsigned long mask;

	addr = _addr + (nr >> LOG_BITS_PER_LONG);
	mask = 1UL << (nr & LOG_BIT_MASK);

	return !!(*addr & mask);
}

static int test_and_clear_bit(int nr, unsigned long *_addr)
{
	unsigned long *addr;
	unsigned long mask;
	int oldbit;

	addr = _addr + (nr >> LOG_BITS_PER_LONG);
	mask = 1UL << (nr & LOG_BIT_MASK);

	oldbit = !!(*addr & mask);
	*addr &= ~mask;

	return oldbit;
}

static int test_and_set_bit(int nr, unsigned long *_addr)
{
	unsigned long *addr;
	unsigned long mask;
	int oldbit;

	addr = _addr + (nr >> LOG_BITS_PER_LONG);
	mask = 1UL << (nr & LOG_BIT_MASK);

	oldbit = !!(*addr & mask);
	*addr |= mask;

	return oldbit;
}
#endif


/****************************************************************************/
static struct list_head		arrays[2];
struct list_head		*active;
static struct list_head		*expired;
static unsigned int		epoch;
static struct iv_fd		*handled_fd;
static int			maxfd;
static struct iv_poll_method	*method;
static int			numfds;
#if IV_DEBUG
static unsigned long		*registered_fds;
#endif


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
	consider_poll_method(exclude, &iv_method_epoll_lt);
#endif
#if defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__NetBSD_) || defined(__OpenBSD__)
	consider_poll_method(exclude, &iv_method_kqueue);
	consider_poll_method(exclude, &iv_method_kqueue_lt);
#endif
#ifdef sun
	consider_poll_method(exclude, &iv_method_dev_poll);
#endif
#ifdef linux
	consider_poll_method(exclude, &iv_method_rtsig);
#endif
	consider_poll_method(exclude, &iv_method_poll);
	consider_poll_method(exclude, &iv_method_select);

	if (method == NULL) {
		syslog(LOG_CRIT, "iv_init: can't find suitable event "
				 "dispatcher");
		abort();
	}

#if IV_DEBUG
	registered_fds = malloc((maxfd + 7) / 8);
	if (registered_fds == NULL) {
		syslog(LOG_CRIT, "iv_init: can't allocate memory for "
				 "fd bitmap");
		abort();
	}
	memset(registered_fds, 0, (maxfd + 7) / 8);
#endif

	iv_task_init();
	iv_timer_init();
}

void iv_register_fd(struct iv_fd *fd)
{
	int flags;
	int yes;

#if IV_DEBUG
	if (!list_empty(&(fd->list_all))) {
		syslog(LOG_CRIT, "iv_register_fd: called with fd which "
				 "is still registered");
		abort();
	}

	if (fd->fd < 0 || fd->fd >= maxfd) {
		syslog(LOG_CRIT, "iv_register_fd: called with invalid fd "
				 "%d (maxfd=%d)", fd->fd, maxfd);
		abort();
	}

	if (test_and_set_bit(fd->fd, registered_fds)) {
		syslog(LOG_CRIT, "iv_register_fd: fd %d already registered",
		       fd->fd);
		abort();
	}
#endif

	flags = fcntl(fd->fd, F_GETFL);
	if (!(flags & O_NONBLOCK)) {
		flags |= O_NONBLOCK;
		fcntl(fd->fd, F_SETFL, flags);
	}

	yes = 1;
	setsockopt(fd->fd, SOL_SOCKET, SO_OOBINLINE, &yes, sizeof(yes));

	INIT_LIST_HEAD(&(fd->list_active));
	fd->flags = 0;
	fd->epoch = epoch - 1;

	numfds++;
	method->register_fd(fd);
}

void iv_unregister_fd(struct iv_fd *fd)
{
#if IV_DEBUG
	if (list_empty(&(fd->list_all))) {
		syslog(LOG_CRIT, "iv_unregister_fd: called with fd which "
				 "is not registered");
		abort();
	}

	if (!test_and_clear_bit(fd->fd, registered_fds)) {
		syslog(LOG_CRIT, "iv_unregister_fd: fd %d not registered",
		       fd->fd);
		abort();
	}
#endif

	method->unregister_fd(fd);
	numfds--;
	list_del(&(fd->list_active));

	if (handled_fd == fd)
		handled_fd = NULL;
}

static int should_be_active(struct iv_fd *fd)
{
	if (fd->flags & (1 << FD_ReadyIn) && fd->handler_in != NULL)
		return 1;
	if (fd->flags & (1 << FD_ReadyOut) && fd->handler_out != NULL)
		return 1;
	if (fd->flags & (1 << FD_ReadyErr) && fd->handler_err != NULL)
		return 1;
	return 0;
}

static void make_active(struct iv_fd *fd)
{
	if (list_empty(&(fd->list_active))) {
		list_add_tail(&(fd->list_active), active);
		if (fd->epoch != epoch) {
			fd->epoch = epoch;
			fd->quotum = MAX_QUOTUM;
		}
	}
}

void iv_fd_make_ready(struct iv_fd *fd, int band)
{
	fd->flags |= 1 << band;
	if (should_be_active(fd))
		make_active(fd);
}

static void make_unready(struct iv_fd *fd, int band)
{
	fd->flags &= ~(1 << band);
	if (!should_be_active(fd))
		list_del_init(&(fd->list_active));

	if (method->reregister_fd != NULL)
		method->reregister_fd(fd);
}

void iv_fd_set_handler_in(struct iv_fd *fd, void (*handler_in)(void *))
{
	void (*old_handler_in)(void *);
	int rereg;

#if IV_DEBUG
	if (list_empty(&(fd->list_all))) {
		syslog(LOG_CRIT, "iv_fd_set_handler_in: called with fd "
				 "which is not registered");
		abort();
	}
#endif

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
			list_del_init(&(fd->list_active));
		rereg = 1;
	}

	if (rereg && method->reregister_fd != NULL)
		method->reregister_fd(fd);
}

void iv_fd_set_handler_out(struct iv_fd *fd, void (*handler_out)(void *))
{
	void (*old_handler_out)(void *);
	int rereg;

#if IV_DEBUG
	if (list_empty(&(fd->list_all))) {
		syslog(LOG_CRIT, "iv_fd_set_handler_out: called with fd "
				 "which is not registered");
		abort();
	}
#endif

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
			list_del_init(&(fd->list_active));
		rereg = 1;
	}

	if (rereg && method->reregister_fd != NULL)
		method->reregister_fd(fd);
}

void iv_fd_set_handler_err(struct iv_fd *fd, void (*handler_err)(void *))
{
	void (*old_handler_err)(void *);
	int rereg;

#if IV_DEBUG
	if (list_empty(&(fd->list_all))) {
		syslog(LOG_CRIT, "iv_fd_set_handler_err: called with fd "
				 "which is not registered");
		abort();
	}
#endif

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
			list_del_init(&(fd->list_active));
		rereg = 1;
	}

	if (rereg && method->reregister_fd != NULL)
		method->reregister_fd(fd);
}


/* main loop ****************************************************************/
static int quit;

void iv_quit(void)
{
	quit = 1;
}

static void iv_run_active_list(void)
{
	while (!list_empty(active)) {
		struct iv_fd *fd;

		fd = list_entry(active->next, struct iv_fd, list_active);
#if IV_DEBUG
		if (fd->handler_in == NULL && fd->handler_out == NULL &&
		    fd->handler_err == NULL) {
			syslog(LOG_CRIT, "iv_run_active_list: active fd "
					 "while no handlers set!");
			abort();
		}
#endif

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

		list_del(&(fd->list_active));
		list_add_tail(&(fd->list_active), expired);
		fd->epoch = epoch + 1;
		fd->quotum = MAX_QUOTUM;
	}
}

static int should_be_running(void)
{
	return !quit && (numfds || iv_pending_tasks() || iv_pending_timers());
}

void iv_main(void)
{
	quit = 0;

	while (should_be_running()) {
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

		if (should_be_running() && !iv_get_soonest_timeout(&to)) {
			int timeout;

			epoch = (epoch + 1) & 0xFFFF;
			timeout = 1000 * to.tv_sec;
			timeout += (to.tv_nsec + 999999) / 1000000;
			method->poll(timeout);
		}
	}
}


/* wrapping *****************************************************************/
#if IV_DEBUG && defined(linux)
#include <sys/syscall.h>

int close(int fd)
{
	if (maxfd) {
		if (fd < 0 || fd >= maxfd) {
			syslog(LOG_CRIT, "iv: attempt to close(2) fd %d", fd);
			abort();
		}

		if (test_bit(fd, registered_fds)) {
			syslog(LOG_CRIT, "iv: attempt to close(2) registered"
					 " fd %d", fd);
			abort();
		}
	}

	return syscall(__NR_close, fd);
}
#endif

static void assert_fd_live(struct iv_fd *_fd)
{
#if IV_DEBUG
	int fd = _fd->fd;

	if (fd < 0 || fd >= maxfd) {
		syslog(LOG_CRIT, "iv: attempt to use fd %d", fd);
		abort();
	}

	if (!test_bit(fd, registered_fds)) {
		syslog(LOG_CRIT, "iv: attempt to use unregistered fd %d", fd);
		abort();
	}
#endif
}

int iv_accept(struct iv_fd *fd, struct sockaddr *addr, socklen_t *addrlen)
{
	int ret;

	assert_fd_live(fd);

	ret = accept(fd->fd, addr, addrlen);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

int iv_connect(struct iv_fd *fd, struct sockaddr *addr, socklen_t addrlen)
{
	int ret;

	assert_fd_live(fd);

	ret = connect(fd->fd, addr, addrlen);
	if (ret == -1 && (errno == EINPROGRESS || errno == EALREADY)) {
		make_unready(fd, FD_ReadyIn);
		make_unready(fd, FD_ReadyOut);
	}

	return ret;
}

ssize_t iv_read(struct iv_fd *fd, void *buf, size_t count)
{
	int ret;

	assert_fd_live(fd);

	ret = read(fd->fd, buf, count);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

ssize_t iv_readv(struct iv_fd *fd, const struct iovec *vector, int count)
{
	int ret;

	assert_fd_live(fd);

	ret = readv(fd->fd, vector, count);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

int iv_recv(struct iv_fd *fd, void *buf, size_t len, int flags)
{
	int ret;

	assert_fd_live(fd);

	ret = recv(fd->fd, buf, len, flags);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

int iv_recvfrom(struct iv_fd *fd, void *buf, size_t len, int flags,
		struct sockaddr *from, socklen_t *fromlen)
{
	int ret;

	assert_fd_live(fd);

	ret = recvfrom(fd->fd, buf, len, flags, from, fromlen);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

int iv_recvmsg(struct iv_fd *fd, struct msghdr *msg, int flags)
{
	int ret;

	assert_fd_live(fd);

	ret = recvmsg(fd->fd, msg, flags);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyIn);

	return ret;
}

int iv_send(struct iv_fd *fd, const void *msg, size_t len, int flags)
{
	int ret;

	assert_fd_live(fd);

	ret = send(fd->fd, msg, len, flags);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}

int iv_sendmsg(struct iv_fd *fd, const struct msghdr *msg, int flags)
{
	int ret;

	assert_fd_live(fd);

	ret = sendmsg(fd->fd, msg, flags);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}

int iv_sendto(struct iv_fd *fd, const void *msg, size_t len, int flags,
	      const struct sockaddr *to, socklen_t tolen)
{
	int ret;

	assert_fd_live(fd);

	ret = sendto(fd->fd, msg, len, flags, to, tolen);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}

ssize_t iv_write(struct iv_fd *fd, const void *buf, size_t count)
{
	int ret;

	assert_fd_live(fd);

	ret = write(fd->fd, buf, count);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}

ssize_t iv_writev(struct iv_fd *fd, const struct iovec *vector, int count)
{
	int ret;

	assert_fd_live(fd);

	ret = writev(fd->fd, vector, count);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}



#ifdef linux
#include <sys/sendfile.h>

ssize_t iv_sendfile(struct iv_fd *fd, int in_fd, off_t *offset, size_t count)
{
	int ret;

	assert_fd_live(fd);

	ret = sendfile(fd->fd, in_fd, offset, count);
	if (ret == -1 && errno == EAGAIN)
		make_unready(fd, FD_ReadyOut);

	return ret;
}
#endif
