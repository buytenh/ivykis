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

#ifndef __IV_FD_PUMP_H
#define __IV_FD_PUMP_H

#ifdef __cplusplus
extern "C" {
#endif

struct iv_fd_pump {
	int		from_fd;
	int		to_fd;
	void		*cookie;
	void		(*set_bands)(void *cookie, int pollin, int pollout);
	unsigned int	flags;

	void		*buf;
	int		bytes;
	int		full;
	int		saw_fin;
};

static inline void IV_FD_PUMP_INIT(struct iv_fd_pump *this)
{
}

#define IV_FD_PUMP_FLAG_RELAY_EOF	1

void iv_fd_pump_init(struct iv_fd_pump *ip);
void iv_fd_pump_destroy(struct iv_fd_pump *ip);
int iv_fd_pump_pump(struct iv_fd_pump *ip);
int iv_fd_pump_is_done(const struct iv_fd_pump *ip);

#ifdef __cplusplus
}
#endif


#endif
