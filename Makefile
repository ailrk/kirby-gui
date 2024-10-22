CC=gcc
CFILES=kirby.c expect.c

hmgui:
	$(CC) main.c $(CFILES) -o $@

install:
	mkdir -p $$out/bin
	install -m 755 hmgui $$out/bin/hmgui

clean:
	rm -rf hmgui
