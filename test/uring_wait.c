/*
 * ivykis, an event handling library
 * Copyright (C) 2020 Lennert Buytenhek
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
#include <iv_uring.h>

static int success;

static void handler(void *cookie, int res, unsigned int flags)
{
	success = 1;
}

int main()
{
	struct iv_uring_cqe_handler ch;
	struct io_uring_sqe *sqe;
	struct __kernel_timespec ts;

	iv_init();

	IV_URING_CQE_HANDLER_INIT(&ch);
	ch.handler = handler;

	sqe = iv_uring_get_sqe();
	if (sqe == NULL) {
		fprintf(stderr, "error obtaining an SQE\n");
		return 77;
	}

	ts.tv_sec = 0;
	ts.tv_nsec = 10L * 1000 * 1000;

	io_uring_prep_timeout(sqe, &ts, 0, 0);
	io_uring_sqe_set_data(sqe, &ch);

	iv_main();

	iv_deinit();

	return !success;
}
