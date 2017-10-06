#include "print.h"
#include <ctype.h>

void print_char(char c, struct strbuf *buf)
{
	switch (c)
	{
	case '\a':
		strbuf_printf(buf, "\\a");
		break;

	case '\b':
		strbuf_printf(buf, "\\b");
		break;

	case '\f':
		strbuf_printf(buf, "\\f");
		break;

	case '\r':
		strbuf_printf(buf, "\\r");
		break;

	case '\n':
		strbuf_printf(buf, "\\n");
		break;

	case '\v':
		strbuf_printf(buf, "\\v");
		break;

	case '\t':
		strbuf_printf(buf, "\\t");
		break;

	case '\\':
		strbuf_printf(buf, "\\\\");
		break;

	case '\"':
		strbuf_printf(buf, "\\\"");
		break;

	case '\'':
		strbuf_printf(buf, "\\\'");
		break;

	default:
		if (isprint(c) || c == ' ' || c == '\t')
			strbuf_printf(buf, "%c", c);
		else
			strbuf_printf(buf, "\'0x%x\'", c);
	}
}

void print_char_stringify(char c, struct strbuf *buf)
{
	switch (c) {
	case '\\':
		strbuf_printf(buf, "\\\\");
		break;

	case '\"':
		strbuf_printf(buf, "\\\"");
		break;

	default:
		strbuf_putc(buf, c);
	}
}

void print_string(utf8_t *str, struct strbuf *buf)
{
	char c;
	size_t i = 0;

	while ((c = str[i++]))
		print_char(c, buf);
}

void print_string_stringify(char *str, struct strbuf *buf)
{
	char c;
	size_t i = 0;

	while ((c = str[i++]) != '\0')
		print_char_stringify(c, buf);
}
