/* $Id$ */

#ifndef KGT_RBNF_IO_H
#define KGT_RBNF_IO_H

struct ast_rule;

struct ast_rule *
rbnf_input(int (*f)(void *opaque), void *opaque);

void
rbnf_output(const struct ast_rule *grammar);

#endif
