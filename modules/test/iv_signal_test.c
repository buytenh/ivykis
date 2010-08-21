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

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <iv_signal.h>
#include <signal.h>

static struct iv_signal is_sigint;
static struct iv_signal is_sigquit;

static void got_sigint(void *_x)
{
	printf("got sigint\n");

	iv_signal_unregister(&is_sigint);
}

static void got_sigquit(void *_x)
{
	printf("got sigquit\n");

	iv_signal_unregister(&is_sigquit);
}

int main()
{
	iv_init();

	is_sigint.signum = SIGINT;
	is_sigint.handler = got_sigint;
	iv_signal_register(&is_sigint);

	is_sigquit.signum = SIGQUIT;
	is_sigquit.handler = got_sigquit;
	iv_signal_register(&is_sigquit);

	iv_main();

	return 0;
}
