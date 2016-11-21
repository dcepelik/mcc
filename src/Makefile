.PHONY: clean

OBJS=mcc.o lexer.o inbuf.o symtab.o objpool.o cpp.o error.o tokinfo.o strbuf.o hashtab.o mempool.o
PROG=mcc
CFLAGS+=-c -std=gnu11 -Wall --pedantic -ggdb3 -DDEBUG
LDFLAGS=-Wall


$(PROG): $(OBJS)
	$(CC) $(LDFALGS) -o $@ $^


lexer.o: lexer.c lexer.h inbuf.o symtab.o error.o tokinfo.o strbuf.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

symtab.o: symtab.c symtab.h objpool.o error.o tokinfo.o hashtab.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

cpp.o: cpp.c cpp.h lexer.o error.o tokinfo.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

objpool.o: objpool.c objpool.h debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

mempool.o: mempool.c mempool.h strbuf.o debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c %.h debug.h common.h
	$(CC) $(CFLAGS) -o $@ $<


clean:
	rm -f -- $(PROG) $(OBJS)
