XGETTEXT = xgettext --keyword=_
MOS:=$(addsuffix .mo, $(LANGS))

all: C.po $(MOS)
	touch build_stamp

C.po:
	$(XGETTEXT) -o $@ ../*.[ch]

%.mo: %.po
	msgfmt -o $@ $<

clean:
	-rm -f C.po build_stamp
	-rm -f *.mo

