/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GSTLIBVPX_H
#define GSTLIBVPX_H

#ifdef MEDIA_BACKEND_GSTREAMER

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Table of libvpx functions needed by VP8 GStreamer decoders.
 *
 *  Opera comes bundled with a VP8 plugin for GStreamer. This plugin does not
 *  contain a copy of libvpx, but instead contains initialization functions
 *  for manual, dynamic linking. This makes it possible to provide prebuilt
 *  binaries for the VP8 plugin itself, but have it use the libvpx linked
 *  with the media backend.
 *
 *  When the GStreamer plugins are loaded, the media backend calls
 *  gstlibvpx_function_table() to get a copy of the table, and calls
 *  the initialization functions in the plugin to link the plugin with (our)
 *  copy of libvpx. */
typedef struct gstlibvpx_ft_
{
	void *vpx_codec_destroy;
	void *vpx_img_free;
	void *vpx_codec_get_frame;
	void *vpx_codec_decode;
	void *vpx_codec_dec_init_ver;
	void *vpx_codec_get_caps;
	void *vpx_codec_peek_stream_info;
	void *vpx_codec_control_;
	void *vpx_codec_vp8_dx;
} gstlibvpx_ft;

/** @return An initialized table of libvpx functions. */
gstlibvpx_ft gstlibvpx_function_table();

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* MEDIA_BACKEND_GSTREAMER */

#endif /* GSTLIBVPX_H */
