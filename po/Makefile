XGETTEXT = xgettext --keyword=_ --keyword=N_ --add-comments=TRANS:
POTFILE = tasksel.pot
MOS:=$(addsuffix .mo, $(LANGS))

all: $(POTFILE) $(MOS)
	touch build_stamp

$(POTFILE):
	$(XGETTEXT) -o $@ ../*.[ch]

%.mo: %.po
	msgfmt -o $@ $<

clean:
	-rm -f $(POTFILE) build_stamp
	-rm -f *.mo

update: $(addprefix update-, $(LANGS))

update-%: %.po $(POTFILE)
	mv $< $<.bak
	msgmerge $<.bak $(POTFILE) > $<

