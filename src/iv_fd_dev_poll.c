/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009 Lennert Buytenhek
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
#include <string.h>
#include <sys/devpoll.h>
#include "iv_private.h"

#define UPLOAD_BATCH		1024

static int
iv_fd_avl_compare(const struct iv_avl_node *_a, const struct iv_avl_node *_b)
{
	const struct iv_fd_ *a =
		iv_container_of(_a, struct iv_fd_, u.avl_node);
	const struct iv_fd_ *b =
		iv_container_of(_b, struct iv_fd_, u.avl_node);

	if (a->fd < b->fd)
		return -1;

	if (a->fd > b->fd)
		return 1;

	return 0;
}

static int iv_fd_dev_poll_init(struct iv_state *st)
{
	int poll_fd;

#ifdef O_CLOEXEC
	poll_fd = open("/dev/poll", O_RDWR | O_CLOEXEC);
	if (poll_fd < 0)
		return -1;
#else
	poll_fd = open("/dev/poll", O_RDWR);
	if (poll_fd < 0)
		return -1;

	iv_fd_set_cloexec(poll_fd);
#endif

	INIT_IV_AVL_TREE(&st->u.dev_poll.fds, iv_fd_avl_compare);
	st->u.dev_poll.poll_fd = poll_fd;
	INIT_IV_LIST_HEAD(&st->u.dev_poll.notify);

	return 0;
}

static void xwrite(int fd, const void *buf, size_t count)
{
	while (count) {
		int ret;

		do {
			ret = write(fd, buf, count);
		} while (ret < 0 && errno == EINTR);

		if (ret < 0) {
			iv_fatal("iv_fd_dev_poll_flush_pending: got error "
				 "%d[%s]", errno, strerror(errno));
		}

		buf += ret;
		count -= ret;
	}
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

static void iv_fd_dev_poll_flush_pending(struct iv_state *st)
{
	int poll_fd;
	struct pollfd pfd[UPLOAD_BATCH];
	int num;

	poll_fd = st->u.dev_poll.poll_fd;
	num = 0;

	while (!iv_list_empty(&st->u.dev_poll.notify)) {
		struct iv_list_head *ilh;
		struct iv_fd_ *fd;

		if (num > UPLOAD_BATCH - 2) {
			xwrite(poll_fd, pfd, num * sizeof(pfd[0]));
			num = 0;
		}

		ilh = st->u.dev_poll.notify.next;
		iv_list_del_init(ilh);

		fd = iv_list_entry(ilh, struct iv_fd_, list_notify);

		if (fd->registered_bands & ~fd->wanted_bands) {
			pfd[num].fd = fd->fd;
			pfd[num].events = POLLREMOVE;
			num++;
		}

		if (fd->wanted_bands) {
			pfd[num].fd = fd->fd;
			pfd[num].events = bits_to_poll_mask(fd->wanted_bands);
			num++;
		}

		fd->registered_bands = fd->wanted_bands;
	}

	if (num)
		xwrite(poll_fd, pfd, num * sizeof(pfd[0]));
}

static struct iv_fd_ *iv_fd_avl_find(const struct iv_avl_tree *root, int fd)
{
	struct iv_avl_node *an;

	an = root->root;
	while (an != NULL) {
		struct iv_fd_ *p;

		p = iv_container_of(an, struct iv_fd_, u.avl_node);
		if (fd == p->fd)
			return p;

		if (fd < p->fd)
			an = an->left;
		else
			an = an->right;
	}

	return NULL;
}

static int iv_fd_dev_poll_poll(struct iv_state *st,
			       struct iv_list_head *active,
			       const struct timespec *abs)
{
	struct pollfd batch[st->numfds ? : 1];
	struct dvpoll dvp;
	int ret;
	int i;

	iv_fd_dev_poll_flush_pending(st);

	dvp.dp_fds = batch;
	dvp.dp_nfds = ARRAY_SIZE(batch);
	dvp.dp_timeout = to_msec(st, abs);

	ret = ioctl(st->u.dev_poll.poll_fd, DP_POLL, &dvp);

	__iv_invalidate_now(st);

	if (ret < 0) {
		if (errno == EINTR)
			return 1;

		iv_fatal("iv_fd_dev_poll_poll: got error %d[%s]",
			 errno, strerror(errno));
	}

	for (i = 0; i < ret; i++) {
		struct iv_fd_ *fd;
		int revents;

		fd = iv_fd_avl_find(&st->u.dev_poll.fds, batch[i].fd);
		if (fd == NULL) {
			iv_fatal("iv_fd_dev_poll_poll: got event for "
				 "unknown fd %d", batch[i].fd);
		}

		revents = batch[i].revents;

		if (revents & (POLLIN | POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKIN);

		if (revents & (POLLOUT | POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKOUT);

		if (revents & (POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKERR);
	}

	return 1;
}

static void iv_fd_dev_poll_register_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	int ret;

	ret = iv_avl_tree_insert(&st->u.dev_poll.fds, &fd->u.avl_node);
	if (ret) {
		iv_fatal("iv_fd_dev_poll_register_fd: got error %d[%s]",
			 ret, strerror(ret));
	}
}

static void iv_fd_dev_poll_unregister_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	iv_avl_tree_delete(&st->u.dev_poll.fds, &fd->u.avl_node);

	if (!iv_list_empty(&fd->list_notify))
		iv_fd_dev_poll_flush_pending(st);
}

static void iv_fd_dev_poll_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	iv_list_del_init(&fd->list_notify);
	if (fd->registered_bands != fd->wanted_bands)
		iv_list_add_tail(&fd->list_notify, &st->u.dev_poll.notify);
}

static int iv_fd_dev_poll_notify_fd_sync(struct iv_state *st, struct iv_fd_ *fd)
{
	struct pollfd pfd;
	int ret;

	pfd.fd = fd->fd;
	pfd.events = bits_to_poll_mask(fd->wanted_bands);

	do {
		ret = write(st->u.dev_poll.poll_fd, &pfd, sizeof(pfd));
	} while (ret < 0 && errno == EINTR);

	if (ret == sizeof(pfd)) {
		fd->registered_bands = fd->wanted_bands;
		return 0;
	}

	return -1;
}

static void iv_fd_dev_poll_deinit(struct iv_state *st)
{
	close(st->u.dev_poll.poll_fd);
}


const struct iv_fd_poll_method iv_fd_poll_method_dev_poll = {
	.name		= "dev_poll",
	.init		= iv_fd_dev_poll_init,
	.poll		= iv_fd_dev_poll_poll,
	.register_fd	= iv_fd_dev_poll_register_fd,
	.unregister_fd	= iv_fd_dev_poll_unregister_fd,
	.notify_fd	= iv_fd_dev_poll_notify_fd,
	.notify_fd_sync	= iv_fd_dev_poll_notify_fd_sync,
	.deinit		= iv_fd_dev_poll_deinit,
};
