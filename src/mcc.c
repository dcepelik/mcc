#include "error.h"
#include "cpp.h"
#include "symbol.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE	4096

/*
 * TODO Global task: make interfaces between components separate, (mainly) hide
 *      implementation of internal structures.
 */

int main(int argc, char *argv[])
{
	char *filename;
	struct cppfile *cppfile;
	struct token *token;
	struct symtab symtab;
	mcc_error_t err;
	struct strbuf buf;
	size_t i;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	filename = argv[1];

	symtab_init(&symtab);

	cppfile = cppfile_new();
	if (!cppfile)
		return EXIT_FAILURE;

	err = cppfile_open(cppfile, filename);
	if (err != MCC_ERROR_OK) {
		fprintf(stderr, "Cannot open input file '%s': %s\n",
			filename,
			error_str(err)
		);

		cppfile_delete(cppfile);
		return EXIT_FAILURE;
	}

	cppfile_set_symtab(cppfile, &symtab);

	strbuf_init(&buf, 256);
	for (i = 1; (token = cpp_next(cppfile)); i++) {	
		if (token->type  == TOKEN_EOL) {
			strbuf_putc(&buf, '\n');
			i = 0;
			continue;
		}

		if (i > 1)
			strbuf_putc(&buf, ' ');
		token_print(token, &buf);

		if (token->type == TOKEN_EOF)
			break;
	}

	if (!token) {
		fprintf(stderr, "Out of memory.\n");
		return EXIT_FAILURE;
	}

	printf("%s\n", strbuf_get_string(&buf));
	strbuf_free(&buf);

	cppfile_close(cppfile);
	cppfile_delete(cppfile);
	symtab_free(&symtab);

	return EXIT_SUCCESS;
}
