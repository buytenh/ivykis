/*
 * ivykis, an event handling library
 * Copyright (C) 2012 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version
 * 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 2.1 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License version 2.1 along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <iv.h>

static int foo;

static void handle_active(void *_h)
{
	struct iv_handle *h = _h;

	printf("got handle event\n");
	Sleep(100);
	printf("returning\n");

	if (++foo > 10)
		iv_handle_unregister(h);
}

static DWORD CALLBACK thread(void *_h)
{
	struct iv_handle *h = _h;

	while (1) {
		SetEvent(h->handle);
		Sleep(1000);
	}

	return 0;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		     LPSTR lpCmdLine, int nCmdShow)
{
	struct iv_handle h;

	iv_init();

	IV_HANDLE_INIT(&h);
	h.handle = CreateEvent(NULL, FALSE, FALSE, NULL);
	h.cookie = &h;
	h.handler = handle_active;
	iv_handle_register(&h);

	CreateThread(NULL, 0, thread, &h, 0, NULL);

	iv_main();

	return 0;
}
