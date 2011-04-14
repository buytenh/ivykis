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

#ifndef __IV_H
#define __IV_H

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Library initialisation, main loop.
 */
void iv_init(void);
int iv_inited(void);
void iv_main(void);
void iv_quit(void);
void iv_deinit(void);


/*
 * Time handling.
 */
struct timespec *__iv_now_location();
void iv_validate_now(void);
void iv_invalidate_now(void);

#define now	(*__iv_now_location())


/*
 * File descriptor handling.
 */
struct iv_fd {
	union {
		struct {
			int	fd;
			void	*cookie;
			void	(*handler_in)(void *);
			void	(*handler_out)(void *);
			void	(*handler_err)(void *);
		};
		void	*pad[16];
	};
};

void IV_FD_INIT(struct iv_fd *);
void iv_fd_register(struct iv_fd *);
void iv_fd_unregister(struct iv_fd *);
int iv_fd_registered(struct iv_fd *);
void iv_fd_set_handler_in(struct iv_fd *, void (*)(void *));
void iv_fd_set_handler_out(struct iv_fd *, void (*)(void *));
void iv_fd_set_handler_err(struct iv_fd *, void (*)(void *));


/*
 * Task handling.
 */
struct iv_task {
	union {
		struct {
			void	*cookie;
			void	(*handler)(void *);
		};
		void	*pad[16];
	};
};

void IV_TASK_INIT(struct iv_task *);
void iv_task_register(struct iv_task *);
void iv_task_unregister(struct iv_task *);
int iv_task_registered(struct iv_task *);


/*
 * Timer handling.
 */
struct iv_timer {
	union {
		struct {
			struct timespec	expires;
			void		*cookie;
			void		(*handler)(void *);
		};
		void	*pad[16];
	};
};

void IV_TIMER_INIT(struct iv_timer *);
void iv_timer_register(struct iv_timer *);
void iv_timer_unregister(struct iv_timer *);
int iv_timer_registered(struct iv_timer *);


#ifdef __cplusplus
}
#endif

#include <iv_compat.h>


#endif
