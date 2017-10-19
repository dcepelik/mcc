/*
 * See 6.10 Preprocessing directives.
 */
#include "context.h"
#include "cpp-internal.h"

#define VA_ARGS_NAME	"__VA_ARGS__"

/*
 * C preprocessor directive information.
 */
struct cpp_dirinfo
{
	char *name;
};

/*
 * Array of `struct cpp_dirinfo' used by `cpp_setup_symtab_directives'.
 */
static const struct cpp_dirinfo dirinfos[] = {
	[CPP_DIRECTIVE_IF] = { .name = "if" },
	[CPP_DIRECTIVE_IFDEF] = { .name = "ifdef" },
	[CPP_DIRECTIVE_IFNDEF] = { .name = "ifndef" },
	[CPP_DIRECTIVE_ELIF] = { .name = "elif" },
	[CPP_DIRECTIVE_ELSE] = { .name = "else" },
	[CPP_DIRECTIVE_ENDIF] = { .name = "endif" },
	[CPP_DIRECTIVE_INCLUDE] = { .name = "include" },
	[CPP_DIRECTIVE_DEFINE] = { .name = "define" },
	[CPP_DIRECTIVE_UNDEF] = { .name = "undef" },
	[CPP_DIRECTIVE_LINE] = { .name = "line" },
	[CPP_DIRECTIVE_ERROR] = { .name = "error" },
	[CPP_DIRECTIVE_PRAGMA] = { .name = "pragma" },
};

/*
 * Provide definitions for symbols corresponding to names of CPP directives.
 * See `struct dirinfo', `dirinfos'. See 6.10 Preprocessing directives.
 */
void cpp_setup_symtab_directives(struct symtab *table)
{
	struct symbol *symbol;
	struct symdef *def;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(dirinfos); i++) {
		symbol = symtab_insert(table, dirinfos[i].name);
		def = symbol_define(table, symbol);
		def->type = SYMBOL_TYPE_CPP_DIRECTIVE;
		def->directive = (enum cpp_directive)i;
	}
}

static char *cpp_directive_to_string(enum cpp_directive dir)
{
	assert(0 <= dir && dir < ARRAY_SIZE(dirinfos));
	return dirinfos[dir].name;
}

/*
 * Expect any directive on the input. If it's not there, issue an error.
 */
static bool expect_directive(struct cpp *cpp)
{
	if (!cpp_expect(cpp, TOKEN_NAME))
		return false;

	if (cpp->token->symbol->def->type != SYMBOL_TYPE_CPP_DIRECTIVE) {
		DEBUG_EXPR("%i", cpp->token->symbol->def->type);
		cpp_error(cpp, "`%s' is not a C preprocessor directive",
			symbol_get_name(cpp->token->symbol));
		return false;
	}
	return true;
}

/******************************** end-of-line handling ********************************/

/*
 * An end-of-line token returned by `next_weol'.
 */
static struct token eol = {
	.type = TOKEN_EOL,
	.spelling = NULL,
	.after_white = false,
	.is_at_bol = false,
};

/*
 * Move to the next token. Does to same as `cpp_next_token', except that
 * `next_weol' will set `cpp->token' to the `eol' static token whenever
 * the following token is on a new logical line. See `eol' defined above.
 *
 * This is suitable for directive processing, because CPP directives
 * are required to be contained within a single logical line. This way, one
 * can depend on having the stream of tokens terminated with either
 * TOKEN_EOF or TOKEN_EOL.
 *
 * NOTE: The `eol' token cannot be eaten by `next_weol'. Once returned,
 *       following calls to `next_weol' will return it again and again,
 *       until `cpp_next_token' is called.
 */
static void next_weol(struct cpp *cpp)
{
	if (cpp_peek(cpp)->is_at_bol)
		cpp->token = &eol;
	else
		cpp_next_token(cpp);
}

/*
 * Skip rest of current line.
 *
 * NOTE: Nothing will be skipped if we're at the beginning of a line!
 */
