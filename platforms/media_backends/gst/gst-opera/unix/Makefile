# we're inside the gst-opera/unix folder, so move one folder up
GStreamerSourceDir ?= $(abspath ..)
GStreamerTempDir   ?= $(GStreamerSourceDir)/unix/tmp
GStreamerOutputDir ?= $(GStreamerSourceDir)/unix/plugins
# used to include glib's headers locally and not from the system
GStreamerIncludes   = $(GStreamerIncludeDir) $(GStreamerTopIncludeDir)

# Opera Core is bundled with the necessary libvpx headers at this location.
# For out-of-Core builds this include path will (most likely) have no effect,
# and libvpx headers should be installed in a system search path instead.
LibvpxSourceDir = $(abspath ../../../libvpx)

.PHONY: all
all: plugins

GST_CFLAGS  = $(GStreamerIncludes:%=-I%)
GST_CFLAGS += $(shell pkg-config --cflags gstreamer-video-0.10)
GST_CFLAGS += -fPIC
ifeq ($(findstring GST_DISABLE_GST_DEBUG,$(OPERA_BUILD_DEFINES)),)
GST_CFLAGS += -g3
else
GST_CFLAGS += -O3
endif

GST_LIBS  = $(shell pkg-config --libs gstreamer-video-0.10)
GST_LIBS += -lm

GST_OPERA_CFLAGS  = -DOPERA_MINIMAL_GST
GST_OPERA_CFLAGS += -DVERSION=\"0.10\"
GST_OPERA_CFLAGS += -DPACKAGE=\"gst-opera\"
GST_OPERA_CFLAGS += -DGST_PACKAGE_NAME=\"GStreamer\ Opera\ Plug-ins\"
GST_OPERA_CFLAGS += -DGST_PACKAGE_ORIGIN=\"http://sourcecode.opera.com/gstreamer/\"
GST_OPERA_CFLAGS += $(OPERA_BUILD_DEFINES:%=-D%)

VPX_CFLAGS=-I$(LibvpxSourceDir)

GST_MATROSKA_SOURCES = \
	$(GStreamerSourceDir)/gst-plugins-good/gst/matroska/ebml-read.c \
	$(GStreamerSourceDir)/gst-plugins-good/gst/matroska/matroska-ids.c \
	$(GStreamerSourceDir)/gst-plugins-good/gst/matroska/matroska-demux.c

$(GStreamerOutputDir)/libgstoperamatroska.so: $(GST_MATROSKA_SOURCES)
	mkdir -p $(GStreamerOutputDir)
	gcc -shared -Wl,--no-undefined -o $@ $^ $(GST_CFLAGS) $(GST_OPERA_CFLAGS) $(GST_LIBS)

GST_VP8_SOURCES = \
	$(GStreamerSourceDir)/gst-plugins-bad/ext/vp8/plugin.c \
	$(GStreamerSourceDir)/gst-plugins-bad/ext/vp8/gstvp8dec.c \
	$(GStreamerSourceDir)/gst-plugins-bad/ext/vp8/gstvp8utils.c \
	$(GStreamerSourceDir)/gst-plugins-bad/ext/vp8/gstvp8lib.c

GST_VP8_CFLAGS = -DHAVE_VP8_DECODER

GST_BASE_VIDEO_SOURCES = \
	$(GStreamerSourceDir)/gst-plugins-bad/gst-libs/gst/video/gstbasevideocodec.c \
	$(GStreamerSourceDir)/gst-plugins-bad/gst-libs/gst/video/gstbasevideodecoder.c \
	$(GStreamerSourceDir)/gst-plugins-bad/gst-libs/gst/video/gstbasevideoutils.c

GST_BASE_VIDEO_CFLAGS = -DGST_USE_UNSTABLE_API -I$(GStreamerSourceDir)/gst-plugins-bad/gst-libs

$(GStreamerOutputDir)/libgstoperavp8.so: $(GST_VP8_SOURCES) $(GST_BASE_VIDEO_SOURCES)
	mkdir -p $(GStreamerOutputDir)
	gcc -shared -Wl,--no-undefined -o $@ $^ $(GST_VP8_CFLAGS) $(GST_BASE_VIDEO_CFLAGS)  $(GST_CFLAGS) $(GST_OPERA_CFLAGS) $(GST_LIBS) $(VPX_CFLAGS)

GST_PLUGINS = \
	$(GStreamerOutputDir)/libgstoperamatroska.so \
	$(GStreamerOutputDir)/libgstoperavp8.so

.PHONY: plugins
plugins: $(GST_PLUGINS)

.PHONY: clean
clean:
	rm -rf $(GST_PLUGINS) $(GStreamerTempDir)
