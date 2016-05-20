##
## Adapted from the libopencm3 project for msp430.
##
## Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (C) 2010 Piotr Esden-Tempski <piotr@esden.net>
## Copyright (C) 2013 Frantisek Burian <BuFran@seznam.cz>
##
## This library is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this library.  If not, see <http://www.gnu.org/licenses/>.
##

# Be silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q		:= @
NULL		:= 2>/dev/null
endif

###############################################################################
# Executables

PREFIX		?= msp430

CC		:= $(PREFIX)-gcc
OBJCOPY		:= $(PREFIX)-objcopy
OBJDUMP		:= $(PREFIX)-objdump
MSPDEBUG        := mspdebug

###############################################################################
# Source files

OBJS		+= $(BINARY).o


###############################################################################
# C flags

CFLAGS 		=-Os -g -Wall

###############################################################################
###############################################################################

.SUFFIXES: .elf .bin .hex .srec .list .map .images
.SECONDEXPANSION:
.SECONDARY:

all: elf list

elf: $(BINARY).elf
list: $(BINARY).list

flash: $(BINARY).flash

%.list: %.elf
	@printf "  OBJDUMP $(*).list\n"
	$(Q)$(OBJDUMP) -S $(*).elf > $(*).list

%.elf: $(OBJS)
	@printf "  LD      $(*).elf\n"
	$(Q)$(CC) $(CFLAGS) $(ARCH_FLAGS) -o $(*).elf $(OBJS)

%.o: %.c
	@printf "  CC      $(*).c\n"
	$(Q)$(CC) $(CFLAGS) $(ARCH_FLAGS) -o $(*).o -c $(*).c

clean:
	@printf "  CLEAN\n"
	$(Q)$(RM) *.o *.d *.elf *.bin *.hex *.srec *.list *.map

%.flash: %.elf
	@printf "  FLASH   $<\n"
	$(Q)$(MSPDEBUG) $(MSPDEBUG_DRIVER) "prog $(*).elf"

.PHONY: clean elf list

-include $(OBJS:.o=.d)
