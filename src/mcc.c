#include "context.h"
#include "cpp.h"
#include "error.h"
#include "symbol.h"
#include "parse.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	char *filename;
	struct parser parser;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	filename = argv[1];

	parser_init(&parser);

	struct ast tree;
	parser_build_ast(&parser, &tree, filename);

	parser_free(&parser);

	return EXIT_SUCCESS;
}
