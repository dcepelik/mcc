#ifndef PP_H
#define PP_H

#include "lexer.h"

bool pp_next_token(struct lexer *lexer, struct token_data *token_data);

#endif
