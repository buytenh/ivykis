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

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iv_list_head {
	struct iv_list_head	*next;
	struct iv_list_head	*prev;
};

#define IV_LIST_HEAD_INIT(name) { &(name), &(name) }

#define INIT_IV_LIST_HEAD(ilh) do { \
	(ilh)->next = (ilh); \
	(ilh)->prev = (ilh); \
} while (0)

static inline void
iv_list_add(struct iv_list_head *ilh, struct iv_list_head *head)
{
	ilh->next = head->next;
	ilh->prev = head;
	head->next->prev = ilh;
	head->next = ilh;
}

static inline void
iv_list_add_tail(struct iv_list_head *ilh, struct iv_list_head *head)
{
	ilh->next = head;
	ilh->prev = head->prev;
	head->prev->next = ilh;
	head->prev = ilh;
}

static inline void iv_list_del(struct iv_list_head *ilh)
{
	ilh->prev->next = ilh->next;
	ilh->next->prev = ilh->prev;
	ilh->prev = NULL;
	ilh->next = NULL;
}

static inline void iv_list_del_init(struct iv_list_head *ilh)
{
	ilh->prev->next = ilh->next;
	ilh->next->prev = ilh->prev;
	INIT_IV_LIST_HEAD(ilh);
}

static inline int iv_list_empty(const struct iv_list_head *head)
{
	return head->next == head;
}

static inline void __iv_list_splice(struct iv_list_head *ilh,
				    struct iv_list_head *prev,
				    struct iv_list_head *next)
{
	struct iv_list_head *first = ilh->next;
	struct iv_list_head *last = ilh->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

static inline void
iv_list_splice(struct iv_list_head *ilh, struct iv_list_head *head)
{
	if (!iv_list_empty(ilh))
		__iv_list_splice(ilh, head, head->next);
}

static inline void
iv_list_splice_init(struct iv_list_head *ilh, struct iv_list_head *head)
{
	if (!iv_list_empty(ilh)) {
		__iv_list_splice(ilh, head, head->next);
		INIT_IV_LIST_HEAD(ilh);
	}
}

static inline void
iv_list_splice_tail(struct iv_list_head *ilh, struct iv_list_head *head)
{
	if (!iv_list_empty(ilh))
		__iv_list_splice(ilh, head->prev, head);
}

static inline void
iv_list_splice_tail_init(struct iv_list_head *ilh, struct iv_list_head *head)
{
	if (!iv_list_empty(ilh)) {
		__iv_list_splice(ilh, head->prev, head);
		INIT_IV_LIST_HEAD(ilh);
	}
}

#define iv_container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__ptr = (ptr);	\
	(type *)((char *)__ptr - (intptr_t)(&((type *)0)->member)); })

#define iv_list_entry(ilh, type, member) \
	iv_container_of(ilh, type, member)

#define iv_list_for_each(ilh, head) \
	for (ilh = (head)->next; ilh != (head); ilh = ilh->next)

#define iv_list_for_each_safe(ilh, ilh2, head) \
	for (ilh = (head)->next, ilh2 = ilh->next; ilh != (head); \
		ilh = ilh2, ilh2 = ilh->next)

#ifdef __cplusplus
}
#endif


#endif
