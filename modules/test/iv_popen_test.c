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
#include <iv_popen.h>

static struct iv_popen_request popen_req;
static struct iv_fd popen_fd;
static struct iv_timer closeit;

static void done(int timeout)
{
	iv_popen_request_close(&popen_req);

	iv_unregister_fd(&popen_fd);
	close(popen_fd.fd);

	if (!timeout)
		iv_unregister_timer(&closeit);
}

static void got_data(void *_dummy)
{
	char buf[1024];
	int ret;

	ret = read(popen_fd.fd, buf, sizeof(buf));
	if (ret <= 0) {
		if (ret == 0) {
			fprintf(stderr, "got EOF\n");
			done(0);
		} else if (errno != EAGAIN && errno != EINTR) {
			perror("read");
			done(0);
		}

		return;
	}

	write(1, buf, ret);
}

static void do_close(void *_dummy)
{
	printf("timeout expired, closing the popen request\n");
	done(1);
}

int main()
{
	char *argv[3];
	int f;

	iv_init();

	popen_req.file = "/usr/bin/vmstat";
	argv[0] = "/usr/bin/vmstat";
	argv[1] = "1";
	argv[2] = NULL;
	popen_req.argv = argv;
	popen_req.type = "r";
	f = iv_popen_request_submit(&popen_req);

	printf("submitted the popen request, fd is %d\n", f);

	INIT_IV_FD(&popen_fd);
	popen_fd.fd = f;
	popen_fd.handler_in = got_data;
	iv_register_fd(&popen_fd);

	INIT_IV_TIMER(&closeit);
	iv_validate_now();
	closeit.expires = now;
	closeit.expires.tv_sec += 5;
	closeit.handler = do_close;
	iv_register_timer(&closeit);

	iv_main();

	iv_deinit();

	return 0;
}