static void skip_rest_of_line(struct cpp *cpp)
{
	while (!token_is_eol_or_eof(cpp->token))
		next_weol(cpp);
}

/*
 * Skip rest of current line, issuing a warning if anything was there. This 
 * is used when I assume there are no more tokens on a line, but if I'm 
 * wrong, I want to skip them and let the user know I omitted something.
 *
 * NOTE: Nothing will be skiped if we're at the beginning of a line!
 */
static void skip_rest_grouching(struct cpp *cpp)
{
	if (!token_is_eol_or_eof(cpp->token)) {
		cpp_warn(cpp, "unexpected extra tokens will be skipped");
		skip_rest_of_line(cpp);
	}
}

/******************************** ifstack helpers ********************************/

/*
 * Represents a currently processed CPP `if' directive.
 */
struct cpp_if
{
	struct lnode list_node;	/* node in the if-stack */
	struct token *token;		/* the corresponding `if' token */
	bool skip_this_branch;		/* are we inside a skipped branch? */
	bool skip_next_branch;		/* should the next branch be skipped? */
};

/*
 * ``Artificial'' item to be kept at the bottom of the if stack to avoid 
 * special cases in the code. Its presence is equivalent to the whole file 
 * being wrapped in a big #if 1 ... #endif directive.
 */
static struct cpp_if ifstack_bottom = {
	.token = NULL,
	.skip_this_branch = false,
	.skip_next_branch = true /* no other branches */
};

/*
 * Initialize the stack of `cpp_if' structures. The stack represents the 
 * currently processed (``open'') `if' directives. Whenever an `if' block 
 * ends with an `endif', the matching `cpp_if' structure is popped off the 
 * stack.
 *
 * The `ifstack_bottom' item is inserted here. See above.
 */
void cpp_init_ifstack(struct cpp *cpp)
{
	list_init(&cpp->ifs);
	list_insert_head(&cpp->ifs, &ifstack_bottom.list_node);
}

/*
 * Allocate and push a new `cpp_if' structure onto the `if' stack and 
 * initialize and return it. The @token shall be the [if] token.
 *
 * PERF: For performance reasons, it might be fruitful to use an objpool 
 *       for `cpp_if' structures.
 */
static struct cpp_if *push_if(struct cpp *cpp, struct token *token)
{
	struct cpp_if *cpp_if = mempool_alloc(&cpp->ctx->token_data, sizeof(*cpp_if));
	cpp_if->token = token;
	cpp_if->skip_this_branch = false;
	cpp_if->skip_next_branch = false;

	list_insert_head(&cpp->ifs, &cpp_if->list_node);

	return cpp_if;
}

/*
 * Pop a `cpp_if' structure off the `if' stack.
 */
static struct cpp_if *pop_if(struct cpp *cpp)
{
	assert(!list_empty(&cpp->ifs));
	return list_remove_head(&cpp->ifs);
}

/*
 * Return the top (``current'') `cpp_if' of the `if' stack.
 */
static struct cpp_if *top_if(struct cpp *cpp)
{
	assert(!list_empty(&cpp->ifs));
	return list_first(&cpp->ifs);
}

/*
 * Are we inside an `if'/`elif'/`else' branch which should be skipped?
 */
bool cpp_is_skip_mode(struct cpp *cpp)
{
	return top_if(cpp)->skip_this_branch;
}

/******************************** directive handlers ********************************/

/*
 * Parse the argument list in the definition of a CPP macro. The argument
 * list may only contain the opening parenthesis, a list of comma-separated
 * identifiers, and a closing parentheses.
 */
