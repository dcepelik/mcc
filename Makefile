# 
#  Makefile for Minimalist C Compiler
#  Copyright 2017 David Čepelík <d@dcepelik.cz>
# 

.SILENT:
.PHONY: dbg opt all clean

SRC_DIR = src
INC_DIR = $(SRC_DIR)/include

BUILD_DIR = build
DEPS_DIR = $(BUILD_DIR)/deps
DBG_DIR = $(BUILD_DIR)/dbg
OPT_DIR = $(BUILD_DIR)/opt

BINS = mcc mcpp
SRCS = array.c ast.c cexpr.c common.c context.c cpp.c cpp-directives.c \
	cpp-files.c cpp-macros.c debug.c errlist.c error.c hashtab.c inbuf.c \
	keyword.c lexer.c list.c mcc.c mcpp.c mempool.c objpool.c \
	operator.c parse.c parse-decl.c parse-expr.c print.c strbuf.c symbol.c \
	token.c toklist.c utf8.c

MAINS = $(patsubst %, %.c, $(BINS))

DEPS = $(addprefix $(DEPS_DIR)/, $(patsubst %.c, %.d, $(SRCS)))

DBG_BINS = $(addprefix $(DBG_DIR)/, $(BINS))
OPT_BINS = $(addprefix $(OPT_DIR)/, $(BINS))

DBG_OBJS = $(addprefix $(DBG_DIR)/, $(patsubst %.c, %.o, $(filter-out $(MAINS), $(SRCS))))
OPT_OBJS = $(addprefix $(OPT_DIR)/, $(patsubst %.c, %.o, $(filter-out $(MAINS), $(SRCS))))

CFLAGS += -c -std=gnu11 \
	-Wall -Wextra -Werror --pedantic -Wno-unused-function \
		-Wno-gnu-statement-expression -Wimplicit-fallthrough=2 \
	-I $(INC_DIR)

DBG_CFLAGS += $(CFLAGS) -g
OPT_CFLAGS += $(CFLAGS) -O

LDFLAGS += -Wall

DBG_LDFLAGS += $(LDFLAGS)
OPT_LDFLAGS += $(LDFLAGS)

$(DEPS_DIR)/%.d: $(SRC_DIR)/%.c Makefile
	echo DEPS $@
	$(CC) $(DBG_CFLAGS) -MM -MT $(DBG_DIR)/$*.o -o $@ $<
	sed 's_:_ $@:_' $@ > $@.tmp
	cp $@.tmp $@
	sed -e 's/ *\\$$//g' -e 's/ \+/ /g' -e 's/^[^:]*: //' -e 's/$$/:/' < $@.tmp >> $@
	rm $@.tmp

$(DBG_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS_DIR)/%.d Makefile
	echo "CC   $@"
	$(CC) $(DBG_CFLAGS) -o $@ $<

$(OPT_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS_DIR)/%.d Makefile
	echo "CC   $@"
	$(CC) $(OPT_CFLAGS) -o $@ $<

dbg: $(DBG_BINS)
opt: $(OPT_BINS)
all: dbg opt

clean:
	rm -f -- $(DEPS_DIR)/*.d $(DBG_DIR)/*.o $(DBG_BINS) $(OPT_DIR)/*.o $(OPT_BINS)

$(DBG_BINS): $(DBG_DIR)/%: $(DBG_OBJS) $(DBG_DIR)/%.o
	echo LINK $@
	$(CC) $(DBG_LDFLAGS) -o $@ $^

$(OPT_BINS): $(OPT_DIR)/%: $(OPT_OBJS) $(OPT_DIR)/%.o
	echo LINK $@
	$(CC) $(OPT_LDFLAGS) -o $@ $^

include $(DEPS)
