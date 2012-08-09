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

#define NUM		4

struct req {
	struct iv_popen_request popen_req;
	char *argv[3];
	struct iv_fd popen_fd;
	struct iv_timer closeit;
};

static void done(struct req *req, int timeout)
{
	iv_popen_request_close(&req->popen_req);

	iv_fd_unregister(&req->popen_fd);
	close(req->popen_fd.fd);

	if (!timeout)
		iv_timer_unregister(&req->closeit);
}

static void got_data(void *_req)
{
	struct req *req = _req;
	char buf[1024];
	int ret;

	ret = read(req->popen_fd.fd, buf, sizeof(buf));
	if (ret <= 0) {
		if (ret == 0) {
			fprintf(stderr, "got EOF\n");
			done(req, 0);
		} else if (errno != EAGAIN && errno != EINTR) {
			perror("read");
			done(req, 0);
		}

		return;
	}

	printf("%p: ", req);
	fwrite(buf, 1, ret, stdout);
}

static void do_close(void *_req)
{
	struct req *req = _req;

	printf("%p: timeout expired, closing the popen request\n", req);
	done(req, 1);
}

static void open_child_request(struct req *req)
{
	int f;

	IV_POPEN_REQUEST_INIT(&req->popen_req);
	req->popen_req.file = "/usr/bin/vmstat";
	req->argv[0] = "/usr/bin/vmstat";
	req->argv[1] = "1";
	req->argv[2] = NULL;
	req->popen_req.argv = req->argv;
	req->popen_req.type = "r";
	f = iv_popen_request_submit(&req->popen_req);

	printf("submitted the popen request, fd is %d\n", f);

	IV_FD_INIT(&req->popen_fd);
	req->popen_fd.fd = f;
	req->popen_fd.cookie = req;
	req->popen_fd.handler_in = got_data;
	iv_fd_register(&req->popen_fd);

	IV_TIMER_INIT(&req->closeit);
	iv_validate_now();
	req->closeit.expires = iv_now;
	req->closeit.expires.tv_sec += 5;
	req->closeit.cookie = req;
	req->closeit.handler = do_close;
	iv_timer_register(&req->closeit);
}

int main()
{
	struct req req[NUM];
	int i;

	iv_init();

	for (i = 0; i < NUM; i++)
		open_child_request(&req[i]);

	iv_main();

	iv_deinit();

	return 0;
}
