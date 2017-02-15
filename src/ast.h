#ifndef AST_H
#define AST_H

#include "context.h"

enum ast_node_type
{
	AST_NODE_TYPE,
	AST_NODE_DECL,
};

enum ctype
{
	CTYPE_INT,
	CTYPE_POINTER,
	CTYPE_ARRAY,
};

struct ast_node
{
	enum ast_node_type node_type;

	union
	{
		struct
		{
			enum ctype ctype;
			struct ast_node *type;
			bool const_flag;
			size_t size;
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
