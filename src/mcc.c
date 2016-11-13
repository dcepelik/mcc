#include "error.h"
#include "pp.h"
#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE	4096

int main(int argc, char *argv[])
{
	char *filename;
	struct token_data token_data;
	struct symtab symtab;
	struct pp pp;
	mcc_error_t err;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	filename = argv[1];

	symtab_init(&symtab);

	pp_init(&pp);
	err = pp_open(&pp, filename);
	if (err != MCC_ERROR_OK) {
		fprintf(stderr, "Cannot open input file '%s': %s\n",
			filename,
			error_str(err)
		);
	}

	pp_set_symtab(&pp, &symtab);

	while (pp_next(&pp, &token_data) == MCC_ERROR_OK) {
		if (token_data.token == TOKEN_EOF)
			break;

		lexer_dump_token(&token_data);
	}

	printf("\n");

	pp_free(&pp);
	symtab_free(&symtab);

	return EXIT_SUCCESS;
}
