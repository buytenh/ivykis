/*
 * ivykis, an event handling library
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
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>

static struct iv_inotify_watch *
__find_watch(const struct iv_inotify *this, int wd)
{
	struct iv_avl_node *an;

	an = this->watches.root;
	while (an != NULL) {
		struct iv_inotify_watch *w;

		w = iv_container_of(an, struct iv_inotify_watch, an);
		if (wd == w->wd)
			return w;

		if (wd < w->wd)
			an = an->left;
		else
			an = an->right;
	}

	return NULL;
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
		if (ret == 0) {
			iv_fatal("iv_inotify: reading from inotify fd "
				 "returned zero");
		} else if (errno != EAGAIN) {
			iv_fatal("iv_inotify: reading from inotify fd "
				 "returned error %d[%s]", errno,
				 strerror(errno));
		}
		return;
	}

	this->term = (void **)&this;

	curr = event_queue;
	end = event_queue + ret;
	while (curr < end) {
		struct inotify_event *event = curr;
		struct iv_inotify_watch *w;

		w = __find_watch(this, event->wd);
		if (w != NULL) {
			if (event->mask & IN_IGNORED || w->mask & IN_ONESHOT)
				iv_avl_tree_delete(&this->watches, &w->an);
			w->handler(w->cookie, event);
		}

		curr += event->len + sizeof(struct inotify_event);

		if (this == NULL)
			break;
	}

	if (this != NULL)
		this->term = NULL;
}

static int __iv_inotify_watch_compare(const struct iv_avl_node *_a,
				      const struct iv_avl_node *_b)
{
	const struct iv_inotify_watch *a =
		iv_container_of(_a, struct iv_inotify_watch, an);
	const struct iv_inotify_watch *b =
		iv_container_of(_b, struct iv_inotify_watch, an);

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

	return 0;
}

void iv_inotify_unregister(struct iv_inotify *this)
{
	iv_fd_unregister(&this->fd);
	close(this->fd.fd);

	if (this->term != NULL)
		*this->term = NULL;
}

int iv_inotify_watch_register(struct iv_inotify_watch *w)
{
	struct iv_inotify *inotify = w->inotify;

	w->wd = inotify_add_watch(inotify->fd.fd, w->pathname, w->mask);
	if (w->wd == -1)
		return -1;

	return iv_avl_tree_insert(&inotify->watches, &w->an);
}

void iv_inotify_watch_unregister(struct iv_inotify_watch *w)
{
	struct iv_inotify *inotify = w->inotify;

	inotify_rm_watch(inotify->fd.fd, w->wd);
	iv_avl_tree_delete(&inotify->watches, &w->an);
}
