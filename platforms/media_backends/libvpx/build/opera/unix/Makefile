# See platforms/build/documentation/ for information about the Opera makefile system

VPX_DIR ?= $(shell pwd)/../../..
VPX_MAKE_DIR ?= $(VPX_DIR)/build/opera/unix
VPX_TMP_DIR ?= $(VPX_MAKE_DIR)/tmp

# Suppress all warnings by default
EXTRA_CFLAGS_OPTION ?= --extra-cflags=-w

ifdef CROSS
	# The configure script for libvpx has a --target option
	# that (presumably) means the same thing as autoconf's
	# --host option.  That is, what system the built
	# executables will be run on.  autoconf's --target option
	# means something completely different.
	VPX_TARGET_OPTION = --target=generic-gnu
endif

.PHONY: .libvpx
.libvpx:
	@echo Building libvpx
	@mkdir -p $(VPX_TMP_DIR)
	@cd $(VPX_TMP_DIR) && \
	$(VPX_DIR)/configure \
		$(VPX_TARGET_OPTION) \
		$(EXTRA_CFLAGS_OPTION) \
		--as=yasm \
		--enable-pic \
		--enable-runtime-cpu-detect \
		--disable-multithread \
		--disable-postproc \
		--disable-examples \
		--disable-install-docs \
		--disable-install-bins \
		--disable-install-srcs && \
	$(MAKE)

all: .libvpx

clean:
	rm -rf tmp
