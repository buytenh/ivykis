/*
 * ivykis, an event handling library
 * Copyright (C) 2011 Lennert Buytenhek
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
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU Lesser General Public License in all respects
 * for all of the code used other than OpenSSL. If you modify files
 * with this exception, you may extend this exception to your version
 * of the files, but you are not obligated to do so. If you do not
 * wish to do so, delete this exception statement from your version.
 */

#ifndef __IV_OPENSSL_H
#define __IV_OPENSSL_H

#include <iv_list.h>
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_openssl {
	SSL_CTX			*ctx;
	int			fd;

	SSL			*ssl;
	struct iv_fd		ifd;
	unsigned		ready_in:1;
	unsigned		ready_out:1;
	struct iv_list_head	req;
	struct iv_list_head	req_rd;
	struct iv_list_head	req_done;
};

int iv_openssl_register(struct iv_openssl *ssl);
void iv_openssl_unregister(struct iv_openssl *ssl);


enum iv_openssl_req_type {
	IV_OPENSSL_REQ_CONNECT,
	IV_OPENSSL_REQ_ACCEPT,
	IV_OPENSSL_REQ_DO_HANDSHAKE,
	IV_OPENSSL_REQ_READ,
	IV_OPENSSL_REQ_WRITE,
	IV_OPENSSL_REQ_SHUTDOWN,
};

struct iv_openssl_request {
	struct iv_openssl		*ssl;
	enum iv_openssl_req_type	type;
	struct {
		union {
			void		*readbuf;
			const void	*writebuf;
		};
		int			num;
	};
	void				*cookie;
	void				(*handler)(void *cookie, int ret);

	struct iv_list_head	list;
	int			want;
	int			ret;
	struct iv_task		complete;
};

void iv_openssl_request_init(struct iv_openssl_request *req);
void iv_openssl_request_submit(struct iv_openssl_request *req);
void iv_openssl_request_cancel(struct iv_openssl_request *req);

#ifdef __cplusplus
}
#endif


#endif
