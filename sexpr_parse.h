#ifndef SEXPR_PARSE_H
#define SEXPR_PARSE_H 

/* generic s-expression parser.  this expects a 'uintptr_t' can store s-expr
 * values and does not do any memory management of the returned list itself. It
 * calls functions supplied by who is using it */

#include <inttypes.h>


// this limits the number of parseable items in a single list.
#define MAX_STACK 1024
// this limits the maximum nesting depth of s-expressions.
#define CONTROL_STACK 256

/* all roots can be iterated over as 
 * for(int i = 0; i < ps->sptr; i++)
 *     add_root(ps->stack[i]);
 * nonce can be set to user data and is not used by the parser
 */
struct parse_state {
        void *nonce;
        char *fname;
        char *cursor, *start;
        int error, sptr, csptr, lineno;
        struct centry {
                bool unary;
                char what; 
                int depth;
        } control_stack[CONTROL_STACK];
        uintptr_t stack[MAX_STACK];
};

#define PARSE_STATE_INITIALIZER { .csptr = 1, .control_stack = { { .depth = -1 } } }

int scan(struct parse_state *ps, char *start);

/* the following are assumed to be defined by the s-expr consumer and are not
 * defined by the parser. */
uintptr_t sp_symbol(struct parse_state *ps, char *s, char *e);
uintptr_t sp_string(struct parse_state *ps, char *s, char *e);
uintptr_t sp_number(struct parse_state *ps, char *s, char *e, int radix);
/* unary operators are '|,|`|,@|# and they are identified by their own
 * characters except ,@ which passes in @ */
uintptr_t sp_unary(struct parse_state *ps, char unop, uintptr_t v);
/* delim will be '(' for lists */
uintptr_t sp_list(struct parse_state *nonce, char delim, uintptr_t *start, int len);
/* explicit cons */
uintptr_t sp_cons(struct parse_state *nonce, char delim, uintptr_t *start, int len, uintptr_t cdr);


#endif
