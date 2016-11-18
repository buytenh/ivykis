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

static void handler(void *cookie, int status, const struct rusage *rusage)
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
	struct timeval curtime;
	struct timeval until;

	gettimeofday(&curtime, NULL);

	until = curtime;
	until.tv_sec += msec / 1000;
	until.tv_usec += (msec % 1000) * 1000;
	if (until.tv_usec >= 1000000) {
		until.tv_sec++;
		until.tv_usec -= 1000000;
	}

	while (1) {
		struct timeval tv;

		tv.tv_sec = until.tv_sec - curtime.tv_sec;
		tv.tv_usec = until.tv_usec - curtime.tv_usec;
		if (tv.tv_usec < 0) {
			tv.tv_sec--;
			tv.tv_usec += 1000000;
		}

		if (tv.tv_sec < 0 || select(0, NULL, NULL, NULL, &tv) == 0)
			break;

		gettimeofday(&curtime, NULL);
	}
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
