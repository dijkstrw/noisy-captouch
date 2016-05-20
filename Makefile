BINARY=captouch
OBJS=uart.o xprint.o

ARCH_FLAGS=-mmcu=msp430g2452
#ARCH_FLAGS=-mmcu=msp430g2553
MSPDEBUG_DRIVER=rf2500
include msp430.rules.mk

size: $(BINARY).elf
	size $<
