XGETTEXT = xgettext --keyword=_

%.po:
	$(XGETTEXT) ../*.[ch]

all: messages.po
	touch build_stamp

clean:
	-rm -f messages.po build_stamp

