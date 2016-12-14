#ifndef PRINT_H
#define PRINT_H

#include "strbuf.h"

void print_char(char c, struct strbuf *buf);
void print_string(char *str, struct strbuf *buf);
void print_string_stringify(char *str, struct strbuf *buf);

#endif
