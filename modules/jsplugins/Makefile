PACKAGE_CONTENTS = jsplugin.h \
		doc/HOWTO.html \
		examples/triggerreceiver.c \
		examples/triggerreceiver.html \
		examples/videoplayer.c \
		examples/videoplayer.html \
		examples/jsplugin_videoplayer.dsp \
		examples/Makefile \
		examples/README.txt

PACKAGE_VERSION = $(shell cut -c2- CVS/Tag)
PACKAGE_DATE = $(shell date +%Y-%m-%d--%H-%M)
PACKAGE_NAME = opera_jsplugins_sdk_$(PACKAGE_VERSION)

package:
	mkdir -p .pack/$(PACKAGE_NAME)
	tar cf .pack/tmpmiff.tar $(PACKAGE_CONTENTS)
	cd .pack/$(PACKAGE_NAME) ; tar xf ../tmpmiff.tar ; rm ../tmpmiff.tar
	cd .pack ; tar cfz ../$(PACKAGE_NAME).tgz $(PACKAGE_NAME)

clean:
	rm -Rf .pack
