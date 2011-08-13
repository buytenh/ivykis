/*
 * iv_inotify, an ivykis inotify component.
 * Copyright (C) 2008 - 2011 Ronald Huizer
 * Dedicated to Kanna Ishihara.
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
#include <inttypes.h>
#include <iv.h>
#include <iv_avl.h>
#include <iv_inotify.h>
#include <iv_list.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>

static struct iv_inotify	__default_instance;
static int			__default_initialized = 0;
static struct iv_inotify	*__dispatched_instance = NULL;
static int			__dispatched_instance_destroyed = 0;

static struct iv_inotify_watch *__find_watch(struct iv_inotify *this, int wd)
{
	struct iv_avl_node *an;

	an = this->watches.root;
	while (an != NULL) {
		struct iv_inotify_watch *w;

		w = container_of(an, struct iv_inotify_watch, an);
		if (wd == w->wd)
			return w;

		if (wd < w->wd)
			an = an->left;
		else
			an = an->right;
	}

	return NULL;
}

static void
__iv_inotify_cleanup_watch(struct iv_inotify *this, struct iv_inotify_watch *w)
{
	iv_avl_tree_delete(&this->watches, &w->an);
	this->num_watches--;

	if (this == &__default_instance && this->num_watches == 0) {
		iv_inotify_unregister(this);
		__default_initialized = 0;
	}
}

static void iv_inotify_got_event(void *_this)
{
	struct iv_inotify *this = (struct iv_inotify *)_this;
	uint8_t event_queue[65536];
	ssize_t ret;
	void *curr;
	void *end;

	do {
		ret = read(this->fd.fd, event_queue, sizeof(event_queue));
	} while (ret == -1 && errno == EINTR);

	if (ret <= 0) {
		if (ret == 0 || errno != EAGAIN) {
			if (ret < 0)
				perror("read");
			abort();
		}
		return;
	}

	__dispatched_instance = this;

	curr = event_queue;
	end = event_queue + ret;
	while (curr < end) {
		struct inotify_event *event = curr;
		struct iv_inotify_watch *w;

		w = __find_watch(this, event->wd);
		if (w != NULL) {
			if (event->mask & IN_IGNORED || w->mask & IN_ONESHOT)
				__iv_inotify_cleanup_watch(this, w);
			w->handler(w->cookie, event);

			if (__dispatched_instance_destroyed == 1)
				break;
		}

		curr += event->len + sizeof(struct inotify_event);
	}

	__dispatched_instance = NULL;
	__dispatched_instance_destroyed = 0;
}

static int
__iv_inotify_watch_compare(struct iv_avl_node *_a, struct iv_avl_node *_b)
{
	struct iv_inotify_watch *a =
		container_of(_a, struct iv_inotify_watch, an);
	struct iv_inotify_watch *b =
		container_of(_b, struct iv_inotify_watch, an);

	if (a->wd < b->wd)
		return -1;

	if (a->wd > b->wd)
		return 1;

	return 0;
}

int iv_inotify_register(struct iv_inotify *this)
{
	int fd;

	fd = inotify_init();
	if (fd == -1)
		return -1;

	IV_FD_INIT(&this->fd);
	this->fd.fd = fd;
	this->fd.cookie = this;
	this->fd.handler_in = iv_inotify_got_event;
	iv_fd_register(&this->fd);

	INIT_IV_AVL_TREE(&this->watches, __iv_inotify_watch_compare);
	this->num_watches = 0;

	return 0;
}

void iv_inotify_unregister(struct iv_inotify *this)
{
	iv_fd_unregister(&this->fd);
	close(this->fd.fd);

	if (this == __dispatched_instance)
		__dispatched_instance_destroyed = 1;
}

int iv_inotify_watch_register(struct iv_inotify_watch *w)
{
	struct iv_inotify *inotify = w->inotify;

	if (inotify == NULL) {
		inotify = &__default_instance;

		if (__default_initialized == 0) {
			if (iv_inotify_register(inotify) == -1)
				return -1;
			else
				__default_initialized = 1;
		}
	}

	w->wd = inotify_add_watch(inotify->fd.fd, w->pathname, w->mask);
	if (w->wd == -1)
		return -1;

	inotify->num_watches++;

	return iv_avl_tree_insert(&inotify->watches, &w->an);
}

void iv_inotify_watch_unregister(struct iv_inotify_watch *w)
{
	struct iv_inotify *inotify = w->inotify;

	if (inotify == NULL)
		inotify = &__default_instance;

	inotify_rm_watch(inotify->fd.fd, w->wd);

	__iv_inotify_cleanup_watch(inotify, w);
}
