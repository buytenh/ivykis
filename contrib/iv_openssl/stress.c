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
#include <netinet/in.h>
#include "iv_openssl.h"

struct querier {
	struct sockaddr_in		addr;
	SSL_CTX				*ctx;
	char				*request;
	int				request_len;
	void				*cookie;
	void				(*handler)(void *cookie, int err);

	struct iv_fd			fd;
	struct iv_openssl		ssl;
	struct iv_openssl_request	req_wr;
	struct iv_openssl_request	req_rd;
	uint8_t				buf_rd[16384];
};


static void read_done(void *_q, int ret)
{
	struct querier *q = _q;

	if (ret <= 0) {
		iv_openssl_unregister(&q->ssl);
		close(q->ssl.ifd.fd);
		q->handler(q->cookie, !!(ret < 0));
		return;
	}

	iv_openssl_request_submit(&q->req_rd);
}

static void shutdown_done(void *_q, int ret)
{
}

static void write_done(void *_q, int ret)
{
	struct querier *q = _q;

	q->req_wr.type = IV_OPENSSL_REQ_SHUTDOWN;
	q->req_wr.handler = shutdown_done;
	iv_openssl_request_submit(&q->req_wr);
}

static const char *cipher_name_prev;

static void ssl_connect_done(void *_q, int ret)
{
	struct querier *q = _q;
	const SSL_CIPHER *cipher;
	const char *cipher_name;

	if (ret <= 0) {
		iv_openssl_unregister(&q->ssl);
		close(q->ssl.ifd.fd);
		q->handler(q->cookie, 1);
		return;
	}

	cipher = SSL_get_current_cipher(q->ssl.ssl);
	cipher_name = SSL_CIPHER_get_name(cipher);
	if (cipher_name != cipher_name_prev) {
		cipher_name_prev = cipher_name;
		printf("cipher: %s\n", cipher_name);
	}

	q->req_rd.ssl = &q->ssl;
	q->req_rd.type = IV_OPENSSL_REQ_READ;
	q->req_rd.readbuf = q->buf_rd;
	q->req_rd.num = sizeof(q->buf_rd);
	q->req_rd.cookie = q;
	q->req_rd.handler = read_done;
	iv_openssl_request_submit(&q->req_rd);

	q->req_wr.type = IV_OPENSSL_REQ_WRITE;
	q->req_wr.writebuf = q->request;
	q->req_wr.num = q->request_len;
	q->req_wr.handler = write_done;
	iv_openssl_request_submit(&q->req_wr);
}

static void connect_done(void *_q)
{
	struct querier *q = _q;
	socklen_t len;
	int ret;

	len = sizeof(ret);
	if (getsockopt(q->fd.fd, SOL_SOCKET, SO_ERROR, &ret, &len) < 0) {
		fprintf(stderr, "connect_done: error %d while "
				"getsockopt(SO_ERROR)\n", errno);
		abort();
	}

	if (ret) {
		if (ret != EINPROGRESS) {
			fprintf(stderr, "connect: %s\n", strerror(ret));
			iv_fd_unregister(&q->fd);
			close(q->fd.fd);
			q->handler(q->cookie, 1);
		}
		return;
	}

	iv_fd_unregister(&q->fd);

	q->ssl.ctx = q->ctx;
	q->ssl.fd = q->fd.fd;
	iv_openssl_register(&q->ssl);

	SSL_set_options(q->ssl.ssl, SSL_OP_NO_COMPRESSION);

	q->req_wr.ssl = &q->ssl;
	q->req_wr.type = IV_OPENSSL_REQ_CONNECT;
	q->req_wr.cookie = q;
	q->req_wr.handler = ssl_connect_done;
	iv_openssl_request_submit(&q->req_wr);
}

static void start_querier(struct querier *q)
{
	int fd;
	int ret;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(1);
	}

	IV_FD_INIT(&q->fd);
	q->fd.fd = fd;
	q->fd.cookie = (void *)q;
	q->fd.handler_in = connect_done;
	q->fd.handler_out = connect_done;
	iv_fd_register(&q->fd);

	ret = connect(fd, (struct sockaddr *)&q->addr, sizeof(q->addr));
	if (ret == 0 || errno != EINPROGRESS)
		connect_done((void *)q);
}


#define NUM	32

static struct querier q[NUM];
static int num_complete;
static int got_err;

static void querier_done(void *_q, int err)
{
	struct querier *q = _q;

	got_err |= err;
	if (!got_err) {
		if (!(num_complete % 1000))
			printf("%d done\n", num_complete);

		if (++num_complete < 10000)
			start_querier(q);
	}
}


static char *request = "GET / HTTP/1.0\r\n\r\n";

int main()
{
	SSL_CTX *ctx;
	int i;

	SSL_load_error_strings();
	SSL_library_init();

	ctx = SSL_CTX_new(TLSv1_method());
	if (ctx == NULL)
		return 1;

	iv_init();

	for (i = 0; i < NUM; i++) {
		q[i].addr.sin_family = AF_INET;
		q[i].addr.sin_addr.s_addr = htonl(0x7f000001);
		q[i].addr.sin_port = htons(12345);
		q[i].ctx = ctx;
		q[i].request = request;
		q[i].request_len = strlen(request);
		q[i].cookie = q + i;
		q[i].handler = querier_done;
		start_querier(q + i);
	}

	iv_main();

	iv_deinit();

	SSL_CTX_free(ctx);

	return 0;
}
