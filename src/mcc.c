#include "error.h"
#include "cpp.h"
#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE	4096

int main(int argc, char *argv[])
{
	char *filename;
	struct tokinfo *tokinfo;
	struct symtab symtab;
	struct cpp cpp;
	mcc_error_t err;
	struct strbuf buf;
	size_t i;

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

	strbuf_init(&buf, 256);
	for (i = 0; (tokinfo = cpp_next(&cpp)); i++) {	
		if (i > 0)
			strbuf_putc(&buf, ' ');
		tokinfo_print(tokinfo, &buf);

		if (tokinfo->token == TOKEN_EOF)
			break;
	}

	if (!tokinfo) {
		fprintf(stderr, "Out of memory.\n");
		return EXIT_FAILURE;
	}

	printf("%s\n", strbuf_get_string(&buf));
	strbuf_free(&buf);

	cpp_close(&cpp);
	cpp_free(&cpp);
	symtab_free(&symtab);

	return EXIT_SUCCESS;
}
