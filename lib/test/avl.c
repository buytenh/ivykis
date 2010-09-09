/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003 Lennert Buytenhek
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
#include <iv.h>
#include <iv_avl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static void
tree_node_print(int depth, struct iv_avl_node *an,
		void (*print)(struct iv_avl_node *an))
{
	int i;

	for (i = 0; i < depth; i++)
		printf("  ");
	printf("%p (parent=%p): ", an, an->parent);
	print(an);

	if (an->left != NULL)
		tree_node_print(depth + 1, an->left, print);
	if (an->right != NULL)
		tree_node_print(depth + 1, an->right, print);
}

static void
tree_print(char *msg, struct iv_avl_tree *this,
	   void (*print)(struct iv_avl_node *an))
{
	printf("%s:\n", msg);
	if (this->root != NULL)
		tree_node_print(0, this->root, print);
	printf("\n");
}


struct foo {
	struct iv_avl_node	an;
	int			num;
};

static void printit(struct iv_avl_node *an)
{
	struct foo *f = container_of(an, struct foo, an);

	printf("%d (height %d)\n", f->num, f->an.height);
}

static int
tree_node_verify(struct iv_avl_tree *this, struct iv_avl_node *an,
		 struct iv_avl_node *parent,
		 struct iv_avl_node *min, struct iv_avl_node *max, int *cnt)
{
	int hl;
	int hr;
	int my;

	if (an->parent != parent) {
		printf("parent mismatch: %p, %p versus %p\n",
		       an, an->parent, parent);
		abort();
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
			printf("violated %p < %p < %p\n", min, an, max);
	}

	hl = 0;
	if (an->left != NULL)
		hl = tree_node_verify(this, an->left, an, min, an, cnt);

	hr = 0;
	if (an->right != NULL)
		hr = tree_node_verify(this, an->right, an, an, max, cnt);

	if (abs(hl - hr) > 1)
		printf("balance mismatch!\n");

	my = 1 + ((hl > hr) ? hl : hr);
	if (an->height != my) {
		printf("height mismatch: %d vs %d/%d\n", an->height, hl, hr);
		abort();
	}

	return my;
}

static void tree_check(struct iv_avl_tree *this, int expected_count)
{
	int count;

	count = 0;
	if (this->root != NULL)
		tree_node_verify(this, this->root, NULL, NULL, NULL, &count);

	if (expected_count >= 0 && expected_count != count) {
		printf("count mismatch: %d versus %d\n", count, expected_count);
		abort();
	}
}


static int docomp(struct iv_avl_node *_a, struct iv_avl_node *_b)
{
	struct foo *a = container_of(_a, struct foo, an);
	struct foo *b = container_of(_b, struct foo, an);

	if (a->num < b->num)
		return -1;
	if (a->num > b->num)
		return 1;
	return 0;
}



#define NUM	16384

static struct iv_avl_tree x;
static struct foo *f[NUM];

int main()
{
	struct iv_avl_node *an;
	int i;

	srandom(time(NULL) ^ getpid());

	INIT_IV_AVL_TREE(&x, docomp);

	for (i = 0; i < NUM; i++) {
		int ret;

		if ((i & 1023) == 0)
			printf("inserting #%d\n", i);

		f[i] = malloc(sizeof(struct foo));

		do {
			f[i]->num = random();
			ret = iv_avl_tree_insert(&x, &f[i]->an);
		} while (ret == -EEXIST);

		if (ret) {
			fprintf(stderr, "error %d\n", ret);
			exit(-1);
		}

		tree_check(&x, i + 1);
	}

	tree_check(&x, NUM);

	iv_avl_tree_for_each (an, &x)
		printit(an);

	for (i = 0; i < NUM; i++) {
		if ((i & 1023) == 0)
			printf("deleting #%d\n", i);

		an = &f[i]->an;

		iv_avl_tree_delete(&x, an);

		tree_check(&x, NUM - i - 1);
	}

	tree_print("deletions done", &x, printit);

	return 0;
}
