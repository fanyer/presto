# -*- Mode: Makefile; tab-width: 4 -*-
# vim: syntax=make ts=4
# See unix-build/documentation/ for information about the Opera makefile system.

ifeq ($(NS4PLUGINS_TEST_PLUGIN), YES)
# This Makefile defines the rules to make the ns4plugins module's test plugin, npo.so.
#
# The test plugin is required to pass selfests and extra spartan tests, and can be
# a handy tool to probe the behavior of other browsers. The library is installed in
# $(SHIPETC)/plugins/linux$(WORDSIZE).
#
.PHONY:	ns4plugins-test-so

defaultrule: ns4plugins-test-so

plugin_DIR		=$(SHIPETC)/plugins/linux$(WORDSIZE)
npo_SRC_BASE	=$(OPERADIR)/modules/ns4plugins/test_plugin
include $(npo_SRC_BASE)/build/sources.mk
npo_OBJECTS		=$(call src2obj,$(npo_SRC))
CREATEFILES		+=$(npo_OBJECTS)
npo_TARGET		=$(plugin_DIR)/npO.so
npo_CPPFLAGS	=-DXP_UNIX
npo_CXXFLAGS	:=$(subst $(COVERAGEFLAGS),,$(CXXFLAGS)) $(SHARE_FLAGS) $(shell pkg-config gtk+-2.0 --cflags)
npo_LDFLAGS		= $(LIBPATH:%=-L%) $(LDSHARED) -lX11 $(shell pkg-config gtk+-2.0 --libs)

$(npo_OBJECTS):	CPPFLAGS+=$(npo_CPPFLAGS)
$(npo_OBJECTS): CXXFLAGS=$(npo_CXXFLAGS)

CREATEFILES += $(npo_TARGET)
ns4plugins-test-so: $(npo_TARGET)

$(npo_TARGET):	$(npo_OBJECTS)
		$(HUSH_LINK) $(LD) -o $@ $(npo_LDFLAGS) $(^:%/.exists=)

endif # NS4PLUGINS_TEST_PLUGIN == YES
