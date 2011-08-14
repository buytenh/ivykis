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
#include <iv_signal.h>
#include <signal.h>

static struct iv_signal is_sigterm;

static void got_sigterm(void *_x)
{
	printf("got SIGTERM in parent process, race?\n");
}

int main()
{
	char *argv[3];
	struct iv_popen_request popen_req;

	iv_init();

	IV_SIGNAL_INIT(&is_sigterm);
	is_sigterm.signum = SIGTERM;
	is_sigterm.handler = got_sigterm;
	iv_signal_register(&is_sigterm);

	IV_POPEN_REQUEST_INIT(&popen_req);
	popen_req.file = "/bin/sleep";
	argv[0] = "/bin/sleep";
	argv[1] = "3600";
	argv[2] = NULL;
	popen_req.argv = argv;
	popen_req.type = "r";
	iv_popen_request_submit(&popen_req);

	iv_popen_request_close(&popen_req);

	iv_main();

	iv_deinit();

	return 0;
}
