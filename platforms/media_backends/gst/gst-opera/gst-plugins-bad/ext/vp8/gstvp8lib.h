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

#include <gst/gst.h>

#ifdef VPX_DISABLE_CTRL_TYPECHECKS
# undef VPX_DISABLE_CTRL_TYPECHECKS
#endif /* VPX_DISABLE_CTRL_TYPECHECKS */

#define VPX_DISABLE_CTRL_TYPECHECKS 1

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

/* Function pointer types. */
typedef vpx_codec_err_t (*vpx_codec_destroy_ft)(vpx_codec_ctx_t *ctx);
typedef void (*vpx_img_free_ft)(vpx_image_t *img);
typedef vpx_image_t *(*vpx_codec_get_frame_ft)(vpx_codec_ctx_t  *ctx, vpx_codec_iter_t *iter);
typedef vpx_codec_err_t (*vpx_codec_decode_ft)(vpx_codec_ctx_t *ctx, const uint8_t *data, unsigned int data_sz, void *user_priv, long deadline);
typedef vpx_codec_err_t (*vpx_codec_dec_init_ver_ft)(vpx_codec_ctx_t *ctx, vpx_codec_iface_t *iface, vpx_codec_dec_cfg_t *cfg, vpx_codec_flags_t flags, int ver);
typedef vpx_codec_caps_t (*vpx_codec_get_caps_ft)(vpx_codec_iface_t *iface);
typedef vpx_codec_err_t (*vpx_codec_peek_stream_info_ft)(vpx_codec_iface_t *iface, const uint8_t *data, unsigned int data_sz, vpx_codec_stream_info_t *si);
typedef vpx_codec_err_t (*vpx_codec_control__ft)(vpx_codec_ctx_t  *ctx, int ctrl_id, ...);
typedef vpx_codec_iface_t *(*vpx_codec_vp8_dx_ft)(void);

/* Function pointer declarations. */
extern vpx_codec_destroy_ft vpx_codec_destroy_fptr;
extern vpx_img_free_ft vpx_img_free_fptr;
extern vpx_codec_get_frame_ft vpx_codec_get_frame_fptr;
extern vpx_codec_decode_ft vpx_codec_decode_fptr;
extern vpx_codec_dec_init_ver_ft vpx_codec_dec_init_ver_fptr;
extern vpx_codec_get_caps_ft vpx_codec_get_caps_fptr;
extern vpx_codec_peek_stream_info_ft vpx_codec_peek_stream_info_fptr;
extern vpx_codec_control__ft vpx_codec_control__fptr;
extern vpx_codec_vp8_dx_ft vpx_codec_vp8_dx_fptr;

/* Bindings. */
#define vpx_codec_destroy vpx_codec_destroy_fptr
#define vpx_img_free vpx_img_free_fptr
#define vpx_codec_get_frame vpx_codec_get_frame_fptr
#define vpx_codec_decode vpx_codec_decode_fptr
#define vpx_codec_dec_init_ver vpx_codec_dec_init_ver_fptr
#define vpx_codec_get_caps vpx_codec_get_caps_fptr
#define vpx_codec_peek_stream_info vpx_codec_peek_stream_info_fptr
#define vpx_codec_control_ vpx_codec_control__fptr
#define vpx_codec_vp8_dx vpx_codec_vp8_dx_fptr

/**
 * Verify that libvpx symbols have been initialized.
 *
 * @return TRUE if all symbols are initialized, FALSE otherwise.
 */
gboolean verify_lib_init();

