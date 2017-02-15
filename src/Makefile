.PHONY: all clean metrics mcc-tests mcpp-tests tests

BINS=mcc mcpp
OBJS=\
	array.o \
	ast.o \
	common.o \
	context.o \
	cpp-directives.o \
	cpp-files.o \
	cpp-macros.o \
	cpp.o \
	debug.o \
	errlist.o \
	error.o \
	hashtab.o \
	inbuf.o \
	lexer.o \
	list.o \
	mempool.o \
	objpool.o \
	parser.o \
	print.o \
	strbuf.o \
	symbol.o \
	token.o \
	toklist.o \
	utf8.o

CFLAGS+=-c -std=gnu11 -Wall -Werror -Wno-unused-function -Wno-unused-variable --pedantic -ggdb3 -DDEBUG
LDFLAGS+=-Wall


all: $(BINS)

mcc: mcc-main.o $(OBJS)
	$(CC) $(LDFALGS) -o $@ $^

mcpp: mcpp-main.o $(OBJS)
	$(CC) $(LDFALGS) -o $@ $^


lexer.o: lexer.c lexer.h cpp.o error.o inbuf.o strbuf.o symbol.o token.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

symbol.o: symbol.c symbol.h error.o hashtab.o objpool.o token.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

cpp.o: cpp.c cpp.h cpp-directives.o cpp-files.o cpp-macros.o error.o lexer.o list.o token.o toklist.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

parser.o: parser.c parser.h common.h debug.h ast.h
	$(CC) $(CFLAGS) -o $@ $<

mempool.o: mempool.c mempool.h strbuf.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c %.h debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<


clean:
	rm -f -- $(BINS) *.o

tests: mcc-tests mcpp-tests

mcc-tests: mcc	
	cd ../tests/; ./run-tests.sh mcc

mcpp-tests: mcpp
	cd ../tests/; ./run-tests.sh mcpp

metrics:
	find . -name "*.c" -or -name "*.h" | xargs cat | wc -l
