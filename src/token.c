#include "symbol.h"
#include "token.h"
#include "print.h"


const char *token_names[] = {
	[TOKEN_AMPERSAND] = "&",
	[TOKEN_AND_EQ] = "&=",
	[TOKEN_ARROW] = "->",
	[TOKEN_ASTERISK] = "*",
	[TOKEN_CHAR_CONST] = "char",
	[TOKEN_COLON] = ":",
	[TOKEN_COMMA] = ",",
	[TOKEN_DEC] = "--",
	[TOKEN_DIV] = "/",
	[TOKEN_DIV_EQ] = "/=",
	[TOKEN_DOT] = ".",
	[TOKEN_ELLIPSIS] = "...",
	[TOKEN_EOF] = "<<EOF>>",
	[TOKEN_EOL] = "<<EOL>>",
	[TOKEN_EQ] = "=",
	[TOKEN_EQ_EQ] = "==",
	[TOKEN_GE] = ">=",
	[TOKEN_GT] = ">",
	[TOKEN_HASH] = "#",
	[TOKEN_HASH_HASH] = "##",
	[TOKEN_HEADER_HNAME] = "header-hname",
	[TOKEN_HEADER_QNAME] = "header-qname",
	[TOKEN_INC] = "++",
	[TOKEN_LBRACE] = "{",
	[TOKEN_LBRACKET] = "[",
	[TOKEN_LE] = "<=",
	[TOKEN_LOGICAL_AND] = "&&",
	[TOKEN_LOGICAL_OR] = "||",
	[TOKEN_LPAREN] = "(",
	[TOKEN_LT] = "<",
	[TOKEN_MINUS] = "-",
	[TOKEN_MINUS_EQ] = "-=",
	[TOKEN_MOD] = "%",
	[TOKEN_MOD_EQ] = "%=",
	[TOKEN_MUL_EQ] = "*=",
	[TOKEN_NAME] = "name",
	[TOKEN_NEG] = "~",
	[TOKEN_NEQ] = "!=",
	[TOKEN_NOT] = "!",
	[TOKEN_NUMBER] = "number",
	[TOKEN_OR] = "|",
	[TOKEN_OR_EQ] = "|=",
	[TOKEN_PLUS] = "+",
	[TOKEN_PLUS_EQ] = "+=",
	[TOKEN_QUESTION_MARK] = "?",
	[TOKEN_RBRACE] = "}",
	[TOKEN_RBRACKET] = "]",
	[TOKEN_RPAREN] = ")",
	[TOKEN_SEMICOLON] = ";",
	[TOKEN_SHL] = "<<",
	[TOKEN_SHL_EQ] = "<<=",
	[TOKEN_SHR] = ">>",
	[TOKEN_SHR_EQ] = ">>=",
	[TOKEN_STRING] = "string",
	[TOKEN_XOR] = "^",
	[TOKEN_XOR_EQ] = "^=",
};


const char *token_get_name(enum token_type token)
{
	return token_names[token];
}


void token_print(struct token *token, struct strbuf *buf)
{
	const char *name;

	name = token_get_name(token->type);

	switch (token->type) {
	case TOKEN_CHAR_CONST:
		strbuf_putc(buf, '\'');
		print_char(token->value, buf);
		strbuf_putc(buf, '\'');
		break;

	case TOKEN_HEADER_HNAME:
		strbuf_printf(buf, "<%s>", token->str);
		break;

	case TOKEN_HEADER_QNAME:
		strbuf_printf(buf, "\"%s\"", token->str);
		break;

	case TOKEN_NAME:
		strbuf_printf(buf, "[%s]", symbol_get_name(token->symbol));
		break;

	case TOKEN_NUMBER:
		strbuf_printf(buf, "%s", token->str);
		break;

	case TOKEN_STRING:
		strbuf_putc(buf, '\"');
		//print_string(token->str, buf);
		strbuf_printf(buf, token->str);
		strbuf_putc(buf, '\"');
		break;

	default:
		strbuf_printf(buf, "%s", name);
	}
}


void token_dump(struct token *token)
{
	struct strbuf buf;
	strbuf_init(&buf, 1024);
	token_print(token, &buf);
	printf("%s\n", strbuf_get_string(&buf));
	strbuf_free(&buf);
}


void location_dump(struct location *loc)
{
	fprintf(stderr, "%lu:%lu\n", loc->line_no, loc->column_no);
}
