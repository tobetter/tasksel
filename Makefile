PROGRAM = tasksel
VERSION=\"0.1\"
CC = gcc
CFLAGS = -g -Wall  -Os
DEFS = -DVERSION=$(VERSION) -DPACKAGE=\"$(PROGRAM)\" -DLOCALEDIR=\"/usr/share/locale\" #-DDEBUG
LIBS = -lslang #-lccmalloc -ldl
OBJS = tasksel.o slangui.o data.o util.o strutl.o

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
	mkdir -p $(DESTDIR)/usr/bin
	install tasksel $(DESTDIR)/usr/bin
	mkdir -p $(DESTDIR)/usr/share/man/man8
	pod2man --center "Debian specific manpage" tasksel.pod | gzip -9c > $(DESTDIR)/usr/share/man/man8/tasksel.8.gz

test:
	$(MAKE) -C scratch

clean:
	rm -f $(PROGRAM) *.o *~
	$(MAKE) -C po clean

