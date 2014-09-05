/* $Id$ */

/*
 * Railroad diagram beautification
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "../xalloc.h"

#include "rrd.h"
#include "blist.h"
#include "pretty.h"
#include "node.h"

static int
leaves_eq(struct node *a, struct node *b)
{
	assert(a->type == NODE_LEAF);
	assert(b->type == NODE_LEAF);

	if (a->u.leaf.type != b->u.leaf.type) {
		return 0;
	}

	if (0 != strcmp(a->u.leaf.text, b->u.leaf.text)) {
		return 0;
	}

	return 1;
}

static int
process_loop_leaf(struct node *loop, struct bnode *bp)
{
	struct node *a, *b;

	if (bp == NULL || bp->v->type != NODE_LEAF) {
		return 0;
	}

	a = loop->u.loop.backward;
	b = bp->v;

	if (leaves_eq(a, b)) {
		struct node *tmp;

		tmp = loop->u.loop.forward;
		loop->u.loop.forward  = loop->u.loop.backward;
		loop->u.loop.backward = tmp;

		return 1;
	}

	return 0;
}

static void
loop_switch_sides(int suflen, struct node *loop, struct bnode **rl)
{
	struct node *v, **n;
	int i;

	if (suflen > 1) {
		struct node *seq;

		seq = node_create_list(LIST_SEQUENCE, NULL);

		n = &seq->u.list.list;
		node_free(loop->u.loop.forward);
		loop->u.loop.forward = seq;

		for (i = 0; i < suflen; i++) {
			(void) b_pop(rl, &v);
			v->next = *n;
			*n = v;
		}
	} else {
		node_free(loop->u.loop.forward);
		(void) b_pop(rl, &v);
		v->next = 0;
		loop->u.loop.forward = v;
	}

	if (b_pop(rl, &v)) {
		v->next = 0;
	} else {
		struct node *skip;

		skip = node_create_skip();
		node_free(loop->u.loop.backward);
		loop->u.loop.backward = skip;
	}

	if (loop->u.loop.backward->type == NODE_LIST) {
		node_collapse(&loop->u.loop.backward);
	}
}

static int
process_loop_list(struct node *loop, struct bnode *bp)
{
	struct node *list;
	struct node *p;
	struct bnode *rl = 0, *rp;
	int suffix = 0;

	list = loop->u.loop.backward;

	if (list->u.list.type == LIST_CHOICE) {
		return 0;
	}

	for (p = list->u.list.list; p != NULL; p = p->next) {
		b_push(&rl, p);
	}

	/* linkedlistcmp() */
	for (rp = rl; rp != NULL && bp != NULL; rp = rp->next, bp = bp->next) {
		struct node *a, *b;

		if (rp->v->type != NODE_LEAF || bp->v->type != NODE_LEAF) {
			break;
		}

		a = rp->v;
		b = bp->v;

		if (!leaves_eq(a, b)) {
			break;
		}

		suffix++;
	}

	if (suffix > 0) {
		loop_switch_sides(suffix, loop, &rl);
	}

	b_clear(&rl);

	return suffix;
}

static int
process_loop(struct node *loop, struct bnode *bp)
{
	if (loop->u.loop.backward->type == NODE_SKIP
	 || loop->u.loop.forward->type != NODE_SKIP) {
		return 0;
	}

	if (loop->u.loop.backward->type == NODE_LIST) {
		return process_loop_list(loop, bp);
	}

	if (loop->u.loop.backward->type == NODE_LEAF) {
		return process_loop_leaf(loop, bp);
	}

	return 0;
}

static struct node_walker pretty_collapse_suffixes;

static int
collapse_list(struct node *n, struct node **np, int depth, void *arg)
{
	struct node *p;
	struct bnode *rl = 0;

	for (p = n->u.list.list; p != NULL; p = p->next) {
		int i, suffix_len;

		if (p->type != NODE_LOOP) {
			b_push(&rl, p);
			continue;
		}

		suffix_len = process_loop(p, rl);

		/* delete suffix_len things from the list */
		for (i = 0; i < suffix_len; i++) {
			struct node *q;

			if (!b_pop(&rl, &q)) {
				return 0;
			}

			node_free(q);

			if (rl) {
				rl->v->next = p;
			} else {
				n->u.list.list = p;
			}
		}
	}

	b_clear(&rl);

	if (!node_walk_list(&n->u.list.list, &pretty_collapse_suffixes, depth + 1, arg)) {
		return 0;
	}

	node_collapse(np);

	return 1;
}

static struct node_walker pretty_collapse_suffixes = {
	NULL,
	NULL, NULL,
	NULL, collapse_list,
	NULL
};

void
rrd_pretty_suffixes(struct node **rrd)
{
	/*
	 * look for bits of diagram like this:
	 * -->--XYZ-->-------------------->--
	 *           |                    |
	 *           ^--<--ZYX--<--SEP--<--
	 * and replace them with the prettier equivalent:
	 * -->---XYZ--->--
	 *   |         |
	 *   ^--<--SEP--
	 */
	node_walk(rrd, &pretty_collapse_suffixes, 0, 0);
}
