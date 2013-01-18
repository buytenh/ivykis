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
#include <fcntl.h>
#include <iv.h>
#include <iv_popen.h>
#include <iv_wait.h>
#include <signal.h>
#include <string.h>

/*
 * This struct contains the child state that may need to persist
 * after iv_popen_request_close().
 *
 * If parent == NULL, the corresponding popen request has been
 * closed, and signal_timer will be running to periodically kill
 * the child until it dies.
 */
struct iv_popen_running_child {
	struct iv_wait_interest		wait;
	struct iv_popen_request		*parent;
	struct iv_timer			signal_timer;
	int				num_kills;
};

static void iv_popen_running_child_wait(void *_ch, int status,
					const struct rusage *rusage)
{
	struct iv_popen_running_child *ch = _ch;

	if (!WIFEXITED(status) && !WIFSIGNALED(status))
		return;

	iv_wait_interest_unregister(&ch->wait);
	if (ch->parent != NULL)
		ch->parent->child = NULL;
	else
		iv_timer_unregister(&ch->signal_timer);

	free(ch);
}

struct iv_popen_spawn_info {
	struct iv_popen_request *this;
	int for_read;
	int data_pipe[2];
};

static void iv_popen_child(void *cookie)
{
	struct iv_popen_spawn_info *info = cookie;
	int devnull;

	devnull = open("/dev/null", O_RDWR);
	if (devnull < 0) {
		iv_fatal("iv_popen_child: got error %d[%s] opening "
			 "/dev/null", errno, strerror(errno));
	}

	if (info->for_read) {
		dup2(devnull, 0);
		dup2(info->data_pipe[1], 1);
		dup2(devnull, 2);
	} else {
		dup2(info->data_pipe[0], 0);
		dup2(devnull, 1);
		dup2(devnull, 2);
	}

	close(info->data_pipe[0]);
	close(info->data_pipe[1]);
	close(devnull);

	execvp(info->this->file, info->this->argv);
	perror("execvp");
}

int iv_popen_request_submit(struct iv_popen_request *this)
{
	struct iv_popen_running_child *ch;
	struct iv_popen_spawn_info info;
	int ret;
	int fd;

	ch = malloc(sizeof(*ch));
	if (ch == NULL) {
		fprintf(stderr, "iv_popen_request_submit: out of memory\n");
		return -1;
	}

	info.this = this;

	if (!strcmp(this->type, "r")) {
		info.for_read = 1;
	} else if (!strcmp(this->type, "w")) {
		info.for_read = 0;
	} else {
		fprintf(stderr, "iv_popen_request_submit: invalid type\n");
		free(ch);
		return -1;
	}

	if (pipe(info.data_pipe) < 0) {
		perror("pipe");
		free(ch);
		return -1;
	}

	IV_WAIT_INTEREST_INIT(&ch->wait);
	ch->wait.cookie = ch;
	ch->wait.handler = iv_popen_running_child_wait;
	ch->parent = this;

	ret = iv_wait_interest_register_spawn(&ch->wait,
					      iv_popen_child, &info);
	if (ret < 0) {
		perror("fork");
		close(info.data_pipe[1]);
		close(info.data_pipe[0]);
		free(ch);
		return -1;
	}

	this->child = ch;

	if (info.for_read) {
		fd = info.data_pipe[0];
		close(info.data_pipe[1]);
	} else {
		fd = info.data_pipe[1];
		close(info.data_pipe[0]);
	}

	return fd;
}


#define MAX_SIGTERM_COUNT	5
#define SIGNAL_INTERVAL		5

static void iv_popen_running_child_timer(void *_ch)
{
	struct iv_popen_running_child *ch = _ch;
	int signum;
	int ret;

	signum = (ch->num_kills++ < MAX_SIGTERM_COUNT) ? SIGTERM : SIGKILL;

	ret = iv_wait_interest_kill(&ch->wait, signum);
	if (ret < 0) {
		iv_wait_interest_unregister(&ch->wait);
		free(ch);
		return;
	}

	iv_validate_now();
	ch->signal_timer.expires = iv_now;
	ch->signal_timer.expires.tv_sec += SIGNAL_INTERVAL;
	iv_timer_register(&ch->signal_timer);
}

void iv_popen_request_close(struct iv_popen_request *this)
{
	struct iv_popen_running_child *ch = this->child;

	if (ch != NULL) {
		ch->parent = NULL;

		IV_TIMER_INIT(&ch->signal_timer);
		iv_validate_now();
		ch->signal_timer.expires = iv_now;
		ch->signal_timer.handler = iv_popen_running_child_timer;
		ch->signal_timer.cookie = ch;
		iv_timer_register(&ch->signal_timer);

		ch->num_kills = 0;
	}
}
