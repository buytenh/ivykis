/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009, 2012, 2013, 2020 Lennert Buytenhek
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
#include <liburing.h>
#include <poll.h>
#include <string.h>
#include "iv_private.h"

static int uring_support = 1;

static int iv_fd_uring_create(struct io_uring *ring)
{
	struct io_uring_probe *p;

	if (io_uring_queue_init(256, ring, 0) < 0)
		return 0;

	p = io_uring_get_probe_ring(ring);
	if (p == NULL) {
		io_uring_queue_exit(ring);
		return 0;
	}

	/*
	 * Without Linux kernel commit 18bceab101ad ("io_uring:
	 * allow POLL_ADD with double poll_wait() users"), POLL_ADD
	 * doesn't work with all possible combinations of file
	 * descriptor types and poll event masks.
	 *
	 * This commit was merged for Linux v5.8, but there is no
	 * direct way to test for the presence of the fix in the
	 * running kernel.
	 *
	 * We also can't just simply restrict the use of uring to
	 * kernels >= 5.8, because this fix may have been backported
	 * to an earlier (vendor) kernel, and it may therefore be
	 * present in the running kernel even if that kernel is
	 * older than 5.8.
	 *
	 * Instead, we'll test for support for IORING_OP_TEE.  This
	 * operation was also added in kernel 5.8, and if we're
	 * running on a kernel that supports that operation, we're
	 * either running on >= 5.8, or we're running on a kernel
	 * that is < 5.8 and that has had the IORING_OP_TEE feature
	 * backported and we'll assume that the POLL_ADD fix will
	 * have been backported as well.
	 *
	 * Instead of referencing IORING_OP_TEE by name, we'll use
	 * its numerical value (33) instead.  We might be compiling
	 * against an older io_uring.h kernel header which doesn't
	 * have IORING_OP_TEE defined, and since uring operations
	 * are defined with an enum, we cannot check for its
	 * presence by using #ifdef.  (And unlike syscall numbers,
	 * uring operation values are the same across architectures.)
	 */
	if (!io_uring_opcode_supported(p, 33)) {
		free(p);
		io_uring_queue_exit(ring);
		return 0;
	}

	free(p);

	return 1;
}

static int iv_fd_uring_init(struct iv_state *st)
{
	if (uring_support) {
		if (iv_fd_uring_create(&st->u.uring.ring)) {
			INIT_IV_LIST_HEAD(&st->u.uring.notify);
			INIT_IV_LIST_HEAD(&st->u.uring.active);
			st->u.uring.unsubmitted_sqes = 0;
			st->u.uring.timer_expired = 0;

			return 0;
		}

		uring_support = 0;
	}

	return -1;
}

static void
iv_fd_uring_handle_fd_cqe(struct iv_state *st, struct iv_list_head *active,
			  struct io_uring_cqe *cqe)
{
	struct iv_fd_ *fd;

	fd = io_uring_cqe_get_data(cqe);

