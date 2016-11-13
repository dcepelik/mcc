.PHONY: clean

OBJS=mcc.o lexer.o inbuf.o symtab.o mempool.o pp.o error.o
PROG=mcc
CFLAGS+=-c -std=gnu11 -Wall --pedantic -DDEBUG -ggdb3
LDFLAGS=-Wall


$(PROG): $(OBJS)
	$(CC) $(LDFALGS) -o $@ $^


lexer.o: lexer.c lexer.h inbuf.o symtab.o error.o
	$(CC) $(CFLAGS) -o $@ $<

symtab.o: symtab.c symtab.h mempool.o error.o
	$(CC) $(CFLAGS) -o $@ $<

pp.o: pp.c lexer.o error.o
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ $<


clean:
	rm -f -- $(PROG) $(OBJS)
