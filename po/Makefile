XGETTEXT = xgettext --keyword=_

%.po:
	$(XGETTEXT) ../*.c

all: messages.po
	touch build_stamp

clean:
	-rm -f messages.po build_stamp