static void parse_macro_arglist(struct cpp *cpp, struct macro *macro)
{
	bool expect_comma = false;
	bool arglist_ended = false;

	assert(token_is(cpp->token, TOKEN_LPAREN));
	next_weol(cpp); /* ( */

	while (!token_is_eol_or_eof(cpp->token)) {
		if (token_is(cpp->token, TOKEN_COMMA)) {
			if (!expect_comma)
				cpp_error(cpp, "comma was unexpected here");
			expect_comma = false;
			next_weol(cpp); /* `,' */
			continue;
		}

		if (token_is(cpp->token, TOKEN_RPAREN)) {
			arglist_ended = true;
			next_weol(cpp);
			break;
		}

		if (expect_comma)
			cpp_error(cpp, "comma was expected here");
			/* keep going: we know there should be a comma, act as if */

		if (cpp->token->type == TOKEN_ELLIPSIS)
			cpp->token->symbol = symtab_search_or_insert(&cpp->ctx->symtab,
				VA_ARGS_NAME);
		else if (!cpp_expect(cpp, TOKEN_NAME))
			continue;

		toklist_insert(&macro->args, cpp->token);
		expect_comma = true;
		next_weol(cpp); /* argument identifier or __VA_ARGS__ */
	}

	if (!arglist_ended)
		cpp_error(cpp, "macro arglist not terminated with a `)'");
}

/*
 * Parse a C preprocessor's `define' directive.
 * See 6.10.3 Macro replacement
 */
static void parse_define(struct cpp *cpp)
{
	struct symdef *macro_def;

	next_weol(cpp); /* directive name (define) */

	if (cpp_is_skip_mode(cpp)) {
		skip_rest_of_line(cpp);
		return;
	}

	if (!cpp_expect(cpp, TOKEN_NAME)) {
		skip_rest_grouching(cpp);
		return;
	}

	/*
	 * We will provide a new definition for macro name, which will be a CPP 
	 * macro (either object-like or function-like).
	 */
	macro_def = symbol_define(&cpp->ctx->symtab, cpp->token->symbol);
	macro_def->type = SYMBOL_TYPE_CPP_MACRO;
	macro_init(&macro_def->macro);
	macro_def->macro.name = symbol_get_name(cpp->token->symbol);

	next_weol(cpp); /* macro name */

	/*
	 * If a left opening parentheses directly follows macro name,
	 * the macro defined is function-like. Otherwise, it is object-like.
	 */
	if (cpp->token->type == TOKEN_LPAREN && !cpp->token->after_white) {
		macro_def->macro.flags = MACRO_FLAGS_FUNCLIKE;
		parse_macro_arglist(cpp, &macro_def->macro);
	} else {
		macro_def->macro.flags = MACRO_FLAGS_OBJLIKE;
	}
		
	//macro_dump(macro);

	/*
	 * For both object-like and function-like macros, construct the
	 * replacement list (the expansion) from the tokens on the current
	 * line.
	 */
	while (!token_is_eol_or_eof(cpp->token)) {
		toklist_insert(&macro_def->macro.expansion, cpp->token);
		next_weol(cpp);
	}

	//symtab_dump(&cpp->ctx->symtab, stderr);
}

/*
 * TODO check it's defined
 * TODO symbol_pop_definition(&cpp->ctx->symtab, cpp->token->symbol);
 */
static void process_undef(struct cpp *cpp)
{
	next_weol(cpp); /* directive name (undef) */

	if (cpp_is_skip_mode(cpp)) {
		skip_rest_of_line(cpp);
		return;
	}

	if (!cpp_expect(cpp, TOKEN_NAME)) {
		skip_rest_grouching(cpp);
		return;
	}

	next_weol(cpp);
	skip_rest_grouching(cpp);
}

/*
 * Process the `error' directive.
 *
 * 6.10.5 Error directive:
 *
 *     A preprocessing directive of the form
 *
 *       # error [pp-tokens] new-line
 *
 *     causes the implementation to produce a diagnostic message that     
 *     includes the specified sequence of preprocessing tokens.
 *
 * To fulfill that, we'll simply raise an error whenever we encounter the 
 * directive and we'll rely on `cpp_error' to produce a pretty error 
 * message which will contain a sample of the input.
 */
