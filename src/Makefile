.phony: clean

OBJS=mcc.o lexer.o buffer.o
PROG=mcc
CFLAGS+=-c -std=c11 -Wall --pedantic -DDEBUG
LDFLAGS=-Wall


$(PROG): $(OBJS)
	$(CC) $(LDFALGS) -o $@ $^


lexer.o: lexer.c lexer.h buffer.h
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ $<


clean:
	rm -f -- $(PROG) $(OBJS)
