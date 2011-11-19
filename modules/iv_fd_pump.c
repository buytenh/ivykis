/*
 * ivykis, an event handling library
 * Copyright (C) 2011 Lennert Buytenhek
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
#include <string.h>
#include <syslog.h>
#include "iv_fd_pump.h"

#define BUF_SIZE	4096


int iv_fd_pump_init(struct iv_fd_pump *ip)
{
	ip->buf = malloc(BUF_SIZE);
	if (ip->buf == NULL)
		return -1;

	ip->bytes = 0;
	ip->full = 0;
	ip->saw_fin = 0;

	ip->set_bands(ip->cookie, 1, 0);

	return 0;
}

void iv_fd_pump_destroy(struct iv_fd_pump *ip)
{
	ip->set_bands(ip->cookie, 0, 0);

	free(ip->buf);
}

static int iv_fd_pump_try_input(struct iv_fd_pump *ip)
{
	int ret;

	ret = read(ip->from_fd, ip->buf + ip->bytes, BUF_SIZE - ip->bytes);
	if (ret < 0) {
		if (errno != EAGAIN)
			return -1;

		return 0;
	}

	if (ret == 0) {
		ip->saw_fin = 1;
		if (!ip->bytes) {
			shutdown(ip->to_fd, SHUT_WR);
			ip->saw_fin = 2;
		}
		return 0;
	}

	ip->bytes += ret;
	if (ip->bytes == BUF_SIZE)
		ip->full = 1;

	return 0;
}

static int iv_fd_pump_try_output(struct iv_fd_pump *ip)
{
	int ret;

	ret = write(ip->to_fd, ip->buf, ip->bytes);
	if (ret <= 0)
		return (ret < 0 && errno == EAGAIN) ? 0 : -1;

	ip->full = 0;

	ip->bytes -= ret;
	memmove(ip->buf, ip->buf + ret, ip->bytes);

	if (!ip->bytes && ip->saw_fin == 1) {
		shutdown(ip->to_fd, SHUT_WR);
		ip->saw_fin = 2;
	}

	return 0;
}

int iv_fd_pump_pump(struct iv_fd_pump *ip)
{
	if (!ip->full && ip->saw_fin == 0) {
		if (iv_fd_pump_try_input(ip))
			return -1;
	}

	if (ip->bytes || ip->saw_fin == 1) {
		if (iv_fd_pump_try_output(ip))
			return -1;
	}


	switch (ip->saw_fin) {
	case 0:
		ip->set_bands(ip->cookie, !ip->full, !!ip->bytes);
		return 1;

	case 1:
		ip->set_bands(ip->cookie, 0, 1);
		return 1;

	case 2:
		ip->set_bands(ip->cookie, 0, 0);
		return 0;
	}

	syslog(LOG_CRIT, "iv_fd_pump_pump: saw_fin == %d", ip->saw_fin);
	abort();
}

int iv_fd_pump_is_done(struct iv_fd_pump *ip)
{
	return !!(ip->saw_fin == 2);
}
