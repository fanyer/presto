/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef OP_SKIN_UTILS_H
#define OP_SKIN_UTILS_H

// Macros copied from vegapixelformat_8888.inl
#define SKIN_UNPACK_R(p) (((p) >> 16) & 0xff)
#define SKIN_UNPACK_G(p) (((p) >> 8) & 0xff)
#define SKIN_UNPACK_B(p) ((p) & 0xff)
#define SKIN_UNPACK_A(p) (((p) >> 24) & 0xff)
#define SKIN_EXTRACT_HI(p) (((p) & 0xff00ff00) >> 8)
#define SKIN_EXTRACT_LO(p) ( (p) & 0x00ff00ff)
#define SKIN_PACK_ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

class OpSkinUtils
{
	friend class OpSkin;
	friend class OpSkinElement;
	friend class OpWidgetImage;

	public:

#ifdef SKIN_SKIN_COLOR_THEME
		static void		ConvertRGBToHSL(UINT32 color, double& h, double& s, double& l);
		static void		ConvertRGBToHSV(UINT32 color, double& h, double& s, double& v);
		static void		ConvertBGRToHSV(UINT32 color, double& h, double& s, double& v);
		static UINT32	ConvertHSLToRGB(double h, double s, double l);
		static UINT32	ConvertHSVToRGB(double h, double s, double v);
		static UINT32	ConvertHSVToBGR(double h, double s, double v);
#endif // SKIN_SKIN_COLOR_THEME

		static void		CopyBitmap(OpBitmap* src_bitmap, OpBitmap* dst_bitmap);
		static void		ScaleBitmap(OpBitmap* src_bitmap, OpBitmap* dst_bitmap);

		/**
		 * Creates a mirror of a bitmap.
		 *
		 * @param src_bitmap the source bitmap
		 * @return a new bitmap that is a mirror of @a src_bitmap, or @c NULL
		 * 		on error
		 */
		static OpBitmap* MirrorBitmap(OpBitmap* src_bitmap);

	//private:

#ifdef SKIN_SKIN_COLOR_THEME
		static void		ColorizeHSV(double& h, double& s, double& v, double& colorize_h, double& colorize_s, double& colorize_v);
		static UINT32	ColorizePixel(UINT32 color, double colorize_h, double colorize_s, double colorize_v);

		/** Does a colorization of the pixel, and it it will make the pixel to 
		* colorize 5% darker before colorization for a stronger effect.
		*/
		static UINT32	ColorizePixelDarker(UINT32 color, double colorize_h, double colorize_s, double colorize_v);
#endif // SKIN_SKIN_COLOR_THEME

		/** Searches the bitmap and returns the average color used. For larger images, it will only search a limited 
		 * section of the of it, generating the average color used in the bitmap.  If the average alpha channel is 
		 * less than 127 (50%),
		 * the average color will be 0. 
		 */
		static INT32	GetAverageColorOfBitmap(OpBitmap *bmp);

		/** Inverts a ARGB premultiplied pixel and applies the alpha to all channels.
		 *  Copies from UnpremultiplyFast() from vegapixelformat_8888.inl
		 */
		static op_force_inline UINT32 InvertPremultiplyAlphaPixel(UINT32 pix)
		{
			unsigned a = SKIN_UNPACK_A(pix);
			if (a == 0 || a == 255)
				return pix;

			unsigned recip_a = (255u << 24) / a;
			unsigned r = ((SKIN_UNPACK_R(pix) * recip_a) + (1 << 23)) >> 24;
			unsigned g = ((SKIN_UNPACK_G(pix) * recip_a) + (1 << 23)) >> 24;
			unsigned b = ((SKIN_UNPACK_B(pix) * recip_a) + (1 << 23)) >> 24;

			return SKIN_PACK_ARGB(a, r, g, b);
		}
		/** Premultiplies a ARGB pixel. 
		 *  Copies from Premultiply() from vegapixelformat_8888.inl
		 */
		static op_force_inline UINT32 PremultiplyAlphaPixel(UINT32 pix)
		{
			unsigned a = SKIN_UNPACK_A(pix);
			if (a == 0)
				return 0;
			if (a == 255)
				return pix;

			unsigned r = SKIN_UNPACK_R(pix);
			unsigned g = SKIN_UNPACK_G(pix);
			unsigned b = SKIN_UNPACK_B(pix);

			r = (r * a + 127) / 255;
			g = (g * a + 127) / 255;
			b = (b * a + 127) / 255;

			return SKIN_PACK_ARGB(a, r, g, b);
		}

};

#endif // !OP_SKIN_UTILS_H
