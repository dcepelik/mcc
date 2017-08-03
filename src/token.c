#include "print.h"
#include "symbol.h"
#include "token.h"
#include <string.h>


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
	[TOKEN_STRING_LITERAL] = "string",
	[TOKEN_XOR] = "^",
	[TOKEN_XOR_EQ] = "^=",
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


bool token_is_eol(struct token *token)
{
	return token_is(token, TOKEN_EOL);
}


bool token_is_eol_or_eof(struct token *token)
{
	return token_is_eol(token) || token_is_eof(token);
}


bool token_is_any_keyword(struct token *token)
{
	return token_is_name(token)
		&& token->symbol->def->type == SYMBOL_TYPE_C_KEYWORD;
}


bool token_is_keyword(struct token *token, enum kwd_type type)
{
	return token_is_any_keyword(token) && token->symbol->def->keyword->type == type;
}


bool token_is_tqual(struct token *token)
{
	return token_is_any_keyword(token)
		&& token->symbol->def->keyword->class == KWD_CLASS_TQUAL;
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
