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

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <iv_event_raw.h>
#include <unistd.h>

int iv_event_raw_register(struct iv_event_raw *this)
{
	HANDLE h;

	h = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (h == NULL)
		return -1;

	IV_HANDLE_INIT(&this->h);
	this->h.handle = h;
	this->h.cookie = this->cookie;
	this->h.handler = this->handler;
	iv_handle_register(&this->h);

	return 0;
}

void iv_event_raw_unregister(struct iv_event_raw *this)
{
	iv_handle_unregister(&this->h);
	CloseHandle(this->h.handle);
}

void iv_event_raw_post(const struct iv_event_raw *this)
{
	SetEvent(this->h.handle);
}
