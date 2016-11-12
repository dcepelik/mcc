.phony: clean

OBJS=mcc.o lexer.o inbuf.o symtab.o mempool.o
PROG=mcc
CFLAGS+=-c -std=gnu11 -Wall --pedantic -DDEBUG -ggdb3
LDFLAGS=-Wall


$(PROG): $(OBJS)
	$(CC) $(LDFALGS) -o $@ $^


lexer.o: lexer.c lexer.h inbuf.h symtab.h
	$(CC) $(CFLAGS) -o $@ $<

symtab.o: symtab.c mempool.o
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ $<


clean:
	rm -f -- $(PROG) $(OBJS)
