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
#include <iv_thread.h>
#include <netinet/in.h>
#include "iv_openssl.h"

struct connection {
	int				fd;
	struct iv_openssl		ssl;

	struct iv_openssl_request	req_rd;
	uint8_t				buf[1024];
	int				buflen;

	struct iv_openssl_request	req_wr;
};


static void kill_connection(struct connection *conn)
{
	iv_openssl_request_cancel(&conn->req_rd);
	iv_openssl_request_cancel(&conn->req_wr);
	iv_openssl_unregister(&conn->ssl);
	close(conn->fd);
	free(conn);
}

static void write_done(void *_conn, int ret)
{
	struct connection *conn = _conn;

	if (ret != conn->req_wr.num) {
		kill_connection(conn);
		return;
	}

	iv_openssl_request_submit(&conn->req_rd);
}

static void read_done(void *_conn, int ret)
{
	struct connection *conn = _conn;

	/* @@@ Implement sending close notify.  */
	if (ret <= 0) {
		kill_connection(conn);
		return;
	}

	conn->req_wr.ssl = &conn->ssl;
	conn->req_wr.type = IV_OPENSSL_REQ_WRITE;
	conn->req_wr.writebuf = conn->buf;
	conn->req_wr.num = ret;
	conn->req_wr.cookie = conn;
	conn->req_wr.handler = write_done;
	iv_openssl_request_submit(&conn->req_wr);
}

static void accept_done(void *_conn, int ret)
{
	struct connection *conn = _conn;

	if (ret <= 0) {
		kill_connection(conn);
		return;
	}

	if (0) {
		int bits;

		SSL_get_cipher_bits(conn->ssl.ssl, &bits);
		printf("%s connection established with %s (%d bit)\n",
			SSL_get_cipher_version(conn->ssl.ssl),
			SSL_get_cipher_name(conn->ssl.ssl), bits);
	}

	conn->req_rd.ssl = &conn->ssl;
	conn->req_rd.type = IV_OPENSSL_REQ_READ;
	conn->req_rd.readbuf = conn->buf;
	conn->req_rd.num = sizeof(conn->buf);
	conn->req_rd.cookie = conn;
	conn->req_rd.handler = read_done;
	iv_openssl_request_submit(&conn->req_rd);
}


struct worker_thread {
	SSL_CTX *ctx;
	struct iv_fd sock;
};

static void got_connection(void *_wt)
{
	struct worker_thread *wt = _wt;
	struct sockaddr_in addr;
	socklen_t addrlen;
	struct connection *conn;
	int fd;

	addrlen = sizeof(addr);
	fd = accept(wt->sock.fd, (struct sockaddr *)&addr, &addrlen);
	if (fd <= 0) {
		if (fd < 0 && errno == EAGAIN)
			return;
		perror("accept");
		exit(-1);
	}

	conn = malloc(sizeof(*conn));
	if (conn == NULL) {
		close(fd);
		return;
	}

	conn->fd = fd;

	conn->ssl.ctx = wt->ctx;
	conn->ssl.fd = fd;
	iv_openssl_register(&conn->ssl);

	SSL_use_certificate_file(conn->ssl.ssl, "server.crt", SSL_FILETYPE_PEM);
	SSL_use_PrivateKey_file(conn->ssl.ssl, "server.key", SSL_FILETYPE_PEM);

	iv_openssl_request_init(&conn->req_rd);
	conn->req_rd.ssl = &conn->ssl;
	conn->req_rd.type = IV_OPENSSL_REQ_ACCEPT;
	conn->req_rd.cookie = conn;
	conn->req_rd.handler = accept_done;
	iv_openssl_request_submit(&conn->req_rd);

	iv_openssl_request_init(&conn->req_wr);
}


static int fd;

static void worker(void *_dummy)
{
	struct worker_thread wt;

	wt.ctx = SSL_CTX_new(TLSv1_method());
	if (wt.ctx == NULL)
		return;

	iv_init();

	IV_FD_INIT(&wt.sock);
	wt.sock.fd = fd;
	wt.sock.cookie = &wt;
	wt.sock.handler_in = got_connection;
	iv_fd_register(&wt.sock);

	iv_main();

	iv_deinit();
}

int main()
{
	struct sockaddr_in addr;
	int i;

	SSL_load_error_strings();
	SSL_library_init();

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(-1);
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(12345);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(-1);
	}

	if (listen(fd, 1000) < 0) {
		perror("listen");
		exit(-1);
	}

	iv_init();

	for (i = 0; i < 16; i++) {
		char name[16];

		snprintf(name, sizeof(name), "worker %d", i);
		iv_thread_create(name, worker, NULL);
	}

	iv_main();

	iv_deinit();

	return 0;
}
