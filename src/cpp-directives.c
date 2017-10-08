/*
 * See 6.10 Preprocessing directives.
 */
#include "context.h"
#include "cpp-internal.h"

#define VA_ARGS_NAME	"__VA_ARGS__"

/*
 * Represents a currently processed CPP `if' directive.
 */
struct cpp_if
{
	struct list_node list_node;
	struct token *token;
	bool skip_this_branch;
	bool skip_next_branch;
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
	list_insert_first(&cpp->ifs, &ifstack_bottom.list_node);
}

static bool have_directive(struct cpp *cpp, enum cpp_directive directive)
{
	return token_is(cpp->token, TOKEN_NAME)
		&& cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_DIRECTIVE
		&& cpp->token->symbol->def->directive == directive;
}

/*
 * Skip rest of current line.
 *
 * NOTE: Nothing will be skipped if we're at the beginning of a line!
 */
static void skip_rest_of_line(struct cpp *cpp)
{
	while (!token_is_eol_or_eof(cpp->token))
		cpp_next_token(cpp);
}

/*
 * Skip rest of current line, issuing a warning if anything was there. This 
 * is used when I assume there are no more tokens on a line, but if I'm 
 * wrong, I want to skip them and let the user know I omitted something.
 *
 * NOTE: Nothing will be skiped if we're at the beginning of a line!
 */
static void skip_rest_and_warn(struct cpp *cpp)
{
	if (!token_is_eol_or_eof(cpp->token)) {
		cpp_warn(cpp, "unexpected extra tokens will be skipped");
		skip_rest_of_line(cpp);
	}
}

/*
 * Expect a token @token on the input. If it's not there, issue an error.
 */
static bool expect_token(struct cpp *cpp, enum token_type token)
{
	if (cpp->token->type == token)
		return true;

	cpp_error(cpp, "%s was expected, got %s",
		token_get_name(token), token_get_name(cpp->token->type));
	return false;
}

/*
 * Expect any directive on the input. If it's not there, issue an error.
 */
static bool expect_directive(struct cpp *cpp)
{
	if (!expect_token(cpp, TOKEN_NAME))
		return false;

	if (cpp->token->symbol->def->type != SYMBOL_TYPE_CPP_DIRECTIVE) {
		cpp_error(cpp, "`%s' is not a C preprocessor directive",
			symbol_get_name(cpp->token->symbol));
		return false;
	}
	return true;
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

	list_insert_first(&cpp->ifs, &cpp_if->list_node);

	return cpp_if;
}

/*
 * Pop a `cpp_if' structure off the `if' stack.
 */
static struct cpp_if *pop_if(struct cpp *cpp)
{
	assert(!list_is_empty(&cpp->ifs));
	return list_remove_first(&cpp->ifs);
}

/*
 * Return the top (or current, matching) `cpp_if' of the `if' stack.
 */
static struct cpp_if *current_if(struct cpp *cpp)
{
	assert(!list_is_empty(&cpp->ifs));
	return list_first(&cpp->ifs);
}

/*
 * Are we inside an `if'/`elif'/`else' branch which should be skipped?
 */
bool lets_skip(struct cpp *cpp)
{
	return current_if(cpp)->skip_this_branch;
}

/*
 * Parse the argument list in the definition of a CPP macro. The document
 * list may only contain the opening parenthesis (already processed TODO),
 * a list of comma-separated identifiers, and a closing parentheses.
 */