static void process_error(struct cpp *cpp)
{
	next_weol(cpp); /* directive name (error) */

	if (cpp_is_skip_mode(cpp)) {
		skip_rest_of_line(cpp);
		return;
	}

	cpp_error(cpp, "%s", "#error");
	skip_rest_of_line(cpp);
}

/*
 * TODO warn if tokens skipped
 */
static void process_include(struct cpp *cpp)
{
	char *filename;
	mcc_error_t err;
	struct cpp_file *file;

	cpp_this_file(cpp)->lexer.inside_include = true;
	next_weol(cpp); /* directive name (include) */
	cpp_this_file(cpp)->lexer.inside_include = false;

	if (cpp_is_skip_mode(cpp)) {
		skip_rest_of_line(cpp);
		return;
	}

	/*
	 * TODO Disallow presence of NULs in header names.
	 */
	filename = cpp->token->str;
	DEBUG_EXPR("%s", filename);
	file = objpool_alloc(&cpp->file_pool);

	if (token_is(cpp->token, TOKEN_HEADER_HNAME)) {
		err = cpp_file_include_hheader(cpp, filename, file);
	}
	else if (token_is(cpp->token, TOKEN_HEADER_QNAME)) {
		err = cpp_file_include_qheader(cpp, filename, file);
	}
	else {
		cpp_error(cpp, "header name was expected, got %s",
			token_get_name(cpp->token->type));
		skip_rest_of_line(cpp);
		return;
	}

	if (err == MCC_ERROR_OK) {
		skip_rest_of_line(cpp); /* get rid of those tokens now */
		cpp_file_include(cpp, file);
	}
	else {
		cpp_error(cpp, "cannot include file %s: %s", filename, error_str(err));
		skip_rest_of_line(cpp); /* skip is delayed: error location */
	}
}

static bool eval_expr(struct cpp *cpp)
{
	enum cpp_directive dir;
	bool defined;

	dir = cpp->token->symbol->def->directive;
	next_weol(cpp); /* directive name */

	if (dir == CPP_DIRECTIVE_IFDEF || dir == CPP_DIRECTIVE_IFNDEF) {
		if (!cpp_expect(cpp, TOKEN_NAME))
			return false;
		defined = token_is_macro(cpp->token);
		next_weol(cpp);
		return (dir == CPP_DIRECTIVE_IFDEF) ? defined : !defined;
	}

	if (dir == CPP_DIRECTIVE_ELSE)
		return true;

	return true; /* TODO */
}

/*
 * Handle the #if, #ifdef, #ifndef, #elif, #else and #endif directives.
 *
 * The logic behind this is simple, but technical. When we're inside
 * a conditional branch, we only need to know:
 *
 *     a) whether this branch should be skipped, for example because the
 *        controlling expression was false (`skip_this_branch'),
 *
 *     b) whether other branches within this #if/#ifdef/#ifndef should be
 *        considered (`skip_next_branch').
 *
 * There are three valid combinations of these flags:
 *
 *     I)   !skip_this_branch && skip_next_branch: this branch had a controlling
 *          expression which evaluated to true, it will be processed and the
 *          following ``sibling'' branches (#elif and #else, if any) will be skipped.
 *
 *     II)  skip_this_branch && !skip_next_branch: this branch had a controlling
 *          condition which evaluated to false and it will be skipped, but other
 *          branches will be considered (#elif and #else, if any) will be skipped.
 *
 *     III) skip_this_branch && skip_next_branch: this branch had a controlling
 *          condition which evaluated to false, or `skip_next_branch' was true.
 *          It will be skipped and so will be any following #elif and #else branches.
 *
 * Things are only complicated by the fact that the whole #if block and all
 * its branches may be nested within another #if's skipped branch, as in:
 *
 *     #if 0
 *         #if 1 // inner if
 *         ...
 *         #else
 *         ...
 *         #endif
 *     #else
 *     ...
 *     #endif
 *
 * The inner if will will be skipped. But we cannot ``just skip it'', because we
 * need to keep record of the nesting (so that we know which #endif matches
 * which #if/#ifdef/#endif).
 *
 * To tell whether we're inside a skipped branch, we use `cpp_is_skip_mode'.
 *
 * I should not say ``#if directive'' (with the hash sign), because the hash is not
 * a part of the directive's name. But I think it makes the comments easier
 * to read and understand.
 */
