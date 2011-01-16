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

#ifndef __IV_POPEN_H
#define __IV_POPEN_H

#ifdef __cplusplus
extern "C" {
#endif

struct iv_popen_request {
	char		*file;
	char		**argv;
	char		*type;

	void		*child;
};

static inline void IV_POPEN_REQUEST_INIT(struct iv_popen_request *this)
{
}

int iv_popen_request_submit(struct iv_popen_request *this);
void iv_popen_request_close(struct iv_popen_request *this);

#ifdef __cplusplus
}
#endif


#endif
