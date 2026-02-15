/* plain testing that reads s-exprs into memory, removes commented out
 * expressions and then pretty prints the result.
 *
 * The data is stored in a single malloced buffer and the uintptr_t nodes just
 * contain offsets into said buffer, all memory can be freed by freeing that one
 * buffer.
 *
 *
 */

#include "sexpr_parse.h"
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>


enum TAG {
        TAG_NONE,
        TAG_CONS,
        TAG_NUMBER,
        TAG_STRING,
        TAG_SYMBOL,
        TAG_UNARY,
        TAG_COMMENT,
        TAG_VECTOR
};

struct value {
        uintptr_t tag;
        uintptr_t rest[];
};

struct value *alloc_bytes(int tag, int n)
{
        struct value *s = calloc(1, sizeof(struct value) + n);
        s->tag = tag;
        return s;
}

struct value *alloc_value(int tag, int n)
{
        return alloc_bytes(tag, sizeof(uintptr_t) * n);
}


int estimate_size(uintptr_t n)
{
        int sz = 0;
        struct value *v = (struct value *)n;
        switch (v->tag) {
        case TAG_STRING:
        case TAG_SYMBOL:
                return strlen((char *)v->rest);
        case TAG_NUMBER:
                return snprintf(NULL, 0, "%li", (long)v->rest[0]);
        case TAG_UNARY:
                return estimate_size(v->rest[1]) + 1;
        case TAG_CONS:
                sz += 2;
        case TAG_VECTOR:
                if (!v->rest[0])
                        return 2;
                for (int i = 0; i < v->rest[0]; i++) {
                        sz++;
                        sz += estimate_size(v->rest[i + 1]);
                }
                sz++;
                return sz;
        case TAG_COMMENT:
                return 0;
        }
        return 0;
}

void dump_value(uintptr_t u, int n, bool bol)
{
        int sz;
        struct value *v = (struct value *)u;
        switch (v->tag) {
        case TAG_NONE:
                printf("(none)");
                break;
        case TAG_STRING:
        case TAG_SYMBOL:
                printf("%s", (char *)v->rest);
                break;
        case TAG_NUMBER:
                printf("%li", (long)v->rest[0]);
                break;
        case TAG_UNARY:
                printf("%c", (char)v->rest[0]);
                dump_value(v->rest[1], n, false);
                break;
        case TAG_VECTOR:
                if (!v->rest[0]) {
                        printf("()");
                        break;
                }
                sz = estimate_size(u);
                // bool byline =  (n + sz > 40) && v->rest[0] > 2;
                bool byline = sz > 10 &&  v->rest[0] > 2;
                char beg = '(';
                for (int i = 0; i < v->rest[0]; i++) {
                        if (((struct value *)(v->rest[i + 1]))->tag == TAG_COMMENT)
                                continue;
                        bool isbol = false;
//                        char beg = i ? ' ' : '(';
                        sz = estimate_size(v->rest[i + 1]);
                        if (byline && sz > 7  && !bol) {
                                // && ((struct value *)v->rest[i + 1])->tag == TAG_VECTOR) {
                                printf("\n");
                                for (int j = 0; j < n ; j++)
                                        printf(" ");
                                putc(beg, stdout);
                                isbol = true;
                        } else
                                putc(beg, stdout);
                        dump_value(v->rest[i + 1], n + 1, isbol);
                        beg = ' ';
                }
                if (beg == '(')
                        putchar('(');
                printf(")");
                break;
        case TAG_CONS:
                printf("(");
                int printed = 0;
                for (int i = 0; i < v->rest[0]; i++) {
                        if (((struct value *)(v->rest[i + 2]))->tag == TAG_COMMENT)
                                continue;
                        if (printed)
                                printf(" ");
                        dump_value(v->rest[i + 2], n, false);
                        printed++;
                }
                if (!printed)
                        printf("#;car-comment");
                printf(" . ");
                if (v->tag == TAG_COMMENT)
                        printf("#;cdr-comment");
                dump_value(v->rest[1], n, false);
                printf(")");
        }
}

uintptr_t sp_string(struct parse_state *nonce, char *b, char *e)
{
        struct value *s =  alloc_bytes(TAG_STRING, e - b + 1);
        memcpy(s->rest, b, e - b);
        ((char *)s->rest)[e - b] = '\0';
        return (uintptr_t)s;
}

uintptr_t sp_symbol(struct parse_state *nonce, char *b, char *e)
{
        struct value *s =  alloc_bytes(TAG_SYMBOL, e - b + 1);
        memcpy(s->rest, b, e - b);
        ((char *)s->rest)[e - b] = '\0';
        return (uintptr_t)s;
}

uintptr_t sp_number(struct parse_state *nonce, char *b, char *e, int radix)
{
        struct value *s = alloc_value(TAG_NUMBER, 1);
        intptr_t num = 0;
        bool negate = false;
        if (*b == '-') {
                negate = true;
                b++;
        }
        --b;
        while (++b < e)
                num = num * radix + (*b - '0');
        s->rest[0] = (uintptr_t)(negate ? -num : num);
        return (uintptr_t)s;
}


uintptr_t sp_unary(struct parse_state *nonce, char unop, uintptr_t v)
{
        if (unop == ';')
                return (uintptr_t)alloc_value(TAG_COMMENT, 0);
        struct value *s = alloc_value(TAG_UNARY, 2);
        s->rest[0] = unop;
        s->rest[1] = v;
        return (uintptr_t)s;
}

uintptr_t sp_list(struct parse_state *nonce, char delim, uintptr_t *start, int len)
{
        struct value *s = alloc_value(TAG_VECTOR, len + 1);
        s->rest[0] = len;
        memcpy(&s->rest[1], start, len * sizeof(uintptr_t));
        return (uintptr_t)s;
}
/* explicit cons */
uintptr_t sp_cons(struct parse_state *nonce, char delim, uintptr_t *start, int len, uintptr_t cdr)
{
        struct value *s = alloc_value(TAG_CONS, len + 2);
        s->rest[0] = len;
        s->rest[1] = cdr;
        memcpy(&s->rest[2], start, len * sizeof(uintptr_t));
        return (uintptr_t)s;
}

int
main(int argc, char *argv[])
{
        char buffer[8192] = {};
        fread(buffer, sizeof(buffer) - 1, 1, stdin);
        struct parse_state ps = PARSE_STATE_INIT("(stdin)",NULL);
        int count = sp_scan(&ps, buffer);
        if (count < 0) {
                printf("sp_scan error: %i\n", count);
                return 1;
        }
        printf("Parsed %i expressions\n", count);
        for (int i = 0; i < ps.csptr; i++) {
                struct centry *c = ps.control_stack + i;
                printf("Control stack %i: %c unary %i depth %i\n", i, c->what, c->unary, c->depth);
        }
        for (int i = 0; i < count; i++) {
                dump_value(ps.stack[i], 0,  false);
                printf("\n\n");
        }
        return 0;
}

