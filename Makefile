CFLAGS 	= -std=c17
CFLAGS 	+= -no-pie
CFLAGS	+= -ftrapv
CFLAGS 	+= -Wall
CFLAGS 	+= -Wextra
CFLAGS 	+= -Warray-bounds
CFLAGS 	+= -Wconversion
CFLAGS 	+= -Wmissing-braces
CFLAGS 	+= -Wno-parentheses
CFLAGS 	+= -Wno-format-truncation
CFLAGS	+= -Wno-type-limits
CFLAGS 	+= -Wpedantic
CFLAGS 	+= -Wstrict-prototypes
CFLAGS 	+= -Wwrite-strings
CFLAGS 	+= -Winline
CFLAGS 	+= -D_FORTIFY_SOURCE=2

BIN 		 := trie
INSTALL_PATH := /usr/local/bin
SRCS		 := trie.c 

ifeq ($(MAKECMDGOALS),debug)
SRCS += size.c
endif

all: CFLAGS += -s -O2
all: $(BIN)

debug: CFLAGS += -g3 -ggdb -fanalyzer -DDEBUG
debug: $(BIN)

$(BIN): $(SRCS)

install: $(BIN)
	install $< $(INSTALL_PATH)

uninstall:
	rm $(INSTALL_PATH)/$(BIN)

clean:
	$(RM) $(BIN) 

.PHONY: all debug clean install uninstall 
.DELETE_ON_ERROR:
