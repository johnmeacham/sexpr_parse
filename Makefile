all: sexpr_parse

CFLAGS= -DTEST_CASE -g

%.c: %.c.re
	re2c -W $< -o $@
