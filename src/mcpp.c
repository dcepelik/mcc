#include "context.h"
#include "cpp.h"
#include "error.h"
#include "symbol.h"
#include "parse.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * TODO Global task: make interfaces between components separate, (mainly) hide
 *      implementation of internal structures.
 */

int main(int argc, char *argv[])
{
	struct context ctx;
	char *filename;
	struct cpp *cpp;
	struct token *token;
	mcc_error_t err;
	struct strbuf buf;
	size_t i;

	context_init(&ctx);

	if (argc != 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	filename = argv[1];

	cpp = cpp_new(&ctx);

	err = cpp_open_file(cpp, filename);
	if (err != MCC_ERROR_OK) {
		fprintf(stderr, "Cannot open input file '%s': %s\n",
			filename,
			error_str(err)
		);

		cpp_delete(cpp);
		return EXIT_FAILURE;
	}

	strbuf_init(&buf, 256);
	for (i = 1; (token = cpp_next(cpp)); i++) {	
		if (token->type  == TOKEN_EOL) {
			strbuf_putc(&buf, '\n');
			i = 0;
			continue;
		}

		if (i != 1)
			strbuf_putc(&buf, ' ');
		token_print(token, &buf);

		if (token->type == TOKEN_EOF)
			break;
	}

	printf("%s\n", strbuf_get_string(&buf));
	strbuf_free(&buf);

	errlist_dump(&ctx.errlist, stderr);

	cpp_close_file(cpp);
	cpp_delete(cpp);

	context_free(&ctx);

	return EXIT_SUCCESS;
}
