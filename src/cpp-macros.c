#include "context.h"
#include "cpp-internal.h"
#include "inbuf.h"
#include "lexer.h"
#include "print.h"
#include "strbuf.h"
#include "toklist.h"
#include <time.h>

void macro_init(struct macro *macro)
{
	macro->name = NULL;
	toklist_init(&macro->args);
	toklist_init(&macro->expansion);
	macro->is_expanding = false;
	macro->flags = 0;
}

void macro_free(struct macro *macro)
{
	toklist_free(&macro->args);
	toklist_free(&macro->expansion);
}

bool macro_is_funclike(struct macro *macro)
{
	return macro->flags & MACRO_FLAGS_FUNCLIKE;
}

static bool token_is_expandable_macro(struct token *token)
{
	struct macro *macro;
	struct token *peek;

	if (!token_is_macro(token) || token->noexpand)
		return false;

	macro = &token->symbol->def->macro;

	peek = toklist_next(token);
	if (macro_is_funclike(macro) && (!peek || !token_is(peek, TOKEN_LPAREN)))
		return false;

	return true;
}

/*
 * Print the given macro to the given string buffer.
 */
static void macro_print(struct macro *macro, struct strbuf *buf)
{
	strbuf_printf(buf, "%s", macro->name);

	if (macro_is_funclike(macro)) {
		strbuf_putc(buf, '(');
		toklist_foreach(arg, &macro->args) {
			token_print(arg, buf);
			if (arg != toklist_last(&macro->args))
				strbuf_printf(buf, ", ");
		}
		strbuf_putc(buf, ')');
	}

	strbuf_putc(buf, ' ');

	toklist_foreach(repl, &macro->expansion) {
		token_print(repl, buf);
		strbuf_putc(buf, ' ');
	}
}

/******************************** built-in macros ********************************/

/*
 * Setup a built-in CPP macro and register a handler which will decide what
 * the macro should expand to on each occurrence.
 */
static void setup_builtin_handler(struct cpp *cpp, char *name, macro_handler_t *handler)
{
	struct symbol *symbol;
	struct symdef *def;

	symbol = symtab_find_or_insert(&cpp->ctx->symtab, name);

	def = symbol_define(&cpp->ctx->symtab, symbol);
	def->type = SYMBOL_TYPE_CPP_MACRO;
	macro_init(&def->macro);
	def->macro.flags = MACRO_FLAGS_BUILTIN | MACRO_FLAGS_HANDLED;
	def->macro.handler = handler;
}

/*
 * Setup a built-in CPP macro and set its static expansion list. This is used
 * for constant-expansion macros.
 */
static void setup_builtin_static(struct cpp *cpp, char *name, char *fmt, ...)
{
	struct symbol *symbol;
	struct symdef *def;
	struct strbuf str;
	va_list args;

	symbol = symtab_find_or_insert(&cpp->ctx->symtab, name);

	strbuf_init(&str, 16);
	va_start(args, fmt);
	strbuf_vprintf_at(&str, 0, fmt, args);
	va_end(args);
	
	def = symbol_define(&cpp->ctx->symtab, symbol);
	def->type = SYMBOL_TYPE_CPP_MACRO;
	macro_init(&def->macro);
	def->macro.flags = MACRO_FLAGS_BUILTIN;
	toklist_load_from_strbuf(&def->macro.expansion, cpp->ctx, &str);

	strbuf_free(&str);
}

/*
 * Expand __FILE__ to the name of this file.
 */
static void cpp_builtin_file(struct cpp *cpp, struct macro *macro, struct toklist *out)
{
	(void) macro;
	toklist_load_from_string(out, cpp->ctx, "\"%s\"", cpp_this_file(cpp)->filename);
}

/*
 * Expand __LINE__ to the number of currently processed line (where the current
 * file's lexer operates).
 */
static void cpp_builtin_line(struct cpp *cpp, struct macro *macro, struct toklist *out)
{
	(void) macro;
	toklist_load_from_string(out, cpp->ctx, "%lu", cpp_this_file(cpp)->lexer.location.line_no);
}

