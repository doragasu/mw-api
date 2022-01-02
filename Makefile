# Makefile for the 1985[ALT] Channel
# Written by Jesus Alonso (doragasu)
TARGET  = hello-wifi
PREFIX ?= m68k-elf-
ifdef DEBUG
	CFLAGS  = -Og -g -Wall -Wextra -m68000 -ffast-math -ffunction-sections
else
	CFLAGS  = -Os -Wall -Wextra -m68000 -fomit-frame-pointer -ffast-math -ffunction-sections -flto -ffat-lto-objects
endif
AFLAGS  = --register-prefix-optional -m68000
LFLAGS  = -T $(LFILE) -Wl,-gc-sections
LFILE   = mdbasic.ld
CC      = gcc
AS      = as
LD      = ld
GDB     = cgdb -d $(PREFIX)gdb --
OBJCOPY = objcopy
MDMA   ?= $(HOME)/src/github/mw-mdma-cli/mdma
WFLASH ?= $(HOME)/src/github/mw-wf-cli/src/wflash
DLM_CL ?= $(HOME)/src/github/mw-ch-fw/src/dlm_cli/dlm_cli -s 192.168.10.224
EMU    ?= blastem
OBJDIR  = tmp

# List of directories with sources, excluding the boot stuff
DIRS=. mw
OBJDIRS = $(foreach DIR, $(DIRS), $(OBJDIR)/$(DIR))
CSRCS = $(foreach DIR, $(DIRS), $(wildcard $(DIR)/*.c))

COBJECTS := $(patsubst %.c,$(OBJDIR)/%.o,$(CSRCS))
ASRCS = $(foreach DIR, $(DIRS), $(wildcard *.s))
AOBJECTS := $(patsubst %.s,$(OBJDIR)/%.o,$(ASRCS)) 

all: $(TARGET)

.PHONY: dlm
dlm:
	$(DLM_CL) -f $(TARGET)

.PHONY: wflash
wflash: $(TARGET)
	$(WFLASH) -Aef $(TARGET)

.PHONY: cart
cart: $(TARGET)
	@$(MDMA) -Vaf $<

.PHONY: emu debug
emu: $(TARGET).bin
	rm -f $(HOME)/.local/share/blastem/$(TARGET)/save.nor
	@$(EMU) $< 2>/dev/null &

debug: $(TARGET).bin
	rm -f $(HOME)/.local/share/blastem/$(TARGET)/save.nor
	$(GDB) $(TARGET).elf -x gdbinit

$(TARGET): $(TARGET).bin
	dd if=$< of=$@ bs=8k conv=sync

$(TARGET).bin: $(TARGET).elf
	$(PREFIX)$(OBJCOPY) -O binary $< $@

$(TARGET).elf: boot/boot.o $(AOBJECTS) $(COBJECTS)
	$(PREFIX)$(CC) -o $(TARGET).elf boot/boot.o $(AOBJECTS) $(COBJECTS) $(CFLAGS) $(LFLAGS) -Wl,-Map=$(OBJDIR)/$(TARGET).map -lgcc

boot/boot.o: boot/rom_head.bin boot/sega.s
	$(PREFIX)$(AS) $(AFLAGS) boot/sega.s -o boot/boot.o

boot/rom_head.bin: boot/rom_head.o
	$(PREFIX)$(OBJCOPY) -O binary $< $@

boot/rom_head.o: boot/rom_head.c
	$(PREFIX)$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/%.o: %.c | $(OBJDIRS)
	$(PREFIX)$(CC) -c -MMD -MP $(CFLAGS) $< -o $@

$(OBJDIR)/%.o: %.s | $(OBJDIRS)
	$(PREFIX)$(AS) $(AFLAGS) $< -o $@

$(OBJDIRS):
	mkdir -p $@

.PHONY: clean
clean:
	@rm -rf $(OBJDIR) boot/rom_head.bin boot/rom_head.o boot/boot.o $(TARGET).elf $(TARGET).bin

.PHONY: mrproper
mrproper: | clean
	@rm -f $(TARGET).bin $(TARGET) head.bin tail.bin

# Include auto-generated dependencies
-include $(patsubst %.c,$(OBJDIR)/%.d,$(CSRCS))

