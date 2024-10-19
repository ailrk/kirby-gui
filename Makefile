CC=gcc

hmgui:
	$(CC) main.c -o $@

install:
	mkdir -p $$out/bin && \
	mv hmgui  $$out/bin/hmgui
