all: test_sexpr_parse

CFLAGS= -g

test_sexpr_parse: test_sexpr_parse.c sexpr_parse.c sexpr_parse.h

%.c: %.c.re
	re2c -W $< -o $@
.PRECIOUS: sexpr_parse.c

.PHONY: test clean

test: test_sexpr_parse
	./test_sexpr_parse <tests/test.scm
	./test_sexpr_parse <tests/bad.txt


clean:
	rm -f test_sexpr_parse
