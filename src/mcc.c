#include "error.h"
#include "cpp.h"
#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE	4096

int main(int argc, char *argv[])
{
	char *filename;
	struct cppfile *cppfile;
	struct tokinfo *tokinfo;
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
	for (i = 1; (tokinfo = cpp_next(cppfile)); i++) {	
		if (tokinfo->token  == TOKEN_EOL) {
			strbuf_putc(&buf, '\n');
			i = 0;
			continue;
		}

		if (i > 1)
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

	cppfile_close(cppfile);
	cppfile_delete(cppfile);
	symtab_free(&symtab);

	return EXIT_SUCCESS;
}
