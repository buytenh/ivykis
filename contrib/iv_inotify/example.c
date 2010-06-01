/*
 * iv_inotify, an ivykis inotify component.
 *
 * Dedicated to Kanna Ishihara.
 *
 * Copyright (C) 2008, 2009 Ronald Huizer
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
#include "iv_inotify.h"

int print_event(int mask)
{
	int ret = 0, ret2;

	ret2 = printf("0x%.8x: ", mask);
	if (mask & IN_ACCESS) {
		mask &= ~IN_ACCESS;
		ret += printf("%sIN_ACCESS", ret ? " | " : "");
	}
	if (mask & IN_ATTRIB) {
		mask &= ~IN_ATTRIB;
		ret += printf("%sIN_ATTRIB", ret ? " | " : "");
	}
	if (mask & IN_CLOSE_WRITE) {
		mask &= ~IN_CLOSE_WRITE;
		ret += printf("%sIN_CLOSE_WRITE", ret ? " | " : "");
	}
	if (mask & IN_CLOSE_NOWRITE) {
		mask &= ~IN_CLOSE_NOWRITE;
		ret += printf("%sIN_CLOSE_NOWRITE", ret ? " | " : "");
	}
	if (mask & IN_CREATE) {
		mask &= ~IN_CREATE;
		ret += printf("%sIN_CREATE", ret ? " | " : "");
	}
	if (mask & IN_DELETE) {
		mask &= ~IN_DELETE;
		ret += printf("%sIN_DELETE", ret ? " | " : "");
	}
	if (mask & IN_DELETE_SELF) {
		mask &= ~IN_DELETE_SELF;
		ret += printf("%sIN_DELETE_SELF", ret ? " | " : "");
	}
	if (mask & IN_MODIFY) {
		mask &= ~IN_MODIFY;
		ret += printf("%sIN_MODIFY", ret ? " | " : "");
	}
	if (mask & IN_MOVE_SELF) {
		mask &= ~IN_MOVE_SELF;
		ret += printf("%sIN_MOVE_SELF", ret ? " | " : "");
	}
	if (mask & IN_MOVED_FROM) {
		mask &= ~IN_MOVED_FROM;
		ret += printf("%sIN_MOVED_FROM", ret ? " | " : "");
	}
	if (mask & IN_MOVED_TO) {
		mask &= ~IN_MOVED_TO;
		ret += printf("%sIN_MOVED_TO", ret ? " | " : "");
	}
	if (mask & IN_OPEN) {
		mask &= ~IN_OPEN;
		ret += printf("%sIN_OPEN", ret ? " | " : "");
	}
	if (mask & IN_IGNORED) {
		mask &= ~IN_IGNORED;
		ret += printf("%sIN_IGNORED", ret ? " | " : "");
	}
	if (mask & IN_ISDIR) {
		mask &= ~IN_ISDIR;
		ret += printf("%sIN_ISDIR", ret ? " | " : "");
	}
	if (mask & IN_Q_OVERFLOW) {
		mask &= ~IN_Q_OVERFLOW;
		ret += printf("%sIN_Q_OVERFLOW", ret ? " | " : "");
	}
	if (mask & IN_UNMOUNT) {
		mask &= ~IN_UNMOUNT;
		ret += printf("%sIN_UNMOUNT", ret ? " | " : "");
	}
	if (mask != 0)
		ret += printf("%s0x%.8x", ret ? " | " : "", mask);

	return ret + ret2;
}

void test_handler(struct inotify_event *event, void *cookie)
{
	struct iv_inotify_watch *watch = (struct iv_inotify_watch *)cookie;

	printf("%s", watch->pathname);
	if (event->len != 0)
		printf("/%s", event->name);
	printf(": ");
	print_event(event->mask);
	printf("\n");
}

void *xmalloc(size_t size)
{
	void *ret;

	if ((ret = malloc(size)) == NULL) {
		perror("malloc()");
		exit(EXIT_FAILURE);
	}

	return ret;
}

int main(int argc, char **argv)
{
	unsigned int i;
	struct iv_inotify inotify;
	struct iv_inotify_watch *watch;

	if (argc <= 1) {
		fprintf(stderr, "Use as: %s <filename> [filename] ...\n",
			argv[0] != NULL ? argv[0] : "");
		exit(EXIT_FAILURE);
	}

	iv_init();
	iv_inotify_init(&inotify);

	for (i = 1; i < argc; i++) {
		watch = (struct iv_inotify_watch *)xmalloc(sizeof(*watch));
		watch->pathname = argv[i];
		watch->mask = IN_ALL_EVENTS;
		watch->handler = test_handler;
		watch->cookie = watch;
		iv_inotify_add_watch(&inotify, watch);
	}

	iv_main();

	exit(EXIT_SUCCESS);
}
