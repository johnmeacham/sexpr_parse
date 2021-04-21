# sexpr_parse
Simple and fast  standalone c parser for s-expressions. 

This is a one file parser for lisp and scheme style s-expressions, it is fully
standalone and does not impose any particular representation on what you do with
the parsed expressions other than you can represent them in a uintptr_t (which
can hold a void * or a number)

In order to use it you should implement these functions to convert the raw atoms
to your representation, the characters pointed to will be inside the buffer you
passed to scan:

```c
uintptr_t sp_symbol(struct parse_state *ps, char *s, char *e);
uintptr_t sp_string(struct parse_state *ps, char *s, char *e);
uintptr_t sp_number(struct parse_state *ps, char *s, char *e, int radix);
```
        
and implement these functions to combine atoms into values:


```c
/* apply unary operator such as quote, quasi-quote, vector etc. */
uintptr_t sp_unary(struct parse_state *ps, char unop, uintptr_t v);

/* create a list from an array of values. delim will always be '(' for
plain lists  */
uintptr_t sp_list(struct parse_state *nonce, char delim, uintptr_t *start, int len);

/* explicit cons operator (a b c . d). passed an array of the list part
as well as the cdr */
uintptr_t sp_cons(struct parse_state *nonce, char delim, uintptr_t *start, int len, uintptr_t cdr);
```

then in order to run the parser pass a null terminated string into scan.
```c
char *data = "(a b c . d)";
struct parse_state ps = PARSE_STATE_INITIALIZER;
scan(&ps, data);
```
no heap memory is allocated so nothing needs to be freed except what you
allocate yourself in the sp_ routines. If you set the 'fname' field of
parse_state it will print that as the filename when reporting lexing errors,
nonce is a field for user data and is not used by the lexer.


## building

The single file sexpr_parse.c and the corresponding header files can just be
linked into your project, there are no dependencies. if you wish to regenerate
the lexer after modifying it then you need re2c to compile the c file.

