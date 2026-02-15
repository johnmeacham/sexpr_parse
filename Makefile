all: test_sexpr_parse

CFLAGS= -g

test_sexpr_parse: test_sexpr_parse.c sexpr_parse.c sexpr_parse.h

%.c: %.c.re
	re2c -W $< -o $@
.PRECIOUS: sexpr_parse.c
