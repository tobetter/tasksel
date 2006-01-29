DOMAIN=debian-tasks
TASKDESC=$(DOMAIN).desc
TASKDIR=/usr/share/tasksel
DESCDIR=tasks
DESCPO=$(DESCDIR)/po
VERSION=$(shell expr "`dpkg-parsechangelog 2>/dev/null |grep Version:`" : '.*Version: \(.*\)' | cut -d - -f 1)
LANGS=ar bg bn bs ca cs cy da de el es et eu fi fr gl he hr hu id it ja ko lt mg mk nb nl nn pa_IN pl pt pt_BR ro ru sk sl sq sv tl tr uk vi zh_CN zh_TW
LANGS_DESC=ar bg bn bs ca cs cy da de el es et eu fi fr gl he hr hu id it ja ko lt mg mk nb nl nn pa_IN pl pt pt_BR ro ru sk sl sq sv tl tr uk vi zh_CN zh_TW
LOCALEDIR=$(DESTDIR)/usr/share/locale

all: $(TASKDESC) $(DESCPO)/build_stamp po/build_stamp

$(TASKDESC): makedesc.pl $(DESCDIR)/[a-z]??*
	./doincludes.pl $(DESCDIR)
	./makedesc.pl $(DESCDIR) $(TASKDESC)

%.o: %.c
	$(COMPILE) $<

po/build_stamp:
	$(MAKE) -C po LANGS="$(LANGS)"

updatepo:
	$(MAKE) -C po update LANGS="$(LANGS)"

$(DESCPO)/build_stamp:
	$(MAKE) -C $(DESCPO) LANGS="$(LANGS_DESC)"

updatetaskspo:
	$(MAKE) -C $(DESCPO) update LANGS="$(LANGS_DESC)"

install:
	install -d $(DESTDIR)/usr/bin $(DESTDIR)$(TASKDIR) \
		$(DESTDIR)/usr/lib/tasksel/tests \
		$(DESTDIR)/usr/lib/tasksel/info \
		$(DESTDIR)/usr/lib/tasksel/packages \
		$(DESTDIR)/usr/share/man/man8
	install -m 755 tasksel.pl $(DESTDIR)/usr/bin/tasksel
	install -m 755 tasksel-debconf $(DESTDIR)/usr/lib/tasksel/
	install -m 0644 $(TASKDESC) $(DESTDIR)$(TASKDIR)
	pod2man --section=8 --center "Debian specific manpage" --release $(VERSION) tasksel.pod | gzip -9c > $(DESTDIR)/usr/share/man/man8/tasksel.8.gz
	for lang in $(LANGS); do \
		[ ! -d $(LOCALEDIR)/$$lang/LC_MESSAGES/ ] && mkdir -p $(LOCALEDIR)/$$lang/LC_MESSAGES/; \
		install -m 644 po/$$lang.mo $(LOCALEDIR)/$$lang/LC_MESSAGES/tasksel.mo; \
	done
	for lang in $(LANGS_DESC); do \
		[ ! -d $(LOCALEDIR)/$$lang/LC_MESSAGES/ ] && mkdir -p $(LOCALEDIR)/$$lang/LC_MESSAGES/; \
		install -m 644 tasks/po/$$lang.mo $(LOCALEDIR)/$$lang/LC_MESSAGES/$(DOMAIN).mo; \
	done
	for test in tests/*; do \
		install -m 755 $$test $(DESTDIR)/usr/lib/tasksel/tests/; \
	done
	for script in info/*; do \
		install -m 755 $$script $(DESTDIR)/usr/lib/tasksel/info/; \
	done
	for package in packages/*; do \
		install -m 755 $$package $(DESTDIR)/usr/lib/tasksel/packages/; \
	done

clean:
	rm -f $(TASKDESC) *~
	$(MAKE) -C po clean
	$(MAKE) -C $(DESCPO) clean

# This taget is run to generate the overrides files.
# It is run from a cron job, so should only generate output if there are
# problems.
override:
	@svn up tasks 2>&1 | grep -v ^U | grep -v "At revision" || true
	@./makeoverride.pl $(DESCDIR) > external-overrides-task
