CFLAGS =$(shell pkg-config --cflags gtk+-3.0)
CFLAGS +=-I/usr/include/vte-2.90/ -std=c99 -O3
LDFLAG =$(shell pkg-config --libs gtk+-3.0)
LDFLAG +=-lvte2_90 -lpthread

all: port.o window.o hex.o stm32.o
	gcc -o stm window.o  port.o hex.o stm32.o  $(LDFLAG)

port.o:port.c port.h
	gcc -o port.o -c port.c	

hex.o:hex.c hex.h
	gcc -o hex.o -c hex.c $(CFLAGS)

window.o:window.c window.h
	gcc -o window.o -c window.c $(CFLAGS)

stm32.o:stm32.c stm32.h
	gcc -o stm32.o -c stm32.c

clean:
	@rm -rf ./*.o
	@rm -rf ./stm
