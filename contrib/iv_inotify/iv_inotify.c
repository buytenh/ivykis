/*
 * iv_inotify, an ivykis inotify component.
 *
 * Dedicated to Kanna Ishihara.
 *
 * Copyright (C) 2008-2011 Ronald Huizer
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

#include <inttypes.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <iv.h>
#include <iv_avl.h>
#include <iv_list.h>
#include "iv_inotify.h"

#ifndef NAME_MAX
#define NAME_MAX	255
#endif

static struct iv_inotify	__default_instance;
static int			__default_initialized = 0;
static struct iv_inotify	*__dispatched_instance = NULL;
static int			__dispatched_instance_destroyed = 0;

static void __iv_inotify_cleanup_watch(struct iv_inotify *inotify,
                                       struct iv_inotify_watch *watch);

/* read() on inotify descriptors can return EINTR, we simply retry. */
static ssize_t read_no_eintr(int fd, void *buf, size_t count)
{
	ssize_t ret;

	do {
		ret = read(fd, buf, count);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

static struct iv_inotify_watch *
__find_watch(struct iv_inotify *iv_ip, int wd)
{
	struct iv_avl_node *an;

	an = iv_ip->avl_tree.root;
        while (an != NULL) {
		struct iv_inotify_watch *watch;

		watch = container_of(an, struct iv_inotify_watch, avl_node);
		if (watch->wd == wd)
			return watch;

		if (watch->wd > wd)
			an = an->left;
		else
			an = an->right;
	}

	return NULL;
}

static int __watch_compare(struct iv_avl_node *_a, struct iv_avl_node *_b)
{
	struct iv_inotify_watch *a =
		container_of(_a, struct iv_inotify_watch, avl_node);
	struct iv_inotify_watch *b =
		container_of(_b, struct iv_inotify_watch, avl_node);

	if (a->wd < b->wd)
		return -1;

	if (a->wd > b->wd)
		return 1;

	return 0;
}

static void
__iv_inotify_dispatch_loop(struct iv_inotify *inotify,
                           uint8_t *event_queue, size_t event_queue_size)
{
	uint8_t *event_queue_ptr = event_queue;

	/* The inotify instance currently in the dispatch loop. */
	__dispatched_instance = inotify;

	while (event_queue_ptr < event_queue + event_queue_size) {
		struct iv_inotify_watch *watch;
		struct inotify_event *event =
			(struct inotify_event *)event_queue_ptr;

		/*
		 * Now we try to find the watch belonging to the event
		 * that we read from the inotify descriptor.  If we find
		 * events on a watch we never registered someone made a
		 * mess.  We discard such events.
		 */
		watch = __find_watch(inotify, event->wd);
		if (watch == NULL)
			continue;

		/*
		 * In case of IN_IGNORED in the event, or IN_ONESHOT in
		 * the watch we know the current watch is to be removed,
		 * so we need to clean it from the avl tree.
		 * We do this before calling the handler, as we have no
		 * idea wether the user will destroy the watch there.
		 */
		if (event->mask & IN_IGNORED || watch->mask & IN_ONESHOT)
			__iv_inotify_cleanup_watch(inotify, watch);

		/*
		 * If we have a watch handler function, call it at this
 		 * point.
 		 */
		if (watch->handler != NULL)
			watch->handler(event, watch->cookie);

		/*
		 * If the dispatched instance was destroyed in the
		 * watch->handler, we need to stop the dispatch loop, as
		 * the inotify instance may be free()d.
		 */
		if (__dispatched_instance_destroyed == 1)
			break;

		event_queue_ptr += event->len + sizeof(struct inotify_event);
	}

	__dispatched_instance = NULL;
	__dispatched_instance_destroyed = 0;
}

/*
 * Notes:
 *  - The kernel only returns complete inotify_event structures per read()
 *    request, so read() sizes should be at least the size of an
 *    inotify_event structure.
 *  - The size of an inotify_event structure varies, as it can have a name
 *    member of arbitrary size.
 *  - The name member is the same as the kernel dentry.d_name.name field,
 *    and normally limited to 256 bytes, but it can be bigger (ramfs).
 *  - We cannot determine the size of a pending event a-priori, but we can
 *    determine the size of the event queue a-priori by using the FIONREAD
 *    ioctl.
 */
static void __iv_inotify_in_fallback(struct iv_inotify *ip)
{
	ssize_t ret;
	int event_queue_size;
	uint8_t *event_queue, *event_queue_ptr;

	/*
	 * Determine how many bytes of data are queued in the inotify
	 * event queue.  Should only fail when the pointer argument points
	 * to non-addressable space, i.e. not here :-)
	 */
	if (ioctl(ip->fd.fd, FIONREAD, &event_queue_size) == -1)
		abort();

	/*
	 * Allocate space for the event queue, in case of failure,
	 * silently return.  We'll probably come back later anyway.
	 */
	event_queue = event_queue_ptr = (uint8_t *)malloc(event_queue_size);
	if (event_queue == NULL)
		return;

	ret = read_no_eintr(ip->fd.fd, event_queue, event_queue_size);
	if (ret == -1) {
		if(errno == EAGAIN) {
			free(event_queue);
			return;
		}
		abort();
	}

	__iv_inotify_dispatch_loop(ip, event_queue, ret);
	free(event_queue);
}

static void __iv_inotify_in(void *cookie)
{
	ssize_t ret;
	struct iv_inotify *ip = (struct iv_inotify *)cookie;
	uint8_t event_queue[(sizeof(struct inotify_event) + NAME_MAX + 1) * 16];

	ret = read_no_eintr(ip->fd.fd, event_queue, sizeof(event_queue));
	if (ret == -1) {
		if (errno == EAGAIN)
			return;
		abort();
	}

	/*
	 * read()s on the inotify fd will fail if the pending event size
	 * is too large, which depends on the 'name' member.  This is a
	 * copy of dentry.d_name, which on most filesystems will be
	 * smaller than 256 bytes, but in case it's not we handle it.
	 * Before 2.6.21 the return value was 0 in this case, afterwards
	 * it was -1 with errno == EINVAL.
	 */
	if ((ret == -1 && errno == EINVAL) || ret == 0)
		__iv_inotify_in_fallback(ip);
	else
		__iv_inotify_dispatch_loop(ip, event_queue, ret);
}

int iv_inotify_init(struct iv_inotify *inotify)
{
	int fd;

	/* Get an inotify instance descriptor. */
	if ((fd = inotify_init()) == -1)
		return -1;

	/* Register the descriptor for ivykis notification. */
	IV_FD_INIT(&inotify->fd);
	inotify->fd.fd = fd;
	inotify->fd.cookie = inotify;
	inotify->fd.handler_in = __iv_inotify_in;
	iv_fd_register(&inotify->fd);

	/* Initialize the watch descriptor avl tree. */
	INIT_IV_AVL_TREE(&inotify->avl_tree, __watch_compare);
	inotify->watches = 0;

	return 0;
}

void iv_inotify_destroy(struct iv_inotify *inotify)
{
	/*
	 * Unregister the inotify descriptor for ivykis notification, and
	 * close it.  There is no need to unregister individual watches,
	 * as the kernel will close all registered watches when there are
	 * no more references to the inotify descriptor.
	 */
	iv_fd_unregister(&inotify->fd);
	close(inotify->fd.fd);

	/* If we destroyed the instance currently dispatched, we must
	 * break the dispatch loop.
	 */
	if (inotify == __dispatched_instance)
		__dispatched_instance_destroyed = 1;
}

int
iv_inotify_add_watch(struct iv_inotify *inotify,
                     struct iv_inotify_watch *watch)
{
	int wd;

	/* If the inotify instance is NULL, use a default one. */
	if (inotify == NULL) {
		inotify = &__default_instance;

		if (__default_initialized == 0) {
			if (iv_inotify_init(inotify) == -1)
				return -1;
			else
				__default_initialized = 1;
		}
	}

	wd = inotify_add_watch(inotify->fd.fd, watch->pathname, watch->mask);
	if (wd == -1)
		return -1;

	watch->wd = wd;
	if (iv_avl_tree_insert(&inotify->avl_tree, &watch->avl_node) != 0)
		abort();
	inotify->watches++;

	return 0;
}

static void
__iv_inotify_cleanup_watch(struct iv_inotify *inotify,
                           struct iv_inotify_watch *watch)
{
	iv_avl_tree_delete(&inotify->avl_tree, &watch->avl_node);
	inotify->watches--;

	if (inotify == &__default_instance && inotify->watches == 0) {
		iv_inotify_destroy(inotify);
		__default_initialized = 0;
	}
}

int
iv_inotify_rm_watch(struct iv_inotify *inotify,
                    struct iv_inotify_watch *watch)
{
	if (inotify == NULL) {
		if (__default_initialized == 0) {
			errno = EINVAL;
			return -1;
		}
		inotify = &__default_instance;
	}

	if (inotify_rm_watch(inotify->fd.fd, watch->wd) == -1)
		return -1;

	__iv_inotify_cleanup_watch(inotify, watch);

	return 0;
}
