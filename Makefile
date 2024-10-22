CC=gcc
CFILES=kirby.c expect.c arena.c alloc.c

kbgui:
	$(CC) main.c $(CFILES) -o $@

install:
	mkdir -p $$out/bin
	install -m 755 kbgui $$out/bin/kbgui

clean:
	rm -rf kbgui

-include .local.mk