static void parse_macro_arglist(struct cpp *cpp, struct macro *macro)
{
	bool expect_comma = false;
	bool arglist_ended = false;

	while (!token_is_eof_or_bol(cpp->token)) {
		if (token_is(cpp->token, TOKEN_COMMA)) {
			if (!expect_comma)
				cpp_error(cpp, "comma was unexpected here");
			expect_comma = false;
			cpp_next_token(cpp); /* `,' */
			continue;
		}

		if (token_is(cpp->token, TOKEN_RPAREN)) {
			arglist_ended = true;
			cpp_next_token(cpp);
			break;
		}

		if (expect_comma)
			cpp_error(cpp, "comma was expected here");
			/* fall-through: we know there should be a comma, act as if */

		if (cpp->token->type == TOKEN_ELLIPSIS)
			cpp->token->symbol = symtab_search_or_insert(&cpp->ctx->symtab,
				VA_ARGS_NAME);
		else if (!expect_token(cpp, TOKEN_NAME))
			continue;

		toklist_insert_last(&macro->args, cpp->token);
		expect_comma = true;
		cpp_next_token(cpp); /* argument identifier or __VA_ARGS__ */
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
	struct symdef *symdef;

	if (lets_skip(cpp)) {
		skip_rest_of_line(cpp);
		return;
	}

	if (!expect_token(cpp, TOKEN_NAME)) {
		skip_rest_and_warn(cpp);
		return;
	}

	/*
	 * We will provide a new definition for macro name, which will be a CPP 
	 * macro; either object-like or function like.
	 */
	symdef = symbol_define(&cpp->ctx->symtab, cpp->token->symbol);
	symdef->type = SYMBOL_TYPE_CPP_MACRO;
	macro_init(&symdef->macro);
	symdef->macro.name = symbol_get_name(cpp->token->symbol);

	cpp_next_token(cpp); /* macro name */

	if (cpp->token->type == TOKEN_LPAREN && !cpp->token->after_white) {
		symdef->macro.flags = MACRO_FLAGS_FUNCLIKE;
		cpp_next_token(cpp); /* ( */
		parse_macro_arglist(cpp, &symdef->macro);
	} else {
		symdef->macro.flags = MACRO_FLAGS_OBJLIKE;
	}
		
	//macro_dump(macro);

	/*
	 * For both object-like and function-like macros, construct the
	 * replacement list (expansion) from the tokens on the current
	 * line.
	 */
	while (!token_is_eol_or_eof(cpp->token)) {
		toklist_insert_last(&symdef->macro.expansion, cpp->token);
		cpp_next_token(cpp);
	}

	//symtab_dump(&cpp->ctx->symtab, stderr);
}

/*
 * TODO check it's defined
 * TODO symbol_pop_definition(&cpp->ctx->symtab, cpp->token->symbol);
 */
static void process_undef(struct cpp *cpp)
{
	if (lets_skip(cpp)) {
		skip_rest_of_line(cpp);
		return;
	}

	if (!expect_token(cpp, TOKEN_NAME)) {
		skip_rest_and_warn(cpp);
		return;
	}

	cpp_next_token(cpp);
	skip_rest_and_warn(cpp);
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
	if (lets_skip(cpp)) {
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

	if (lets_skip(cpp)) {
		skip_rest_of_line(cpp);
		return;
	}

	/*
	 * TODO Disallow presence of NULs in header names.
	 */

	filename = cpp->token->str;
	file = objpool_alloc(&cpp->file_pool);

	if (token_is(cpp->token, TOKEN_HEADER_HNAME)) {
		err = cpp_file_include_hheader(cpp, filename, file);
	}
	else if (token_is(cpp->token, TOKEN_HEADER_QNAME)) {
		err = cpp_file_include_qheader(cpp, filename, file);
	}
	else {
		cpp_error(cpp, "header name was expected, got %s", token_get_name(cpp->token->type));
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

	cpp_cur_file(cpp)->lexer.inside_include = false;
}

void cpp_parse_directive(struct cpp *cpp)
{
	struct cpp_if *cpp_if;
	struct token *prev;
	enum cpp_directive dir;
	bool skipping;
	bool test_cond;

	assert(token_is(cpp->token, TOKEN_HASH));
	assert(cpp->token->is_at_bol);

	cpp_next_token(cpp);

	if (cpp->token->is_at_bol)
		return;

	if (!expect_directive(cpp)) {
		skip_rest_of_line(cpp);
		return;
	}

	prev = cpp->token;
	dir = cpp->token->symbol->def->directive;

	cpp_cur_file(cpp)->lexer.inside_include = have_directive(cpp, CPP_DIRECTIVE_INCLUDE);

	cpp_next_token(cpp);

	switch (dir) {
	case CPP_DIRECTIVE_IFDEF:
		test_cond = cpp->token->symbol->def->type == SYMBOL_TYPE_CPP_MACRO;
		cpp_next_token(cpp);
		goto push_if;

	case CPP_DIRECTIVE_IFNDEF:
		test_cond = cpp->token->symbol->def->type != SYMBOL_TYPE_CPP_MACRO;
		cpp_next_token(cpp);
		goto push_if;

	case CPP_DIRECTIVE_IF:
		test_cond = true; /* TODO */

push_if:
		skipping = lets_skip(cpp);
		cpp_if = push_if(cpp, prev);
		cpp_if->skip_next_branch = skipping;

		goto conclude;

	case CPP_DIRECTIVE_ELIF:
		test_cond = true; /* TODO */
		cpp_if = current_if(cpp);

conclude:
		cpp_if->skip_this_branch = !test_cond || cpp_if->skip_next_branch;
		cpp_if->skip_next_branch |= !cpp_if->skip_this_branch;
		break;
	
	case CPP_DIRECTIVE_ELSE:
		/* TODO check elif after else */
		cpp_if = current_if(cpp);
		cpp_if->skip_this_branch = cpp_if->skip_next_branch;
		break;

	case CPP_DIRECTIVE_ENDIF:
		assert(current_if(cpp) != &ifstack_bottom); /* TODO */

		if (current_if(cpp) == &ifstack_bottom)
			cpp_error(cpp, "endif without matching if/ifdef/ifndef");
		else
			pop_if(cpp);

		break;

	case CPP_DIRECTIVE_DEFINE:
		parse_define(cpp);
		//symtab_dump(&cpp->ctx->symtab, stderr);
		break;

	case CPP_DIRECTIVE_INCLUDE:
		process_include(cpp);

		/*
		 * TODO To keep tokens after the #include "filename" (if any).
		 *      How to get rid of this?
		 */
		return;

	case CPP_DIRECTIVE_ERROR:
		process_error(cpp);
		break;

	case CPP_DIRECTIVE_UNDEF:
		process_undef(cpp);
		break;

	default:
		return;
	}

	skip_rest_and_warn(cpp);
}
