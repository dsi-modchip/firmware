
DFU_UTIL ?= dfu-util
FLASHROM ?= flashrom
FLASHROM_ARGS ?=
FLASHROM_PRGM ?=

default: all

all:
	$(MAKE) -C arm7 && $(MAKE) -C arm9 && $(MAKE) -C genimg

clean:
	$(MAKE) -C arm7 clean && $(MAKE) -C arm9 clean && $(MAKE) -C genimg clean

payload256.bin: all
	@cat genimg/test.bin genimg/test.bin > "$@"

dfu: payload256.bin
	$(DFU_UTIL) -a 0 -D "$<"

flash: payload256.bin
	@if [ "x$(FLASHROM_PRGM)" = "x" ]; then >&2 echo "ERROR: FLASHROM_PRGM must be provided."; false; fi
	$(FLASHROM) -p $(FLASHROM_PRGM) $(FLASHROM_ARGS) -w "$<"

.PHONY: all default clean dfu flash

