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

#include <stdint.h>

struct iv_avl_node {
	struct iv_avl_node	*left;
	struct iv_avl_node	*right;
	struct iv_avl_node	*parent;
	uint8_t			height;
};

struct iv_avl_tree {
	int			(*compare)(struct iv_avl_node *a,
					   struct iv_avl_node *b);

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

static inline int iv_avl_tree_empty(struct iv_avl_tree *tree)
{
	return tree->root == NULL;
}

#ifdef __cplusplus
}
#endif


#endif
