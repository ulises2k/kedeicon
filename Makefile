CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra
PREFIX  ?= /usr/local

kedeicon: kedeicon.c font.h
	$(CC) $(CFLAGS) -o kedeicon kedeicon.c

# Regenerate the embedded font from a real system console font.
font.h:
	python3 genfont.py > font.h

install: kedeicon
	install -m 0755 kedeicon $(PREFIX)/bin/kedeicon
	install -m 0644 kedeicon.service /etc/systemd/system/kedeicon.service
	systemctl daemon-reload

clean:
	rm -f kedeicon

.PHONY: install clean
