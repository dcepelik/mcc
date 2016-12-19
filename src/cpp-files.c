#include "cpp-internal.h"
#include "context.h"

#define INBUF_BLOCK_SIZE	2048


struct cpp_file *cpp_cur_file(struct cpp *cpp)
{
	return list_first(&cpp->file_stack);
}


mcc_error_t cpp_file_init(struct cpp *cpp, struct cpp_file *file, char *filename)
{
	mcc_error_t err;

	if ((err = inbuf_open(&file->inbuf, INBUF_BLOCK_SIZE, filename)) != MCC_ERROR_OK)
		goto out;

	if ((err = lexer_init(&file->lexer, cpp->ctx, &file->inbuf)) != MCC_ERROR_OK)
		goto out_inbuf;
	
	file->filename = mempool_strdup(&cpp->ctx->token_data, filename);
	if (!file->filename) {
		err = MCC_ERROR_NOMEM;
		goto out_lexer;
	}

	return MCC_ERROR_OK;

out_lexer:
	lexer_free(&file->lexer);

out_inbuf:
	inbuf_close(&file->inbuf);

out:
	return err;
}


void cpp_file_free(struct cpp *cpp, struct cpp_file *file)
{
	lexer_free(&file->lexer);
}


void cpp_file_include(struct cpp *cpp, struct cpp_file *file)
{
	list_insert_first(&cpp->file_stack, &file->list_node);
}


mcc_error_t cpp_open_file(struct cpp *cpp, char *filename)
{
	struct cpp_file *file;
	mcc_error_t err;

	file = objpool_alloc(&cpp->file_pool);
	if (!file)
		return MCC_ERROR_NOMEM;

	if ((err = cpp_file_init(cpp, file, filename)) != MCC_ERROR_OK) {
		objpool_dealloc(&cpp->file_pool, file);
		return err;
	}

	cpp_file_include(cpp, file);
	cpp_next_token(cpp);

	return MCC_ERROR_OK;
}


void cpp_close_file(struct cpp *cpp)
{
	assert(!list_is_empty(&cpp->file_stack));

	struct cpp_file *file;

	file = list_first(&cpp->file_stack);
	lexer_free(&file->lexer);
	list_remove_first(&cpp->file_stack);

	objpool_dealloc(&cpp->file_pool, file);
}
