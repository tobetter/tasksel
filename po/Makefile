XGETTEXT = xgettext --keyword=_ --keyword=N_ --add-comments=TRANS:
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

update: $(addprefix update-, $(LANGS))

update-%: %.po C.po
	mv $< $<.bak
	msgmerge $<.bak C.po > $<

