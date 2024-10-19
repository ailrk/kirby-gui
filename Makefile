CC=gcc

hmgui:
	$(CC) main.c -o $@

install:
	mkdir -p $$out/bin
	install -m 755 hmgui $$out/bin/hmgui
