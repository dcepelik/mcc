#include "symtab.h"
#include "tokinfo.h"
#include <ctype.h>


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
	[TOKEN_HEADER_NAME] = "header-name",
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


const char *token_get_name(enum token token)
{
	return token_names[token];
}


static inline size_t print_char(char c, struct strbuf *buf)
{
	switch (c)
	{
	case '\a':
		return strbuf_printf(buf, "\\a");

	case '\b':
		return strbuf_printf(buf, "\\b");

	case '\f':
		return strbuf_printf(buf, "\\f");

	case '\r':
		return strbuf_printf(buf, "\\r");

	case '\n':
		return strbuf_printf(buf, "\\n");

	case '\v':
		return strbuf_printf(buf, "\\v");

	case '\t':
		return strbuf_printf(buf, "\\t");

	case '\\':
		return strbuf_printf(buf, "\\");

	case '\"':
		return strbuf_printf(buf, "\\\"");

	case '\'':
		return strbuf_printf(buf, "\\\'");

	case '\?':
		return strbuf_printf(buf, "\\\?");
	}

	if (isprint(c))
		return strbuf_printf(buf, "%c", c);
	else
		return strbuf_printf(buf, "\'0x%x\'", c);
}


static inline void print_string(char *str, struct strbuf *buf)
{
	char c;
	size_t i = 0;

	while ((c = str[i++]))
		print_char(c, buf);
}


void tokinfo_print(struct tokinfo *tokinfo, struct strbuf *buf)
{
	const char *name;

	name = token_get_name(tokinfo->token);

	switch (tokinfo->token) {
	case TOKEN_CHAR_CONST:
		strbuf_putc(buf, '\'');
		print_char(tokinfo->value, buf);
		strbuf_putc(buf, '\'');
		break;

	case TOKEN_HEADER_NAME:
		strbuf_printf(buf, "<<%s:%s>>", name, tokinfo->str);
		break;

	case TOKEN_NAME:
		strbuf_printf(buf, "%s[%s]", symbol_get_name(tokinfo->symbol),
			symdef_get_type(tokinfo->symbol->def));
		break;

	case TOKEN_NUMBER:
		strbuf_printf(buf, "%s", tokinfo->str);
		break;

	case TOKEN_STRING:
		strbuf_putc(buf, '\"');
		print_string(tokinfo->str, buf);
		strbuf_putc(buf, '\"');
		break;

	default:
		strbuf_printf(buf, "%s", name);
	}
}
