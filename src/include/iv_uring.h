/*
 * ivykis, an event handling library
 * Copyright (C) 2020 Lennert Buytenhek
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

#ifndef __IV_URING_H
#define __IV_URING_H

#include <liburing.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_uring_cqe_handler {
	void	*cookie;
	void	(*handler)(void *cookie, int res, unsigned int flags);
	void	*pad[6];
};

void IV_URING_CQE_HANDLER_INIT(struct iv_uring_cqe_handler *);

struct io_uring_sqe *iv_uring_get_sqe(void);

#ifdef __cplusplus
}
#endif


#endif
