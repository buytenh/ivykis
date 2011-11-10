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
#include <iv_thread.h>
#include <iv_wait.h>
#include <signal.h>
#include <time.h>

static void handler(void *cookie, int status, struct rusage *rusage)
{
	struct iv_wait_interest *this = cookie;

	printf("wait4: status of pid %d is %.4x: ", (int)this->pid, status);

	if (WIFSTOPPED(status))
		printf("stopped by signal %d\n", WSTOPSIG(status));

	/*
	 * On FreeBSD, WIFCONTINUED(status) => WIFSIGNALED(status).
	 */
#ifdef WIFCONTINUED
	if (WIFCONTINUED(status)) {
		printf("resumed by delivery of SIGCONT\n");
	} else
#endif
	if (WIFSIGNALED(status)) {
		printf("terminated by signal %d, core %s\n", WTERMSIG(status),
#ifdef WCOREDUMP
		       WCOREDUMP(status) ? "dumped" : "not dumped");
#else
		       "dump status unknown");
#endif
		iv_wait_interest_unregister(this);
	}

	if (WIFEXITED(status)) {
		printf("exited with status %d\n", WEXITSTATUS(status));
		iv_wait_interest_unregister(this);
	}
}

static void dosleep(int msec)
{
	struct timespec ts;
	int ret;

	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;

	do {
		ret = nanosleep(&ts, &ts);
	} while (ret);
}

static void thr(void *cookie)
{
	int pid = (int)(unsigned long)cookie;

	dosleep(2500);

	printf("sending child SIGSTOP\n");
	kill(pid, SIGSTOP);

	dosleep(2500);

	printf("sending child SIGCONT\n");
	kill(pid, SIGCONT);

	dosleep(2500);

	printf("sending child SIGTERM\n");
	kill(pid, SIGTERM);
}

static void child(void *cookie)
{
	int i;

	for (i = 0; i < 10; i++) {
		printf("child sleeping %d\n", i);
		dosleep(1000);
	}

	printf("dying\n");

	exit(0);
}

int main()
{
	struct iv_wait_interest this;
	int ret;

	iv_init();

	IV_WAIT_INTEREST_INIT(&this);
	this.cookie = &this;
	this.handler = handler;

	ret = iv_wait_interest_register_spawn(&this, child, NULL);
	if (ret < 0) {
		perror("fork");
		return -1;
	}

	iv_thread_create("thr", thr, (void *)(unsigned long)this.pid);

	iv_main();

	iv_deinit();

	return 0;
}
