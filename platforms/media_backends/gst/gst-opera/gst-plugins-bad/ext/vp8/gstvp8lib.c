/* VP8
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "gstvp8lib.h"

/* Global function pointers. */
vpx_codec_destroy_ft vpx_codec_destroy_fptr;
vpx_img_free_ft vpx_img_free_fptr;
vpx_codec_get_frame_ft vpx_codec_get_frame_fptr;
vpx_codec_decode_ft vpx_codec_decode_fptr;
vpx_codec_dec_init_ver_ft vpx_codec_dec_init_ver_fptr;
vpx_codec_get_caps_ft vpx_codec_get_caps_fptr;
vpx_codec_peek_stream_info_ft vpx_codec_peek_stream_info_fptr;
vpx_codec_control__ft vpx_codec_control__fptr;
vpx_codec_vp8_dx_ft vpx_codec_vp8_dx_fptr;

/* Init functions. */
#define EXPORT_LIBVPX_INIT(SYM) \
	GST_PLUGIN_EXPORT void libvpx_init_##SYM(void *p) { SYM##_fptr = (SYM##_ft)p; }

EXPORT_LIBVPX_INIT(vpx_codec_destroy);
EXPORT_LIBVPX_INIT(vpx_img_free);
EXPORT_LIBVPX_INIT(vpx_codec_get_frame);
EXPORT_LIBVPX_INIT(vpx_codec_decode);
EXPORT_LIBVPX_INIT(vpx_codec_dec_init_ver);
EXPORT_LIBVPX_INIT(vpx_codec_get_caps);
EXPORT_LIBVPX_INIT(vpx_codec_peek_stream_info);
EXPORT_LIBVPX_INIT(vpx_codec_control_);
EXPORT_LIBVPX_INIT(vpx_codec_vp8_dx);

gboolean verify_lib_init()
{
	return
		vpx_codec_destroy &&
		vpx_img_free &&
		vpx_codec_get_frame &&
		vpx_codec_decode &&
		vpx_codec_dec_init_ver &&
		vpx_codec_get_caps &&
		vpx_codec_peek_stream_info &&
		vpx_codec_control_ &&
		vpx_codec_vp8_dx;
}