/*
 * Expand __TIME__ to current time string.
 */
static void cpp_builtin_time(struct cpp *cpp, struct macro *macro, struct toklist *out)
{
	(void) macro;

	time_t rawtime;
	struct tm *timeinfo;
	char timestr[32];

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(timestr, sizeof(timestr), "%T", timeinfo);
	toklist_load_from_string(out, cpp->ctx, "\"%s\"", timestr);
}

/*
 * Expand __DATE__ to current date string.
 *
 * TODO The date string shouldn't be localized (I think).
 */
static void cpp_builtin_date(struct cpp *cpp, struct macro *macro, struct toklist *out)
{
	(void) macro;

	time_t rawtime;
	struct tm *timeinfo;
	char datestr[32];

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(datestr, sizeof(datestr), "%b %e %Y", timeinfo);
	toklist_load_from_string(out, cpp->ctx, "\"%s\"", datestr);
}

/*
 * Setup CPP built-in (predefined) macros.
 * See 6.10.8 Predefined macros.
 */
void cpp_setup_builtin_macros(struct cpp *cpp)
{
	setup_builtin_static(cpp, "__STDC__", "1");
	setup_builtin_static(cpp, "__STDC_VERSION__", "201102L"); /* TODO check */
	setup_builtin_static(cpp, "__STDC_HOSTED__", "0");

	setup_builtin_handler(cpp, "__FILE__", cpp_builtin_file);
	setup_builtin_handler(cpp, "__LINE__", cpp_builtin_line);
	setup_builtin_handler(cpp, "__TIME__", cpp_builtin_time);
	setup_builtin_handler(cpp, "__DATE__", cpp_builtin_date);
}

/******************************** stringification ********************************/

/*
 * See 6.10.3.2 The # operator, par. 2.
 * TODO Work correctly with the lstrs.
 */
static struct token *cpp_stringify(struct cpp *cpp, struct token *token)
{
	struct strbuf str;
	struct toklist *repl_list;
	struct token *first;
	struct token *last;
	struct token *result;

	assert(token_is_macro_arg(token));
	repl_list = &token->symbol->def->macro_arg.tokens;

	strbuf_init(&str, 128);
	toklist_foreach(t, repl_list) {
		if (t->after_white && t != toklist_first(repl_list))
			strbuf_putc(&str, ' ');

		switch (t->type) {
		case TOKEN_CHAR_CONST:
		case TOKEN_STRING_LITERAL:
			print_string_stringify(token_get_spelling(t), &str);
			break;

		default:
			strbuf_printf(&str, token_get_spelling(t));
		}
	}

	result = objpool_alloc(&cpp->ctx->token_pool);
	result->type = TOKEN_STRING_LITERAL;
	result->lstr.str = strbuf_copy_to_mempool(&str, &cpp->ctx->token_data);
	result->lstr.len = strbuf_strlen(&str);
	result->noexpand = false;

	first = toklist_first(repl_list);
	last = toklist_last(repl_list);

	if (first && last) {
		result->startloc = first->startloc;
		result->endloc = last->endloc;
		result->is_at_bol = first->is_at_bol;
		result->after_white = first->after_white;
	}

	strbuf_free(&str);

	return result;
}

/******************************** token pasting ********************************/

/*
 * Alloc and init new placemarker token.
 */
static struct token *new_placemarker(struct cpp *cpp)
{
	struct token *token;

	token = objpool_alloc(&cpp->ctx->token_pool);
	token->type = TOKEN_PLACEMARKER;

	return token;
}

/*
 * Prepare token for pasting. If the token is a macro parameter, copy its
 * replacement list into the output list and if the resulting list is empty,
 * insert a placemarker.
 *
 * If the token is not a macro parameter, just insert it to the output.
 */
static void paste_prepare(struct cpp *cpp, struct token *arg, struct toklist *lst)
{
	if (token_is_macro_arg(arg)) {
		toklist_copy(cpp->ctx, &arg->symbol->def->macro_arg.tokens, lst);

		if (toklist_is_empty(lst))
			toklist_insert(lst, new_placemarker(cpp));
	} else {
		toklist_insert(lst, arg);
	}
}

