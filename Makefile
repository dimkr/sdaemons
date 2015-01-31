CC ?= cc
CFLAGS ?= -O2
LDFLAGS ?=
DESTDIR ?= /

PREFIX ?=
BIN_DIR ?= $(PREFIX)/bin
MAN_DIR ?= $(PREFIX)/man
DOC_DIR ?= $(PREFIX)/doc

CFLAGS += -Wall -pedantic -D_GNU_SOURCE

SRCS = $(wildcard *.c)
OBJECTS = $(SRCS:.c=.o)
PROGS = $(SRCS:.c=)

all: $(PROGS)

syslog.o: syslog.c
	$(CC) -c -o $@ $< -pthread $(CFLAGS)

syslog: syslog.o
	$(CC) -o $@ $^ -pthread $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%: %.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(PROGS) $(OBJECTS)

install: $(PROGS)
	for prog in $^; \
	do \
		install -D -m 755 $$prog $(DESTDIR)$(BIN_DIR)/$$prog; \
		install -D -m 644 $$prog.8 $(DESTDIR)$(MAN_DIR)/man8/$$prog.8; \
	done
	install -d -m 755 $(DESTDIR)/run
	install -D -m 644 README $(DESTDIR)$(DOC_DIR)/README
	install -m 644 AUTHORS $(DESTDIR)$(DOC_DIR)/AUTHORS
	install -m 644 COPYING $(DESTDIR)$(DOC_DIR)/COPYING
