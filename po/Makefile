XGETTEXT = xgettext --keyword=_

all: C.po cs.mo de.mo hu.mo ja.mo sv.mo pl.mo
	touch build_stamp

C.po:
	$(XGETTEXT) -o $@ ../*.[ch]

%.mo: %.po
	msgfmt -o $@ $<

clean:
	-rm -f C.po build_stamp
	-rm -f *.mo

