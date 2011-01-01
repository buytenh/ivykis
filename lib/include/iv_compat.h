/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009 Lennert Buytenhek
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

#ifndef __IV_COMPAT_H
#define __IV_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#define __deprecated __attribute__((deprecated))

extern inline void __deprecated INIT_IV_FD(struct iv_fd *fd)
{
	IV_FD_INIT(fd);
}

extern inline void __deprecated iv_register_fd(struct iv_fd *fd)
{
	iv_fd_register(fd);
}

extern inline void __deprecated iv_unregister_fd(struct iv_fd *fd)
{
	iv_fd_unregister(fd);
}

extern inline void __deprecated INIT_IV_TASK(struct iv_task *t)
{
	IV_TASK_INIT(t);
}

extern inline void __deprecated iv_register_task(struct iv_task *t)
{
	iv_task_register(t);
}

extern inline void __deprecated iv_unregister_task(struct iv_task *t)
{
	iv_task_unregister(t);
}

extern inline void __deprecated INIT_IV_TIMER(struct iv_timer *t)
{
	IV_TIMER_INIT(t);
}

extern inline void __deprecated iv_register_timer(struct iv_timer *t)
{
	iv_timer_register(t);
}

extern inline void __deprecated iv_unregister_timer(struct iv_timer *t)
{
	iv_timer_unregister(t);
}

#ifdef __cplusplus
}
#endif


#endif
