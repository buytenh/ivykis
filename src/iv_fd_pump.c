/*
 * ivykis, an event handling library
 * Copyright (C) 2011 Lennert Buytenhek
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
#include <fcntl.h>
#include <iv.h>
#include <iv_fd_pump.h>
#include <iv_list.h>
#include <iv_tls.h>
#include <string.h>
#include <sys/ioctl.h>
#include "iv_private.h"

/* thread state handling ****************************************************/
struct iv_fd_pump_thr_info {
	int			num_bufs;
	struct iv_list_head	bufs;
};

static void buf_purge(struct iv_fd_pump_thr_info *tinfo);

static void iv_fd_pump_tls_init_thread(void *_tinfo)
{
	struct iv_fd_pump_thr_info *tinfo = _tinfo;

	tinfo->num_bufs = 0;
	INIT_IV_LIST_HEAD(&tinfo->bufs);
}

static void iv_fd_pump_tls_deinit_thread(void *_tinfo)
{
	struct iv_fd_pump_thr_info *tinfo = _tinfo;

	buf_purge(tinfo);
}

static struct iv_tls_user iv_fd_pump_tls_user = {
	.sizeof_state	= sizeof(struct iv_fd_pump_thr_info),
	.init_thread	= iv_fd_pump_tls_init_thread,
	.deinit_thread	= iv_fd_pump_tls_deinit_thread,
};

static void iv_fd_pump_tls_init(void) __attribute__((constructor));
static void iv_fd_pump_tls_init(void)
{
	iv_tls_user_register(&iv_fd_pump_tls_user);
}


/* pipe acquisition *********************************************************/
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif

#if (defined(__NR_pipe2) || defined(HAVE_PIPE2)) && defined(O_CLOEXEC)
static int pipe2_support = 1;
#endif

static int grab_pipe(int *fd)
{
	int ret;

#if (defined(__NR_pipe2) || defined(HAVE_PIPE2)) && defined(O_CLOEXEC)
	if (pipe2_support) {
#ifdef __NR_pipe2
		ret = syscall(__NR_pipe2, fd, O_CLOEXEC);
#else
		ret = pipe2(fd, O_CLOEXEC);
#endif
		if (ret == 0 || errno != ENOSYS)
			return ret;

		pipe2_support = 0;
	}
#endif

	ret = pipe(fd);
	if (ret == 0) {
		iv_fd_set_cloexec(fd[0]);
		iv_fd_set_cloexec(fd[1]);
	}

	return ret;
}


/* buffer management ********************************************************/
#define MAX_CACHED_BUFS		20
#define BUF_SIZE		4096

#ifndef HAVE_SPLICE
 #define splice_available	 0
 #define splice(...)		-1
 #ifndef FIONREAD
  #define FIONREAD		0
 #endif
#else
static int splice_available = -1;
#endif

struct iv_fd_pump_buf {
	struct iv_list_head	list;
	union {
		unsigned char	buf[0];
		int		pfd[2];
	} u;
};

static struct iv_fd_pump_buf *buf_alloc(void)
{
	int size;
	struct iv_fd_pump_buf *buf;

	if (!splice_available)
		size = sizeof(struct iv_list_head) + BUF_SIZE;
	else
		size = sizeof(struct iv_fd_pump_buf);

	buf = malloc(size);

	if (buf != NULL && splice_available && grab_pipe(buf->u.pfd) < 0) {
		free(buf);
		buf = NULL;
	}

	return buf;
}

static void __buf_free(struct iv_fd_pump_buf *buf)
{
	if (splice_available) {
		close(buf->u.pfd[0]);
		close(buf->u.pfd[1]);
	}
	free(buf);
}

static void buf_put(struct iv_fd_pump_buf *buf, int bytes)
{
	struct iv_fd_pump_thr_info *tinfo;

	if (splice_available && bytes) {
		__buf_free(buf);
		return;
	}

	tinfo = iv_tls_user_ptr(&iv_fd_pump_tls_user);
	if (tinfo->num_bufs < MAX_CACHED_BUFS) {
		tinfo->num_bufs++;
		iv_list_add(&buf->list, &tinfo->bufs);
	} else {
		__buf_free(buf);
	}
}

static void check_splice_available(void)
{
#ifdef HAVE_SPLICE
	struct iv_fd_pump_buf *b0;
	struct iv_fd_pump_buf *b1;
	int ret;

	splice_available = 1;

	b0 = buf_alloc();
	if (b0 == NULL) {
		splice_available = 0;
		return;
	}

	b1 = buf_alloc();
	if (b1 == NULL) {
		__buf_free(b0);
		splice_available = 0;
		return;
	}

	ret = splice(b0->u.pfd[0], NULL, b1->u.pfd[1], NULL,
		     1, SPLICE_F_NONBLOCK);
	if (ret < 0 && errno == EAGAIN) {
		buf_put(b1, 0);
		buf_put(b0, 0);
	} else {
		__buf_free(b0);
		__buf_free(b1);
		splice_available = 0;
	}
#endif
}

static struct iv_fd_pump_buf *__buf_dequeue(struct iv_fd_pump_thr_info *tinfo)
{
	if (!iv_list_empty(&tinfo->bufs)) {
		struct iv_list_head *ilh;

		tinfo->num_bufs--;

		ilh = tinfo->bufs.next;
		iv_list_del(ilh);

		return iv_container_of(ilh, struct iv_fd_pump_buf, list);
	}

