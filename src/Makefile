.PHONY: clean gdb tests all

OBJS=mcc.o lexer.o inbuf.o symbol.o objpool.o cpp.o error.o token.o strbuf.o \
	hashtab.o mempool.o array.o debug.o list.o macro.o errlist.o print.o \
	context.o
PROG=mcc
CFLAGS+=-c -std=gnu11 -Wall -Werror -Wno-unused-function --pedantic -ggdb3 -DDEBUG
LDFLAGS=-Wall

$(PROG): $(OBJS)
	$(CC) $(LDFALGS) -o $@ $^


array.o: array.c array.h debug.h
	$(CC) $(CFLAGS) -o $@ $<

lexer.o: lexer.c lexer.h inbuf.o symbol.o error.o token.o strbuf.o cpp.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

symbol.o: symbol.c symbol.h objpool.o error.o token.o hashtab.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

cpp.o: cpp.c cpp.h lexer.o error.o token.o list.o macro.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

objpool.o: objpool.c objpool.h debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

mempool.o: mempool.c mempool.h strbuf.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c %.h debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<


clean:
	rm -f -- $(PROG) $(OBJS)

gdb: $(PROG)
	gdb --args $(PROG) ../tests/pp/ifs.c

tests: $(PROG)
	cd ../tests; ./run-tests.sh
