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

#ifndef __IV_AVL_H
#define __IV_AVL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

struct iv_avl_node {
	struct iv_avl_node	*left;
	struct iv_avl_node	*right;
	struct iv_avl_node	*parent;
	uint8_t			height;
};

struct iv_avl_tree {
	int			(*compare)(const struct iv_avl_node *a,
					   const struct iv_avl_node *b);

	struct iv_avl_node	*root;
};

#define IV_AVL_TREE_INIT(comp)				\
	{ .compare = comp, .root = NULL }

#define INIT_IV_AVL_TREE(tree, comp)			\
	do {						\
		(tree)->compare = (comp);		\
		(tree)->root = NULL;			\
	} while (0)

int iv_avl_tree_insert(struct iv_avl_tree *tree, struct iv_avl_node *an);
void iv_avl_tree_delete(struct iv_avl_tree *tree, struct iv_avl_node *an);
struct iv_avl_node *iv_avl_tree_next(struct iv_avl_node *an);
struct iv_avl_node *iv_avl_tree_prev(struct iv_avl_node *an);

static inline int iv_avl_tree_empty(const struct iv_avl_tree *tree)
{
	return tree->root == NULL;
}

static inline struct iv_avl_node *
iv_avl_tree_min(const struct iv_avl_tree *tree)
{
	if (tree->root != NULL) {
		struct iv_avl_node *an;

		an = tree->root;
		while (an->left != NULL)
			an = an->left;

		return an;
	}

	return NULL;
}

static inline struct iv_avl_node *
iv_avl_tree_max(const struct iv_avl_tree *tree)
{
	if (tree->root != NULL) {
		struct iv_avl_node *an;

		an = tree->root;
		while (an->right != NULL)
			an = an->right;

		return an;
	}

	return NULL;
}

#define iv_avl_tree_for_each(an, tree) \
	for (an = iv_avl_tree_min(tree); an != NULL; an = iv_avl_tree_next(an))

static inline struct iv_avl_node *iv_avl_tree_next_safe(struct iv_avl_node *an)
{
	return an != NULL ? iv_avl_tree_next(an) : NULL;
}

#define iv_avl_tree_for_each_safe(an, an2, tree) \
	for (an = iv_avl_tree_min(tree), an2 = iv_avl_tree_next_safe(an); \
	     an != NULL; an = an2, an2 = iv_avl_tree_next_safe(an))

#ifdef __cplusplus
}
#endif


#endif