	return NULL;
}

static struct iv_fd_pump_buf *buf_get(void)
{
	struct iv_fd_pump_thr_info *tinfo =
		iv_tls_user_ptr(&iv_fd_pump_tls_user);
	struct iv_fd_pump_buf *buf;

	buf = __buf_dequeue(tinfo);
	if (buf == NULL)
		buf = buf_alloc();

	return buf;
}

static void buf_purge(struct iv_fd_pump_thr_info *tinfo)
{
	struct iv_fd_pump_buf *buf;

	while ((buf = __buf_dequeue(tinfo)) != NULL)
		__buf_free(buf);
}


/* iv_fd_pump ***************************************************************/
static struct iv_fd_pump_buf *iv_fd_pump_buf(struct iv_fd_pump *ip)
{
	return (struct iv_fd_pump_buf *)ip->buf;
}

void iv_fd_pump_init(struct iv_fd_pump *ip)
{
	if (splice_available == -1)
		check_splice_available();

	ip->buf = NULL;
	ip->bytes = 0;
	ip->full = 0;
	ip->saw_fin = 0;

	ip->set_bands(ip->cookie, 1, 0);
}

void iv_fd_pump_destroy(struct iv_fd_pump *ip)
{
	struct iv_fd_pump_buf *buf = iv_fd_pump_buf(ip);

	if (ip->saw_fin != 2)
		ip->set_bands(ip->cookie, 0, 0);

	if (buf != NULL) {
		buf_put(buf, ip->bytes);
		ip->buf = NULL;
	}
}

static int iv_fd_pump_try_input(struct iv_fd_pump *ip)
{
	struct iv_fd_pump_buf *buf = iv_fd_pump_buf(ip);
	int ret;

	if (buf == NULL) {
		buf = buf_get();
		if (buf == NULL)
			return -1;

		ip->buf = (void *)buf;
	}

	do {
		if (!splice_available) {
			ret = read(ip->from_fd, buf->u.buf + ip->bytes,
				   BUF_SIZE - ip->bytes);
		} else {
			ret = splice(ip->from_fd, NULL, buf->u.pfd[1], NULL,
				     1048576, SPLICE_F_NONBLOCK);
		}
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		if (errno != EAGAIN)
			return -1;

		if (splice_available && ip->bytes) {
			int bytes = 1;

			ioctl(ip->from_fd, FIONREAD, &bytes);
			if (bytes > 0)
				ip->full = 1;
		}

		return 0;
	}

	if (ret == 0) {
		ip->saw_fin = 1;
		if (!ip->bytes) {
			if (ip->flags & IV_FD_PUMP_FLAG_RELAY_EOF)
				shutdown(ip->to_fd, SHUT_WR);
			ip->saw_fin = 2;
		}
		return 0;
	}

	ip->bytes += ret;
	if (!splice_available && ip->bytes == BUF_SIZE)
		ip->full = 1;

	return 0;
}

static int iv_fd_pump_try_output(struct iv_fd_pump *ip)
{
	struct iv_fd_pump_buf *buf = iv_fd_pump_buf(ip);
	int ret;

	do {
		if (!splice_available) {
			ret = write(ip->to_fd, buf->u.buf, ip->bytes);
		} else {
			ret = splice(buf->u.pfd[0], NULL, ip->to_fd, NULL,
				     ip->bytes, 0);
		}
	} while (ret < 0 && errno == EINTR);

	if (ret <= 0)
		return (ret < 0 && errno == EAGAIN) ? 0 : -1;

	ip->full = 0;

	ip->bytes -= ret;
	if (!splice_available)
		memmove(buf->u.buf, buf->u.buf + ret, ip->bytes);

	if (!ip->bytes && ip->saw_fin == 1) {
		if (ip->flags & IV_FD_PUMP_FLAG_RELAY_EOF)
			shutdown(ip->to_fd, SHUT_WR);
		ip->saw_fin = 2;
	}

	return 0;
}

static int __iv_fd_pump_pump(struct iv_fd_pump *ip)
{
	if (!ip->full && ip->saw_fin == 0 && iv_fd_pump_try_input(ip))
		return -1;

	if (ip->bytes && iv_fd_pump_try_output(ip))
		return -1;


	switch (ip->saw_fin) {
	case 0:
		ip->set_bands(ip->cookie, !ip->full, !!ip->bytes);
		return 1;

	case 1:
		ip->set_bands(ip->cookie, 0, 1);
		return 1;

	case 2:
		ip->set_bands(ip->cookie, 0, 0);
		return 0;
	}

	iv_fatal("iv_fd_pump_pump: saw_fin == %d", ip->saw_fin);
}

int iv_fd_pump_pump(struct iv_fd_pump *ip)
{
	int ret;

	ret = __iv_fd_pump_pump(ip);
	if (ret < 0 || !ip->bytes) {
		struct iv_fd_pump_buf *buf = iv_fd_pump_buf(ip);

		if (buf != NULL) {
			buf_put(buf, ip->bytes);
			ip->buf = NULL;
		}
	}

	return ret;
}

int iv_fd_pump_is_done(const struct iv_fd_pump *ip)
{
	return !!(ip->saw_fin == 2);
}
