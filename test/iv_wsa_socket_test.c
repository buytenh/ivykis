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
#include <iv_wsa_socket.h>

static char *events[] = {
	"FD_READ_BIT",
	"FD_WRITE_BIT",
	"FD_OOB_BIT",
	"FD_ACCEPT_BIT",
	"FD_CONNECT_BIT",
	"FD_CLOSE_BIT",
	"FD_QOS_BIT",
	"FD_GROUP_QOS_BIT",
	"FD_ROUTING_INTERFACE_CHANGE_BIT",
	"FD_ADDRESS_LIST_CHANGE_BIT",
};


static struct iv_wsa_socket ws;
static int count;

static void handler(void *cookie, int event, int error)
{
	printf("got event: %s, error %d\n", events[event], error);

	send(ws.socket, "hello\r\n", 7, 0);
	Sleep(1000);

	if (++count == 5) {
		iv_wsa_socket_unregister(&ws);
		closesocket(ws.socket);
	}
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		     LPSTR lpCmdLine, int nCmdShow)
{
	WSADATA wsaData;
	SOCKET s;
	struct sockaddr_in addr;
	int ret;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		printf("WSAStartup() failed\n");
		return 1;
	}

	iv_init();

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET) {
		printf("socket() failed\n");
		return 1;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0x7f000001);
	addr.sin_port = htons(12345);

	ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
	if (ret == SOCKET_ERROR) {
		printf("connect() failed\n");
		return 1;
	}

	ws.socket = s;
	ws.cookie = NULL;
	ws.handler[FD_READ_BIT] = handler;
	ws.handler[FD_WRITE_BIT] = handler;
	ws.handler[FD_ACCEPT_BIT] = handler;
	ws.handler[FD_CONNECT_BIT] = handler;
	ws.handler[FD_CLOSE_BIT] = handler;
	iv_wsa_socket_register(&ws);

	iv_main();

	iv_deinit();

	return 0;
}
