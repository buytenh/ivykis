/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __IV_H
#define __IV_H

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <iv_list.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_fd;
struct iv_task;
struct iv_timer;

extern struct timespec now;

void iv_init(void);
void iv_register_fd(struct iv_fd *);
void iv_unregister_fd(struct iv_fd *);
void iv_fd_set_handler_in(struct iv_fd *, void (*)(void *));
void iv_fd_set_handler_out(struct iv_fd *, void (*)(void *));
void iv_fd_set_handler_err(struct iv_fd *, void (*)(void *));
void iv_register_task(struct iv_task *);
void iv_unregister_task(struct iv_task *);
void iv_register_timer(struct iv_timer *);
void iv_unregister_timer(struct iv_timer *);
void iv_validate_now(void);
void iv_invalidate_now(void);
void iv_main(void);
void iv_quit(void);

int iv_accept(struct iv_fd *, struct sockaddr *, socklen_t *);
int iv_connect(struct iv_fd *, struct sockaddr *, socklen_t);
ssize_t iv_read(struct iv_fd *, void *, size_t);
ssize_t iv_readv(struct iv_fd *, const struct iovec *, int);
int iv_recv(struct iv_fd *, void *, size_t, int);
int iv_recvfrom(struct iv_fd *, void *, size_t, int, struct sockaddr *,
		socklen_t *);
int iv_recvmsg(struct iv_fd *, struct msghdr *, int);
ssize_t iv_sendfile(struct iv_fd *, int, off_t *, size_t);
int iv_send(struct iv_fd *, const void *, size_t, int);
int iv_sendmsg(struct iv_fd *, const struct msghdr *, int);
int iv_sendto(struct iv_fd *, const void *, size_t, int,
	      const struct sockaddr *, socklen_t);
ssize_t iv_write(struct iv_fd *, const void *, size_t);
ssize_t iv_writev(struct iv_fd *, const struct iovec *, int);


/* internals *****************************************************************/
struct iv_fd
{
	int			fd;
	void			*cookie;
	void			(*handler_in)(void *);
	void			(*handler_out)(void *);
	void			(*handler_err)(void *);

	struct list_head	list_all;
	struct list_head	list_active;
	unsigned int		quotum:8,
				flags:8,
				epoch:16;
};

#define INIT_IV_FD(ptr) do {				\
	(ptr)->fd = -1;					\
	(ptr)->handler_in = NULL;			\
	(ptr)->handler_out = NULL;			\
	(ptr)->handler_err = NULL;			\
	INIT_LIST_HEAD(&((ptr)->list_all));		\
} while (0);


struct iv_task
{
	void			*cookie;
	void			(*handler)(void *);

	struct list_head	list;
};

#define INIT_IV_TASK(ptr) do {				\
	INIT_LIST_HEAD(&((ptr)->list));			\
} while (0);


struct iv_timer
{
	struct timespec		expires;
	void			*cookie;
	void			(*handler)(void *);

	int			index;
};

#define INIT_IV_TIMER(ptr) do {				\
	(ptr)->index = -1;				\
} while (0);

#ifdef __cplusplus
}
#endif


#endif
