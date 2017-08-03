#include "cpp-internal.h"
#include "context.h"
#include <unistd.h>

#define INBUF_BLOCK_SIZE	2048

/*
 * #include <file> search paths.
 */
static const char *include_dirs[] = {
	"",
	".",
	"/usr/include",
	NULL,
};


struct cpp_file *cpp_cur_file(struct cpp *cpp)
{
	return list_first(&cpp->file_stack);
}


mcc_error_t cpp_file_init(struct cpp *cpp, struct cpp_file *file, char *filename)
{
	mcc_error_t err;

	if ((err = inbuf_open(&file->inbuf, INBUF_BLOCK_SIZE, filename)) != MCC_ERROR_OK)
		return err;

	lexer_init(&file->lexer, cpp->ctx, &file->inbuf);
	file->filename = mempool_strdup(&cpp->ctx->token_data, filename);

	return MCC_ERROR_OK;
}


void cpp_file_include(struct cpp *cpp, struct cpp_file *file)
{
	list_insert_first(&cpp->file_stack, &file->list_node);
}


static mcc_error_t cpp_file_include_searchpath_do(struct cpp *cpp, const char **search_dirs, char *filename, struct cpp_file *file)
{
	(void) search_dirs; /* TODO Use this */

	bool file_found;
	struct strbuf pathbuf;
	char *path;
	size_t i;
	mcc_error_t err;

	strbuf_init(&pathbuf, 128);

	file_found = false;

	if (filename[0] == '/') {
		file_found = (access(filename, F_OK) == 0);
		path = filename;
	}
	else {
		for (i = 0; i < ARRAY_SIZE(include_dirs); i++) {
			strbuf_reset(&pathbuf);
			strbuf_printf(&pathbuf, "%s/%s", include_dirs[i], filename);

			path = strbuf_get_string(&pathbuf);

			DEBUG_PRINTF("Testing: %s\n", path);

			if (access(path, F_OK) == 0) {
				file_found = true;
				break;
			}
		}
	}

	if (!file_found)
		return MCC_ERROR_NOENT;

	err = cpp_file_init(cpp, file, path); 
	strbuf_free(&pathbuf);

	return err;
}


mcc_error_t cpp_file_include_qheader(struct cpp *cpp, char *filename, struct cpp_file *file)
{
	return cpp_file_include_searchpath_do(cpp, include_dirs, filename, file);
}


mcc_error_t cpp_file_include_hheader(struct cpp *cpp, char *filename, struct cpp_file *file)
{
	return cpp_file_include_searchpath_do(cpp, include_dirs, filename, file);
}


void cpp_file_free(struct cpp *cpp, struct cpp_file *file)
{
	(void) cpp;
	inbuf_close(&file->inbuf);
	lexer_free(&file->lexer);
}


mcc_error_t cpp_open_file(struct cpp *cpp, char *filename)
{
	struct cpp_file *file;
	mcc_error_t err;

	file = objpool_alloc(&cpp->file_pool);

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
	list_remove_first(&cpp->file_stack);

	cpp_file_free(cpp, file);
	objpool_dealloc(&cpp->file_pool, file);
}
