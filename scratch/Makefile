CC = gcc
CFLAGS = -g -Wall -I..
DEFS = -DVERSION=$(VERSION) -DPACKAGE=\"$(PROGRAM)\" -DLOCALEDIR=\"/usr/share/locale\" -DDEBUG
LIBS = -lslang

COMPILE = $(CC) $(CFLAGS) $(DEFS) -c
LINK = $(CC) $(CFLAGS) $(DEFS) -o

all: testdata flowtest

../%.o: ../%.c
	$(COMPILE) $< -o $@

%.o: %.c
	$(COMPILE) $<

testdata: ../data.o ../util.o testdata.o
	$(LINK) testdata $^

flowtest: ../strutl.o ../util.o flowtest.o
	$(LINK) flowtest $^

clean:
	rm -f *.o *~ testdata flowtest

