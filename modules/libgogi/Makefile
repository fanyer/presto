# -*- Mode: Makefile; tab-width: 4 -*-
# vim: syntax=make ts=4

#choose your backend:
#BACKEND=GTK
#BACKEND=SDL

DEBUG ?= YES

USE_FREETYPE=YES

CPPFLAGS+=-Wall

LIBSOURCES=mde.cpp mde_blit.cpp mde_mouse.cpp mde_view.cpp mde_font.cpp
LIBOBJECTS=$(LIBSOURCES:.cpp=.o)

all: libgogi.a test

libgogi.a: $(LIBOBJECTS)
	ar crs $@ $^

TESTSOURCES=test/mde_test.cpp

ifeq ($(BACKEND),)
$(error "you need to choose what backend to use")
endif

ifeq ($(BACKEND), GTK)
TESTLIBS=`pkg-config gtk+-2.0 --libs`
TESTSOURCES+=test/mde_test_gtk.cpp
CPPFLAGS+=$(shell pkg-config gtk+-2.0 --cflags)
TESTTARGET=test/mde_test_gtk
endif

ifeq ($(BACKEND), SDL)
TESTLIBS=`sdl-config --libs`
TESTSOURCES+=test/mde_test_sdl.cpp
CPPFLAGS+=$(shell sdl-config --cflags)
TESTTARGET=test/mde_test_sdl
endif

ifeq ($(USE_FREETYPE), YES)
CPPFLAGS+=$(shell freetype-config --cflags)
TESTLIBS+=$(shell freetype-config --libs)
endif

TESTOBJECTS=$(TESTSOURCES:.cpp=.o)

test: $(TESTTARGET)

$(TESTTARGET): $(TESTOBJECTS) $(LIBOBJECTS)
	g++ $(LDFLAGS) -o $@ $(TESTLIBS) $^

clean:
	rm -f libgogi.a $(LIBOBJECTS) $(TESTOBJECTS)

ifeq ($(DEBUG), YES)
CPPFLAGS+=-g
LDFLAGS+=-g
else
CPPFLAGS+=-Os
endif
