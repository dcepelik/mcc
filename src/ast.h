#ifndef AST_H
#define AST_H

#include "context.h"

enum ast_node_type
{
	AST_NODE_TYPE,
};

enum ctype
{
	CTYPE_INT,
	CTYPE_POINTER,
	CTYPE_ARRAY,
};

struct ast_node
{
	enum ast_node_type type;

	union
	{
		struct
		{
			enum ctype ctype;
			struct ast_node *ptr_to;
			struct ast_node *array_of;
			bool const_flag;
		};
	};
};

void ast_node_init(struct ast_node *node);
struct ast_node *ast_node_new(struct context *ctx);

struct ast
{
	struct ast_node root;
};

#endif
