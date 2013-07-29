/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2012 Lennert Buytenhek
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
#include <stdlib.h>
#include <iv_avl.h>
#include <iv_list.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

struct node {
	struct iv_avl_node	an;
	int			num;
};

static struct iv_avl_tree x;

static void tree_node_print(int depth, const struct iv_avl_node *an)
{
	struct node *f = iv_container_of(an, struct node, an);
	int i;

	for (i = 0; i < depth; i++)
		fprintf(stderr, "  ");
	fprintf(stderr, "%p (parent=%p): %d (height %d)\n",
		an, an->parent, f->num, f->an.height);

	if (an->left != NULL)
		tree_node_print(depth + 1, an->left);
	if (an->right != NULL)
		tree_node_print(depth + 1, an->right);
}

static void tree_print(const char *msg)
{
	fprintf(stderr, "%s:\n", msg);
	if (x.root != NULL)
		tree_node_print(0, x.root);
	fprintf(stderr, "\n");
}

static void __attribute__((noreturn)) fatal(const char *fmt, ...)
{
	va_list ap;
	char msg[1024];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	msg[sizeof(msg) - 1] = 0;

	tree_print(msg);

	exit(1);
}

static int
tree_node_verify(const struct iv_avl_tree *this, const struct iv_avl_node *an,
		 const struct iv_avl_node *parent,
		 const struct iv_avl_node *min, const struct iv_avl_node *max,
		 int *cnt)
{
	int hl;
	int hr;
	int my;

	if (an->parent != parent) {
		fatal("parent mismatch: %p, %p versus %p",
		      an, an->parent, parent);
	}

	(*cnt)++;

	if (min != NULL || max != NULL) {
		int err;

		err = 0;
		if (min != NULL && this->compare(min, an) >= 0)
			err++;
		if (max != NULL && this->compare(an, max) >= 0)
			err++;

		if (err)
			fatal("violated %p < %p < %p", min, an, max);
	}

	hl = 0;
	if (an->left != NULL)
		hl = tree_node_verify(this, an->left, an, min, an, cnt);

	hr = 0;
	if (an->right != NULL)
		hr = tree_node_verify(this, an->right, an, an, max, cnt);

	if (abs(hl - hr) > 1)
		fatal("balance mismatch: %d vs %d", hl, hr);

	my = 1 + ((hl > hr) ? hl : hr);
	if (an->height != my)
		fatal("height mismatch: %d vs %d/%d", an->height, hl, hr);

	return my;
}

static void tree_check(const struct iv_avl_tree *this, int expected_count)
{
	int count;

	count = 0;
	if (this->root != NULL)
		tree_node_verify(this, this->root, NULL, NULL, NULL, &count);

	if (expected_count >= 0 && expected_count != count)
		fatal("count mismatch: %d versus %d", count, expected_count);
}


#define NUM	1024

static struct node *f[NUM];

static int docomp(const struct iv_avl_node *_a, const struct iv_avl_node *_b)
{
	const struct node *a = iv_container_of(_a, struct node, an);
	const struct node *b = iv_container_of(_b, struct node, an);

	if (a->num < b->num)
		return -1;
	if (a->num > b->num)
		return 1;

	return 0;
}

static int mkrand(void)
{
	int r;

	r = rand();
#if RAND_MAX == 0x7fff
	r |= rand() << 15;
#endif

	return r;
}

int main()
{
	int i;

	alarm(180);

	srand(time(NULL) ^ getpid());

	INIT_IV_AVL_TREE(&x, docomp);

	tree_check(&x, 0);

	for (i = 0; i < NUM; i++) {
		int ret;

		f[i] = malloc(sizeof(struct node));

		do {
			f[i]->num = mkrand();
			ret = iv_avl_tree_insert(&x, &f[i]->an);
		} while (ret < 0);

		tree_check(&x, i + 1);
	}

	for (i = 0; i < NUM; i++) {
		iv_avl_tree_delete(&x, &f[i]->an);

		tree_check(&x, NUM - i - 1);

		free(f[i]);
	}

	return 0;
}
