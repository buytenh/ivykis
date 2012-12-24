/*
 * ivykis, an event handling library
 * Copyright (C) 2012 Lennert Buytenhek
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

#ifndef _WIN32
static inline int event_rx_on(struct iv_state *st)
{
	if (method->event_rx_on != NULL)
		return method->event_rx_on(st);

	return -1;
}

static inline void event_rx_off(struct iv_state *st)
{
	method->event_rx_off(st);
}

static inline void event_send(struct iv_state *dest)
{
	method->event_send(dest);
}
#else
static inline int event_rx_on(struct iv_state *st)
{
	return -1;
}

static inline void event_rx_off(struct iv_state *st)
{
}

static inline void event_send(struct iv_state *dest)
{
}
#endif
