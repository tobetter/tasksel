DOMAIN = debian-tasks
MOS:=$(addsuffix .mo, $(LANGS))

all: C.po $(MOS)
	touch build_stamp

C.po:
	sed -e 's/^Description:/_&/' ../../$(DOMAIN).desc > $(DOMAIN).desc && PODEBCONF_HEADER=/dev/null debconf-updatepo --podir=. && mv templates.pot $@

%.mo: %.po
	msgfmt -o $@ $<

clean:
	-rm -f C.po build_stamp
	-rm -f $(DOMAIN).desc
	-rm -f *.mo

update: $(addprefix update-, $(LANGS))

update-%: %.po C.po
	mv $< $<.bak
	msgmerge $<.bak C.po > $<
