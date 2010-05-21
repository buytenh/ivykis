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
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
 * MA 02110-1301, USA.
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
void iv_main(void);
void iv_quit(void);


/*
 * Time handling.
 */
extern struct timespec now;
void iv_validate_now(void);
void iv_invalidate_now(void);


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

void INIT_IV_FD(struct iv_fd *);
void iv_register_fd(struct iv_fd *);
void iv_unregister_fd(struct iv_fd *);
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

void INIT_IV_TASK(struct iv_task *);
void iv_register_task(struct iv_task *);
void iv_unregister_task(struct iv_task *);


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

void INIT_IV_TIMER(struct iv_timer *);
void iv_register_timer(struct iv_timer *);
void iv_unregister_timer(struct iv_timer *);
int iv_timer_registered(struct iv_timer *);

#ifdef __cplusplus
}
#endif


#endif
