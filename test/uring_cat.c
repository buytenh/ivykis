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
#include <fcntl.h>
#include <iv.h>
#include <iv_uring.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char *argv0;
static struct iv_uring_cqe_handler ch;
static const char *path;
static int fd;
static off_t off;
static char buffer[4096];

static struct io_uring_sqe *get_sqe(void)
{
	struct io_uring_sqe *sqe;

	sqe = iv_uring_get_sqe();
	if (sqe == NULL) {
		fprintf(stderr, "error obtaining an sqe\n");
		exit(EXIT_FAILURE);
	}

	return sqe;
}

static void close_done(void *cookie, int res, unsigned int flags)
{
	if (res < 0)
		fprintf(stderr, "closing %s: %s\n", path, strerror(-res));
}

static void read_done(void *cookie, int res, unsigned int flags)
{
	struct io_uring_sqe *sqe;

	if (res < 0) {
		fprintf(stderr, "reading from %s: %s\n", path, strerror(-res));
		return;
	}

	if (res > 0) {
		write(1, buffer, res);
		off += res;

		sqe = get_sqe();
		io_uring_prep_read(sqe, fd, buffer, sizeof(buffer), off);
		io_uring_sqe_set_data(sqe, &ch);
	} else if (fd > 0) {
		sqe = get_sqe();
		io_uring_prep_close(sqe, fd);
		io_uring_sqe_set_data(sqe, &ch);

		ch.handler = close_done;
	}
}

static void start_read(int _fd)
{
	struct io_uring_sqe *sqe;

	fd = _fd;
	off = 0;

	sqe = get_sqe();
	io_uring_prep_read(sqe, fd, buffer, sizeof(buffer), off);
	io_uring_sqe_set_data(sqe, &ch);

	ch.handler = read_done;
}

static void open_done(void *cookie, int res, unsigned int flags)
{
	if (res < 0) {
		fprintf(stderr, "%s: %s: %s\n", argv0, path, strerror(-res));
		return;
	}

	start_read(res);
}

int main(int argc, char *argv[])
{
	argv0 = argv[0];

	iv_init();

	IV_URING_CQE_HANDLER_INIT(&ch);

	if (argc == 1) {
		path = "stdin";
		start_read(0);

		iv_main();
	} else {
		int i;

		for (i = 1; i < argc; i++) {
			struct io_uring_sqe *sqe;

			path = argv[i];

			sqe = get_sqe();
			io_uring_prep_openat(sqe, AT_FDCWD, path, 0, O_RDONLY);
			io_uring_sqe_set_data(sqe, &ch);

			ch.handler = open_done;

			iv_main();
		}
	}

	iv_deinit();

	return 0;
}
