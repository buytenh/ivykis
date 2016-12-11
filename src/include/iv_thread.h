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

#ifndef __IV_THREAD_H
#define __IV_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

int iv_thread_create(const char *name, void (*start_routine)(void *),
		     void *arg);
void iv_thread_set_debug_state(int state);
unsigned long iv_thread_get_id(void);
void iv_thread_set_name(const char *name);
void iv_thread_list_children(void);

#ifdef __cplusplus
}
#endif


#endif
