# See platforms/build/documentation/ for information about the Opera makefile system

VPX_DIR = $(OPERADIR)/platforms/media_backends/libvpx
VPX_MAKE_DIR = $(VPX_DIR)/build/opera/unix
VPX_TMP_DIR = $(VPX_MAKE_DIR)/tmp
VPX_LIB = $(VPX_TMP_DIR)/libvpx.a

LIBS += $(VPX_LIB)

.PHONY: .libvpx
.libvpx:
	cd $(VPX_MAKE_DIR) && \
	$(MAKE) -f Makefile .libvpx

.PHONY: .libvpx-clean
.libvpx-clean:
	rm -rf $(VPX_TMP_DIR)

defaultrule: .libvpx
all: .libvpx
clean: .libvpx-clean
