# -*- Mode: Makefile; tab-width: 4 -*-
# vim: syntax=make ts=4

npo_ARCH = 32
npo_SRC_BASE	= ..
include sources.mk
npo_OBJTEMP		= $(npo_SRC:.cpp=.o)
npo_OBJECTS		= $(npo_OBJTEMP:.mm=.o)

ifeq ($(shell uname), Darwin)
npo_TARGET		= npo.plugin/Contents/npo.dylib
npo_LDFLAGS		= -framework Cocoa -framework QuartzCore -bundle
npo_PLATFLAG	= -install_name
else
npo_TARGET		= npo.so
npo_LDFLAGS		= -lrt
npo_PLATFLAG	= -shared -Wl,-soname
ifeq ($(XP_GOGI), 1)
npo_CXXFLAGS	+= -DXP_GOGI -I../../../../platforms/linuxsdk/include/
else
npo_CXXFLAGS   += -DXP_UNIX
endif
endif

npo_CXXFLAGS	+= -Wall -W -g -fPIC

ifeq ($(npo_ARCH), 32)
npo_CXXFLAGS	+= -m32
npo_LDFLAGS		+= -m32
endif

default:	all
all:		$(npo_TARGET)

$(npo_OBJECTS): CXXFLAGS=$(npo_CXXFLAGS)

%.o: %.mm
	$(CXX) $(npo_CXXFLAGS) -c -o $@ $^

$(npo_TARGET): $(npo_OBJECTS)
	$(CXX) $(npo_PLATFLAG),$@ -o $@ $(npo_LDFLAGS) $^

clean:
	rm -f $(npo_OBJECTS)