	if (cqe->res > 0) {
		if (cqe->res & (POLLIN | POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKIN);

		if (cqe->res & (POLLOUT | POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKOUT);

		if (cqe->res & (POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKERR);
	} else if (cqe->res != -ECANCELED) {
		iv_fatal("iv_fd_uring_handle_fd_cqe: got error "
			 "%d[%s] for fd %d", -cqe->res,
			 strerror(-cqe->res), fd->fd);
	}

	fd->u.sqes_in_flight--;
	if (!fd->u.sqes_in_flight) {
		fd->registered_bands = 0;

		iv_list_del_init(&fd->list_notify);
		if (fd->wanted_bands) {
			iv_list_add_tail(&fd->list_notify,
					 &st->u.uring.notify);
		}
	}
}

static void
iv_fd_uring_drain_cqes(struct iv_state *st, struct iv_list_head *active)
{
	int count;
	uint32_t head;
	struct io_uring_cqe *cqe;

	count = 0;
	io_uring_for_each_cqe (&st->u.uring.ring, head, cqe) {
		count++;

		if (cqe->user_data == (unsigned long)&st->time) {
			if (cqe->res == -ETIME)
				st->u.uring.timer_expired = 1;
		} else {
			iv_fd_uring_handle_fd_cqe(st, active, cqe);
		}
	}

	io_uring_cq_advance(&st->u.uring.ring, count);
}

static void iv_fd_uring_submit_sqes(struct iv_state *st)
{
	if (st->u.uring.unsubmitted_sqes) {
		int ret;

		ret = io_uring_submit(&st->u.uring.ring);
		if (ret < 0) {
			if (errno != EINTR && errno != EBUSY) {
				iv_fatal("iv_fd_uring_submit_sqes: got error "
					 "%d[%s]", errno, strerror(errno));
			}
			return;
		}

		st->u.uring.unsubmitted_sqes -= ret;
	}
}

static struct io_uring_sqe *iv_fd_uring_get_sqe(struct iv_state *st)
{
	struct io_uring_sqe *sqe;

	sqe = io_uring_get_sqe(&st->u.uring.ring);
	while (sqe == NULL) {
		iv_fd_uring_drain_cqes(st, &st->u.uring.active);
		iv_fd_uring_submit_sqes(st);
		sqe = io_uring_get_sqe(&st->u.uring.ring);
	}

	st->u.uring.unsubmitted_sqes++;

	return sqe;
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = 0;
	if (bits & MASKIN)
		mask |= POLLIN;
	if (bits & MASKOUT)
		mask |= POLLOUT;

	return mask;
}

static void iv_fd_uring_flush_one(struct iv_state *st, struct iv_fd_ *fd)
{
	struct io_uring_sqe *sqe;

	iv_list_del_init(&fd->list_notify);

	if (fd->registered_bands == fd->wanted_bands)
		return;

	if (fd->registered_bands) {
		sqe = iv_fd_uring_get_sqe(st);
		io_uring_prep_poll_remove(sqe, fd);
		io_uring_sqe_set_data(sqe, fd);
		fd->u.sqes_in_flight++;
	}

	if (fd->wanted_bands) {
		sqe = iv_fd_uring_get_sqe(st);
		io_uring_prep_poll_add(sqe, fd->fd,
				       bits_to_poll_mask(fd->wanted_bands));
		io_uring_sqe_set_data(sqe, fd);
		fd->u.sqes_in_flight++;
	}

	fd->registered_bands = fd->wanted_bands;
}

static void iv_fd_uring_flush_pending(struct iv_state *st)
{
	while (!iv_list_empty(&st->u.uring.notify)) {
		struct iv_fd_ *fd;

		fd = iv_list_entry(st->u.uring.notify.next,
				   struct iv_fd_, list_notify);

		iv_fd_uring_flush_one(st, fd);
	}
}

static void iv_fd_uring_set_timeout(struct iv_state *st,
				    const struct timespec *abs,
				    unsigned int count)
{
	struct io_uring_sqe *sqe;

	memset(&st->u.uring.ts, 0, sizeof(st->u.uring.ts));
	st->u.uring.ts.tv_sec = abs->tv_sec;
	st->u.uring.ts.tv_nsec = abs->tv_nsec;

	sqe = iv_fd_uring_get_sqe(st);
	io_uring_prep_timeout(sqe, &st->u.uring.ts, count, IORING_TIMEOUT_ABS);
	io_uring_sqe_set_data(sqe, &st->time);

	st->u.uring.timer_expired = 0;
}

static int iv_fd_uring_set_poll_timeout(struct iv_state *st,
					const struct timespec *abs)
{
	iv_fd_uring_set_timeout(st, abs, 0);

	return 1;
}

static void iv_fd_uring_clear_poll_timeout(struct iv_state *st)
{
	struct io_uring_sqe *sqe;

	sqe = iv_fd_uring_get_sqe(st);
	io_uring_prep_timeout_remove(sqe, (unsigned long)&st->time, 0);
	io_uring_sqe_set_data(sqe, &st->time);
}

static int iv_fd_uring_poll(struct iv_state *st,
			    struct iv_list_head *active,
			    const struct timespec *abs)
{
	int ret;

	iv_fd_uring_drain_cqes(st, &st->u.uring.active);

	if (!iv_list_empty(&st->u.uring.active)) {
		__iv_list_steal_elements(&st->u.uring.active, active);
		return 1;
	}

	iv_fd_uring_flush_pending(st);

	if (abs != NULL) {
		if (!st->time_valid) {
			st->time_valid = 1;
			iv_time_get(&st->time);
		}

		if (!timespec_gt(abs, &st->time)) {
			iv_fd_uring_submit_sqes(st);
			iv_fd_uring_drain_cqes(st, active);
			return 1;
		}

		iv_fd_uring_set_timeout(st, abs, 1);
	}

	ret = io_uring_submit_and_wait(&st->u.uring.ring, 1);

	__iv_invalidate_now(st);

	if (ret < 0 && errno != EINTR && errno != EBUSY) {
		iv_fatal("iv_fd_uring_poll: got error %d[%s]", errno,
			 strerror(errno));
	}

	iv_fd_uring_drain_cqes(st, active);

	return st->u.uring.timer_expired;
}

static void iv_fd_uring_unregister_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	if (!iv_list_empty(&fd->list_notify)) {
		iv_fd_uring_flush_one(st, fd);

		do {
			iv_fd_uring_submit_sqes(st);
			iv_fd_uring_drain_cqes(st, &st->u.uring.active);
		} while (fd->u.sqes_in_flight && st->u.uring.unsubmitted_sqes);

		if (fd->u.sqes_in_flight) {
			iv_fatal("iv_fd_uring_unregister_fd: fd %d appears "
				 "stuck (%d)\n", fd->fd, fd->u.sqes_in_flight);
		}
	}
}

static void iv_fd_uring_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	iv_list_del_init(&fd->list_notify);
	if (fd->registered_bands != fd->wanted_bands)
		iv_list_add_tail(&fd->list_notify, &st->u.uring.notify);
}

static int iv_fd_uring_notify_fd_sync(struct iv_state *st, struct iv_fd_ *fd)
{
	iv_fd_uring_flush_one(st, fd);
	iv_fd_uring_submit_sqes(st);

	return 0;
}

static void iv_fd_uring_deinit(struct iv_state *st)
{
	io_uring_queue_exit(&st->u.uring.ring);
}

const struct iv_fd_poll_method iv_fd_poll_method_uring = {
	.name			= "uring",
	.init			= iv_fd_uring_init,
	.set_poll_timeout	= iv_fd_uring_set_poll_timeout,
	.clear_poll_timeout	= iv_fd_uring_clear_poll_timeout,
	.poll			= iv_fd_uring_poll,
	.unregister_fd		= iv_fd_uring_unregister_fd,
	.notify_fd		= iv_fd_uring_notify_fd,
	.notify_fd_sync		= iv_fd_uring_notify_fd_sync,
	.deinit			= iv_fd_uring_deinit,
};
