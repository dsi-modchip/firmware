
default: all

CC := /opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc
LD := /opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc
AS := /opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc
OBJCOPY := /opt/devkitpro/devkitARM/bin/arm-none-eabi-objcopy
GDB := arm-none-eabi-gdb

SRC := src
INC := inc
OBJ := obj
BIN := bin

TARGET := payload9

SRCFILES_C = $(wildcard $(SRC)/*.c)
SRCFILES_S = $(wildcard $(SRC)/*.S)
SRCFILES_s = $(wildcard $(SRC)/*.s)

OBJFILES = $(patsubst $(SRC)/%,$(OBJ)/%.o,$(SRCFILES_C)) \
           $(patsubst $(SRC)/%,$(OBJ)/%.o,$(SRCFILES_S)) \
           $(patsubst $(SRC)/%,$(OBJ)/%.o,$(SRCFILES_s))

LDFILE := link.ld

CFLAGS := -g -I$(INC)/ -O2 -Wall -Wextra -mcpu=arm946e-s -mtune=arm946e-s -DARM9 \
    -fomit-frame-pointer -ffunction-sections -fdata-sections -mthumb-interwork \
    -Werror=implicit-function-declaration -Werror=return-type -Werror=aggressive-loop-optimizations

#CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS := -g -I$(INC)/ -mthumb-interwork -DARM7

LDFLAGS := -nostartfiles -nostdlib -T$(LDFILE) -g -mthumb-interwork \
    -Wl,--gc-sections -Wl,--cref


all: $(BIN)/$(TARGET).elf $(BIN)/$(TARGET).bin


%/:
	@mkdir -v "$@"

data/logo.c: data/conv.sh data/logo.png
	cd data && ./conv.sh && cd ..

$(OBJ)/%.c.o: $(SRC)/%.c $(OBJ)/ data/logo.c
	$(CC) -o "$@" -c "$<" $(CFLAGS)

$(OBJ)/%.S.o: $(SRC)/%.S $(OBJ)/
	$(AS) -o "$@" -c "$<" $(ASFLAGS)

$(OBJ)/%.s.o: $(SRC)/%.s $(OBJ)/
	$(AS) -o "$@" -c "$<" $(ASFLAGS)

$(BIN)/$(TARGET).elf: $(OBJFILES) $(LDFILE) $(BIN)/
	$(LD) -o "$@" -Wl,-Map="$@.map" $(LDFLAGS) $(OBJFILES)

$(BIN)/%.bin: $(BIN)/%.elf
	$(OBJCOPY) -O binary "$<" "$@"

clean:
	@$(RM) -r $(OBJ)/ $(BIN)/ data/logo.{c,h}

debug:
	$(GDB) -ex 'target remote localhost:3333' -ex 'b main' $(GDBFLAGS) $(BIN)/$(TARGET).elf

.PHONY: all default clean
.PRECIOUS: $(OBJFILES) $(BIN)/$(TARGET).elf $(OBJ)/ $(BIN)/

