/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009 Lennert Buytenhek
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

#ifndef __IV_FD_COMPAT_H
#define __IV_FD_COMPAT_H

#include <sys/uio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern inline
int iv_accept(struct iv_fd *fd, struct sockaddr *addr, socklen_t *addrlen)
{
	return accept(fd->fd, addr, addrlen);
}

extern inline
int iv_connect(struct iv_fd *fd, struct sockaddr *addr, socklen_t addrlen)
{
	return connect(fd->fd, addr, addrlen);
}

extern inline
ssize_t iv_read(struct iv_fd *fd, void *buf, size_t count)
{
	return read(fd->fd, buf, count);
}

extern inline
ssize_t iv_readv(struct iv_fd *fd, const struct iovec *vector, int count)
{
	return readv(fd->fd, vector, count);
}

extern inline
int iv_recv(struct iv_fd *fd, void *buf, size_t len, int flags)
{
	return recv(fd->fd, buf, len, flags);
}

extern inline
int iv_recvfrom(struct iv_fd *fd, void *buf, size_t len, int flags,
		struct sockaddr *from, socklen_t *fromlen)
{
	return recvfrom(fd->fd, buf, len, flags, from, fromlen);
}

extern inline
int iv_recvmsg(struct iv_fd *fd, struct msghdr *msg, int flags)
{
	return recvmsg(fd->fd, msg, flags);
}

extern inline
int iv_send(struct iv_fd *fd, const void *msg, size_t len, int flags)
{
	return send(fd->fd, msg, len, flags);
}

#ifdef linux
#include <sys/sendfile.h>

extern inline
ssize_t iv_sendfile(struct iv_fd *fd, int in_fd, off_t *offset, size_t count)
{
	return sendfile(fd->fd, in_fd, offset, count);
}
#endif

extern inline
int iv_sendmsg(struct iv_fd *fd, const struct msghdr *msg, int flags)
{
	return sendmsg(fd->fd, msg, flags);
}

extern inline
int iv_sendto(struct iv_fd *fd, const void *msg, size_t len, int flags,
	      const struct sockaddr *to, socklen_t tolen)
{
	return sendto(fd->fd, msg, len, flags, to, tolen);
}

extern inline
ssize_t iv_write(struct iv_fd *fd, const void *buf, size_t count)
{
	return write(fd->fd, buf, count);
}

extern inline
ssize_t iv_writev(struct iv_fd *fd, const struct iovec *vector, int count)
{
	return writev(fd->fd, vector, count);
}

#ifdef __cplusplus
}
#endif


#endif
