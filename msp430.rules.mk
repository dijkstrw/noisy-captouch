ifneq ($(V),1)
Q		:= @
NULL		:= 2>/dev/null
endif

PREFIX		?= msp430
CC		:= $(PREFIX)-gcc
OBJCOPY		:= $(PREFIX)-objcopy
OBJDUMP		:= $(PREFIX)-objdump
SIZE		:= $(PREFIX)-size
MSPDEBUG        := mspdebug
MSPDEBUG_DRIVER ?= rf2500

OBJS		+= $(BINARY).o
CFLAGS 		=-Os -g -Wall

.SUFFIXES: .elf .list .map
.SECONDEXPANSION:
.SECONDARY:

all: elf list size

elf: $(BINARY).elf
list: $(BINARY).list
flash: $(BINARY).flash
size: $(BINARY).size

%.o: %.c
	@printf "  CC      $(*).c\n"
	$(Q)$(CC) $(CFLAGS) $(ARCH_FLAGS) -o $(*).o -c $(*).c

%.elf %.map: $(OBJS)
	@printf "  LD      $(*).elf\n"
	$(Q)$(CC) $(CFLAGS) $(ARCH_FLAGS) -Wl,-Map,$(*).map -o $(*).elf $(OBJS)

%.list: %.elf
	@printf "  OBJDUMP $(*).list\n"
	$(Q)$(OBJDUMP) -S $(*).elf > $(*).list

%.flash: %.elf
	@printf "  FLASH   $<\n"
	$(Q)$(MSPDEBUG) $(MSPDEBUG_DRIVER) "prog $(*).elf"

%.size: %.elf
	@printf "  SIZE   $<\n"
	$(Q)$(SIZE) $(*).elf

clean:
	@printf "  CLEAN\n"
	$(Q)$(RM) *.o *.elf *.list *.map


.PHONY: clean elf list flash size
