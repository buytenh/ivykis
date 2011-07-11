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
 */

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <openssl/err.h>
#include <pthread.h>
#include <signal.h>
#include "iv_openssl.h"
#include "../../modules/thr.h"

/* openssl pthreads locking *************************************************/
static pthread_mutex_t *openssl_lock;

static void iv_openssl_lock_cb(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		pthr_mutex_lock(&openssl_lock[n]);
	else
		pthr_mutex_unlock(&openssl_lock[n]);
}

static void iv_openssl_init(void) __attribute__((constructor));
static void iv_openssl_init(void)
{
	int num_locks = CRYPTO_num_locks();
	int i;

	openssl_lock = malloc(num_locks * sizeof(pthread_mutex_t));
	if (openssl_lock == NULL) {
		fprintf(stderr, "iv_openssl_init: out of memory\n");
		exit(1);
	}

	for (i = 0; i < num_locks; i++)
		pthr_mutex_init(openssl_lock + i, NULL);

	CRYPTO_set_locking_callback(iv_openssl_lock_cb);
}


/* iv_openssl proper ********************************************************/
int iv_openssl_register(struct iv_openssl *ssl)
{
	ssl->ssl = SSL_new(ssl->ctx);
	if (ssl->ssl == NULL)
		return 1;

	SSL_set_fd(ssl->ssl, ssl->fd);

	IV_FD_INIT(&ssl->ifd);
	ssl->ifd.fd = ssl->fd;
	ssl->ifd.cookie = ssl;
	iv_fd_register(&ssl->ifd);

	ssl->ready_in = 1;
	ssl->ready_out = 1;
	INIT_LIST_HEAD(&ssl->req);
	INIT_LIST_HEAD(&ssl->req_rd);
	INIT_LIST_HEAD(&ssl->req_done);

	return 0;
}

void iv_openssl_unregister(struct iv_openssl *ssl)
{
	struct list_head *lh;

	list_for_each (lh, &ssl->req_done) {
		struct iv_openssl_request *req;

		req = container_of(lh, struct iv_openssl_request, list);
		iv_task_unregister(&req->complete);
	}

	iv_fd_unregister(&ssl->ifd);
	SSL_free(ssl->ssl);
}

static void iv_openssl_request_complete(void *_req)
{
	struct iv_openssl_request *req = _req;

	list_del_init(&req->list);
	req->handler(req->cookie, req->ret);
}

static int
__iv_openssl_attempt_request(struct iv_openssl *ssl,
			     struct iv_openssl_request *req)
{
	int ret;
	int ssl_err;
	int err;

	switch (req->type) {
	case IV_OPENSSL_REQ_CONNECT:
		ret = SSL_connect(ssl->ssl);
		break;

	case IV_OPENSSL_REQ_ACCEPT:
		ret = SSL_accept(ssl->ssl);
		break;

	case IV_OPENSSL_REQ_READ:
		ret = SSL_read(ssl->ssl, req->readbuf, req->num);
		break;

	case IV_OPENSSL_REQ_WRITE:
		ret = SSL_write(ssl->ssl, req->writebuf, req->num);
		break;

	case IV_OPENSSL_REQ_SHUTDOWN:
		ret = SSL_shutdown(ssl->ssl);
		break;

	default:
		req->ret = -EOPNOTSUPP;
		return 1;
	}

	if (ret > 0 || (req->type == IV_OPENSSL_REQ_SHUTDOWN && ret == 0)) {
		req->ret = ret;
		return 1;
	}

	ssl_err = SSL_get_error(ssl->ssl, ret);

	if (ssl_err == SSL_ERROR_ZERO_RETURN) {
		req->ret = 0;
		return 1;
	}

	if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
		req->want = ssl_err;
		return 0;
	}

	if (ssl_err == SSL_ERROR_WANT_X509_LOOKUP ||
	    ssl_err == SSL_ERROR_WANT_CONNECT ||
	    ssl_err == SSL_ERROR_WANT_ACCEPT) {
		req->ret = -EINVAL;
		return 1;
	}

	err = ERR_get_error();
	if (err) {
		req->ret = -EIO;
		return 1;
	}

	req->ret = ret ? -errno : 0;

	return 1;
}

