#include "sexpr_parse.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>


static void
push(struct parse_state *ps,  sp_cell_t c)
{
        sp_cell_t *ts = ps->stack + ps->sptr++;
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
        ctop->unary = !strchr("([{.", control);
        ctop->depth = ps->sptr;
}


static int error(struct parse_state *ps, char *s, ...)
{
        ps->error++;
        char errbuf[128];
        va_list ap;
        va_start(ap, s);
        vsnprintf(errbuf, sizeof(errbuf), s, ap);
        va_end(ap);
        return sp_error(ps, ps->fname, ps->lineno + 1, errbuf);
}

#define OC(x,y) ((x) << 8 | (y))

static int
close_enclosed(struct parse_state *ps, char control)
{
        struct centry *ctop = ps->control_stack + ps->csptr - 1;
        bool is_cons = false;
        sp_cell_t cdr = 0;
        int sptr = ps->sptr, err = 0;
        if (ctop->what == '.') {
                ps->csptr--;
                int len = sptr - ctop->depth;
                if (len != 1) {
                        err = error(ps, "dot must be followed by exactly one item");
                } else {
                        cdr = ps->stack[--sptr];
                        is_cons = true;
                }
                ctop--;
        }
        switch (OC(ctop->what, control)) {
        case OC('(', ']'):
        case OC('(', '}'):
        case OC('{', ')'):
        case OC('{', ']'):
        case OC('[', ')'):
        case OC('[', '}'):
                err |=  error(ps, "mismatched delimiters '%c' '%c'", ctop->what, control);
        case OC('(', ')'):
        case OC('[', ']'):
        case OC('{', '}'):
                ps->csptr--;
                int len = sptr - ctop->depth;
                sp_cell_t v =
                        is_cons && !len ? cdr :
                        is_cons ? sp_cons(ps, ctop->what, &ps->stack[sptr - len], len, cdr) :
                        sp_list(ps, ctop->what, &ps->stack[sptr - len], len);
                ps->sptr = sptr - len;
                push(ps, v);
                return err;
        default:
                return err ? err : error(ps, "unexpected closing '%c'", control);
        }
}


int
sp_scan(struct parse_state *ps, char *start)
{
        ps->cursor = ps->start = start;
        char *t;
        int err = 0;
        char *YYMARKER;
        while (!err) {
                t = ps->cursor;
                /*!re2c
                re2c:define:YYCTYPE  = "unsigned char";
                re2c:define:YYCURSOR = ps->cursor;
                re2c:variable:yych   = curr;
                re2c:indent:top      = 2;
                re2c:yyfill:enable   = 0;
                re2c:yych:conversion = 1;
                re2c:labelprefix     = sp_scan;

                SINITIAL        = [a-zA-Z$!%&*:/<=>?^_~];

                DIGIT           = [0-9] ;
                OCT             = "0" DIGIT+ ;
                SYMBOL          = SINITIAL (SINITIAL | [-0-9a-z+@.])* ;
                INT             = "0" | ( [1-9] DIGIT* ) ;
                WS              = [ \t\r]+;

                "\000"          { err = 1; break; }
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
                [.{[('`,#]     { push_control(ps, *t); continue; }
                [\])}]          { err = close_enclosed(ps, *t); continue; }
                [^]             { err = error(ps, "Unexpected char '%c'", *t); continue; }
                */
        }
        int csptr = ps->csptr;
        while (csptr) {
                struct centry *c = ps->control_stack + csptr - 1;
                if (c->depth == -1)
                        break;
                if (c->unary)
                        err = error(ps, "Hanging Unary '%c'", c->what);
                else
                        err = error(ps, "Unterminated '%c'", c->what);
                csptr--;
        }
        return err < 0 ? err : ps->sptr;
}

#if SP_ERROR != 4
int sp_error(struct parse_state *ps, char *fname, int line, char *str)
{
        if (!SP_ERROR)
                return 0;
        if (fname)
                fprintf(stderr, "%s:%i: ", fname, line);
        else
                fprintf(stderr, "line %i: ", line);
        fprintf(stderr, "%s\n", str);
        if (SP_ERROR == 3)
                exit(1);
        return SP_ERROR == 2 ? -1 : 0;
}
#endif
