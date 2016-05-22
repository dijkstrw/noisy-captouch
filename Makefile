BINARY=captouch
OBJS=uart.o xprint.o

ARCH_FLAGS=-mmcu=msp430g2452
#ARCH_FLAGS=-mmcu=msp430g2553
include msp430.rules.mk
