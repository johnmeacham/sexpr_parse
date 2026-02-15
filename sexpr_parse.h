#ifndef SEXPR_PARSE_H
#define SEXPR_PARSE_H

/* generic s-expression parser. This expects a 'uintptr_t' can store s-expr
 * values and does not do any memory management of the returned list itself. It
 * calls functions supplied by who is using it.
 * nodes are represented by 'uintptr_t' and the parser does not care what they
 * actually contain. They can hold a user defined pointer or a number. You can
 * redefine the type by setting SP_CELL_TYPE to something else.
 */

#include <inttypes.h>
#include <stdbool.h>

/* user definable options here */

// this limits the number of parseable items in a single list.
#define SP_MAX_STACK 1024
// this limits the maximum nesting depth of s-expressions.
#define SP_CONTROL_STACK 256

// What to do on error
// 0 - ignore silently and try to continue parsing
// 1 - print a warning and continue parsing
// 2 - print an error and return -1
// 3 - print an error and exit(1)
// 4 - user implemented sp_error
#define SP_ERROR 1

/* feel free to redefine this to whatever you want as your cell type. */
#define SP_CELL_TYPE uintptr_t

/* If you are doing garbage collection you can iterate over the roots held by
 * the parse state with the following snippet.
 * for(int i = 0; i < ps->sptr; i++)
 *     add_root(ps->stack[i]);
 * user_data is not used by the parser and may be set by you.
 */


typedef SP_CELL_TYPE sp_cell_t;

struct parse_state {
        void *user_data;
        char *fname, *cursor, *start;
        int error, sptr, csptr, lineno;
        struct centry {
                bool unary;
                char what;
                int depth;
        } control_stack[SP_CONTROL_STACK];
        sp_cell_t stack[SP_MAX_STACK];
};

#define PARSE_STATE_INIT(filename, _user_data) { .fname = filename, .user_data = _user_data, .csptr = 1, .control_stack = { { .depth = -1 } } }

// this is the main entry points, returns the number of parsed
// expressions which will be on ps->stack[].
//
// returns a negative number if sp_error does.
// even if it returns an error code, you may be able to find partially parsed
// data on ps->stack.
int sp_scan(struct parse_state *ps, char *start);

/* the following are assumed to be defined by the s-expr consumer and are not
 * defined by the parser. */
sp_cell_t sp_symbol(struct parse_state *ps, char *s, char *e);
sp_cell_t sp_string(struct parse_state *ps, char *s, char *e);
sp_cell_t sp_number(struct parse_state *ps, char *s, char *e, int radix);
/* unary operators are '|,|`|,@|# and they are identified by their own
 * characters except ,@ which passes in @ */
sp_cell_t sp_unary(struct parse_state *ps, char unop, sp_cell_t v);

/* delim will be '(', '[' or '{' */
sp_cell_t sp_list(struct parse_state *nonce, char delim, sp_cell_t *start, int len);
/* explicit cons ( car . cdr ) */
sp_cell_t sp_cons(struct parse_state *nonce, char delim, sp_cell_t *start, int len, sp_cell_t cdr);

/* this is called when an unknown character is encountered, if it returns non
 * zero the parser will stop and return that as the error code,
 * otherwise it will ignore the character and continue parsing.
 * */
int sp_error(struct parse_state *ps, char *fname, int line, char *errstr);



#endif
