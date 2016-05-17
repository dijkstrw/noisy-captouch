CC=msp430-gcc
CFLAGS  =-Os -Wall -mmcu=msp430g2553

OBJ = $(patsubst %.c,.obj/%.o,$(wildcard *.c)) 
OUT = captouch.elf

default: dirs $(OUT)

dirs: dot_obj
dot_obj:
	if [ ! -e .obj ]; then mkdir .obj; fi;

# The following are for the Master
.obj/%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(OUT): $(OBJ)
	$(CC)  $(CFLAGS) -o $(OUT) $(OBJ)

clean:
	rm -f $(OBJ) $(OUT)


flash:
	mspdebug rf2500 "prog $(OUT)"
