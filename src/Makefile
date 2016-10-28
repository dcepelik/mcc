.phony: clean

OBJS=mcc.o
PROG=mcc
CFLAGS+=-c -std=c11 -Wall --pedantic
LDFLAGS=-Wall

$(PROG): $(OBJS)
	$(CC) $(LDFALGS) -o $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f -- $(PROG) $(OBJS)
