CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
CPPFLAGS += $(shell pkg-config --cflags wayland-client libpng)
LDLIBS += $(shell pkg-config --libs wayland-client libpng)
PREFIX ?= /usr/local

.PHONY: all clean install install-bin install-docs uninstall
all: wlcrosshair wlcrosshairctl

wlcrosshair: main.c daemon/daemon.c cli/cli.c vendor/utils.h $(wildcard protocols/generated/*-client-protocol.[ch])
	$(CC) -std=gnu23 $(CPPFLAGS) $(CFLAGS) -o $@ main.c $(LDFLAGS) $(LDLIBS)

wlcrosshairctl: wlcrosshair
	ln -sf wlcrosshair $@

install: install-bin install-docs

install-bin: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 wlcrosshair $(DESTDIR)$(PREFIX)/bin/wlcrosshair
	ln -sf wlcrosshair $(DESTDIR)$(PREFIX)/bin/wlcrosshairctl

install-docs:
	install -Dm644 doc/wlcrosshair.1 $(DESTDIR)$(PREFIX)/share/man/man1/wlcrosshair.1
	install -Dm644 doc/wlcrosshairctl.1 $(DESTDIR)$(PREFIX)/share/man/man1/wlcrosshairctl.1
	install -Dm644 completions/wlcrosshairctl.bash $(DESTDIR)$(PREFIX)/share/bash-completion/completions/wlcrosshairctl
	install -Dm644 completions/wlcrosshairctl.zsh $(DESTDIR)$(PREFIX)/share/zsh/site-functions/_wlcrosshairctl
	install -Dm644 completions/wlcrosshairctl.fish $(DESTDIR)$(PREFIX)/share/fish/vendor_completions.d/wlcrosshairctl.fish

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/wlcrosshair $(DESTDIR)$(PREFIX)/bin/wlcrosshairctl \
		$(DESTDIR)$(PREFIX)/share/man/man1/wlcrosshair.1 $(DESTDIR)$(PREFIX)/share/man/man1/wlcrosshairctl.1 \
		$(DESTDIR)$(PREFIX)/share/bash-completion/completions/wlcrosshairctl \
		$(DESTDIR)$(PREFIX)/share/zsh/site-functions/_wlcrosshairctl \
		$(DESTDIR)$(PREFIX)/share/fish/vendor_completions.d/wlcrosshairctl.fish

clean:
	rm -f wlcrosshair wlcrosshairctl
