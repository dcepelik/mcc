#ifndef AST_H
#define AST_H

#include "context.h"
#include "keyword.h"

enum ast_node_type
{
	AST_ARRAY,
	AST_DECL,
	AST_DECL_PART,
	AST_POINTER,
	AST_STRUCT_SPEC,
	AST_UNION_SPEC,
};

struct ast_node
{
	enum ast_node_type type;

	struct {
		uint16_t tspec;		/* see enum tspec */
		uint8_t tflags;		/* see enum tflags */
		uint8_t tquals;		/* see enum tquals */
		uint8_t storcls;	/* see enum storcls */
	};
	char *ident;
	size_t size;
	struct ast_node *spec;	/* struct or union specifiaction */
	struct ast_node *decl;	/* declarator */
	struct ast_node **parts;
	struct ast_node **decls;
};

void ast_node_init(struct ast_node *node);
struct ast_node *ast_node_new(struct context *ctx, enum ast_node_type type);

struct ast
{
	struct ast_node root;
};

#endif
