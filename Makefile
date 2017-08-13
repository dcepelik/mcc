.PHONY: all clean

BUILD_DIR = build
SRC_DIR = src
OBJS_DIR = $(BUILD_DIR)/objs
DEPS_DIR = $(BUILD_DIR)/deps
INC_DIRS = $(SRC_DIR)/include

BINS = $(BUILD_DIR)/mcc $(BUILD_DIR)/mcpp
SRCS = array.c \
	ast.c \
	common.c \
	context.c \
	cpp.c \
	cpp-directives.c \
	cpp-files.c \
	cpp-macros.c \
	debug.c \
	errlist.c \
	error.c \
	hashtab.c \
	inbuf.c \
	keyword.c \
	lexer.c \
	list.c \
	mcc.c \
	mcpp.c \
	mempool.c \
	objpool.c \
	operator.c \
	parse.c \
	parse-decl.c \
	parse-expr.c \
	print.c \
	strbuf.c \
	symbol.c \
	token.c \
	toklist.c \
	utf8.c
MAINS = $(OBJS_DIR)/mcc.o $(OBJS_DIR)/mcpp.o
OBJS = $(filter-out $(MAINS), $(addprefix $(OBJS_DIR)/, $(patsubst %.c, %.o, $(SRCS))))
DEPS = $(addprefix $(DEPS_DIR)/, $(patsubst %.c, %.d, $(SRCS)))

CFLAGS += -c -std=gnu11 \
	-I $(INC_DIRS) \
	-Wall -Wextra -Werror --pedantic -Wno-unused-function \
	-Wimplicit-fallthrough=1 \
	-ggdb3 -DDEBUG
LDFLAGS += -Wall

$(OBJS_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS_DIR)/%.d Makefile
	$(CC) $(CFLAGS) -o $@ $<

$(DEPS_DIR)/%.d: $(SRC_DIR)/%.c Makefile
	$(CC) $(CFLAGS) -MM -MT $(OBJS_DIR)/$*.o -o $@ $<; sed 's_:_ $@:_' $@ > $@.tmp; mv $@.tmp $@

all: $(BINS)

clean:
	rm -f -- $(OBJS_DIR)/*.o $(DEPS_DIR)/*.d $(BINS)

$(BUILD_DIR)/mcc: $(OBJS) $(OBJS_DIR)/mcc.o
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/mcpp: $(OBJS) $(OBJS_DIR)/mcpp.o
	$(CC) $(LDFLAGS) -o $@ $^

include $(DEPS)
