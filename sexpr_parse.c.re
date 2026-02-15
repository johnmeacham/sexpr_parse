#include "sexpr_parse.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


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
        ctop->unary = control != '(' && control != '.';
        ctop->depth = ps->sptr;
}

static int
close_enclosed(struct parse_state *ps, char control)
{
        struct centry *ctop = ps->control_stack + ps->csptr - 1;
        bool is_cons = false;
        sp_cell_t cdr = 0;
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
                sp_cell_t v = is_cons
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
                [^]             { int r = sp_error(ps, ps->fname, ps->lineno, *t); if (r < 0) return r; }
                */
        }
        return res;
}

#if SP_ERROR != 3
int sp_error(struct parse_state *ps, char *fname, int line, char unknown)
{
				if (!SP_ERROR)
								return 0;
				if (fname)
								printf("%s:%i: ", fname, line);
				else 
								printf("line %i: ", line);
				if (isprint(unknown))
								printf("unknown character '%c'\n", unknown);
				else
								printf("unknown character '\\x%02x'\n", unknown);
				return SP_ERROR == 2 ? -1 : 0;
}
#endif

