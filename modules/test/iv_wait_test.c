/*
 * ivykis, an event handling library
 * Copyright (C) 2010 Lennert Buytenhek
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
#include <iv.h>
#include <iv_wait.h>

static void handler(void *cookie, int status, struct rusage *rusage)
{
	struct iv_wait_interest *this = cookie;

	printf("wait4: status of pid %d is %d\n", this->pid, status);
	iv_wait_interest_unregister(this);
}

int main()
{
	int p[2];
	pid_t pid;

	if (pipe(p) < 0) {
		perror("pipe");
		return 1;
	}

	pid = fork();
	if (pid) {
		struct iv_wait_interest this;

		iv_init();

		this.pid = pid;
		this.cookie = &this;
		this.handler = handler;
		iv_wait_interest_register(&this);

		sleep(1);
		write(p[1], "", 1);

		iv_main();

		iv_deinit();
	} else {
		char c;

		/*
		 * Wait for parent task to run first, so that it can
		 * setup its wait interest (for which it needs to know
		 * our pid).  It will notify us when it's done with that
		 * by writing to the pipe.
		 */
		read(p[0], &c, 1);

		printf("dying\n");
	}

	return 0;
}
