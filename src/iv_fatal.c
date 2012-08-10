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
#include <stdarg.h>

#ifndef _WIN32
#include <syslog.h>

static void iv_fatal_default_handler(const char *msg)
{
	syslog(LOG_CRIT, "%s", msg);
}
#else
static void iv_fatal_default_handler(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
}
#endif

static void (*fatal_msg_handler)(const char *msg);

void iv_fatal(const char *fmt, ...)
{
	va_list ap;
	char msg[1024];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	msg[sizeof(msg) - 1] = 0;

	if (fatal_msg_handler != NULL)
		fatal_msg_handler(msg);
	else
		iv_fatal_default_handler(msg);

	abort();
}

void iv_set_fatal_msg_handler(void (*handler)(const char *msg))
{
	fatal_msg_handler = handler;
}
