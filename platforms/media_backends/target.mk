# -*- Mode: Makefile; tab-width: 4 -*-
# vim: syntax=make ts=4
# See platforms/build/documentation/ for information about the Opera makefile system

ifeq (YES, $(USE_MEDIA_BACKEND_GSTREAMER))

ifeq ($(GSTREAMER_OUTPUT_DIR),)
$(error GSTREAMER_OUTPUT_DIR must be set to the folder where to deploy the gstreamer libraries)
endif

# Note: earlier $(GStreamerDir) and $(GStreamerPLuginsDir) were added to
# OUTDIRS, but that caused prebuilt GStreamer DLLs to be deleted by 'make
# clean'. Instead, we now remove just the .so's in gst-plugins-clean below.

GStreamerPlugins = libgstoperamatroska.so libgstoperavp8.so

GStreamerSourceDir = $(OPERADIR)/platforms/media_backends/gst/gst-opera
GStreamerIncludeDir = $(OPERADIR)/platforms/media_backends/gst/include
GStreamerTopIncludeDir = $(OPERADIR)
GStreamerOutputDir = $(GSTREAMER_OUTPUT_DIR)
GStreamerTempDir = $(BUILTTMP)/gstreamer

# Paths to built plugins, e.g.
# "opera/gstreamer/plugins/foo.so opera/gstreamer/plugins/bar.so ..."
GStreamerPluginsOutputPaths = $(GStreamerPlugins:%=$(GStreamerOutputDir)/%)

GStreamerMakeVars += GStreamerSourceDir=$(abspath $(GStreamerSourceDir))
GStreamerMakeVars += GStreamerIncludeDir=$(abspath $(GStreamerIncludeDir))
GStreamerMakeVars += GStreamerTopIncludeDir=$(abspath $(GStreamerTopIncludeDir))
GStreamerMakeVars += GStreamerOutputDir=$(abspath $(GStreamerOutputDir))
GStreamerMakeVars += GStreamerTempDir=$(abspath $(GStreamerTempDir))
GStreamerMakeVars += OPERA_BUILD_DEFINES='$(DEFINES)'

$(GStreamerPluginsOutputPaths): $(GStreamerOutputDir)/%:
	@$(MAKE) -C $(GStreamerSourceDir)/unix $(GStreamerMakeVars) $(abspath $@)

.PHONY: gst-plugins
gst-plugins: $(GStreamerPluginsOutputPaths)

.PHONY: gst-plugins-clean
gst-plugins-clean:
	@$(MAKE) -C $(GStreamerSourceDir)/unix $(GStreamerMakeVars) clean

.PHONY: gst-plugins-list
gst-plugins-list:
	@echo $$(ALINE); \
	$(foreach B,$(GStreamerPlugins), \
		$(call bcd_list_file, $(GStreamerOutputDir),      $B); )

 ifeq (YES, $(BUILD_GSTREAMER_PLUGINS))
defaultrule: gst-plugins
list: gst-plugins-list
 endif
clean: gst-plugins-clean

endif # USE_MEDIA_BACKEND_GSTREAMER
