#include "buffer.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE	4096

int main(int argc, char *argv[])
{
	char *filename;
	struct buffer buffer;
	struct lexer lexer;
	struct token_data token_data;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	filename = argv[1];

	if (buffer_open(&buffer, BUFFER_SIZE, filename) != MCC_ERROR_OK) {
		fprintf(stderr, "Cannot open input buffer '%s' for reading\n",
			filename);
		exit(EXIT_FAILURE);
	}

	lexer_reset(&lexer, &buffer);

	while (lexer_next_token(&lexer, &token_data)) {
		switch (token_data.token) {
		case TOKEN_STRING_LITERAL:
			printf("\"%s\"\n", token_data.string);
			break;

		case TOKEN_IDENT:
			printf("[%s]\n", token_data.ident);
			break;

		case TOKEN_PP_NUMBER:
			printf("%i\n", token_data.value);
			break;

		case TOKEN_LBRACE:
			printf("{\n");
			break;

		case TOKEN_RBRACE:
			printf("}\n");
			break;

		case TOKEN_LPAREN:
			printf("(\n");
			break;

		case TOKEN_RPAREN:
			printf(")\n");
			break;

		case TOKEN_SEMICOLON:
			printf(";\n");
			break;

		case TOKEN_EOF:
			printf("EOF\n");
			break;

		case TOKEN_ERROR:
			printf("#ERROR\n");
			break;

		default:
			printf("Token unknown\n");
			break;
		}

		if (token_data.token == TOKEN_EOF)
			break;
	}

	buffer_close(&buffer);

	return EXIT_SUCCESS;
}
