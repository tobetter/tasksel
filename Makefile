PROGRAM=tasksel
TASKDESC=debian-tasks.desc
DESCDIR=tasks/
CC=gcc
CFLAGS=-g -Wall  #-Os
DEBUG=1
ifeq (0,$(DEBUG))
DEFS=-DVERSION=\"$(VERSION)\" -DPACKAGE=\"$(PROGRAM)\" -DLOCALEDIR=\"/usr/share/locale\" \
     -DTASKDESC=\"/usr/share/tasksel/$(TASKDESC)\"
else
DEFS=-DVERSION=\"$(VERSION)\" -DPACKAGE=\"$(PROGRAM)\" -DLOCALEDIR=\"/usr/share/locale\" \
     -DTASKDESC=\"$(TASKDESC)\" -DDEBUG
endif
VERSION=$(shell expr "`dpkg-parsechangelog 2>/dev/null |grep Version:`" : '.*Version: \(.*\)' | cut -d - -f 1)
LIBS=-lslang #-lccmalloc -ldl
OBJS=tasksel.o slangui.o data.o util.o strutl.o
LANGS=cs da de es hu it ja nb_NO pl pt_BR ru sv zh_TW
LOCALEDIR=$(DESTDIR)/usr/share/locale
COMPILE = $(CC) $(CFLAGS) $(DEFS) -c
LINK = $(CC) $(CFLAGS) $(DEFS) -o

all: $(PROGRAM) $(TASKDESC)

$(TASKDESC): makedesc.pl $(DESCDIR)/*
	perl doincludes.pl $(DESCDIR)
	perl makedesc.pl $(DESCDIR) $(TASKDESC)

%.o: %.c
	$(COMPILE) $<

po/build_stamp:
	$(MAKE) -C po LANGS="$(LANGS)"

$(PROGRAM): $(OBJS) po/build_stamp
	$(LINK) $(PROGRAM) $(OBJS) $(LIBS)

install:
	install -m 755 tasksel $(DESTDIR)/usr/bin
	install -m 0644 $(TASKDESC) $(DESTDIR)/usr/share/tasksel
	pod2man --center "Debian specific manpage" --release $(VERSION) tasksel.pod | gzip -9c > $(DESTDIR)/usr/share/man/man8/tasksel.8.gz
	for lang in $(LANGS); do \
	  [ ! -d $(LOCALEDIR)/$$lang/LC_MESSAGES/ ] && mkdir -p $(LOCALEDIR)/$$lang/LC_MESSAGES/; \
	  install -m 644 po/$$lang.mo $(LOCALEDIR)/$$lang/LC_MESSAGES/$(PROGRAM).mo; \
	done

test:
	$(MAKE) -C scratch

clean:
	rm -f $(PROGRAM) $(TASKDESC) *.o *~
	$(MAKE) -C po clean

