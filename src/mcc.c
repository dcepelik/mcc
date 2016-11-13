#include "error.h"
#include "cpp.h"
#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE	4096

int main(int argc, char *argv[])
{
	char *filename;
	struct tokinfo tokinfo;
	struct symtab symtab;
	struct cpp cpp;
	mcc_error_t err;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	filename = argv[1];

	symtab_init(&symtab);

	cpp_init(&cpp);
	err = cpp_open(&cpp, filename);
	if (err != MCC_ERROR_OK) {
		fprintf(stderr, "Cannot open input file '%s': %s\n",
			filename,
			error_str(err)
		);
	}

	cpp_set_symtab(&cpp, &symtab);

	while (cpp_next(&cpp, &tokinfo) == MCC_ERROR_OK) {
		if (tokinfo.token == TOKEN_EOF)
			break;

		lexer_dump_token(&tokinfo);
	}

	printf("\n");

	cpp_free(&cpp);
	symtab_free(&symtab);

	return EXIT_SUCCESS;
}
