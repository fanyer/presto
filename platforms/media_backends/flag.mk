# -*- Mode: Makefile; tab-width: 4 -*-
# vim: syntax=make ts=4
# See platforms/build/documentation/ for information about the Opera makefile system

# Enabled only by product's config (e.g. gogi.mk)
USE_MEDIA_BACKEND_GSTREAMER ?= NO

# Set to YES only when TWEAK_MEDIA_BACKEND_GSTREAMER_USE_OPDLL is NO
LINK_WITH_GSTREAMER ?= NO

# Set to NO to disable building of GStreamer plugins
BUILD_GSTREAMER_PLUGINS ?= YES

ifeq (YES, $(USE_MEDIA_BACKEND_GSTREAMER))
 PLATMOD += media_backends
 ifneq ($(DEBUGCODE), YES)
  DEFINES += GST_DISABLE_GST_DEBUG
 endif
 DEFINES += GST_DISABLE_LOADSAVE GST_DISABLE_XML

 ifeq (YES, $(LINK_WITH_GSTREAMER))
  GStreamerPackages=gstreamer-0.10 gstreamer-base-0.10 gstreamer-plugins-base-0.10
  GStreamerPkgConfig=pkg-config $(GStreamerPackages)
  ifeq ($(shell $(GStreamerPkgConfig) && echo YES), YES)
   GStreamerCPPCONF := $(shell $(GStreamerPkgConfig) --cflags)
   GStreamerLDCONF := $(shell $(GStreamerPkgConfig) --libs) -lgstvideo-0.10 -lgstriff-0.10
   CPPCONF += $(GStreamerCPPCONF)
   LDCONF  += $(GStreamerLDCONF)
  else # $(GStreamerPackages) not available:
   $(warning You need $(GStreamerPackages) to build the GStreamer media backend)
   $(warning See platforms/media_backends/documentation/gst.html)
  endif
 endif # LINK_WITH_GSTREAMER
endif # USE_MEDIA_BACKEND_GSTREAMER
