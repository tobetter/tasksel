PROGRAM=tasksel
VERSION=\"1.0\"
CC=gcc
CFLAGS=-g -Wall  #-Os
DEFS=-DVERSION=$(VERSION) -DPACKAGE=\"$(PROGRAM)\" -DLOCALEDIR=\"/usr/share/locale\" #-DDEBUG
LIBS=-lslang #-lccmalloc -ldl
OBJS=tasksel.o slangui.o data.o util.o strutl.o
LANGS=de hu sv pl
LOCALEDIR=$(DESTDIR)/usr/share/locale

COMPILE = $(CC) $(CFLAGS) $(DEFS) -c
LINK = $(CC) $(CFLAGS) $(DEFS) -o

all: $(PROGRAM)

%.o: %.c
	$(COMPILE) $<

po/build_stamp:
	$(MAKE) -C po 

$(PROGRAM): $(OBJS) po/build_stamp
	$(LINK) $(PROGRAM) $(OBJS) $(LIBS)

install:
	install -m 755 tasksel $(DESTDIR)/usr/bin
	pod2man --center "Debian specific manpage" tasksel.pod | gzip -9c > $(DESTDIR)/usr/share/man/man8/tasksel.8.gz
	for lang in $(LANGS); do \
	  [ ! -d $(LOCALEDIR)/$$lang/LC_MESSAGES/ ] && mkdir -p $(LOCALEDIR)/$$lang/LC_MESSAGES/; \
	  install -m 644 po/$$lang.mo $(LOCALEDIR)/$$lang/LC_MESSAGES/$(PROGRAM).mo; \
	done

test:
	$(MAKE) -C scratch

clean:
	rm -f $(PROGRAM) *.o *~
	$(MAKE) -C po clean

