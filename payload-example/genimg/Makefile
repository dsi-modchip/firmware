
default: all

PYTHON3 ?= python3

-include config.mk

EXTRA_GENIMG_ARGS ?=

CFLAGS = -O2

PL7DIR := ../arm7/
PL7 := $(PL7DIR)/bin/payload7.elf

PL9DIR := ../arm9/
PL9 := $(PL9DIR)/bin/payload9.elf

all: test.bin

lzss: lzss.c

$(PL7):
	$(MAKE) -C $(PL7DIR)

$(PL9):
	$(MAKE) -C $(PL9DIR)

test.bin: genimg.py lzss Makefile config.mk $(PL7) $(PL9)
	$(PYTHON3) ./genimg.py $(EXTRA_GENIMG_ARGS) -o "$@" $(PL7) $(PL9)

clean:
	@$(RM) -v lzss

distclean: clean
	@$(RM) -v test.bin

.PHONY: all clean distclean $(PL7) $(PL9)

