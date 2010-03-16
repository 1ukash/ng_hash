sbin_PROGRAMS=load

load_OBJS=load.o mkpeer.o name.o msg.o connect.o

CC=gcc
#CFLAGS=-O2 -pipe -std=c99 -Wall -lnetgraph

CFLAGS=-O -g -pipe -std=c99 -Wall -lnetgraph
CPPFLAGS=-I/usr/local/include

PREFIX=$(HOME)/load

RM=/bin/rm
INSTALL=/usr/bin/install
INSTALL_OPTS=-m 550

all: $(sbin_PROGRAMS)

load: $(load_OBJS)
	$(CC) -o load $(load_OBJS)

c.o.:
	$(CC) $(CFLAGS) -c $<

clean:
	$(RM) -f $(sbin_PROGRAMS)
	$(RM) -f $(load_OBJS)
	$(RM) -f *~

install: all
	$(INSTALL) -d -m 750 $(PREFIX)/sbin
	for i in $(sbin_PROGRAMS); do \
	$(INSTALL) $(INSTALL_OPTS) $$i $(PREFIX)/sbin; \
	done

install-strip: all
	$(INSTALL) -d -m 750 $(PREFIX)/sbin
	for i in $(sbin_PROGRAMS); do \
	$(INSTALL) -s $(INSTALL_OPTS) $$i $(PREFIX)/sbin; \
	done
