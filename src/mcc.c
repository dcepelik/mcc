#include "inbuf.h"
#include "lexer.h"
#include "lexer.h"
#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE	4096

int main(int argc, char *argv[])
{
	char *filename;
	struct inbuf buffer;
	struct lexer lexer;
	struct token_data token_data;
	struct symtab symtab;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	filename = argv[1];

	if (inbuf_open(&buffer, BUFFER_SIZE, filename) != MCC_ERROR_OK) {
		fprintf(stderr, "Cannot open input buffer '%s' for reading\n",
			filename);
		exit(EXIT_FAILURE);
	}

	symtab_init(&symtab);

	lexer_init(&lexer);
	lexer_set_buffer(&lexer, &buffer);
	lexer_set_symtab(&lexer, &symtab);

	while (lexer_next(&lexer, &token_data) == MCC_ERROR_OK) {
		if (token_data.token == TOKEN_EOF)
			break;

		lexer_dump_token(&token_data);
	}

	printf("\n");

	inbuf_close(&buffer);
	lexer_free(&lexer);
	symtab_free(&symtab);

	return EXIT_SUCCESS;
}
