# -*- Mode: Makefile; tab-width: 4 -*-
# Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
#
# Build the lngtest utility for Linux x86/amd64 and Windows.
# Build the ZIP file for distribution in the translations SVN repository
# (the Mac OS command-line binary must be built separately and placed
#  into zipdir/macos/ first to create a complete archive).
#
# Tested on Linux amd64. May work for other configurations as well.
# The Debian package mingw32 allows Windows cross-compilation on Linux.
#
all: binaries strip

BIN=lngtest.i386 lngtest.amd64 lngtest.exe

binaries: $(BIN)

lngtest.i386: lngtest.c
	gcc -Wall -g -m32 -o lngtest.i386 lngtest.c

lngtest.amd64: lngtest.c
	gcc -Wall -g -m64 -o lngtest.amd64 lngtest.c

lngtest.exe: lngtest.c
	i586-mingw32msvc-gcc -Wall -o lngtest.exe lngtest.c

strip: $(BIN)
	strip $(BIN)

clean:
	-rm $(BIN)
	-rm -rf zipdir

zip: strip
	-mkdir -p zipdir/linux.amd64 zipdir/linux.i386 zipdir/windows zipdir/macos zipdir/src/lngtest.xcodeproj
	cp lngtest.amd64 zipdir/linux.amd64/lngtest
	cp lngtest.i386 zipdir/linux.i386/lngtest
	cp lngtest.exe zipdir/windows/
	cp Makefile lngtest.c lngtest.dsp lngtest.dsw zipdir/src/
	cp lngtest.xcodeproj/project.pbxproj zipdir/src/lngtest.xcodeproj/
	test -x zipdir/macos/lngtest || echo 'Mac binary missing, and cannot build' >&2
	cd zipdir ; zip -r /tmp/lngtest-`date +"%Y%m%d"` src macos linux.amd64 linux.i386 windows

.PHONY: binaries strip clean zip
