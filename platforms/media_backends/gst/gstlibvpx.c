/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch_system_includes.h"

#ifdef MEDIA_BACKEND_GSTREAMER

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "platforms/media_backends/gst/gstlibvpx.h"

/* Silence warnings about macro redefinition. */

#undef INT8_MAX
#undef INT8_MIN
#undef UINT8_MAX

#undef INT16_MAX
#undef INT16_MIN
#undef UINT16_MAX

#undef INT32_MAX
#undef INT32_MIN
#undef UINT32_MAX

#undef INT64_MAX
#undef INT64_MIN
#undef UINT64_MAX

#include "platforms/media_backends/libvpx/vpx/vpx_decoder.h"
#include "platforms/media_backends/libvpx/vpx/vp8dx.h"

gstlibvpx_ft
gstlibvpx_function_table()
{
	gstlibvpx_ft t;

	t.vpx_codec_destroy = (void*)&vpx_codec_destroy;
	t.vpx_img_free = (void*)&vpx_img_free;
	t.vpx_codec_get_frame = (void*)&vpx_codec_get_frame;
	t.vpx_codec_decode = (void*)&vpx_codec_decode;
	t.vpx_codec_dec_init_ver = (void*)&vpx_codec_dec_init_ver;
	t.vpx_codec_get_caps = (void*)&vpx_codec_get_caps;
	t.vpx_codec_peek_stream_info = (void*)&vpx_codec_peek_stream_info;
	t.vpx_codec_control_ = (void*)&vpx_codec_control_;
	t.vpx_codec_vp8_dx = (void*)&vpx_codec_vp8_dx;

	return t;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* MEDIA_BACKEND_GSTREAMER */