/*
 * This function ``pastes'' two tokens. It does so by taking the spellings of
 * those tokens, concatenating them and then feeding the result back to the lexer.
 * The lexing of the concatenation should produce a single valid preprocessing token.
 *
 * See TODO.
 */
static void paste(struct cpp *cpp, struct token *t, struct token *u, struct toklist *out)
{
	struct strbuf buf;
	struct toklist lst_t1;
	struct toklist lst_t2;
	struct token *a;
	struct token *b;
	struct toklist paste_result;

	toklist_init(&lst_t1);
	toklist_init(&lst_t2);
	toklist_init(&paste_result);
	toklist_init(out);

	paste_prepare(cpp, t, &lst_t1);
	assert(!toklist_is_empty(&lst_t1));

	paste_prepare(cpp, u, &lst_t2);
	assert(!toklist_is_empty(&lst_t2));

	a = toklist_remove_last(&lst_t1);
	b = toklist_remove_first(&lst_t2);

	if (token_is(a, TOKEN_PLACEMARKER)) {
		toklist_insert_first(&paste_result, b);
	} else if (token_is(b, TOKEN_PLACEMARKER)) {
		toklist_insert_first(&paste_result, a);
	} else {
		strbuf_init(&buf, 32);
		strbuf_printf(&buf, "%s%s", token_get_spelling(a), token_get_spelling(b));

		DEBUG_EXPR("%s", strbuf_get_string(&buf));

		toklist_load_from_strbuf(&paste_result, cpp->ctx, &buf);
		if (toklist_length(&paste_result) != 1)
			cpp_error(cpp, "pasting `%s' and `%s' does not yield a single preprocessing token",
				token_get_spelling(a), token_get_spelling(b));

		strbuf_free(&buf);
	}

	toklist_append(out, &lst_t1);
	toklist_append(out, &paste_result);
	toklist_append(out, &lst_t2);
}

/******************************** macro expansion ********************************/

static struct token *macro_expand_internal(struct cpp *cpp, struct toklist *in, struct toklist *out);
static void macro_expand_rescan(struct cpp *cpp, struct toklist *in, struct toklist *out);

/*
 * Identify and recursively expand arguments of macro invocation.
 */
static struct token *macro_parse_args(struct cpp *cpp, struct macro *macro, struct toklist *invocation)
{
	struct token *token;
	struct token *next;
	struct symdef *def;
	struct toklist args;
	int parens_balance = 0;

	token = toklist_first(invocation);
	assert(token_is_expandable_macro(token));

	token = toklist_next(token);
	assert(token_is(token, TOKEN_LPAREN));

	token = toklist_next(token);

	/*
	 * The `macro->args' list is the list of TOKEN_NAME tokens which are
	 * the formal parameters of the macro.
	 */
	toklist_foreach(param, &macro->args) {
		/*
		 * Define the symbol corresponding to the formal parameter to
		 * be a CPP macro argument.
		 */
		def = symbol_define(&cpp->ctx->symtab, param->symbol);
		def->type = SYMBOL_TYPE_UNDEF;
		toklist_init(&def->macro_arg.tokens);

		toklist_init(&args);
		while (token) {
			if (token_is(token, TOKEN_LPAREN)) {
				parens_balance++;
			} else if (token_is(token, TOKEN_RPAREN)) {
				if (parens_balance > 0)
					parens_balance--;
				else
					break;
			} else if (token_is(token, TOKEN_COMMA) && parens_balance == 0) {
				if (param->type != TOKEN_ELLIPSIS) {
					token = toklist_next(token); /* `,' */
					break;
				}
			}

			assert(parens_balance >= 0);

			next = toklist_next(token);
			toklist_remove(invocation, token); /* TODO make this cleaner */
			toklist_insert(&args, token);
			token = next;
		}

		/* TODO refactor the rest of this block */

		/*
		 * Copy the accumulated arguments in `args' to the argument's
		 * definition.
		 *
		 * Expand the arguments recursively.
		 */
		toklist_copy(cpp->ctx, &args, &def->macro_arg.tokens);
		toklist_init(&def->macro_arg.expansion);
		macro_expand_rescan(cpp, &args, &def->macro_arg.expansion);

		/*
		 * NOTE: Set only after the expansion of the arguments too place.
		 */
		def->type = SYMBOL_TYPE_CPP_MACRO_ARG;
	}