static void process_condition(struct cpp *cpp)
{
	enum cpp_directive dir = cpp->token->symbol->def->directive;
	struct cpp_if *cur_if;
	bool skip_outer;
	
	/*
	 * This directive is either an #if/#ifdef/#ifndef directive, in which
	 * case a new `cpp_if' will be pushed onto the stack, or it is an
	 * #elif/#else/#endif directive, in which case we'll get the matching
	 * if from the #if stack.
	 *
	 * In the former case, when the current branch is skipped, set
	 * `skip_next_branch' on the newly pushed #if. This has the effect of
	 * skipping all branches of the newly pushed #if. (See the comments above.)
	 */
	if (dir == CPP_DIRECTIVE_IF || dir == CPP_DIRECTIVE_IFDEF || dir == CPP_DIRECTIVE_IFNDEF) {
		skip_outer = top_if(cpp)->skip_this_branch;
		cur_if = push_if(cpp, cpp->token);
		cur_if->skip_next_branch = skip_outer;
	} else {
		cur_if = top_if(cpp);
	}

	/*
	 * If this was an #elif/#else/#endif directive and there's no matching
	 * #if/#ifdef/#ifndef directive, it's an error.
	 */
	if (cur_if == &ifstack_bottom) {
		cpp_error(cpp, "%s without a matching #if/#ifdef/#ifndef directive",
			cpp_directive_to_string(dir));
		skip_rest_grouching(cpp);
		return;
	}

	if (dir == CPP_DIRECTIVE_ENDIF) {
		pop_if(cpp);
		next_weol(cpp); /* directive name (endif) */
		skip_rest_grouching(cpp);
		return;
	}

	/*
	 * Sometimes we now immediately that a branch should be skipped without
	 * looking at the condition. For example, when this whole #if is nested
	 * in another #if's skipped branch, or when `skip_next_branch' is set.
	 */
	if (cur_if->skip_next_branch) {
		cur_if->skip_this_branch = true;
		skip_rest_of_line(cpp);
		return;
	}

	/*
	 * Otherwise, the condition needs to be evaluated. The condition is
	 * implicitly true for the #else directive.
	 */
	cur_if->skip_this_branch = !eval_expr(cpp);

	/*
	 * If this branch is taken, the next is skipped.
	 */
	cur_if->skip_next_branch = !cur_if->skip_this_branch;

	skip_rest_grouching(cpp);
}

/******************************** public API ********************************/

void cpp_process_directive(struct cpp *cpp)
{
	enum cpp_directive dir;

	assert(token_is(cpp->token, TOKEN_HASH) && cpp->token->is_at_bol);
	next_weol(cpp); /* `#' */

	/*
	 * Detect a null directive (line with only a `#' on the beginning).
	 */
	if (token_is_eol_or_eof(cpp->token)) {
		cpp_next_token(cpp); /* EOL or EOF */
		return;
	}

	if (!expect_directive(cpp)) {
		skip_rest_of_line(cpp);
		return;
	}

	/*
	 * Call the appropriate directive handler. The conditions are
	 * processed separately by `process_condition' for convenience.
	 */
	dir = cpp->token->symbol->def->directive;
	switch (dir) {
	case CPP_DIRECTIVE_DEFINE:
		parse_define(cpp);
		break;
	case CPP_DIRECTIVE_UNDEF:
		process_undef(cpp);
		break;
	case CPP_DIRECTIVE_INCLUDE:
		process_include(cpp);
		break;
	case CPP_DIRECTIVE_ERROR:
		process_error(cpp);
		break;
	default:
		process_condition(cpp);
	}

	skip_rest_grouching(cpp);
}
