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

/*
 * Initialize a `cpp_file' structure: open the backing input buffer
 * and initialize the lexer.
 */
mcc_error_t cpp_file_init(struct cpp *cpp, struct cpp_file *file, char *filename)
{
	mcc_error_t err;

	if ((err = inbuf_open(&file->inbuf, INBUF_BLOCK_SIZE, filename)) != MCC_ERROR_OK)
		return err;

	lexer_init(&file->lexer, cpp->ctx, &file->inbuf);
	file->filename = mempool_strdup(&cpp->ctx->token_data, filename);
	toklist_init(&file->tokens);

	return MCC_ERROR_OK;
}

/*
 * Free the given instance of `cpp_file'.
 */
void cpp_file_free(struct cpp *cpp, struct cpp_file *file)
{
	(void) cpp;
	inbuf_close(&file->inbuf);
	lexer_free(&file->lexer);
	toklist_free(&file->tokens);
}

/*
 * Return the currently processed file.
 */
struct cpp_file *cpp_this_file(struct cpp *cpp)
{
	return list_first(&cpp->file_stack);
}

/*
 * Call to `cpp_file_include' will push @file on top of the file stack.
 * All token processing functions will operate on @file until a call to
 * `cpp_close_file' is made.
 */
void cpp_file_include(struct cpp *cpp, struct cpp_file *file)
{
	/*
	 * Whenever we process a token, we call `cpp_next_token' to indicate
	 * that we're done with it. Therefore, we're not done with `cpp->token',
	 * whatever it is, and thus we'd like to keep it (and read it when
	 * we're done reading the included file). Therefore, we'll enqueue
	 * the token into the current file's unprocessed queue, then include
	 * the other file.
	 */
	if (cpp->token)
		toklist_insert_first(&cpp_this_file(cpp)->tokens, cpp->token);

	list_insert_head(&cpp->file_stack, &file->list_node);
}

/*
 * Search the file @filename to be included in @search_dirs. Configure the
 * `cpp_file' instance accordingly and return success indicator.
 */
static mcc_error_t search_file(struct cpp *cpp, const char **search_dirs, char *filename,
	struct cpp_file *file)
{
	bool file_found;
	struct strbuf pathbuf;
	char *path;
	size_t i;
	mcc_error_t err;

	(void) search_dirs; /* TODO use this */

	strbuf_init(&pathbuf, 128);

	file_found = false;

	if (filename[0] == '/') {
		file_found = (access(filename, F_OK) == 0);
		path = filename;
	} else {
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
	return search_file(cpp, include_dirs, filename, file);
}

mcc_error_t cpp_file_include_hheader(struct cpp *cpp, char *filename, struct cpp_file *file)
{
	return search_file(cpp, include_dirs, filename, file);
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
	assert(!list_empty(&cpp->file_stack));

	struct cpp_file *file;

	file = list_first(&cpp->file_stack);
	list_remove_head(&cpp->file_stack);

	cpp_file_free(cpp, file);
	objpool_dealloc(&cpp->file_pool, file);
}
