#include "toklist.h"
#include "inbuf.h"
#include "context.h"

void toklist_init(struct toklist *lst)
{
	list_init(&lst->tokens);
}

void toklist_free(struct toklist *lst)
{
	list_free(&lst->tokens);
}

struct token *toklist_first(struct toklist *lst)
{
	return list_first(&lst->tokens);
}

struct token *toklist_last(struct toklist *lst)
{
	return list_last(&lst->tokens);
}

struct token *toklist_next(struct token *token)
{
	return lnode_next(&token->list_node);
}

bool toklist_is_empty(struct toklist *lst)
{
	return list_empty(&lst->tokens);
}

size_t toklist_length(struct toklist *lst)
{
	return list_len(&lst->tokens);
}

struct token *toklist_insert_first(struct toklist *lst, struct token *token)
{
	return list_insert_head(&lst->tokens, &token->list_node);
}

struct token *toklist_insert(struct toklist *lst, struct token *token)
{
	return list_insert(&lst->tokens, &token->list_node);
}

void toklist_prepend(struct toklist *lst, struct toklist *lst_to_prepend)
{
	list_prepend(&lst->tokens, &lst_to_prepend->tokens);
}

void toklist_append(struct toklist *lst, struct toklist *lst_to_append)
{
	list_append(&lst->tokens, &lst_to_append->tokens);
}

struct token *toklist_remove_first(struct toklist *lst)
{
	return list_remove_head(&lst->tokens);
}

struct token *toklist_remove_last(struct toklist *lst)
{
	return list_remove(&lst->tokens);
}

struct token *toklist_remove(struct toklist *lst, struct token *token)
{
	return list_remove_node(&lst->tokens, &token->list_node);
}

void toklist_remove_range(struct toklist *lst, struct token *a, struct token *b)
{
	list_remove_range(&lst->tokens, &a->list_node, &b->list_node);
}

void toklist_copy(struct context *ctx, struct toklist *src, struct toklist *dst)
{
	struct token *dst_token;

	toklist_foreach(src_token, src) {
		dst_token = objpool_alloc(&ctx->token_pool);
		*dst_token = *src_token;
		toklist_insert(dst, dst_token);
	}

	/* TODO handle errors */
}

void toklist_print(struct toklist *tokens, struct strbuf *buf)
{
	int level = 0;

	toklist_foreach(token, tokens) {
		level++;
		token_print(token, buf);
		if (token != toklist_last(tokens))
			strbuf_putc(buf, ' ');

		if (level > 100) {
			fprintf(stderr, "*** MAYBE RECURSION ***\n");
			break;
		}
	}
}

void toklist_dump(struct toklist *tokens, FILE *fout)
{
	struct strbuf buf;
	strbuf_init(&buf, 1024);
	toklist_print(tokens, &buf);
	fprintf(fout, "%s\n", strbuf_get_string(&buf));
	strbuf_free(&buf);
}

void toklist_load_from_strbuf(struct toklist *lst, struct context *ctx, struct strbuf *str)
{
	struct inbuf inbuf;
	struct lexer lexer;
	struct token *token;

	inbuf_open_mem(&inbuf, strbuf_get_string(str), strbuf_strlen(str));
	lexer_init(&lexer, ctx, &inbuf);

	while (1) {
		token = objpool_alloc(&ctx->token_pool);
		lexer_next(&lexer, token);
		if (token_is_eof(token))
			break;
		/* TODO refactoring aid, remove */
		if (token_is_eol(token)) {
			objpool_dealloc(&ctx->token_pool, token);
			continue;
		}
		toklist_insert(lst, token);
	}

	assert(token_is_eof(token));
	objpool_dealloc(&ctx->token_pool, token);

	lexer_free(&lexer);
	inbuf_close(&inbuf);
}

void toklist_load_from_string(struct toklist *lst, struct context *ctx, char *fmt, ...)
{
	va_list args;
	struct strbuf str;

	va_start(args, fmt);
	strbuf_init(&str, 128);
	strbuf_vprintf_at(&str, 0, fmt, args);
	va_end(args);

	toklist_load_from_strbuf(lst, ctx, &str);

	strbuf_free(&str);
}

bool toklist_contains(struct toklist *lst, struct token *token)
{
	return list_contains(&lst->tokens, &token->list_node);
}
