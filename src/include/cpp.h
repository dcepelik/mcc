#ifndef CPP_H
#define CPP_H

#include "context.h"
#include "error.h"

struct cpp *cpp_new(struct context *ctx);
void cpp_delete(struct cpp *cpp);

mcc_error_t cpp_open_file(struct cpp *cpp, char *filename);
void cpp_close_file(struct cpp *file);

struct token *cpp_next(struct cpp *cpp);

#endif
