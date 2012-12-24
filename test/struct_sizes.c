/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
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
#include "../iv_private.h"

int main()
{
	int fail;

	fail = 0;

#ifndef _WIN32
	if (sizeof(struct iv_fd) < sizeof(struct iv_fd_)) {
		fprintf(stderr, "struct iv_fd: %d\n",
			(int)sizeof(struct iv_fd));
		fprintf(stderr, "struct iv_fd_: %d\n",
			(int)sizeof(struct iv_fd_));
		fprintf(stderr, "\t=> TOO SMALL\n");
		fprintf(stderr, "\n");
		fail = 1;
	}
#else
	if (sizeof(struct iv_handle) < sizeof(struct iv_handle_)) {
		fprintf(stderr, "struct iv_handle: %d\n",
			(int)sizeof(struct iv_handle));
		fprintf(stderr, "struct iv_handle_: %d\n",
			(int)sizeof(struct iv_handle_));
		fprintf(stderr, "\t=> TOO SMALL\n");
		fprintf(stderr, "\n");
		fail = 1;
	}
#endif

	if (sizeof(struct iv_task) < sizeof(struct iv_task_)) {
		fprintf(stderr, "struct iv_task: %d\n",
			(int)sizeof(struct iv_task));
		fprintf(stderr, "struct iv_task_: %d\n",
			(int)sizeof(struct iv_task_));
		fprintf(stderr, "\t=> TOO SMALL\n");
		fprintf(stderr, "\n");
		fail = 1;
	}

	if (sizeof(struct iv_timer) < sizeof(struct iv_timer_)) {
		fprintf(stderr, "struct iv_timer: %d\n",
			(int)sizeof(struct iv_timer));
		fprintf(stderr, "struct iv_timer_: %d\n",
			(int)sizeof(struct iv_timer_));
		fprintf(stderr, "\t=> TOO SMALL\n");
		fprintf(stderr, "\n");
		fail = 1;
	}

	return fail;
}
