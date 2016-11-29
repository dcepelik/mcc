#ifndef LEXER_H
#define LEXER_H

#include "cppfile.h"

/*
 * Lexer for translation phases 1-3.
 *
 * TODO Process universal character names
 * TODO Handle various encoding-related issues
 */

struct tokinfo *lexer_next(struct cppfile *file);

#endif
