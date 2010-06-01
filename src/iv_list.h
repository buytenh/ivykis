/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
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

/*
 * This file contains a doubly linked list implementation API-compatible
 * with the one found in the Linux kernel (in include/linux/list.h).
 */

#ifndef __IV_LIST_H
#define __IV_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

struct list_head {
	struct list_head	*next;
	struct list_head	*prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define INIT_LIST_HEAD(lh) do { \
	(lh)->next = (lh); \
	(lh)->prev = (lh); \
} while (0)

static inline void list_add(struct list_head *lh, struct list_head *head)
{
	lh->next = head->next;
	lh->prev = head;
	head->next->prev = lh;
	head->next = lh;
}

static inline void list_add_tail(struct list_head *lh, struct list_head *head)
{
	lh->next = head;
	lh->prev = head->prev;
	head->prev->next = lh;
	head->prev = lh;
}

static inline void list_del(struct list_head *lh)
{
	lh->prev->next = lh->next;
	lh->next->prev = lh->prev;
	lh->prev = NULL;
	lh->next = NULL;
}

static inline void list_del_init(struct list_head *lh)
{
	lh->prev->next = lh->next;
	lh->next->prev = lh->prev;
	INIT_LIST_HEAD(lh);
}

static inline int list_empty(struct list_head *head)
{
	return head->next == head;
}

#define list_entry(lh, type, member) \
	container_of(lh, type, member)

#define list_for_each(lh, head) \
	for (lh = (head)->next; lh != (head); lh = lh->next)

#define list_for_each_safe(lh, lh2, head) \
	for (lh = (head)->next, lh2 = lh->next; lh != (head); \
		lh = lh2, lh2 = lh->next)

#ifdef __cplusplus
}
#endif


#endif