	/* TODO Error checking */
	return token;
}

static void macro_expand_rescan(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	struct token *token;
	struct token *end;
	struct toklist expansion;

	token = toklist_first(in);

	while (token) {
		if (!token_is_expandable_macro(token)) {
			toklist_insert(out, toklist_remove_first(in));
		} else if (token->symbol->def->macro.is_expanding) {
			token->noexpand = true;
			toklist_insert(out, toklist_remove_first(in));
		} else {
			toklist_init(&expansion);
			end = macro_expand_internal(cpp, in, &expansion);
			toklist_remove_range(in, token, end);
			toklist_append(out, &expansion);
		}

		token = toklist_first(in);
	}
}

static void macro_replace_args(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	struct token *token;
	struct token *next;
	struct token *arg1;
	struct token *arg2;
	struct toklist paste_result;
	bool hash = false;

	while ((token = toklist_first(in)) != NULL) {
		next = toklist_next(token);

		if (next && token_is(next, TOKEN_HASH_HASH)) {
			toklist_remove(in, next); /* remove ## */
			next = toklist_next(token); /* second arg */

			arg1 = token;
			arg2 = next;

			toklist_remove(in, arg1);
			toklist_remove(in, arg2);

			assert(arg2 != NULL);

			paste(cpp, arg1, arg2, &paste_result);
			toklist_append(out, &paste_result);
		}
		else if (token_is(token, TOKEN_HASH)) {
			assert(!hash); /* Not # after # TODO Error reporting */
			hash = true;
			toklist_remove_first(in);
		}
		else if (!token_is_macro_arg(token)) {
			assert(!hash); /* Not # {notarg} TODO Error reporting */
			toklist_insert(out, toklist_remove_first(in));
		}
		else if (hash) {
			toklist_insert(out, cpp_stringify(cpp, token));
			toklist_remove_first(in);
			hash = false;
		}
		else {
			toklist_copy(cpp->ctx, &token->symbol->def->macro_arg.expansion, out);
			toklist_remove_first(in);
		}
	}
}

static struct token *macro_expand_internal(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	struct macro *macro;
	struct token *token;
	struct token *end;
	struct toklist expansion;
	struct toklist replaced_args;

	token = toklist_first(in);

	assert(token_is_expandable_macro(token));
	macro = &token->symbol->def->macro;

	toklist_init(&expansion);
	toklist_init(&replaced_args);

	assert(!macro->is_expanding);

	if (macro->flags & MACRO_FLAGS_HANDLED) {
		/*
		 * Temporary assert. Right now, we're only using handled
		 * macros to implement CPP built-ins, all of which are
		 * object-like.
		 */
		assert(!macro_is_funclike(macro));

		macro->handler(cpp, macro, out);
		return token;
	}

	symtab_scope_begin(&cpp->ctx->symtab);

	if (macro_is_funclike(macro))
		end = macro_parse_args(cpp, macro, in);
	else
		end = token;

	toklist_copy(cpp->ctx, &macro->expansion, &expansion);
	macro_replace_args(cpp, &expansion, &replaced_args);

	toklist_foreach(token, &replaced_args) {
		if (token->type == TOKEN_PLACEMARKER)
			toklist_remove(&replaced_args, token);
		assert(!token_is_eol(token));
	}

	/*
	 * NOTE: symtab scope ended prior to recursive expansion.
	 */
	symtab_scope_end(&cpp->ctx->symtab);

	macro->is_expanding = true;
	macro_expand_rescan(cpp, &replaced_args, out);
	macro->is_expanding = false;

	return end;
}

/******************************** public API ********************************/

void macro_expand(struct cpp *cpp, struct toklist *in, struct toklist *out)
{
	macro_expand_internal(cpp, in, out);
}
