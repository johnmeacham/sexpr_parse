
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bsd/string.h>
#include <stdbool.h>
#include "sexpr_parse.h"


static void
push(struct parse_state *ps,  uintptr_t c)
{
        uintptr_t *ts = ps->stack + ps->sptr++;
        struct centry *ctop =  ps->control_stack + ps->csptr - 1;
        *ts = c;
        while (ctop->unary && ps->sptr - 1 == ctop->depth) {
                *ts = sp_unary(ps, ctop->what, *ts);
                ctop--; ps->csptr--;
        }
}


static void
push_control(struct parse_state *ps, char control)
{
        struct centry *ctop = ps->control_stack + ps->csptr++;
        ctop->what = control;
        ctop->unary = control != '(' && control != '.';
        ctop->depth = ps->sptr;
}

static int
close_enclosed(struct parse_state *ps, char control)
{
        struct centry *ctop = ps->control_stack + ps->csptr - 1;
        bool is_cons = false;
        uintptr_t cdr = 0;
        int sptr = ps->sptr;
        if (ctop->what == '.') {
                ps->csptr--;
                int len = sptr - ctop->depth;
                if (len != 1) {
                        if (ps->fname)
                                printf("%s:%i: ", ps->fname, ps->lineno);
                        printf("bad cons tail %i\n", len);
                        return 6;
                }
                cdr = ps->stack[--sptr];
                ctop--;
                is_cons = true;
        }
        if (ctop->what == '(') {
                ps->csptr--;
                int len = sptr - ctop->depth;
                uintptr_t v = is_cons
                              ?  sp_cons(ps, ctop->what, &ps->stack[sptr - len], len, cdr)
                              :  sp_list(ps, ctop->what, &ps->stack[sptr - len], len);
                ps->sptr = sptr - len;
                push(ps, v);
                return 0;
        }
        if (ps->fname)
                printf("%s:%i: ", ps->fname, ps->lineno);
        printf("bad closing %c %02x\n", ctop->what, ctop->what);
        return 5;
}


int
scan(struct parse_state *ps, char *start)
{
        ps->cursor = ps->start = start;
        char *t;
        int res = 0;
        char *YYMARKER;
        while (!res) {
                t = ps->cursor;
                /*!re2c
                re2c:define:YYCTYPE  = "unsigned char";
                re2c:define:YYCURSOR = ps->cursor;
                re2c:variable:yych   = curr;
                re2c:indent:top      = 2;
                re2c:yyfill:enable   = 0;
                re2c:yych:conversion = 1;
                re2c:labelprefix     = scan;

                SINITIAL        = [a-zA-Z$!%&*:/<=>?^_~];

                DIGIT           = [0-9] ;
                OCT             = "0" DIGIT+ ;
                SYMBOL          = SINITIAL (SINITIAL | [-0-9a-z+@.])* ;
                INT             = "0" | ( [1-9] DIGIT* ) ;
                WS              = [ \t\r]+;

                "\000"          { res = ps->sptr == 1 ? 0 : 2;  break; }
                ";" [^\n\000]*  { continue; }
                "\n"            { ps->lineno++; continue; }
                WS              { continue; }
                SYMBOL          { push(ps, sp_symbol(ps, t, ps->cursor)); continue; }
                '"' ([^"\000] | "\\\"")* "\"" { push(ps, sp_string(ps, t, ps->cursor)); continue; }
                '-'? OCT             { push(ps, sp_number(ps, t, ps->cursor, 8)); continue; }
                '-'? INT             { push(ps, sp_number(ps, t, ps->cursor, 10)); continue; }

                "..."           { push(ps, sp_symbol(ps, t, ps->cursor)); continue; }
                "+"             { push(ps, sp_symbol(ps, t, ps->cursor)); continue; }
                "-"             { push(ps, sp_symbol(ps, t, ps->cursor)); continue; }
                ",@"            { push_control(ps, '@'); continue; }
                "#;"            { push_control(ps, ';'); continue; }
                [.('`,#]        { push_control(ps, *t); continue; }
                ")"             { res = close_enclosed(ps, *t); continue; }
                [^]             { if (ps->fname) printf("%s:%i: ", ps->fname, ps->lineno); printf("unknown character %c\n", *t); res = 1;  continue; }
                */
        }
        return res;
}


#ifdef TEST_CASE
/* plain testing that reads s-exprs into a static buffer then prints them out
 * does no memory management at all and just mallocs everything.
 * this representation is only a toy example. */

#include <err.h>
#include <stdio.h>

enum TAG {
        TAG_NONE,
        TAG_CONS,
        TAG_NUMBER,
        TAG_STRING,
        TAG_SYMBOL,
        TAG_UNARY,
        TAG_VECTOR
};

struct value {
        uintptr_t tag;
        uintptr_t rest[];
};

struct value *alloc_value(int n)
{
        return calloc(1, sizeof(struct value) + sizeof(uintptr_t) * n);
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
                for (int i = 0; i < v->rest[0]; i++) {
                        bool isbol = false;
                        char beg = i ? ' ' : '(';
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
                }
                printf(")");
                break;
        case TAG_CONS:
                printf("(");
                for (int i = 0; i < v->rest[0]; i++) {
                        if (i)
                                printf(" ");
                        dump_value(v->rest[i + 2], n, false);
                }
                printf(" . ");
                dump_value(v->rest[1], n, false);
                printf(")");
        }
}

size_t
my_strlcpy(char * dst, const char * src, size_t maxlen) {
    const size_t srclen = strlen(src);
    if (srclen + 1 < maxlen) {
        memcpy(dst, src, srclen + 1);
    } else if (maxlen != 0) {
        memcpy(dst, src, maxlen - 1);
        dst[maxlen-1] = '\0';
    }
    return srclen;
}

uintptr_t sp_string(struct parse_state *nonce, char *b, char *e)
{
        struct value *s = alloc_value(((e - b + 1) + 7) / sizeof(uintptr_t));
        s->tag = TAG_STRING;
        my_strlcpy((char *)s->rest, b, e - b + 1);
        return (uintptr_t)s;
}
uintptr_t sp_symbol(struct parse_state *nonce, char *b, char *e)
{
        struct value *s = alloc_value(((e - b + 1) + 7) / sizeof(uintptr_t));
        s->tag = TAG_SYMBOL;
        my_strlcpy((char *)s->rest, b, e - b + 1);
        return (uintptr_t)s;
}

uintptr_t sp_number(struct parse_state *nonce, char *b, char *e, int radix)
{
        struct value *s = alloc_value(1);
        s->tag = TAG_NUMBER;
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
        struct value *s = alloc_value(2);
        s->tag = TAG_UNARY;
        s->rest[0] = unop;
        s->rest[1] = v;
        return (uintptr_t)s;
}

uintptr_t sp_list(struct parse_state *nonce, char delim, uintptr_t *start, int len)
{
        struct value *s = alloc_value(len + 1);
        s->tag = TAG_VECTOR;
        s->rest[0] = len;
        memcpy(&s->rest[1], start, len * sizeof(uintptr_t));
        return (uintptr_t)s;
}
/* explicit cons */
uintptr_t sp_cons(struct parse_state *nonce, char delim, uintptr_t *start, int len, uintptr_t cdr)
{
        struct value *s = alloc_value(len + 2);
        s->tag = TAG_CONS;
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
        struct parse_state ps = PARSE_STATE_INITIALIZER;
        scan(&ps, buffer);
        for (int i = 0; i < ps.sptr; i++) {
                dump_value(ps.stack[i], 0,  false);
                printf("\n\n");
        }
        return 0;
}


#endif
