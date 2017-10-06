#include "print.h"
#include "symbol.h"
#include "token.h"
#include <string.h>

const char *token_names[] = {
	[TOKEN_AMP] = "&",
	[TOKEN_OP_BITANDEQ] = "&=",
	[TOKEN_OP_ARROW] = "->",
	[TOKEN_ASTERISK] = "*",
	[TOKEN_CHAR_CONST] = "char",
	[TOKEN_COLON] = ":",
	[TOKEN_COMMA] = ",",
	[TOKEN_MINUSMINUS] = "--",
	[TOKEN_OP_DIV] = "/",
	[TOKEN_OP_DIVEQ] = "/=",
	[TOKEN_OP_DOT] = ".",
	[TOKEN_ELLIPSIS] = "...",
	[TOKEN_EOF] = "<<EOF>>",
	[TOKEN_OP_ASSIGN] = "=",
	[TOKEN_OP_EQ] = "==",
	[TOKEN_OP_GE] = ">=",
	[TOKEN_OP_GT] = ">",
	[TOKEN_HASH] = "#",
	[TOKEN_HASH_HASH] = "##",
	[TOKEN_HEADER_HNAME] = "header-hname",
	[TOKEN_HEADER_QNAME] = "header-qname",
	[TOKEN_PLUSPLUS] = "++",
	[TOKEN_LBRACE] = "{",
	[TOKEN_LBRACKET] = "[",
	[TOKEN_OP_LE] = "<=",
	[TOKEN_OP_AND] = "&&",
	[TOKEN_OP_OR] = "||",
	[TOKEN_LPAREN] = "(",
	[TOKEN_OP_LT] = "<",
	[TOKEN_MINUS] = "-",
	[TOKEN_OP_SUBEQ] = "-=",
	[TOKEN_OP_MOD] = "%",
	[TOKEN_OP_MODEQ] = "%=",
	[TOKEN_OP_MULEQ] = "*=",
	[TOKEN_NAME] = "name",
	[TOKEN_OP_NEG] = "~",
	[TOKEN_OP_NEQ] = "!=",
	[TOKEN_OP_NOT] = "!",
	[TOKEN_NUMBER] = "number",
	[TOKEN_OP_BITOR] = "|",
	[TOKEN_OP_BITOREQ] = "|=",
	[TOKEN_PLUS] = "+",
	[TOKEN_OP_ADDEQ] = "+=",
	[TOKEN_QMARK] = "?",
	[TOKEN_RBRACE] = "}",
	[TOKEN_RBRACKET] = "]",
	[TOKEN_RPAREN] = ")",
	[TOKEN_SEMICOLON] = ";",
	[TOKEN_OP_SHL] = "<<",
	[TOKEN_OP_SHLEQ] = "<<=",
	[TOKEN_OP_SHR] = ">>",
	[TOKEN_OP_SHREQ] = ">>=",
	[TOKEN_STRING_LITERAL] = "string",
	[TOKEN_OP_XOR] = "^",
	[TOKEN_OP_XOREQ] = "^=",
	[TOKEN_OTHER] = "^@",
	[TOKEN_PLACEMARKER] = "{placemarker}",
};

const char *token_get_name(enum token_type token)
{
	return token_names[token];
}

char *token_get_spelling(struct token *token)
{
	switch (token->type)
	{
	case TOKEN_NAME:
	case TOKEN_CHAR_CONST:
	case TOKEN_HEADER_HNAME:
	case TOKEN_HEADER_QNAME:
	case TOKEN_NUMBER:
	case TOKEN_OTHER: /* TODO simplify this case */
	case TOKEN_STRING_LITERAL:
		return token->spelling;

	default:
		return (char *)token_get_name(token->type);
	}
}

bool token_is(struct token *token, enum token_type token_type)
{
	return token && token->type == token_type;
}

bool token_is_name(struct token *token)
{
	return token_is(token, TOKEN_NAME);
}

bool token_is_macro(struct token *token)
{
	return token_is_name(token)
		&& token->symbol->def->type == SYMBOL_TYPE_CPP_MACRO;
}

bool token_is_macro_arg(struct token *token)
{
	return token_is_name(token)
		&& token->symbol->def->type == SYMBOL_TYPE_CPP_MACRO_ARG;
}

bool token_is_eof(struct token *token)
{
	return token_is(token, TOKEN_EOF);
}

bool token_is_any_keyword(struct token *token)
{
	return token_is_name(token)
		&& token->symbol->def->type == SYMBOL_TYPE_C_KEYWORD;
}

bool token_is_keyword(struct token *token, enum kwd kwd)
{
	return token_is_any_keyword(token) && token->symbol->def->kwdinfo->kwd == kwd;
}

bool token_is_tqual(struct token *token)
{
	return token_is_any_keyword(token)
		&& (token->symbol->def->kwdinfo->class == KWD_CLASS_TQUAL
		|| token->symbol->def->kwdinfo->class == KWD_CLASS_TFLAG);
}

char *token_to_string(struct token *token)
{
	struct strbuf buf;
	char *copy;

	strbuf_init(&buf, 1024);
	token_print(token, &buf);

	copy = strbuf_strcpy(&buf);
	strbuf_free(&buf);

	return copy;
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

	case TOKEN_STRING_LITERAL:
		strbuf_putc(buf, '\"');
		print_string(token->lstr.str, buf);
		//strbuf_printf(buf, token->str);
		strbuf_putc(buf, '\"');
		break;

	default:
		strbuf_printf(buf, "%s", name);
	}
}

void token_dump(struct token *token, FILE *fout)
{
	struct strbuf buf;
	strbuf_init(&buf, 1024);
	token_print(token, &buf);
	fprintf(fout, "%s\n", strbuf_get_string(&buf));
	strbuf_free(&buf);
}

void location_dump(struct location *loc)
{
	fprintf(stderr, "%lu:%lu\n", loc->line_no, loc->column_no);
}
