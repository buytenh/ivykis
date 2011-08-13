/*
 * iv_inotify, an ivykis inotify component.
 * Copyright (C) 2008, 2009 Ronald Huizer
 * Dedicated to Kanna Ishihara.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version
 * 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 2.1 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License version 2.1 along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <iv_inotify.h>

static void print_event(int mask)
{
	int ret = 0;

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
}

static void test_handler(void *_w, struct inotify_event *event)
{
	struct iv_inotify_watch *w = _w;

	printf("%s", w->pathname);
	if (event->len != 0)
		printf("/%s", event->name);
	printf(": ");
	print_event(event->mask);
	printf("\n");
}

int main(int argc, char **argv)
{
	struct iv_inotify inotify;
	int i;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s FILE...\n", argv[0]);
		return 1;
	}

	iv_init();

	IV_INOTIFY_INIT(&inotify);
	iv_inotify_register(&inotify);

	for (i = 1; i < argc; i++) {
		struct iv_inotify_watch *w;

		w = malloc(sizeof(*w));
		if (w == NULL) {
			perror("malloc");
			return 1;
		}

		IV_INOTIFY_WATCH_INIT(w);
		w->inotify = &inotify;
		w->pathname = argv[i];
		w->mask = IN_ALL_EVENTS;
		w->cookie = w;
		w->handler = test_handler;
		iv_inotify_watch_register(w);
	}

	iv_main();

	iv_deinit();

	return 0;
}
