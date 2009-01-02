/*
 *  include/linux/eventpoll.h
 *
 *  Copyright (C) 2001, Davide Libenzi <davidel@xmailserver.org>
 *
 *  Efficent event polling implementation
 */


#ifndef _LINUX_EVENTPOLL_H
#define _LINUX_EVENTPOLL_H




#define EVENTPOLL_MINOR	124
#define POLLFD_X_PAGE	(PAGE_SIZE / sizeof(struct pollfd))
#define MAX_FDS_IN_EVENTPOLL	(1024 * 128)
#define MAX_EVENTPOLL_PAGES	(MAX_FDS_IN_EVENTPOLL / POLLFD_X_PAGE)
#define EVENT_PAGE_INDEX(n)	((n) / POLLFD_X_PAGE)
#define EVENT_PAGE_REM(n)	((n) % POLLFD_X_PAGE)
#define EVENT_PAGE_OFFSET(n)	(((n) % POLLFD_X_PAGE) * sizeof(struct pollfd))
#define EP_FDS_PAGES(n)	(((n) + POLLFD_X_PAGE - 1) / POLLFD_X_PAGE)
#define EP_MAP_SIZE(n)	(EP_FDS_PAGES(n) * PAGE_SIZE * 2)





struct evpoll {
	int ep_timeout;
	unsigned long ep_resoff;
};

#define EP_ALLOC	_IOR('P', 1, int)
#define EP_POLL		_IOWR('P', 2, struct evpoll)
#define EP_FREE		_IO('P', 3)
#define EP_ISPOLLED	_IOWR('P', 4, struct pollfd)



#endif

