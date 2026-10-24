CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
CPPFLAGS += $(shell pkg-config --cflags wayland-client libpng)
LDLIBS += $(shell pkg-config --libs wayland-client libpng)
PREFIX ?= /usr/local

.PHONY: all clean install uninstall
all: wlcrosshair wlcrosshairctl

wlcrosshair: main.c daemon/daemon.c cli/cli.c vendor/utils.h vendor/tomlc17.c vendor/tomlc17.h $(wildcard protocols/generated/*-client-protocol.[ch])
	$(CC) -std=gnu23 $(CPPFLAGS) $(CFLAGS) -o $@ main.c $(LDFLAGS) $(LDLIBS)

wlcrosshairctl: wlcrosshair
	ln -sf wlcrosshair $@

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 wlcrosshair $(DESTDIR)$(PREFIX)/bin/wlcrosshair
	ln -sf wlcrosshair $(DESTDIR)$(PREFIX)/bin/wlcrosshairctl

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/wlcrosshair $(DESTDIR)$(PREFIX)/bin/wlcrosshairctl

clean:
	rm -f wlcrosshair wlcrosshairctl
