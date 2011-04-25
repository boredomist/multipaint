CSRC := $(shell find src -name "*.c")
CHDR := $(shell find include -name "*.h")

COBJ := $(CSRC:.c=.o)

CC := clang
CFLAGS := -Wall -Wextra -pedantic -std=c99 -Iinclude/ -Wno-unused-parameter
LNFLAGS := -lm -lSDL -lpthread
EXE := multipaintc

###

all: $(COBJ) $(EXE)

$(EXE): $(COBJ)
	@ echo "  LINK" $(EXE)
	@ $(CC) $(COBJ) $(LNFLAGS) -o $(EXE)

%.o: %.c
	@ echo "  CC" $<
	@ $(CC) $(CFLAGS) -c $< -o $@

debug:
	@ cd plugins; make debug
	@$(MAKE) "CFLAGS=$(CFLAGS) -g -O0"

gcc:
	@$(MAKE) "CC=gcc"

clang:
	@$(MAKE) "CC=clang"

clean:
	rm -f $(COBJ) $(EXE)

.PHONY=all clean debug gcc