static void
iv_openssl_attempt_request(struct iv_openssl *ssl,
			   struct iv_openssl_request *req)
{
	int ret;

	if (req->want == SSL_ERROR_WANT_READ && !ssl->ready_in)
		return;

	if (req->want == SSL_ERROR_WANT_WRITE && !ssl->ready_out)
		return;

	ret = __iv_openssl_attempt_request(ssl, req);
	if (ret) {
		list_del(&req->list);
		list_add_tail(&req->list, &ssl->req_done);
		iv_task_register(&req->complete);
	} else {
		if (req->want == SSL_ERROR_WANT_READ)
			ssl->ready_in = 0;
		else
			ssl->ready_out = 0;
	}
}

static struct iv_openssl_request *
iv_openssl_list_first_req(struct list_head *lh)
{
	if (!list_empty(lh))
		return container_of(lh->next, struct iv_openssl_request, list);

	return NULL;
}

static void
iv_openssl_attempt_request_list(struct iv_openssl *ssl, struct list_head *lh,
				int *want_rd, int *want_wr)
{
	struct iv_openssl_request *req;

	req = iv_openssl_list_first_req(lh);
	if (req != NULL) {
		iv_openssl_attempt_request(ssl, req);

		req = iv_openssl_list_first_req(lh);
		if (req != NULL) {
			if (req->want == SSL_ERROR_WANT_READ)
				*want_rd = 1;
			else if (req->want == SSL_ERROR_WANT_WRITE)
				*want_wr = 1;
		}
	}
}


static void iv_openssl_attempt_requests(struct iv_openssl *ssl);

static void iv_openssl_handler_in(void *_ssl)
{
	struct iv_openssl *ssl = _ssl;

	ssl->ready_in = 1;
	iv_openssl_attempt_requests(ssl);
}

static void iv_openssl_handler_out(void *_ssl)
{
	struct iv_openssl *ssl = _ssl;

	ssl->ready_out = 1;
	iv_openssl_attempt_requests(ssl);
}

static void iv_openssl_attempt_requests(struct iv_openssl *ssl)
{
	int want_rd = 0;
	int want_wr = 0;

	iv_openssl_attempt_request_list(ssl, &ssl->req, &want_rd, &want_wr);
	iv_openssl_attempt_request_list(ssl, &ssl->req_rd, &want_rd, &want_wr);

	if (want_rd && ssl->ifd.handler_in == NULL)
		iv_fd_set_handler_in(&ssl->ifd, iv_openssl_handler_in);
	else if (!want_rd && ssl->ifd.handler_in != NULL)
		iv_fd_set_handler_in(&ssl->ifd, NULL);

	if (want_wr && ssl->ifd.handler_out == NULL)
		iv_fd_set_handler_out(&ssl->ifd, iv_openssl_handler_out);
	else if (!want_wr && ssl->ifd.handler_out != NULL)
		iv_fd_set_handler_out(&ssl->ifd, NULL);
}

void iv_openssl_request_init(struct iv_openssl_request *req)
{
	INIT_LIST_HEAD(&req->list);
	IV_TASK_INIT(&req->complete);
}

void iv_openssl_request_submit(struct iv_openssl_request *req)
{
	struct iv_openssl *ssl = req->ssl;
	struct list_head *list;
	int is_empty;

	if (req->type == IV_OPENSSL_REQ_READ)
		list = &ssl->req_rd;
	else
		list = &ssl->req;

	is_empty = !!list_empty(list);

	list_add_tail(&req->list, list);
	req->want = 0;
	req->ret = 0;
	IV_TASK_INIT(&req->complete);
	req->complete.cookie = req;
	req->complete.handler = iv_openssl_request_complete;

	if (is_empty)
		iv_openssl_attempt_requests(ssl);
}

void iv_openssl_request_cancel(struct iv_openssl_request *req)
{
	/* @@@ Check if this request has already been submitted.  */

	list_del(&req->list);
	if (iv_task_registered(&req->complete))
		iv_task_unregister(&req->complete);
}
