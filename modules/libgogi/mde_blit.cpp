/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
*/

#include "core/pch.h"
#include "modules/libgogi/mde.h"

#ifdef MDE_BILINEAR_BLITSTRETCH
#define MDE_BILINEAR_INTERPOLATION_X_IMPL(dstdata, srcdata, dstlen, srclen, sx, dx) for (int x = 0; x < dstlen; ++x){ \
	int pix = sx>>16; \
	int weight2 = sx&0xffff; \
	int weight1 = 0x10000-weight2; \
	if (weight2 && pix+1 < srclen){ \
		dstdata[x].red = (srcdata[pix].red*weight1 + srcdata[pix+1].red*weight2)>>16; \
		dstdata[x].green = (srcdata[pix].green*weight1 + srcdata[pix+1].green*weight2)>>16; \
		dstdata[x].blue = (srcdata[pix].blue*weight1 + srcdata[pix+1].blue*weight2)>>16; \
		dstdata[x].alpha = (srcdata[pix].alpha*weight1 + srcdata[pix+1].alpha*weight2)>>16; \
	} else \
		dstdata[x] = srcdata[pix]; \
	sx += dx; }
#define MDE_BILINEAR_INTERPOLATION_X_NO_ALPHA_IMPL(dstdata, srcdata, dstlen, srclen, sx, dx) for (int x = 0; x < dstlen; ++x){ \
	int pix = sx>>16; \
	int weight2 = sx&0xffff; \
	int weight1 = 0x10000-weight2; \
	if (weight2 && pix+1 < srclen){ \
		dstdata[x].red = (srcdata[pix].red*weight1 + srcdata[pix+1].red*weight2)>>16; \
		dstdata[x].green = (srcdata[pix].green*weight1 + srcdata[pix+1].green*weight2)>>16; \
		dstdata[x].blue = (srcdata[pix].blue*weight1 + srcdata[pix+1].blue*weight2)>>16; \
	} else \
		dstdata[x] = srcdata[pix]; \
	sx += dx; }

#define MDE_BILINEAR_INTERPOLATION_Y_IMPL(dstdata, src1data, src2data, len, sy) int weight2 = sy&0xffff; \
	int weight1 = 0x10000-weight2; \
	for (int x = 0; x < len; ++x){ \
		dstdata[x].red = (src1data[x].red*weight1 + src2data[x].red*weight2)>>16; \
		dstdata[x].green = (src1data[x].green*weight1 + src2data[x].green*weight2)>>16; \
		dstdata[x].blue = (src1data[x].blue*weight1 + src2data[x].blue*weight2)>>16; \
		dstdata[x].alpha = (src1data[x].alpha*weight1 + src2data[x].alpha*weight2)>>16; \
	}
#define MDE_BILINEAR_INTERPOLATION_Y_NO_ALPHA_IMPL(dstdata, src1data, src2data, len, sy) int weight2 = sy&0xffff; \
	int weight1 = 0x10000-weight2; \
	for (int x = 0; x < len; ++x){ \
		dstdata[x].red = (src1data[x].red*weight1 + src2data[x].red*weight2)>>16; \
		dstdata[x].green = (src1data[x].green*weight1 + src2data[x].green*weight2)>>16; \
		dstdata[x].blue = (src1data[x].blue*weight1 + src2data[x].blue*weight2)>>16; \
	}
#endif // MDE_BILINEAR_BLITSTRETCH

void MDE_Scanline_InvColor_unsupported(void *dst, int len, unsigned int dummy)							{ MDE_ASSERT(0); }
void MDE_Scanline_SetColor_unsupported(void *dst, int len, unsigned int col)							{ MDE_ASSERT(0); }

bool MDE_Scanline_BlitNormal_unsupported(void *dst, void *src, int len,
										 MDE_BUFFER_INFO *srcinf,
										 MDE_BUFFER_INFO *dstinf)
{
	MDE_ASSERT(len == 0);
	return false;
}

void MDE_Scanline_BlitStretch_unsupported(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx,
										  MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	MDE_ASSERT(len == 0);
}

int MDE_GetBytesPerPixel(MDE_FORMAT format)
{
	switch(format)
	{
		case MDE_FORMAT_BGRA32:
#ifdef MDE_SUPPORT_RGBA32
		case MDE_FORMAT_RGBA32:
#endif
#ifdef MDE_SUPPORT_ARGB32
		case MDE_FORMAT_ARGB32:
#endif
			return 4;
		case MDE_FORMAT_BGR24: return 3;
#ifdef MDE_SUPPORT_RGB16
		case MDE_FORMAT_RGB16: return 2;
#endif
#ifdef MDE_SUPPORT_MBGR16
		case MDE_FORMAT_MBGR16: return 2;
#endif
#ifdef MDE_SUPPORT_RGBA16
		case MDE_FORMAT_RGBA16: return 2;
#endif
#ifdef MDE_SUPPORT_SRGB16
		case MDE_FORMAT_SRGB16: return 2;
#endif
#ifdef MDE_SUPPORT_RGBA24
		case MDE_FORMAT_RGBA24: return 3;
#endif // MDE_SUPPORT_RGBA24
		case MDE_FORMAT_INDEX8: return 1;
		default: MDE_ASSERT(0); return 0;
	}
}

#if defined(MDE_SUPPORT_RGB16) || defined(MDE_SUPPORT_SRGB16) || defined(MDE_SUPPORT_MBGR16)

void memset16bit(unsigned short *dst16, unsigned short val, unsigned int len16)
{
	if ((val & 0xff) == (val >> 8))
	{
		op_memset(dst16, val, len16 << 1);
		return;
	}
	if (len16 && (((UINTPTR)dst16) & 3) != 0)
	{
		// Consume first 16 bits so we are aligned with 32bit
		*dst16++ = val;
		len16--;
	}

	if (len16 > 1)
	{
		int len32 = len16 >> 1;
		unsigned int* dst32 = (unsigned int*) dst16;
		unsigned int* stop32 = (unsigned int*) dst32 + len32;
		unsigned int col32 = val + (val << 16);

		while (dst32 < stop32)
		{
			OP_ASSERT((((UINTPTR)dst32) & 3) == 0);
			*dst32++ = col32;
		}
		// I thought this would be faster, but with Visual Studio 2005 standard optimization it is slower than above loop.
		/*if (len32)
		{
			int dufflen = len32;
			switch(dufflen & 7)
			{
				while(dufflen > 0)
				{
				case 0: *dst32++ = col32;
				case 7: *dst32++ = col32;
				case 6: *dst32++ = col32;
				case 5: *dst32++ = col32;
				case 4: *dst32++ = col32;
				case 3: *dst32++ = col32;
				case 2: *dst32++ = col32;
				case 1: *dst32++ = col32;
					dufflen -= 8;
				}
			}
		}*/

		// Consume written data in 16bit so last 16 bits can be written.
		dst16 += len32 << 1;
		len16 -= len32 << 1;
	}

	if (len16)
	{
		// Consume last 16 bits
		*dst16++ = val;
	}
}

#endif // defined(MDE_SUPPORT_RGB16) || defined(MDE_SUPPORT_SRGB16) || defined(MDE_SUPPORT_MBGR16)

// == BGRA32 ==============================================================

void MDE_Scanline_InvColor_BGRA32(void *dst, int len, unsigned int dummy)
{
	unsigned char *dst8 = (unsigned char *) dst;
	for(int x = 0; x < len; x++)
	{
#ifdef USE_PREMULTIPLIED_ALPHA
		dst8[MDE_COL_OFS_B] = dst8[MDE_COL_OFS_A] - dst8[MDE_COL_OFS_B];
		dst8[MDE_COL_OFS_G] = dst8[MDE_COL_OFS_A] - dst8[MDE_COL_OFS_G];
		dst8[MDE_COL_OFS_R] = dst8[MDE_COL_OFS_A] - dst8[MDE_COL_OFS_R];
#else // !USE_PREMULTIPLIED_ALPHA
		dst8[MDE_COL_OFS_B] = 255 - dst8[MDE_COL_OFS_B];
		dst8[MDE_COL_OFS_G] = 255 - dst8[MDE_COL_OFS_G];
		dst8[MDE_COL_OFS_R] = 255 - dst8[MDE_COL_OFS_R];
#endif // !USE_PREMULTIPLIED_ALPHA
		dst8 += 4;
	}
}

#ifdef USE_PREMULTIPLIED_ALPHA
static LIBGOGI_FORCEINLINE UINT32 PremultiplyColor(UINT32 dst, unsigned char s)
{
	int a = s+1;
	UINT32 tmp = (dst>>8)&0xff;
	dst &= 0xff00ff;
	tmp *= a;
	dst *= a;
	dst >>= 8;
	return (dst&0xff00ff) | (tmp&0xff00) | (s<<24);
}

// col_map is filled with premultiplied color values
void SetColorMap(unsigned int* col_map, unsigned int color)
{
#ifdef MDE_USE_ALPHA_LOOKUPTABLE
	unsigned char* ltab = g_opera->libgogi_module.lutbl_alpha;
	const UINT8 a = MDE_COL_A(color);
	const UINT16 offs = a << 8;
	const UINT8 r = ltab[offs + MDE_COL_R(color)];
	const UINT8 g = ltab[offs + MDE_COL_G(color)];
	const UINT8 b = ltab[offs + MDE_COL_B(color)];
	for (UINT32 i = 0; i < 256; ++i)
	{
		col_map[i] = MDE_RGBA(ltab[r], ltab[g], ltab[b], ltab[a]);
		ltab += 256;
	}
#else // MDE_USE_ALPHA_LOOKUPTABLE
	UINT16 a = MDE_COL_A(color) + 1;
	const UINT32 tmp = (color >> 8) & 0xff;
	const UINT32 dst = color & 0xff00ff;
	for (UINT32 i = 0; i < 256; ++i)
    {
        UINT8 ta = (UINT8)((a * i) >> 8);
		UINT32 t = tmp * (ta+1);
		UINT32 d = (dst * (ta+1)) >> 8;
		col_map[i] = (d & 0xff00ff) | (t & 0xff00) | (ta << 24);
    }
#endif // MDE_USE_ALPHA_LOOKUPTABLE
}

static LIBGOGI_FORCEINLINE UINT32 ScalePremultipliedColor( UINT32 dst, unsigned char s)
{
	UINT32 tmp = (dst>>8)&0xff00ff;
	dst &= 0xff00ff;
	tmp *= s;
	dst *= s;
	dst >>= 8;
	return (dst&0xff00ff) | (tmp&0xff00ff00);
}

static LIBGOGI_FORCEINLINE void SetPixel_BGRA32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (srca == 0)
		return;
#ifdef MDE_USE_ALPHA_LOOKUPTABLE
	const UINT32 offs = (255 - srca) << 8;
	const unsigned char* ltab = g_opera->libgogi_module.lutbl_alpha + offs;
	dst8[MDE_COL_OFS_B] = ltab[dst8[MDE_COL_OFS_B]] + src8[MDE_COL_OFS_B];
	dst8[MDE_COL_OFS_G] = ltab[dst8[MDE_COL_OFS_G]] + src8[MDE_COL_OFS_G];
	dst8[MDE_COL_OFS_R] = ltab[dst8[MDE_COL_OFS_R]] + src8[MDE_COL_OFS_R];
	dst8[MDE_COL_OFS_A] = ltab[dst8[MDE_COL_OFS_A]] + src8[MDE_COL_OFS_A];
#else // MDE_USE_ALPHA_LOOKUPTABLE
	int i_alpha = 256 - srca;
	dst8[MDE_COL_OFS_B] = ((dst8[MDE_COL_OFS_B]*i_alpha)>>8) + src8[MDE_COL_OFS_B];
	dst8[MDE_COL_OFS_G] = ((dst8[MDE_COL_OFS_G]*i_alpha)>>8) + src8[MDE_COL_OFS_G];
	dst8[MDE_COL_OFS_R] = ((dst8[MDE_COL_OFS_R]*i_alpha)>>8) + src8[MDE_COL_OFS_R];
	dst8[MDE_COL_OFS_A] = ((dst8[MDE_COL_OFS_A]*i_alpha)>>8) + src8[MDE_COL_OFS_A];
#endif // MDE_USE_ALPHA_LOOKUPTABLE
}
#define SetPixelWithDestAlpha_BGRA32 SetPixel_BGRA32
#else // !USE_PREMULTIPLIED_ALPHA
static LIBGOGI_FORCEINLINE void SetPixel_BGRA32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (srca == 0)
		return;
	int alpha = srca+1;
	dst8[MDE_COL_OFS_B] = dst8[MDE_COL_OFS_B] + (alpha * (src8[MDE_COL_OFS_B] - dst8[MDE_COL_OFS_B]) >> 8);
	dst8[MDE_COL_OFS_G] = dst8[MDE_COL_OFS_G] + (alpha * (src8[MDE_COL_OFS_G] - dst8[MDE_COL_OFS_G]) >> 8);
	dst8[MDE_COL_OFS_R] = dst8[MDE_COL_OFS_R] + (alpha * (src8[MDE_COL_OFS_R] - dst8[MDE_COL_OFS_R]) >> 8);
}

static LIBGOGI_FORCEINLINE void SetPixelWithDestAlpha_BGRA32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (srca == 0)
		return;
	if (dst8[MDE_COL_OFS_A] == 0)
	{
		dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_B];
		dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_G];
		dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_R];
		dst8[MDE_COL_OFS_A] = srca;
	}
	else
	{
		// OPTO: Can calculate 2 pixels at the same time for 16bit mode.
		// OPTO: Can optimize away dst8 += 4 and src8 += 4 in caller.

#ifdef MDE_USE_ALPHA_LOOKUPTABLE
		int a = g_opera->libgogi_module.lutbl_alpha[srca + ((dst8[MDE_COL_OFS_A]) << 8)] + 1;
#else // MDE_USE_ALPHA_LOOKUPTABLE
		int total_alpha = MDE_MIN(dst8[MDE_COL_OFS_A] + srca, 255);
		int a = ((srca<<8) / (total_alpha + 1)) + 1;
#endif // MDE_USE_ALPHA_LOOKUPTABLE
		if (a > 1)
		{
			dst8[MDE_COL_OFS_B] += a * (src8[MDE_COL_OFS_B] - dst8[MDE_COL_OFS_B]) >> 8;
			dst8[MDE_COL_OFS_G] += a * (src8[MDE_COL_OFS_G] - dst8[MDE_COL_OFS_G]) >> 8;
			dst8[MDE_COL_OFS_R] += a * (src8[MDE_COL_OFS_R] - dst8[MDE_COL_OFS_R]) >> 8;
		}
		dst8[MDE_COL_OFS_A] += (unsigned char) (long((srca) * (256 - dst8[MDE_COL_OFS_A])) >> 8);
	}
}
#endif // !USE_PREMULTIPLIED_ALPHA

void MDE_Scanline_SetColor_BGRA32_NoBlend(void *dst, int len, unsigned int col)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	// Premultiply the color
	const unsigned char alpha = MDE_COL_A(col);
# ifdef MDE_USE_ALPHA_LOOKUPTABLE
	const unsigned char* ltab = g_opera->libgogi_module.lutbl_alpha + (alpha << 8);
	const unsigned char red = ltab[MDE_COL_R(col)];
	const unsigned char green = ltab[MDE_COL_G(col)];
	const unsigned char blue = ltab[MDE_COL_B(col)];
# else // MDE_USE_ALPHA_LOOKUPTABLE
	int a = alpha + 1;
	int red = (MDE_COL_R(col)*a)>>8;
	int green = (MDE_COL_G(col)*a)>>8;
	int blue = (MDE_COL_B(col)*a)>>8;
# endif // MDE_USE_ALPHA_LOOKUPTABLE
	col = MDE_RGBA(red, green, blue, alpha);
#endif // USE_PREMULTIPLIED_ALPHA
	for(int x = 0; x < len; x++)
		((unsigned int*)dst)[x] = col;
}

void MDE_Scanline_SetColor_BGRA32_Blend(void *dst, int len, unsigned int col)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	// Premultiply the color
	const unsigned char alpha = MDE_COL_A(col);
# ifdef MDE_USE_ALPHA_LOOKUPTABLE
	const unsigned char* ltab = g_opera->libgogi_module.lutbl_alpha + (alpha << 8);
	const unsigned char red = ltab[MDE_COL_R(col)];
	const unsigned char green = ltab[MDE_COL_G(col)];
	const unsigned char blue = ltab[MDE_COL_B(col)];
# else // MDE_USE_ALPHA_LOOKUPTABLE
	int a = alpha + 1;
	int red = (MDE_COL_R(col)*a)>>8;
	int green = (MDE_COL_G(col)*a)>>8;
	int blue = (MDE_COL_B(col)*a)>>8;
# endif // MDE_USE_ALPHA_LOOKUPTABLE
	col = MDE_RGBA(red, green, blue, alpha);
#endif // USE_PREMULTIPLIED_ALPHA
	if (MDE_COL_A(col) > 0)
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *col8 = (unsigned char *) &col;
		for(int x = 0; x < len; x++)
		{
			SetPixelWithDestAlpha_BGRA32(dst8, col8, col8[MDE_COL_OFS_A]);
			dst8 += 4;
		}
	}
}

#ifdef MDE_SUPPORT_ARGB32

#ifdef USE_PREMULTIPLIED_ALPHA
static LIBGOGI_FORCEINLINE void SetPixel_ARGB32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (srca == 0)
		return;
	dst8[MDE_COL_OFS_A] = ((dst8[MDE_COL_OFS_A]*(256-srca))>>8) + src8[MDE_COL_OFS_B];
	dst8[MDE_COL_OFS_R] = ((dst8[MDE_COL_OFS_R]*(256-srca))>>8) + src8[MDE_COL_OFS_G];
	dst8[MDE_COL_OFS_G] = ((dst8[MDE_COL_OFS_G]*(256-srca))>>8) + src8[MDE_COL_OFS_R];
	dst8[MDE_COL_OFS_B] = ((dst8[MDE_COL_OFS_B]*(256-srca))>>8) + src8[MDE_COL_OFS_A];
}
#define SetPixelWithDestAlpha_ARGB32 SetPixel_ARGB32
#else // !USE_PREMULTIPLIED_ALPHA
static LIBGOGI_FORCEINLINE void SetPixel_ARGB32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (srca == 0)
		return;
	int alpha = srca + 1;
	dst8[MDE_COL_OFS_A] = dst8[MDE_COL_OFS_A] + (alpha * (src8[MDE_COL_OFS_B] - dst8[MDE_COL_OFS_A]) >> 8);
	dst8[MDE_COL_OFS_R] = dst8[MDE_COL_OFS_R] + (alpha * (src8[MDE_COL_OFS_G] - dst8[MDE_COL_OFS_R]) >> 8);
	dst8[MDE_COL_OFS_G] = dst8[MDE_COL_OFS_G] + (alpha * (src8[MDE_COL_OFS_R] - dst8[MDE_COL_OFS_G]) >> 8);
}

static LIBGOGI_FORCEINLINE void SetPixelWithDestAlpha_ARGB32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (srca == 0)
		return;
	if (dst8[MDE_COL_OFS_B] == 0)
	{
		dst8[MDE_COL_OFS_B] = srca;
		dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_R];
		dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_G];
		dst8[MDE_COL_OFS_A] = src8[MDE_COL_OFS_B];
	}
	else
	{
		int total_alpha = MDE_MIN(dst8[MDE_COL_OFS_B] + srca, 255);
		int a = ((srca<<8) / (total_alpha + 1)) + 1;
		dst8[MDE_COL_OFS_B] += (unsigned char) (long((srca) * (256 - dst8[MDE_COL_OFS_B])) >> 8);
		if (a > 1)
		{
			dst8[MDE_COL_OFS_G] += a * (src8[MDE_COL_OFS_R] - dst8[MDE_COL_OFS_G]) >> 8;
			dst8[MDE_COL_OFS_R] += a * (src8[MDE_COL_OFS_G] - dst8[MDE_COL_OFS_R]) >> 8;
			dst8[MDE_COL_OFS_A] += a * (src8[MDE_COL_OFS_B] - dst8[MDE_COL_OFS_A]) >> 8;
		}
	}
}
#endif // !USE_PREMULTIPLIED_ALPHA

#endif // MDE_SUPPORT_ARGB32

#ifdef MDE_SUPPORT_RGBA32
#ifdef USE_PREMULTIPLIED_ALPHA
static LIBGOGI_FORCEINLINE void SetPixel_RGBA32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (srca == 0)
		return;
#ifdef MDE_USE_ALPHA_LOOKUPTABLE
	const UINT32 offs = (255 - srca) << 8;
	const unsigned char* ltab = g_opera->libgogi_module.lutbl_alpha + offs;
	dst8[MDE_COL_OFS_B] = ltab[dst8[MDE_COL_OFS_B]] + src8[MDE_COL_OFS_R];
	dst8[MDE_COL_OFS_G] = ltab[dst8[MDE_COL_OFS_G]] + src8[MDE_COL_OFS_G];
	dst8[MDE_COL_OFS_R] = ltab[dst8[MDE_COL_OFS_R]] + src8[MDE_COL_OFS_B];
	dst8[MDE_COL_OFS_A] = ltab[dst8[MDE_COL_OFS_A]] + src8[MDE_COL_OFS_A];
#else // MDE_USE_ALPHA_LOOKUPTABLE
	int i_alpha = 256 - srca;
	dst8[MDE_COL_OFS_B] = ((dst8[MDE_COL_OFS_B]*i_alpha)>>8) + src8[MDE_COL_OFS_R];
	dst8[MDE_COL_OFS_G] = ((dst8[MDE_COL_OFS_G]*i_alpha)>>8) + src8[MDE_COL_OFS_G];
	dst8[MDE_COL_OFS_R] = ((dst8[MDE_COL_OFS_R]*i_alpha)>>8) + src8[MDE_COL_OFS_B];
	dst8[MDE_COL_OFS_A] = ((dst8[MDE_COL_OFS_A]*i_alpha)>>8) + src8[MDE_COL_OFS_A];
#endif // MDE_USE_ALPHA_LOOKUPTABLE
}
#define SetPixelWithDestAlpha_RGBA32 SetPixel_RGBA32
#else // !USE_PREMULTIPLIED_ALPHA
static LIBGOGI_FORCEINLINE void SetPixel_RGBA32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (srca == 0)
		return;
	int alpha = srca + 1;
	dst8[MDE_COL_OFS_B] = dst8[MDE_COL_OFS_B] + (alpha * (src8[MDE_COL_OFS_R] - dst8[MDE_COL_OFS_B]) >> 8);
	dst8[MDE_COL_OFS_G] = dst8[MDE_COL_OFS_G] + (alpha * (src8[MDE_COL_OFS_G] - dst8[MDE_COL_OFS_G]) >> 8);
	dst8[MDE_COL_OFS_R] = dst8[MDE_COL_OFS_R] + (alpha * (src8[MDE_COL_OFS_B] - dst8[MDE_COL_OFS_R]) >> 8);
}

static LIBGOGI_FORCEINLINE void SetPixelWithDestAlpha_RGBA32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (srca == 0)
		return;
	if (dst8[MDE_COL_OFS_A] == 0)
	{
		dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_R];
		dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_G];
		dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_B];
		dst8[MDE_COL_OFS_A] = srca;
	}
	else
	{
#ifdef MDE_USE_ALPHA_LOOKUPTABLE
		int a = g_opera->libgogi_module.lutbl_alpha[srca + ((dst8[MDE_COL_OFS_A]) << 8)] + 1;
#else // MDE_USE_ALPHA_LOOKUPTABLE
		int total_alpha = MDE_MIN(dst8[MDE_COL_OFS_A] + srca, 255);
		int a = ((srca<<8) / (total_alpha + 1)) + 1;
#endif // MDE_USE_ALPHA_LOOKUPTABLE
		if (a > 1)
		{
			dst8[MDE_COL_OFS_B] += a * (src8[MDE_COL_OFS_R] - dst8[MDE_COL_OFS_B]) >> 8;
			dst8[MDE_COL_OFS_G] += a * (src8[MDE_COL_OFS_G] - dst8[MDE_COL_OFS_G]) >> 8;
			dst8[MDE_COL_OFS_R] += a * (src8[MDE_COL_OFS_B] - dst8[MDE_COL_OFS_R]) >> 8;
		}
		dst8[MDE_COL_OFS_A] += (unsigned char) (long((srca) * (256 - dst8[MDE_COL_OFS_A])) >> 8);
	}
}
#endif // !USE_PREMULTIPLIED_ALPHA
#endif // MDE_SUPPORT_RGBA32

bool MDE_Scanline_BlitNormal_BGRA32_To_BGRA32(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
#if 1
		if (len)
	    {
		int *idst = (int *)dst;
		int *isrc = (int *)src;
		switch(len & 7)
		{
		    while(len > 0)
		    {
			case 0: *idst++ = *isrc++;
			case 7: *idst++ = *isrc++;
			case 6: *idst++ = *isrc++;
			case 5: *idst++ = *isrc++;
			case 4: *idst++ = *isrc++;
			case 3: *idst++ = *isrc++;
			case 2: *idst++ = *isrc++;
			case 1: *idst++ = *isrc++;
   		        len-=8;
		    }
		}
	    }
#else
	    op_memcpy(dst, src, len * 4);
#endif
	    break;

	case MDE_METHOD_MASK:
		for(x = 0; x < len; x++)
			if (((unsigned int*)src)[x] != srcinf->mask)
				((unsigned int*)dst)[x] = ((unsigned int*)src)[x];
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;

			if (dstinf->method == MDE_METHOD_ALPHA)
				for(x = 0; x < len; x++)
				{
					SetPixelWithDestAlpha_BGRA32( dst8, src8, src8[MDE_COL_OFS_A] );
					dst8 += 4;
					src8 += 4;
				}
			else
				for(x = 0; x < len; x++)
				{
					SetPixel_BGRA32( dst8, src8, src8[MDE_COL_OFS_A] );
					dst8 += 4;
					src8 += 4;
				}
		}
		break;
	case MDE_METHOD_COLOR:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
			int alpha = MDE_COL_A(srcinf->col);
			if (alpha > 0)
			{
				++alpha;
				UINT32 c;
				unsigned char *col8 = (unsigned char *) &c;
				for(x = 0; x < len; x++)
				{
					c = PremultiplyColor(srcinf->col, (src8[x]*alpha)>>8);
					SetPixel_BGRA32( dst8, col8, src8[x] );
					dst8 += 4;
				}
			}
#else // !USE_PREMULTIPLIED_ALPHA
			unsigned char *col8 = (unsigned char *) &srcinf->col;
			int alpha = MDE_COL_A(srcinf->col);
			if (alpha == 255)
			{
				if (dstinf->method == MDE_METHOD_ALPHA)
					for(x = 0; x < len; x++)
					{
						SetPixelWithDestAlpha_BGRA32( dst8, col8, src8[MDE_COL_OFS_A] );
						dst8 += 4;
						src8 += 4;
					}
				else
					for(x = 0; x < len; x++)
					{
						SetPixel_BGRA32( dst8, col8, src8[MDE_COL_OFS_A] );
						dst8 += 4;
						src8 += 4;
					}
			}
			else if (alpha > 0)
			{
				if (dstinf->method == MDE_METHOD_ALPHA)
					for(x = 0; x < len; x++)
					{
						SetPixelWithDestAlpha_BGRA32( dst8, col8, (alpha*src8[MDE_COL_OFS_A])>>8 );
						dst8 += 4;
						src8 += 4;
					}
				else
					for(x = 0; x < len; x++)
					{
						SetPixel_BGRA32( dst8, col8,(alpha* src8[MDE_COL_OFS_A])>>8 );
						dst8 += 4;
						src8 += 4;
					}

			}
#endif // !USE_PREMULTIPLIED_ALPHA
		}
		break;
	case MDE_METHOD_OPACITY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			unsigned char srca = MDE_COL_A(srcinf->col);
			if (!srca)
				break;
#ifdef USE_PREMULTIPLIED_ALPHA
			if (srca == 255)
			{
				for(x = 0; x < len; x++)
				{
					SetPixel_BGRA32( dst8, src8, src8[MDE_COL_OFS_A] );
					dst8 += 4;
					src8 += 4;
				}
			}
			else if (srca > 0)
			{
				UINT32 c;
				unsigned char *col8 = (unsigned char *) &c;
				for(x = 0; x < len; x++)
				{
					c = ScalePremultipliedColor(*(UINT32*)src8, srca);
					SetPixel_BGRA32( dst8, col8, col8[MDE_COL_OFS_A] );
					dst8 += 4;
					src8 += 4;
				}
			}
#else // !USE_PREMULTIPLIED_ALPHA
			if (dstinf->method == MDE_METHOD_ALPHA)
				for(x = 0; x < len; x++)
				{
					int alpha = (srca * (src8[MDE_COL_OFS_A] + 1)) >> 8;
					if (dst8[MDE_COL_OFS_A] == 0)
					{
						dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_B];
						dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_G];
						dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_R];
						dst8[MDE_COL_OFS_A] = alpha;
					}
					else
					{
						int total_alpha = MDE_MIN(dst8[MDE_COL_OFS_A] + alpha, 255);
						int a = ((alpha<<8) / (total_alpha + 1)) + 1;
						if (a > 1)
						{
							dst8[MDE_COL_OFS_B] += a * (src8[MDE_COL_OFS_B] - dst8[MDE_COL_OFS_B]) >> 8;
							dst8[MDE_COL_OFS_G] += a * (src8[MDE_COL_OFS_G] - dst8[MDE_COL_OFS_G]) >> 8;
							dst8[MDE_COL_OFS_R] += a * (src8[MDE_COL_OFS_R] - dst8[MDE_COL_OFS_R]) >> 8;
						}
						dst8[MDE_COL_OFS_A] += (unsigned char) (long((alpha) * (256 - dst8[MDE_COL_OFS_A])) >> 8);
					}
					dst8 += 4;
					src8 += 4;
				}
			else
				for(x = 0; x < len; x++)
				{
					int alpha = (((int)srca * (src8[MDE_COL_OFS_A] + 1)) >> 8) + 1;
					if (alpha > 1)
					{
						dst8[MDE_COL_OFS_B] = dst8[MDE_COL_OFS_B] + (alpha * (src8[MDE_COL_OFS_B] - dst8[MDE_COL_OFS_B]) >> 8);
						dst8[MDE_COL_OFS_G] = dst8[MDE_COL_OFS_G] + (alpha * (src8[MDE_COL_OFS_G] - dst8[MDE_COL_OFS_G]) >> 8);
						dst8[MDE_COL_OFS_R] = dst8[MDE_COL_OFS_R] + (alpha * (src8[MDE_COL_OFS_R] - dst8[MDE_COL_OFS_R]) >> 8);
					}
					dst8 += 4;
					src8 += 4;
				}
#endif // !USE_PREMULTIPLIED_ALPHA
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_BGRA32_To_BGRA32(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		for(x = 0; x < len; x++)
		{
			((unsigned int *)dst)[x] = ((unsigned int *)src)[sx >> 16];
			sx += dx;
		}
		break;
	case MDE_METHOD_MASK:
		for(x = 0; x < len; x++)
		{
			unsigned int s = ((unsigned int *)src)[sx >> 16];
			if (s != srcinf->mask)
				((unsigned int*)dst)[x] = s;
			sx += dx;
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			if (dstinf->method == MDE_METHOD_ALPHA)
				for(x = 0; x < len; x++)
				{
					unsigned char *src8 = (unsigned char *) &((unsigned int *)src)[sx >> 16];
					SetPixelWithDestAlpha_BGRA32( dst8, src8, src8[MDE_COL_OFS_A] );
					dst8 += 4;
					sx += dx;
				}
			else
				for(x = 0; x < len; x++)
				{
					unsigned char *src8 = (unsigned char *) &((unsigned int *)src)[sx >> 16];
					SetPixel_BGRA32( dst8, src8, src8[MDE_COL_OFS_A] );
					dst8 += 4;
					sx += dx;
				}
		}
		break;
	case MDE_METHOD_COLOR:
		{
			unsigned char *dst8 = (unsigned char *) dst;
#ifdef USE_PREMULTIPLIED_ALPHA
			int alpha = MDE_COL_A(srcinf->col);
			if (alpha > 0)
			{
				++alpha;
				UINT32 c;
				unsigned char *col8 = (unsigned char *) &c;
				for(x = 0; x < len; x++)
				{
					unsigned char *src8 = (unsigned char *) &((unsigned int *)src)[sx >> 16];
					c = PremultiplyColor(srcinf->col, (src8[MDE_COL_OFS_A]*alpha)>>8);
					SetPixel_BGRA32( dst8, col8, col8[MDE_COL_OFS_A] );
					dst8 += 4;
					sx += dx;
				}
			}
#else // !USE_PREMULTIPLIED_ALPHA
			unsigned char *col8 = (unsigned char *) &srcinf->col;
			int alpha = MDE_COL_A(srcinf->col);
			if (alpha == 255)
			{
				if (dstinf->method == MDE_METHOD_ALPHA)
					for(x = 0; x < len; x++)
					{
						unsigned char *src8 = (unsigned char *) &((unsigned int *)src)[sx >> 16];
						SetPixelWithDestAlpha_BGRA32( dst8, col8, src8[MDE_COL_OFS_A] );
						dst8 += 4;
						sx += dx;
					}
				else
					for(x = 0; x < len; x++)
					{
						unsigned char *src8 = (unsigned char *) &((unsigned int *)src)[sx >> 16];
						SetPixel_BGRA32( dst8, col8, src8[MDE_COL_OFS_A] );
						dst8 += 4;
						sx += dx;
					}
			}
			else if (alpha > 0)
			{
				if (dstinf->method == MDE_METHOD_ALPHA)
					for(x = 0; x < len; x++)
					{
						unsigned char *src8 = (unsigned char *) &((unsigned int *)src)[sx >> 16];
						SetPixelWithDestAlpha_BGRA32( dst8, col8, (alpha*src8[MDE_COL_OFS_A])>>8 );
						dst8 += 4;
						sx += dx;
					}
				else
					for(x = 0; x < len; x++)
					{
						unsigned char *src8 = (unsigned char *) &((unsigned int *)src)[sx >> 16];
						SetPixel_BGRA32( dst8, col8, (alpha*src8[MDE_COL_OFS_A])>>8 );
						dst8 += 4;
						sx += dx;
					}
			}
#endif // !USE_PREMULTIPLIED_ALPHA
		}
		break;
	}
}

#ifdef MDE_BILINEAR_BLITSTRETCH
struct MDE_COLOR_BGRA32
{
	unsigned int blue : 8;
	unsigned int green : 8;
	unsigned int red : 8;
	unsigned int alpha : 8;
};
void MDE_BilinearInterpolationX_BGRA32(void* dst, void* src, int dstlen, int srclen, MDE_F1616 sx, MDE_F1616 dx)
{
	MDE_COLOR_BGRA32* dstdata = (MDE_COLOR_BGRA32*)dst;
	MDE_COLOR_BGRA32* srcdata = (MDE_COLOR_BGRA32*)src;
	MDE_BILINEAR_INTERPOLATION_X_IMPL(dstdata, srcdata, dstlen, srclen, sx, dx)
}
void MDE_BilinearInterpolationY_BGRA32(void* dst, void* src1, void* src2, int len, MDE_F1616 sy)
{
	MDE_COLOR_BGRA32* dstdata = (MDE_COLOR_BGRA32*)dst;
	MDE_COLOR_BGRA32* src1data = (MDE_COLOR_BGRA32*)src1;
	MDE_COLOR_BGRA32* src2data = (MDE_COLOR_BGRA32*)src2;
	MDE_BILINEAR_INTERPOLATION_Y_IMPL(dstdata, src1data, src2data, len, sy)
}
#endif // MDE_BILINEAR_BLITSTRETCH


#ifdef MDE_SUPPORT_BGR24

bool MDE_Scanline_BlitNormal_BGR24_To_BGR24(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		op_memcpy(dst, src, len*3);
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_BGR24_To_BGR24(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned int ofs = (sx >> 16) * 3;
				dst8[0] = src8[ofs + 0];
				dst8[1] = src8[ofs + 1];
				dst8[2] = src8[ofs + 2];
				dst8 += 3;
				sx += dx;
			}
		}
		break;
	}
}

bool MDE_Scanline_BlitNormal_RGBA32_To_BGR24(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY: {
		unsigned char *dst8 = (unsigned char *)dst;
		unsigned char *src8 = (unsigned char *)src;

#ifdef USE_PREMULTIPLIED_ALPHA
		OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
		for (x = 0; x<len;x++) {
			dst8[0] = src8[MDE_COL_OFS_R];
			dst8[1] = src8[MDE_COL_OFS_G];
			dst8[2] = src8[MDE_COL_OFS_B];
			dst8+=3;
			src8+=4;
		}
		break;
	}
	case MDE_METHOD_MASK:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++)
		{
			if (!(src8[MDE_COL_OFS_B] == MDE_COL_R(srcinf->mask) &&
				  src8[MDE_COL_OFS_G] == MDE_COL_G(srcinf->mask) &&
				  src8[MDE_COL_OFS_R] == MDE_COL_B(srcinf->mask)))
			{
#ifdef USE_PREMULTIPLIED_ALPHA
				OP_ASSERT(src8[MDE_COL_OFS_A] == 255);
#endif // !USE_PREMULTIPLIED_ALPHA
				dst8[0] = src8[MDE_COL_OFS_R];
				dst8[1] = src8[MDE_COL_OFS_G];
				dst8[2] = src8[MDE_COL_OFS_B];
			}
			dst8 += 3;
			src8 += 4;
		}
		break;
	}
	case MDE_METHOD_ALPHA:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++,src8+=4,dst8+=3)
		{
			if (src8[MDE_COL_OFS_A] == 0)
			{
				continue;
			}
			if (src8[MDE_COL_OFS_A] == 255)
			{
				dst8[0] = src8[MDE_COL_OFS_R];
				dst8[1] = src8[MDE_COL_OFS_G];
				dst8[2] = src8[MDE_COL_OFS_B];
				continue;
			}
			unsigned char dst_b = dst8[2];
			unsigned char dst_g = dst8[1];
			unsigned char dst_r = dst8[0];
#ifdef USE_PREMULTIPLIED_ALPHA
			int tmpa = 256 - src8[MDE_COL_OFS_A];
			dst8[2] = ((dst_r*tmpa)>>8)+src8[MDE_COL_OFS_R];
			dst8[1] = ((dst_g*tmpa)>>8)+src8[MDE_COL_OFS_G];
			dst8[0] = ((dst_b*tmpa)>>8)+src8[MDE_COL_OFS_B];
#else // !USE_PREMULTIPLIED_ALPHA
			int tmpa = src8[MDE_COL_OFS_A] + 1;
			dst8[0] = dst_b+((tmpa * (src8[MDE_COL_OFS_R]-dst_b)) >> 8);
			dst8[1] = dst_g+((tmpa * (src8[MDE_COL_OFS_G]-dst_g)) >> 8);
			dst8[2] = dst_r+((tmpa * (src8[MDE_COL_OFS_B]-dst_r)) >> 8);
#endif // !USE_PREMULTIPLIED_ALPHA

		}
		break;
	}
	case MDE_METHOD_COLOR:
	{
		// Only alpha from source is used, and the color used is not premultiplied -> no need to premultiply
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		unsigned char *col8 = (unsigned char *) &srcinf->col;
		int alpha = MDE_COL_A(srcinf->col);
		if (alpha == 255)
		{
			const int srca = src8[MDE_COL_OFS_A] + 1;
			for(x = 0; x < len; x++)
			{
				if (src8[MDE_COL_OFS_A] != 0)
				{
					dst8[0] = dst8[0] + (srca * (col8[MDE_COL_OFS_R] - dst8[0]) >> 8);
					dst8[1] = dst8[1] + (srca * (col8[MDE_COL_OFS_G] - dst8[1]) >> 8);
					dst8[2] = dst8[2] + (srca * (col8[MDE_COL_OFS_B] - dst8[2]) >> 8);
				}
				dst8 += 3;
				src8 += 4;
			}
		}
		else if (alpha > 0)
		{
			++alpha;
			for(x = 0; x < len; x++)
			{
				if (src8[MDE_COL_OFS_A] != 0)
				{
					int tmpa = ((alpha*src8[MDE_COL_OFS_A]) >> 8) + 1;
					dst8[0] = dst8[0] + (tmpa * (col8[MDE_COL_OFS_R] - dst8[0]) >> 8);
					dst8[1] = dst8[1] + (tmpa * (col8[MDE_COL_OFS_G] - dst8[1]) >> 8);
					dst8[2] = dst8[2] + (tmpa * (col8[MDE_COL_OFS_B] - dst8[2]) >> 8);
				}
				dst8 += 3;
				src8 += 4;
			}
		}
		break;
	}
	}
	return true;
}

bool MDE_Scanline_BlitNormal_BGRA32_To_BGR24(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY: {
		unsigned char *dst8 = (unsigned char *)dst;
		unsigned char *src8 = (unsigned char *)src;

#ifdef USE_PREMULTIPLIED_ALPHA
		OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
		for (x = 0; x<len;x++) {
			dst8[0] = src8[MDE_COL_OFS_B];
			dst8[1] = src8[MDE_COL_OFS_G];
			dst8[2] = src8[MDE_COL_OFS_R];
			dst8+=3;
			src8+=4;
		}
		break;
	}
	case MDE_METHOD_MASK:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++)
		{
			if (!(src8[MDE_COL_OFS_B] == MDE_COL_B(srcinf->mask) &&
				  src8[MDE_COL_OFS_G] == MDE_COL_G(srcinf->mask) &&
				  src8[MDE_COL_OFS_R] == MDE_COL_R(srcinf->mask)))
			{
#ifdef USE_PREMULTIPLIED_ALPHA
				OP_ASSERT(src8[MDE_COL_OFS_A] == 255);
#endif // !USE_PREMULTIPLIED_ALPHA
				dst8[0] = src8[MDE_COL_OFS_B];
				dst8[1] = src8[MDE_COL_OFS_G];
				dst8[2] = src8[MDE_COL_OFS_R];
			}
			dst8 += 3;
			src8 += 4;
		}
		break;
	}
	case MDE_METHOD_ALPHA:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++,src8+=4,dst8+=3)
		{
			if (src8[MDE_COL_OFS_A] == 0)
			{
				continue;
			}
			if (src8[MDE_COL_OFS_A] == 255)
			{
				dst8[0] = src8[MDE_COL_OFS_B];
				dst8[1] = src8[MDE_COL_OFS_G];
				dst8[2] = src8[MDE_COL_OFS_R];
				continue;
			}
			unsigned char dst_r = dst8[2];
			unsigned char dst_g = dst8[1];
			unsigned char dst_b = dst8[0];
#ifdef USE_PREMULTIPLIED_ALPHA
			dst8[2] = ((dst_r*(256-src8[MDE_COL_OFS_A]))>>8)+src8[MDE_COL_OFS_R];
			dst8[1] = ((dst_g*(256-src8[MDE_COL_OFS_A]))>>8)+src8[MDE_COL_OFS_G];
			dst8[0] = ((dst_b*(256-src8[MDE_COL_OFS_A]))>>8)+src8[MDE_COL_OFS_B];
#else // !USE_PREMULTIPLIED_ALPHA
			dst8[0]= dst_b+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_B]-dst_b)) >> 8);
			dst8[1] = dst_g+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_G]-dst_g)) >> 8);
			dst8[2] = dst_r+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_R]-dst_r)) >> 8);
#endif // !USE_PREMULTIPLIED_ALPHA

		}
		break;
	}
	case MDE_METHOD_COLOR:
	{
		// Only alpha from source is used, and the color used is not premultiplied -> no need to premultiply
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		unsigned char *col8 = (unsigned char *) &srcinf->col;
		int alpha = MDE_COL_A(srcinf->col);
		if (alpha == 255)
		{
			for(x = 0; x < len; x++)
			{
				if (src8[MDE_COL_OFS_A] != 0)
				{
					dst8[0] = dst8[0] + (src8[MDE_COL_OFS_A] * (col8[MDE_COL_OFS_B] - dst8[0]) >> 8);
					dst8[1] = dst8[1] + (src8[MDE_COL_OFS_A] * (col8[MDE_COL_OFS_G] - dst8[1]) >> 8);
					dst8[2] = dst8[2] + (src8[MDE_COL_OFS_A] * (col8[MDE_COL_OFS_R] - dst8[2]) >> 8);
				}
				dst8 += 3;
				src8 += 4;
			}
		}
		else if (alpha > 0)
		{
			++alpha;
			for(x = 0; x < len; x++)
			{
				if (src8[MDE_COL_OFS_A] != 0)
				{
					int tmpa = (alpha*src8[MDE_COL_OFS_A]) >> 8;
					dst8[0] = dst8[0] + (tmpa * (col8[MDE_COL_OFS_B] - dst8[0]) >> 8);
					dst8[1] = dst8[1] + (tmpa * (col8[MDE_COL_OFS_G] - dst8[1]) >> 8);
					dst8[2] = dst8[2] + (tmpa * (col8[MDE_COL_OFS_R] - dst8[2]) >> 8);
				}
				dst8 += 3;
				src8 += 4;
			}
		}
		break;
	}
	}
	return true;
}

void MDE_Scanline_BlitStretch_RGBA32_To_BGR24(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	case MDE_METHOD_COPY:
	{
		unsigned char *dst8 = (unsigned char *) dst;
#ifdef USE_PREMULTIPLIED_ALPHA
		OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
		for (x = 0; x < len; ++x, dst8+=3, sx+=dx)
		{
			unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
			dst8[0] = src8[MDE_COL_OFS_R];
			dst8[1] = src8[MDE_COL_OFS_G];
			dst8[2] = src8[MDE_COL_OFS_B];
		}
		break;
	}
	case MDE_METHOD_MASK:
		MDE_ASSERT(!"Implement BGRA32_To_BGR24 Mask");
		break;
	case MDE_METHOD_ALPHA:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		for(x = 0; x < len; x++, sx+=dx, dst8+=3)
		{
			unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
			unsigned char src_r = src8[MDE_COL_OFS_B];
			unsigned char src_g = src8[MDE_COL_OFS_G];
			unsigned char src_b = src8[MDE_COL_OFS_R];
			unsigned char src_a = src8[MDE_COL_OFS_A];

			if (src_a == 0)
			{
				continue;
			}
			if (src_a == 255)
			{
				dst8[0]=src_b;
				dst8[1]=src_g;
				dst8[2]=src_r;
				continue;
			}
			unsigned char dst_r = dst8[2];
			unsigned char dst_g = dst8[1];
			unsigned char dst_b = dst8[0];
#ifdef USE_PREMULTIPLIED_ALPHA
			dst8[2] = ((dst_r*(256-src_a))>>8)+src_r;
			dst8[1] = ((dst_g*(256-src_a))>>8)+src_g;
			dst8[0] = ((dst_b*(256-src_a))>>8)+src_b;
#else // !USE_PREMULTIPLIED_ALPHA
			++ src_a;
			dst8[0]=dst_b+((src_a * (src_b-dst_b)) >> 8);
			dst8[1]=dst_g+((src_a * (src_g-dst_g)) >> 8);
			dst8[2]=dst_r+((src_a * (src_r-dst_r)) >> 8);
#endif // !USE_PREMULTIPLIED_ALPHA
		}
		break;
	}
	case MDE_METHOD_COLOR:
		MDE_ASSERT(!"Implement BGRA32_To_BGR24 Color");
		break;
	}
}

void MDE_Scanline_BlitStretch_BGRA32_To_BGR24(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	case MDE_METHOD_COPY:
	{
		unsigned char *dst8 = (unsigned char *) dst;
#ifdef USE_PREMULTIPLIED_ALPHA
		OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
		for (x = 0; x < len; ++x, dst8+=3, sx+=dx)
		{
			unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
			dst8[0] = src8[MDE_COL_OFS_B];
			dst8[1] = src8[MDE_COL_OFS_G];
			dst8[2] = src8[MDE_COL_OFS_R];
		}
		break;
	}
	case MDE_METHOD_MASK:
		MDE_ASSERT(!"Implement BGRA32_To_BGR24 Mask");
		break;
	case MDE_METHOD_ALPHA:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		for(x = 0; x < len; x++, sx+=dx, dst8+=3)
		{
			unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
			unsigned char src_r = src8[MDE_COL_OFS_R];
			unsigned char src_g = src8[MDE_COL_OFS_G];
			unsigned char src_b = src8[MDE_COL_OFS_B];
			unsigned char src_a = src8[MDE_COL_OFS_A];

			if (src_a == 0)
			{
				continue;
			}
			if (src_a == 255)
			{
				dst8[0]=src_b;
				dst8[1]=src_g;
				dst8[2]=src_r;
				continue;
			}
			unsigned char dst_r = dst8[2];
			unsigned char dst_g = dst8[1];
			unsigned char dst_b = dst8[0];
#ifdef USE_PREMULTIPLIED_ALPHA
			dst8[2] = ((dst_r*(256-src_a))>>8)+src_r;
			dst8[1] = ((dst_g*(256-src_a))>>8)+src_g;
			dst8[0] = ((dst_b*(256-src_a))>>8)+src_b;
#else // !USE_PREMULTIPLIED_ALPHA
			dst8[0]=dst_b+((src_a * (src_b-dst_b)) >> 8);
			dst8[1]=dst_g+((src_a * (src_g-dst_g)) >> 8);
			dst8[2]=dst_r+((src_a * (src_r-dst_r)) >> 8);
#endif // !USE_PREMULTIPLIED_ALPHA
		}
		break;
	}
	case MDE_METHOD_COLOR:
		MDE_ASSERT(!"Implement BGRA32_To_BGR24 Color");
		break;
	}
}
#ifdef MDE_BILINEAR_BLITSTRETCH
struct MDE_COLOR_BGR24
{
	unsigned int blue : 8;
	unsigned int green : 8;
	unsigned int red : 8;
};
void MDE_BilinearInterpolationX_BGR24(void* dst, void* src, int dstlen, int srclen, MDE_F1616 sx, MDE_F1616 dx)
{
	MDE_COLOR_BGR24* dstdata = (MDE_COLOR_BGR24*)dst;
	MDE_COLOR_BGR24* srcdata = (MDE_COLOR_BGR24*)src;
	MDE_BILINEAR_INTERPOLATION_X_NO_ALPHA_IMPL(dstdata, srcdata, dstlen, srclen, sx, dx)
}
void MDE_BilinearInterpolationY_BGR24(void* dst, void* src1, void* src2, int len, MDE_F1616 sy)
{
	MDE_COLOR_BGR24* dstdata = (MDE_COLOR_BGR24*)dst;
	MDE_COLOR_BGR24* src1data = (MDE_COLOR_BGR24*)src1;
	MDE_COLOR_BGR24* src2data = (MDE_COLOR_BGR24*)src2;
	MDE_BILINEAR_INTERPOLATION_Y_NO_ALPHA_IMPL(dstdata, src1data, src2data, len, sy)
}
#endif // MDE_BILINEAR_BLITSTRETCH

#endif // MDE_SUPPORT_BGR24

#ifdef MDE_SUPPORT_RGB16

bool MDE_Scanline_BlitNormal_BGRA32_To_RGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
		OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
		for(x = 0; x < len; x++)
		{
			dst16[x] = MDE_RGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]);
			src8 += 4;
		}
		break;
	}
	case MDE_METHOD_MASK:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++)
		{
			if (!(src8[MDE_COL_OFS_B] == MDE_COL_B(srcinf->mask) &&
				  src8[MDE_COL_OFS_G] == MDE_COL_G(srcinf->mask) &&
				  src8[MDE_COL_OFS_R] == MDE_COL_R(srcinf->mask)))
			{
#ifdef USE_PREMULTIPLIED_ALPHA
				OP_ASSERT(src8[MDE_COL_OFS_A] == 255);
#endif // !USE_PREMULTIPLIED_ALPHA
				dst16[x] = MDE_RGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]);
			}
			src8 += 4;
		}
		break;
	}
	case MDE_METHOD_ALPHA:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++, src8+=4)
		{
			if (src8[MDE_COL_OFS_A] == 0)
			{
				continue;
			}
			if (src8[MDE_COL_OFS_A] == 255)
			{
				dst16[x] = MDE_RGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]);
				continue;
			}
			unsigned char dst_r = MDE_COL_R16(dst16[x]);
			unsigned char dst_g = MDE_COL_G16(dst16[x]);
			unsigned char dst_b = MDE_COL_B16(dst16[x]);
#ifdef USE_PREMULTIPLIED_ALPHA
			dst16[x] = MDE_RGB16(
				((dst_r*(256-src8[MDE_COL_OFS_A]))>>8) + src8[MDE_COL_OFS_R],
				((dst_g*(256-src8[MDE_COL_OFS_A]))>>8) + src8[MDE_COL_OFS_G],
				((dst_b*(256-src8[MDE_COL_OFS_A]))>>8) + src8[MDE_COL_OFS_B]);
#else // !USE_PREMULTIPLIED_ALPHA
			dst16[x] = MDE_RGB16(
				dst_r+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_R]-dst_r)) >> 8),
				dst_g+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_G]-dst_g)) >> 8),
				dst_b+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_B]-dst_b)) >> 8)
				);
#endif // !USE_PREMULTIPLIED_ALPHA
		}
		break;
	}
	case MDE_METHOD_COLOR:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
		unsigned char *col8 = (unsigned char *) &srcinf->col;

		for(x = 0; x < len; x++, src8+=4)
		{
			if (src8[MDE_COL_OFS_A] == 0)
				continue;

			unsigned char dst_r = MDE_COL_R16(dst16[x]);
			unsigned char dst_g = MDE_COL_G16(dst16[x]);
			unsigned char dst_b = MDE_COL_B16(dst16[x]);
			dst16[x] = MDE_RGB16(
				dst_r+((src8[MDE_COL_OFS_A] * (col8[MDE_COL_OFS_R]-dst_r)) >> 8),
				dst_g+((src8[MDE_COL_OFS_A] * (col8[MDE_COL_OFS_G]-dst_g)) >> 8),
				dst_b+((src8[MDE_COL_OFS_A] * (col8[MDE_COL_OFS_B]-dst_b)) >> 8)
				);
		}
		break;
	}


	}
	return true;
}

void MDE_Scanline_BlitStretch_BGRA32_To_RGB16(void *dst, void *src, int len,
											  MDE_F1616 sx, MDE_F1616 dx,
											  MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
#ifdef USE_PREMULTIPLIED_ALPHA
		OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for (x = 0; x < len; ++x, ++dst16, sx+=dx)
			{
				unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
				unsigned char src_r = src8[MDE_COL_OFS_R];
				unsigned char src_g = src8[MDE_COL_OFS_G];
				unsigned char src_b = src8[MDE_COL_OFS_B];
				*dst16 = (unsigned short)MDE_RGB16(src_r, src_g, src_b);
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			for(x = 0; x < len; x++, sx+=dx)
			{
				unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
				unsigned char src_r = src8[MDE_COL_OFS_R];
				unsigned char src_g = src8[MDE_COL_OFS_G];
				unsigned char src_b = src8[MDE_COL_OFS_B];
				unsigned char src_a = src8[MDE_COL_OFS_A];

				if (src_a == 0)
				{
					continue;
				}
				if (src_a == 255)
				{
					dst16[x] = MDE_RGB16(src_r, src_g, src_b);
					continue;
				}
				unsigned char dst_r = MDE_COL_R16(dst16[x]);
				unsigned char dst_g = MDE_COL_G16(dst16[x]);
				unsigned char dst_b = MDE_COL_B16(dst16[x]);
#ifdef USE_PREMULTIPLIED_ALPHA
				dst16[x] = MDE_RGB16(
					((dst_r*(256-src_a))>>8) + src_r,
					((dst_g*(256-src_a))>>8) + src_g,
					((dst_b*(256-src_a))>>8) + src_b);
#else // !USE_PREMULTIPLIED_ALPHA
				dst16[x] = MDE_RGB16(
							dst_r+((src_a * (src_r-dst_r)) >> 8),
							dst_g+((src_a * (src_g-dst_g)) >> 8),
							dst_b+((src_a * (src_b-dst_b)) >> 8)
							);
#endif // !USE_PREMULTIPLIED_ALPHA
			}
		}
		break;
	}
}

#ifdef MDE_BILINEAR_BLITSTRETCH
struct MDE_COLOR_RGB16
{
	UINT16 red : 5;
	UINT16 green : 6;
	UINT16 blue : 5;
};
// this is a compiletime assert to verify that the size of the
// MDE_COLOR_RGB16 struct is as expected. if this line generates a
// compilation error the interpolation code will have to be rewritten.
typedef char MDE_COLOR_RGB16_ASSERT[sizeof(MDE_COLOR_RGB16) == 2 ? 1 : -1];
void MDE_BilinearInterpolationX_RGB16(void* dst, void* src, int dstlen, int srclen, MDE_F1616 sx, MDE_F1616 dx)
{
	MDE_COLOR_RGB16* dstdata = (MDE_COLOR_RGB16*)dst;
	MDE_COLOR_RGB16* srcdata = (MDE_COLOR_RGB16*)src;
	MDE_BILINEAR_INTERPOLATION_X_NO_ALPHA_IMPL(dstdata, srcdata, dstlen, srclen, sx, dx)
}
void MDE_BilinearInterpolationY_RGB16(void* dst, void* src1, void* src2, int len, MDE_F1616 sy)
{
	MDE_COLOR_RGB16* dstdata = (MDE_COLOR_RGB16*)dst;
	MDE_COLOR_RGB16* src1data = (MDE_COLOR_RGB16*)src1;
	MDE_COLOR_RGB16* src2data = (MDE_COLOR_RGB16*)src2;
	MDE_BILINEAR_INTERPOLATION_Y_NO_ALPHA_IMPL(dstdata, src1data, src2data, len, sy)
}
#endif // MDE_BILINEAR_BLITSTRETCH

#endif // MDE_SUPPORT_RGB16

// == RGBA24 ==============================================================

#ifdef MDE_SUPPORT_RGBA24

#define MDE_RGBA24_OFS_A 2

// FIXME There should probably be a tweak here to do the best thing also on
// misalignment-safe platforms.
//#define MDE_USHORT(char_p) *((unsigned short*) char_p)
//#define MDE_SET_USHORT(char_p, val) ((MDE_USHORT(char_p)) = (val))

/**
 * Since the RGBA24 format has 3-byte pixels, every other 16-bit color value
 * will be misaligned which causes occasional misalignment exceptions.
 *
 * So we use these inline functions instead for an alignment-safe
 * implementations.
 */

/**
* Description: alignment-safe casting from an array of two unsigned characters 
* to a single unsigned short.
*
* @param [in] char_p pointer to two-element char array that is to be converted
*             to unsigned short
* @return unsigned short which is the result of conversion
**/
LIBGOGI_FORCEINLINE unsigned short MDE_USHORT(unsigned char *char_p)
{
	unsigned short ret;
#if defined(OPERA_BIG_ENDIAN) || defined(SYSTEM_BIG_ENDIAN)
	ret = char_p[0];
	ret <<= 8;
	ret |= char_p[1];
#else
	ret = char_p[1];
	ret <<= 8;
	ret |= char_p[0];
#endif //OPERA_BIG_ENDIAN || SYSTEM_BIG_ENDIAN
	return ret;
}

/**
* Description: alignment-safe asignment which splits an unsigned short value
* into an array of two unsigned characters.
*
* @param [in] src an unsigned short value which is to be converted to a pair of
*             unsigned characters.
* @param [out] dst pointer to two-element char array that is to store the result of the
*              conversion from unsigned short. As a result of this function dst will
*              keep the value of src split into two unsigned characters.
**/
LIBGOGI_FORCEINLINE void MDE_SET_USHORT(unsigned char *dst, unsigned short src)
{
#if defined(OPERA_BIG_ENDIAN) || defined(SYSTEM_BIG_ENDIAN)
	dst[1] = (src & 0x00ff);
	dst[0] = ((src >> 8) & 0x00ff);
#else
	dst[0] = (src & 0x00ff);
	dst[1] = ((src >> 8) & 0x00ff);
#endif //OPERA_BIG_ENDIAN || SYSTEM_BIG_ENDIAN
}

bool MDE_Scanline_BlitNormal_INDEX8_To_RGBA24(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf = NULL)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for (x = 0; x < len; x++, dst8 += 3, src8++)
		{
			MDE_SET_USHORT(dst8, MDE_RGB16(srcinf->pal[3**src8], srcinf->pal[3**src8+1], srcinf->pal[3**src8+2]));

			dst8[MDE_RGBA24_OFS_A] = 0xff;
		}
		break;
	}
	case MDE_METHOD_MASK:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++, dst8 += 3, src8++)
		{
			if (*src8 != srcinf->mask)
			{
				MDE_SET_USHORT(dst8, MDE_RGB16(srcinf->pal[3**src8], srcinf->pal[3**src8+1], srcinf->pal[3**src8+2]));

				dst8[MDE_RGBA24_OFS_A] = 0xff;
			}
		}
		break;
	}
	}
	return true;
}

bool MDE_Scanline_BlitNormal_BGRA32_To_RGBA24(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf = NULL)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for (x = 0; x < len; x++, dst8 += 3, src8 += 4)
		{
			MDE_SET_USHORT(dst8, MDE_RGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]));

			dst8[MDE_RGBA24_OFS_A] = src8[MDE_COL_OFS_A];
		}
		break;
	}
	case MDE_METHOD_MASK:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++, dst8 += 3, src8 += 4)
		{
			if (*((unsigned int*)src8) != srcinf->mask)
			{
				MDE_SET_USHORT(dst8, MDE_RGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]));
				dst8[MDE_RGBA24_OFS_A] = src8[MDE_COL_OFS_A];
			}
		}
		break;
	}
	case MDE_METHOD_ALPHA:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;

		if (dstinf->method == MDE_METHOD_ALPHA)
		{
			for(x = 0; x < len; x++, dst8+=3, src8+=4)
			{
				if (src8[MDE_COL_OFS_A] == 0)
				{
					continue;
				}
				else if (dst8[MDE_RGBA24_OFS_A] == 0)
				{
					MDE_SET_USHORT(dst8, MDE_RGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]));

					dst8[MDE_RGBA24_OFS_A] = src8[MDE_COL_OFS_A];
				}
				else
				{
					int total_alpha = MDE_MIN(dst8[MDE_RGBA24_OFS_A] + src8[MDE_COL_OFS_A], 255);
					int a = (src8[MDE_COL_OFS_A] << 8) / total_alpha + 1;
					unsigned char dst_r = MDE_COL_R16(MDE_USHORT(dst8));
					unsigned char dst_g = MDE_COL_G16(MDE_USHORT(dst8));
					unsigned char dst_b = MDE_COL_B16(MDE_USHORT(dst8));

					MDE_SET_USHORT(dst8, MDE_RGB16(
						dst_r + (a * (src8[MDE_COL_OFS_R] - dst_r) >> 8),
						dst_g + (a * (src8[MDE_COL_OFS_G] - dst_g) >> 8),
						dst_b + (a * (src8[MDE_COL_OFS_B] - dst_b) >> 8)));

					dst8[MDE_RGBA24_OFS_A] += (unsigned char) (long((src8[MDE_COL_OFS_A]) * (256 - dst8[MDE_RGBA24_OFS_A])) >> 8);
				}

			}
		}
		else // dstinf->method != MDE_METHOD_ALPHA
		{
			for(x = 0; x < len; x++, dst8+=3, src8+=4)
			{
				if (src8[MDE_COL_OFS_A] == 0)
				{
					continue;
				}
				else if (src8[MDE_COL_OFS_A] == 255)
				{
					MDE_SET_USHORT(dst8, MDE_RGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]));
				}
				else
				{
					unsigned char dst_r = MDE_COL_R16(MDE_USHORT(dst8));
					unsigned char dst_g = MDE_COL_G16(MDE_USHORT(dst8));
					unsigned char dst_b = MDE_COL_B16(MDE_USHORT(dst8));
					int alpha = src8[MDE_COL_OFS_A] + 1;
					MDE_SET_USHORT(dst8, MDE_RGB16(
						dst_r + (alpha * (src8[MDE_COL_OFS_R] - dst_r) >> 8),
						dst_g + (alpha * (src8[MDE_COL_OFS_G] - dst_g) >> 8),
						dst_b + (alpha * (src8[MDE_COL_OFS_B] - dst_b) >> 8)));
				}
			}
		}
		break;
	}
	}
	return true;
}

void MDE_Scanline_BlitStretch_BGRA32_To_RGBA24(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf = NULL)
{
	int x;
	switch (srcinf->method)
	{
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for (x = 0; x < len; x++, dst8 += 3, sx += dx)
			{
				unsigned char *src = src8 + (sx>>16) * 4;

				MDE_SET_USHORT(dst8, MDE_RGB16(src[MDE_COL_OFS_R], src[MDE_COL_OFS_G], src[MDE_COL_OFS_B]));

				dst8[MDE_RGBA24_OFS_A] = src[MDE_COL_OFS_A];
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for (x = 0; x < len; x++, dst8 += 3, sx += dx)
			{
				unsigned char *src = src8 + (sx>>16) * 4;
				if (*((unsigned int*)src) != srcinf->mask)
				{
					MDE_SET_USHORT(dst8, MDE_RGB16(src[MDE_COL_OFS_R], src[MDE_COL_OFS_G], src[MDE_COL_OFS_B]));

					dst8[MDE_RGBA24_OFS_A] = src[MDE_COL_OFS_A];
				}
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char*) dst;
			unsigned char *src8 = (unsigned char*) src;
			if (dstinf->method == MDE_METHOD_ALPHA)
			{
				for (x = 0; x < len; x++, dst8 += 3, sx += dx)
				{
					unsigned char *src = src8 + (sx>>16) * 4;
					if (src[MDE_COL_OFS_A] == 0)
					{
						continue;
					}
					else if (dst8[MDE_RGBA24_OFS_A] == 0)
					{
						MDE_SET_USHORT(dst8, MDE_RGB16(src[MDE_COL_OFS_R], src[MDE_COL_OFS_G], src[MDE_COL_OFS_B]));

						dst8[MDE_RGBA24_OFS_A] = src[MDE_COL_OFS_A];
					}
					else
					{
						int total_alpha = MDE_MIN(dst8[MDE_RGBA24_OFS_A] + src[MDE_COL_OFS_A], 255);
						int a = (src[MDE_COL_OFS_A] << 8) / total_alpha + 1;
						unsigned char dst_r = MDE_COL_R16(MDE_USHORT(dst8));
						unsigned char dst_g = MDE_COL_G16(MDE_USHORT(dst8));
						unsigned char dst_b = MDE_COL_B16(MDE_USHORT(dst8));

						MDE_SET_USHORT(dst8, MDE_RGB16(
							dst_r + (a * (src[MDE_COL_OFS_R] - dst_r) >> 8),
							dst_g + (a * (src[MDE_COL_OFS_G] - dst_g) >> 8),
							dst_b + (a * (src[MDE_COL_OFS_B] - dst_b) >> 8)));

						dst8[MDE_RGBA24_OFS_A] += (unsigned char) (long((src[MDE_COL_OFS_A]) * (256 - dst8[MDE_RGBA24_OFS_A])) >> 8);
					}
				}
			}
			else // dstinf->method != MDE_METHOD_ALPHA
			{
				for (x = 0; x < len; x++, dst8 += 3, sx += dx)
				{
					unsigned char *src = src8 + (sx>>16) * 4;
					if (src[MDE_COL_OFS_A] == 0)
					{
						continue;
					}
					else if (src[MDE_COL_OFS_A] == 255)
					{
						MDE_SET_USHORT(dst8, MDE_RGB16(src[MDE_COL_OFS_R], src[MDE_COL_OFS_G], src[MDE_COL_OFS_B]));
					}
					else
					{
						unsigned char dst_r = MDE_COL_R16(MDE_USHORT(dst8));
						unsigned char dst_g = MDE_COL_G16(MDE_USHORT(dst8));
						unsigned char dst_b = MDE_COL_B16(MDE_USHORT(dst8));
						int alpha = src[MDE_COL_OFS_A] + 1;

						MDE_SET_USHORT(dst8, MDE_RGB16(
							dst_r + (alpha * (src[MDE_COL_OFS_R] - dst_r) >> 8),
							dst_g + (alpha * (src[MDE_COL_OFS_G] - dst_g) >> 8),
							dst_b + (alpha * (src[MDE_COL_OFS_B] - dst_b) >> 8)));
					}
				}
			}
		}
		break;
	default:
		MDE_ASSERT(len == 0);
		break;
	}
}

bool MDE_Scanline_BlitNormal_RGBA24_To_BGRA32(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf = NULL)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for (x = 0; x < len; x++, dst8 += 4, src8 += 3)
			{
				unsigned short c = MDE_USHORT(src8);
				dst8[MDE_COL_OFS_R] = MDE_COL_R16(c);
				dst8[MDE_COL_OFS_G] = MDE_COL_G16(c);
				dst8[MDE_COL_OFS_B] = MDE_COL_B16(c);
				dst8[MDE_COL_OFS_A] = src8[MDE_RGBA24_OFS_A];
			}
			break;
		}
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++, dst8 += 4, src8 += 3)
			{
				unsigned short c = MDE_USHORT(src8);
				if (c != srcinf->mask)
				{
					dst8[MDE_COL_OFS_R] = MDE_COL_R16(c);
					dst8[MDE_COL_OFS_G] = MDE_COL_G16(c);
					dst8[MDE_COL_OFS_B] = MDE_COL_B16(c);
					dst8[MDE_COL_OFS_A] = src8[MDE_RGBA24_OFS_A];
				}
			}
			break;
		}
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;

			if (dstinf->method == MDE_METHOD_ALPHA)
			{
				for(x = 0; x < len; x++, dst8+=4, src8+=3)
				{
					if (src8[MDE_RGBA24_OFS_A] == 0)
					{
						continue;
					}
					else if (dst8[MDE_COL_OFS_A] == 0)
					{
						unsigned short c = MDE_USHORT(src8);
						dst8[MDE_COL_OFS_R] = MDE_COL_R16(c);
						dst8[MDE_COL_OFS_G] = MDE_COL_G16(c);
						dst8[MDE_COL_OFS_B] = MDE_COL_B16(c);
						dst8[MDE_COL_OFS_A] = src8[MDE_RGBA24_OFS_A];
					}
					else
					{
						int total_alpha = MDE_MIN(dst8[MDE_COL_OFS_A] + src8[MDE_RGBA24_OFS_A], 255);
						int a = (src8[MDE_RGBA24_OFS_A] << 8) / total_alpha + 1;
						unsigned short c = MDE_USHORT(src8);

						dst8[MDE_COL_OFS_R] += (a * (MDE_COL_R16(c) - dst8[MDE_COL_OFS_R]) >> 8);
						dst8[MDE_COL_OFS_G] += (a * (MDE_COL_G16(c) - dst8[MDE_COL_OFS_G]) >> 8);
						dst8[MDE_COL_OFS_B] += (a * (MDE_COL_B16(c) - dst8[MDE_COL_OFS_B]) >> 8);
						dst8[MDE_COL_OFS_A] += (unsigned char) (long((src8[MDE_RGBA24_OFS_A]) * (256 - dst8[MDE_COL_OFS_A])) >> 8);
					}
				}
			}
			else // dstinf->method != MDE_METHOD_ALPHA
			{
				for(x = 0; x < len; x++, dst8 += 4, src8 += 3)
				{
					if (src8[MDE_RGBA24_OFS_A] == 0)
					{
						continue;
					}
					else if (src8[MDE_RGBA24_OFS_A] == 255)
					{
						unsigned short c = MDE_USHORT(src8);
						dst8[MDE_COL_OFS_R] = MDE_COL_R16(c);
						dst8[MDE_COL_OFS_G] = MDE_COL_G16(c);
						dst8[MDE_COL_OFS_B] = MDE_COL_B16(c);
					}
					else
					{
						int alpha = src8[MDE_RGBA24_OFS_A] + 1;
						unsigned short c = MDE_USHORT(src8);

						dst8[MDE_COL_OFS_R] += (alpha * (MDE_COL_R16(c) - dst8[MDE_COL_OFS_R]) >> 8);
						dst8[MDE_COL_OFS_G] += (alpha * (MDE_COL_G16(c) - dst8[MDE_COL_OFS_G]) >> 8);
						dst8[MDE_COL_OFS_B] += (alpha * (MDE_COL_B16(c) - dst8[MDE_COL_OFS_B]) >> 8);
					}
				}
			}
			break;
		}
	}
	return TRUE;
}

void MDE_Scanline_BlitStretch_RGBA24_To_BGRA32(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf = NULL)
{
	int x;
	switch (srcinf->method)
	{
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for (x = 0; x < len; x++, dst8 += 4, sx += dx)
			{
				unsigned char *src = src8 + (sx>>16) * 3;
				unsigned short c = MDE_USHORT(src);
				dst8[MDE_COL_OFS_R] = MDE_COL_R16(c);
				dst8[MDE_COL_OFS_G] = MDE_COL_G16(c);
				dst8[MDE_COL_OFS_B] = MDE_COL_B16(c);
				dst8[MDE_COL_OFS_A] = src[MDE_RGBA24_OFS_A];
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for (x = 0; x < len; x++, dst8 += 4, sx += dx)
			{
				unsigned char *src = src8 + (sx>>16) * 3;
				unsigned short c = MDE_USHORT(src);
				if (c != srcinf->mask)
				{
					dst8[MDE_COL_OFS_R] = MDE_COL_R16(c);
					dst8[MDE_COL_OFS_G] = MDE_COL_G16(c);
					dst8[MDE_COL_OFS_B] = MDE_COL_B16(c);
					dst8[MDE_COL_OFS_A] = src[MDE_RGBA24_OFS_A];
				}
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char*) dst;
			unsigned char *src8 = (unsigned char*) src;
			if (dstinf->method == MDE_METHOD_ALPHA)
			{
				for (x = 0; x < len; x++, dst8 += 4, sx += dx)
				{
					unsigned char *src = src8 + (sx>>16) * 3;
					if (src[MDE_RGBA24_OFS_A] == 0)
					{
						continue;
					}
					else if (dst8[MDE_COL_OFS_A] == 0)
					{
						unsigned short c = MDE_USHORT(src);
						dst8[MDE_COL_OFS_R] = MDE_COL_R16(c);
						dst8[MDE_COL_OFS_G] = MDE_COL_G16(c);
						dst8[MDE_COL_OFS_B] = MDE_COL_B16(c);
						dst8[MDE_COL_OFS_A] = src[MDE_RGBA24_OFS_A];
					}
					else
					{
						int total_alpha = MDE_MIN(dst8[MDE_COL_OFS_A] + src[MDE_RGBA24_OFS_A], 255);
						int a = (src[MDE_RGBA24_OFS_A] << 8) / total_alpha + 1;
						unsigned short c = MDE_USHORT(src);

						dst8[MDE_COL_OFS_R] += a * (MDE_COL_R16(c) - dst8[MDE_COL_OFS_R]) >> 8;
						dst8[MDE_COL_OFS_G] += a * (MDE_COL_G16(c) - dst8[MDE_COL_OFS_G]) >> 8;
						dst8[MDE_COL_OFS_B] += a * (MDE_COL_B16(c) - dst8[MDE_COL_OFS_B]) >> 8;
						dst8[MDE_COL_OFS_A] += (unsigned char) (long((src[MDE_RGBA24_OFS_A]) * (256 - dst8[MDE_COL_OFS_A])) >> 8);
					}
				}
			}
			else // dstinf->method != MDE_METHOD_ALPHA
			{
				for (x = 0; x < len; x++, dst8 += 4, sx += dx)
				{
					unsigned char *src = src8 + (sx>>16) * 3;
					if (src[MDE_RGBA24_OFS_A] == 0)
					{
						continue;
					}
					else if (src[MDE_RGBA24_OFS_A] == 255)
					{
						unsigned short c = MDE_USHORT(src);
						dst8[MDE_COL_OFS_R] = MDE_COL_R16(c);
						dst8[MDE_COL_OFS_G] = MDE_COL_G16(c);
						dst8[MDE_COL_OFS_B] = MDE_COL_B16(c);
					}
					else
					{
						int alpha = src[MDE_RGBA24_OFS_A] + 1;
						unsigned short c = MDE_USHORT(src);
						dst8[MDE_COL_OFS_R] += alpha * (MDE_COL_R16(c) - dst8[MDE_COL_OFS_R]) >> 8;
						dst8[MDE_COL_OFS_G] += alpha * (MDE_COL_G16(c) - dst8[MDE_COL_OFS_G]) >> 8;
						dst8[MDE_COL_OFS_B] += alpha * (MDE_COL_B16(c) - dst8[MDE_COL_OFS_B]) >> 8;
					}
				}
			}
		}
		break;
	default:
		MDE_ASSERT(len == 0);
		break;
	}
}

bool MDE_Scanline_BlitNormal_RGBA24_To_RGBA24(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf = NULL)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		op_memcpy(dst, src, len*3);
		break;
	case MDE_METHOD_ALPHA:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		if (dstinf->method == MDE_METHOD_ALPHA)
		{
			for (x = 0; x < len; x++, dst8 += 3, src8 += 3)
			{
				if (src8[MDE_RGBA24_OFS_A] == 0)
				{
					continue;
				}
				else if (dst8[MDE_RGBA24_OFS_A] == 0)
				{
					MDE_SET_USHORT(dst8, MDE_USHORT(src8));

					dst8[MDE_RGBA24_OFS_A] = src8[MDE_RGBA24_OFS_A];
				}
				else
				{
					int total_alpha = MDE_MIN(dst8[MDE_RGBA24_OFS_A] + src8[MDE_RGBA24_OFS_A], 255);
					int a = (src8[MDE_RGBA24_OFS_A] << 8) / total_alpha + 1;

					unsigned char dst_r = MDE_COL_R16(MDE_USHORT(dst8));
					unsigned char dst_g = MDE_COL_G16(MDE_USHORT(dst8));
					unsigned char dst_b = MDE_COL_B16(MDE_USHORT(dst8));

					unsigned short c = MDE_USHORT(src8);

					MDE_SET_USHORT(dst8, MDE_RGB16(
						dst_r + (a * (MDE_COL_R16(c) - dst_r) >> 8),
						dst_g + (a * (MDE_COL_G16(c) - dst_g) >> 8),
						dst_b + (a * (MDE_COL_B16(c) - dst_b) >> 8)));

					dst8[MDE_RGBA24_OFS_A] += (unsigned char) (long((src8[MDE_RGBA24_OFS_A]) * (256 - dst8[MDE_RGBA24_OFS_A])) >> 8);
				}
			}
		}
		else // dstinf->method != MDE_METHOD_ALPHA
		{
			for(x = 0; x < len; x++, dst8+=3, src8+=3)
			{
				if (src8[MDE_RGBA24_OFS_A] == 0)
				{
					continue;
				}
				else if (src8[MDE_RGBA24_OFS_A] == 255)
				{
					MDE_SET_USHORT(dst8, MDE_USHORT(src8));

					continue;
				}
				else
				{
					unsigned short c = MDE_USHORT(src8);
					unsigned char dst_r = MDE_COL_R16(MDE_USHORT(dst8));
					unsigned char dst_g = MDE_COL_G16(MDE_USHORT(dst8));
					unsigned char dst_b = MDE_COL_B16(MDE_USHORT(dst8));

					MDE_SET_USHORT(dst8, MDE_RGB16(
						dst_r+((src8[MDE_RGBA24_OFS_A] * (MDE_COL_R16(c)-dst_r)) >> 8),
						dst_g+((src8[MDE_RGBA24_OFS_A] * (MDE_COL_G16(c)-dst_g)) >> 8),
						dst_b+((src8[MDE_RGBA24_OFS_A] * (MDE_COL_B16(c)-dst_b)) >> 8)
						));
				}
			}
		}
		break;
	}
	}
	return true;
}

void MDE_Scanline_BlitStretch_RGBA24_To_RGBA24(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf = NULL)
{
	int x;
	switch (srcinf->method)
	{
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for (x = 0; x < len; x++, dst8 += 3, sx += dx)
			{
				unsigned char *src = src8 + (sx>>16) * 3;

				MDE_SET_USHORT(dst8, MDE_USHORT(src));

				dst8[MDE_RGBA24_OFS_A] = src[MDE_RGBA24_OFS_A];
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for (x = 0; x < len; x++, dst8 += 3, sx += dx)
			{
				unsigned char *src = src8 + (sx>>16) * 3;
				unsigned short c = MDE_USHORT(src);
				if (c != srcinf->mask)
				{
					MDE_SET_USHORT(dst8, MDE_USHORT(src));

					dst8[MDE_RGBA24_OFS_A] = src[MDE_RGBA24_OFS_A];
				}
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char*) dst;
			unsigned char *src8 = (unsigned char*) src;
			if (dstinf->method == MDE_METHOD_ALPHA)
			{
				for (x = 0; x < len; x++, dst8 += 3, sx += dx)
				{
					unsigned char *src = src8 + (sx>>16) * 3;
					if (src[MDE_RGBA24_OFS_A] == 0)
					{
						continue;
					}
					else if (dst8[MDE_RGBA24_OFS_A] == 0)
					{
						MDE_SET_USHORT(dst8, MDE_USHORT(src));

						dst8[MDE_RGBA24_OFS_A] = src[MDE_RGBA24_OFS_A];
					}
					else
					{
						int total_alpha = MDE_MIN(dst8[MDE_RGBA24_OFS_A] + src[MDE_RGBA24_OFS_A], 255);
						int a = (src[MDE_RGBA24_OFS_A] << 8) / total_alpha + 1;
						unsigned char dst_r = MDE_COL_R16(MDE_USHORT(dst8));
						unsigned char dst_g = MDE_COL_G16(MDE_USHORT(dst8));
						unsigned char dst_b = MDE_COL_B16(MDE_USHORT(dst8));
						unsigned short src_c = MDE_USHORT(src);

						MDE_SET_USHORT(dst8, MDE_RGB16(
							dst_r + (a * (MDE_COL_R16(src_c) - dst_r) >> 8),
							dst_g + (a * (MDE_COL_G16(src_c) - dst_g) >> 8),
							dst_b + (a * (MDE_COL_B16(src_c) - dst_b) >> 8)));

						dst8[MDE_RGBA24_OFS_A] += (unsigned char) (long((src[MDE_RGBA24_OFS_A]) * (256 - dst8[MDE_RGBA24_OFS_A])) >> 8);
					}
				}
			}
			else // dstinf->method != MDE_METHOD_ALPHA
			{
				for (x = 0; x < len; x++, dst8 += 3, sx += dx)
				{
					unsigned char *src = src8 + (sx>>16) * 3;
					if (src[MDE_RGBA24_OFS_A] == 0)
					{
						continue;
					}
					else if (src[MDE_RGBA24_OFS_A] == 255)
					{
						MDE_SET_USHORT(dst8, MDE_USHORT(src));
					}
					else
					{
						unsigned char dst_r = MDE_COL_R16(MDE_USHORT(dst8));
						unsigned char dst_g = MDE_COL_G16(MDE_USHORT(dst8));
						unsigned char dst_b = MDE_COL_B16(MDE_USHORT(dst8));
						int alpha = src[MDE_RGBA24_OFS_A] + 1;
						unsigned short c = MDE_USHORT(src);

						MDE_SET_USHORT(dst8, MDE_RGB16(
							dst_r + (alpha * (MDE_COL_R16(c) - dst_r) >> 8),
							dst_g + (alpha * (MDE_COL_G16(c) - dst_g) >> 8),
							dst_b + (alpha * (MDE_COL_B16(c) - dst_b) >> 8)));
					}
				}
			}
		}
		break;
	default:
		MDE_ASSERT(len == 0);
		break;
	}
}

bool MDE_Scanline_BlitNormal_RGBA24_To_RGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf = NULL)
{
	int x;
	switch(srcinf->method)
	{
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				*dst16 = MDE_USHORT(src8);
				++dst16;
				src8 += 3;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned short rgb = MDE_USHORT(src8);
				if (rgb != srcinf->mask)
				{
					*dst16 = rgb;
				}
				++dst16;
				src8 += 3;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for (x = 0; x < len; x++, dst16++, src8 += 3)
			{
				int alpha = src8[MDE_RGBA24_OFS_A];
				if (alpha == 0)
				{
					continue;
				}
				if (alpha == 255)
				{
					*dst16 = MDE_USHORT(src8);
				}
				else
				{
					unsigned char dst_r = MDE_COL_R16(*dst16);
					unsigned char dst_g = MDE_COL_G16(*dst16);
					unsigned char dst_b = MDE_COL_B16(*dst16);
					unsigned short c = MDE_USHORT(src8);

					++alpha;

					*dst16 = MDE_RGB16(
						dst_r + (alpha * (MDE_COL_R16(c) - dst_r) >> 8),
						dst_g + (alpha * (MDE_COL_G16(c) - dst_g) >> 8),
						dst_b + (alpha * (MDE_COL_B16(c) - dst_b) >> 8));
				}
			}
		}
		break;

	default:
		MDE_ASSERT(0);
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_RGBA24_To_RGB16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf = NULL)
{
	int x;
	switch(srcinf->method)
	{
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++, dst16++, sx += dx)
			{
				unsigned char *src = src8 + (sx >> 16) * 3;
				*dst16 =  MDE_USHORT(src);
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++, dst16++, sx += dx)
			{
				unsigned char *src = src8 + (sx >> 16) * 3;
				unsigned short rgb = MDE_USHORT(src);
				if (rgb != srcinf->mask)
				{
					*dst16 = rgb;
				}
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++, dst16++, sx += dx)
			{
				unsigned char *src = src8 + (sx >> 16) * 3;
				int alpha = src[MDE_RGBA24_OFS_A];
				if (alpha == 0)
				{
					continue;
				}
				if (alpha == 255)
				{
					*dst16 = MDE_USHORT(src);
					continue;
				}

				unsigned char dst_r = MDE_COL_R16(*dst16);
				unsigned char dst_g = MDE_COL_G16(*dst16);
				unsigned char dst_b = MDE_COL_B16(*dst16);
				unsigned short c = MDE_USHORT(src);

				++alpha;

				*dst16 = MDE_RGB16(
					dst_r + (alpha * (MDE_COL_R16(c) - dst_r) >> 8),
					dst_g + (alpha * (MDE_COL_G16(c) - dst_g) >> 8),
					dst_b + (alpha * (MDE_COL_B16(c) - dst_b) >> 8));
			}
		}
		break;

	default:
		MDE_ASSERT(0);
		break;
	}
}

static inline void SetPixelWithDestAlpha_RGBA24(unsigned char *dst8, unsigned char *src8, unsigned char srca)
{
	if (srca == 0)
		return;
	if (dst8[MDE_RGBA24_OFS_A] == 0)
	{
		MDE_SET_USHORT(dst8, MDE_RGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]));

		dst8[MDE_RGBA24_OFS_A] = srca;
	}
	else
	{
		unsigned short dst16 = MDE_USHORT(dst8);
		int total_alpha = MDE_MIN(dst8[MDE_RGBA24_OFS_A] + srca, 255);
		int a = (srca<<8) / (total_alpha + 1);

		MDE_SET_USHORT(dst8, MDE_RGB16(
			MDE_COL_R16(dst16) + ((a * (src8[MDE_COL_OFS_R] - MDE_COL_R16(dst16))) >> 8),
			MDE_COL_G16(dst16) + ((a * (src8[MDE_COL_OFS_G] - MDE_COL_G16(dst16))) >> 8),
			MDE_COL_B16(dst16) + ((a * (src8[MDE_COL_OFS_B] - MDE_COL_B16(dst16))) >> 8)));

		dst8[MDE_RGBA24_OFS_A] += (unsigned char) (long((srca) * (256 - dst8[MDE_RGBA24_OFS_A])) >> 8);
	}
}

void MDE_Scanline_SetColor_RGBA24_NoBlend(void *dst, int len, unsigned int col)
{
	unsigned char* dst8 = (unsigned char*)dst;
	for(int x = 0; x < len; x++, dst8 += 3)
	{
		MDE_SET_USHORT(dst8, MDE_RGB16(MDE_COL_R(col), MDE_COL_G(col), MDE_COL_B(col)));
		dst8[MDE_RGBA24_OFS_A] = MDE_COL_A(col);
	}
}

void MDE_Scanline_SetColor_RGBA24_Blend(void *dst, int len, unsigned int col)
{
	if (MDE_COL_A(col) == 255)
	{
		MDE_Scanline_SetColor_RGBA24_NoBlend(dst, len, col);
	}
	else if (MDE_COL_A(col) > 0)
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *col8 = (unsigned char *) &col;
		for(int x = 0; x < len; x++, dst8 += 3)
		{
			SetPixelWithDestAlpha_RGBA24(dst8, col8, MDE_COL_A(col));
		}
	}
}

#endif // MDE_SUPPORT_RGBA24

#ifdef MDE_SUPPORT_SRGB16

bool MDE_Scanline_BlitNormal_BGRA32_To_SRGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
		OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
		for(x = 0; x < len; x++)
		{
			dst16[x] = MDE_SRGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]);
			src8 += 4;
		}
		break;
	}
	case MDE_METHOD_MASK:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++)
		{
			if (!(src8[MDE_COL_OFS_B] == MDE_COL_B(srcinf->mask) &&
				  src8[MDE_COL_OFS_G] == MDE_COL_G(srcinf->mask) &&
				  src8[MDE_COL_OFS_R] == MDE_COL_R(srcinf->mask)))
			{
#ifdef USE_PREMULTIPLIED_ALPHA
				OP_ASSERT(src8[MDE_COL_OFS_A]);
#endif // USE_PREMULTIPLIED_ALPHA
				dst16[x] = MDE_SRGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]);
			}
			src8 += 4;
		}
		break;
	}
	case MDE_METHOD_ALPHA:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++, src8+=4)
		{
			if (src8[MDE_COL_OFS_A] == 0)
			{
				continue;
			}
			if (src8[MDE_COL_OFS_A] == 255)
			{
				dst16[x] = MDE_SRGB16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B]);
				continue;
			}
			unsigned char dst_r = MDE_COL_SR16(dst16[x]);
			unsigned char dst_g = MDE_COL_SG16(dst16[x]);
			unsigned char dst_b = MDE_COL_SB16(dst16[x]);
#ifdef USE_PREMULTIPLIED_ALPHA
			dst16[x] = MDE_SRGB16(
				((dst_r*(256-src8[MDE_COL_OFS_A]))>>8) + src8[MDE_COL_OFS_R],
				((dst_g*(256-src8[MDE_COL_OFS_A]))>>8) + src8[MDE_COL_OFS_G],
				((dst_b*(256-src8[MDE_COL_OFS_A]))>>8) + src8[MDE_COL_OFS_B]
				);
#else // !USE_PREMULTIPLIED_ALPHA
			dst16[x] = MDE_SRGB16(
				dst_r+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_R]-dst_r)) >> 8),
				dst_g+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_G]-dst_g)) >> 8),
				dst_b+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_B]-dst_b)) >> 8)
				);
#endif // !USE_PREMULTIPLIED_ALPHA
		}

		break;
	}
	}
	return true;
}

void MDE_Scanline_BlitStretch_BGRA32_To_SRGB16(void *dst, void *src, int len,
											   MDE_F1616 sx, MDE_F1616 dx,
											   MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
#ifdef USE_PREMULTIPLIED_ALPHA
			OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for (x = 0; x < len; ++x, ++dst16, sx+=dx)
			{
				unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
				unsigned char src_r = src8[MDE_COL_OFS_R];
				unsigned char src_g = src8[MDE_COL_OFS_G];
				unsigned char src_b = src8[MDE_COL_OFS_B];
				*dst16 = (unsigned short)MDE_SRGB16(src_r, src_g, src_b);
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			for(x = 0; x < len; x++, sx+=dx)
			{
				unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
				unsigned char src_r = src8[MDE_COL_OFS_R];
				unsigned char src_g = src8[MDE_COL_OFS_G];
				unsigned char src_b = src8[MDE_COL_OFS_B];
				unsigned char src_a = src8[MDE_COL_OFS_A];

				if (src_a == 0)
				{
					continue;
				}
				if (src_a == 255)
				{
					dst16[x] = MDE_SRGB16(src_r, src_g, src_b);
					continue;
				}
				unsigned char dst_r = MDE_COL_SR16(dst16[x]);
				unsigned char dst_g = MDE_COL_SG16(dst16[x]);
				unsigned char dst_b = MDE_COL_SB16(dst16[x]);
#ifdef USE_PREMULTIPLIED_ALPHA
				dst16[x] = MDE_SRGB16(
					((dst_r*(256-src_a))>>8) + src_r,
					((dst_g*(256-src_a))>>8) + src_g,
					((dst_b*(256-src_a))>>8) + src_b
					);
#else // !USE_PREMULTIPLIED_ALPHA

				dst16[x] = MDE_SRGB16(
							dst_r+((src_a * (src_r-dst_r)) >> 8),
							dst_g+((src_a * (src_g-dst_g)) >> 8),
							dst_b+((src_a * (src_b-dst_b)) >> 8)
							);
#endif // !USE_PREMULTIPLIED_ALPHA
			}
		}
		break;
	}
}

#endif // MDE_SUPPORT_SRGB16


#ifdef MDE_SUPPORT_RGBA16
bool MDE_Scanline_BlitNormal_BGRA32_To_RGBA16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
		OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
		for(x = 0; x < len; x++)
		{
			dst16[x] = MDE_RGBA16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B], src8[MDE_COL_OFS_A]);
			src8 += 4;
		}
		break;
	}
	case MDE_METHOD_MASK:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++)
		{
			if (!(src8[MDE_COL_OFS_B] == MDE_COL_B(srcinf->mask) &&
				  src8[MDE_COL_OFS_G] == MDE_COL_G(srcinf->mask) &&
				  src8[MDE_COL_OFS_R] == MDE_COL_R(srcinf->mask)))
			{
#ifdef USE_PREMULTIPLIED_ALPHA
				OP_ASSERT(src8[MDE_COL_OFS_A] == 255);
#endif // !USE_PREMULTIPLIED_ALPHA
				dst16[x] = MDE_RGBA16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B], src8[MDE_COL_OFS_A]);
			}
			src8 += 4;
		}
		break;
	}
	case MDE_METHOD_ALPHA:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++, src8+=4)
		{
			if (src8[MDE_COL_OFS_A] == 0)
			{
				continue;
			}
			if (src8[MDE_COL_OFS_A] == 255)
			{
				dst16[x] = MDE_RGBA16(src8[MDE_COL_OFS_R], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_B], src8[MDE_COL_OFS_A]);
				continue;
			}

			unsigned char dst_r = MDE_COL_R16A(dst16[x]);
			unsigned char dst_g = MDE_COL_G16A(dst16[x]);
			unsigned char dst_b = MDE_COL_B16A(dst16[x]);
			unsigned char dst_a = MDE_COL_A16A(dst16[x]);
			unsigned char src_a = src8[MDE_COL_OFS_A];
#ifdef USE_PREMULTIPLIED_ALPHA
			dst16[x] = MDE_RGBA16(
				((dst_r*(256-src_a))>>8) + src8[MDE_COL_OFS_R],
				((dst_g*(256-src_a))>>8) + src8[MDE_COL_OFS_G],
				((dst_b*(256-src_a))>>8) + src8[MDE_COL_OFS_B],
				((dst_a*(256-src_a))>>8) + src_a
				);
#else // !USE_PREMULTIPLIED_ALPHA
			int total_alpha = MDE_MIN(dst_a + src_a, 255);
			int a = ((src_a<<8) / (total_alpha + 1)) + 1;
			dst16[x] = a > 1 ?
				MDE_RGBA16(
					dst_r + ((a * (src8[MDE_COL_OFS_R]-dst_r))>>8),
					dst_g + ((a * (src8[MDE_COL_OFS_G]-dst_g))>>8),
					dst_b + ((a * (src8[MDE_COL_OFS_B]-dst_b))>>8),
					dst_a + (unsigned char) (long((src_a) * (256-dst_a))>>8)
					) :
				MDE_RGBA16(
					dst_r,
					dst_g,
					dst_b,
					dst_a + (unsigned char) (long((src_a) * (256-dst_a))>>8)
					);
#endif // !USE_PREMULTIPLIED_ALPHA
		}

		break;
	}
	}
	return true;
}

void MDE_Scanline_BlitStretch_BGRA32_To_RGBA16(void *dst, void *src, int len,
											  MDE_F1616 sx, MDE_F1616 dx,
											  MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
#ifdef USE_PREMULTIPLIED_ALPHA
			OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for (x = 0; x < len; ++x, ++dst16, sx+=dx)
			{
				unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
				unsigned char src_r = src8[MDE_COL_OFS_R];
				unsigned char src_g = src8[MDE_COL_OFS_G];
				unsigned char src_b = src8[MDE_COL_OFS_B];
				unsigned char src_a = src8[MDE_COL_OFS_A];
				*dst16 = (unsigned short)MDE_RGBA16(src_r, src_g, src_b, src_a);
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			for(x = 0; x < len; x++, sx+=dx)
			{
				unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
				unsigned char src_r = src8[MDE_COL_OFS_R];
				unsigned char src_g = src8[MDE_COL_OFS_G];
				unsigned char src_b = src8[MDE_COL_OFS_B];
				unsigned char src_a = src8[MDE_COL_OFS_A];

				if (src_a == 0)
				{
					continue;
				}
				if (src_a == 255)
				{
					dst16[x] = (unsigned short) MDE_RGBA16(src_r, src_g, src_b, 255);
					continue;
				}
				unsigned char dst_r = MDE_COL_R16A(dst16[x]);
				unsigned char dst_g = MDE_COL_G16A(dst16[x]);
				unsigned char dst_b = MDE_COL_B16A(dst16[x]);
				unsigned char dst_a = MDE_COL_A16A(dst16[x]);
#ifdef USE_PREMULTIPLIED_ALPHA
				dst16[x] = MDE_RGBA16(
					((dst_r*(256-src_a))>>8) + src_r,
					((dst_g*(256-src_a))>>8) + src_g,
					((dst_b*(256-src_a))>>8) + src_b,
					((dst_a*(256-src_a))>>8) + src_a
					);
#else // !USE_PREMULTIPLIED_ALPHA
				int a = ((src_a<<8) / (MDE_MIN(dst_a + src_a, 255) + 1)) + 1;
				dst16[x] = a > 1 ?
					MDE_RGBA16(
						dst_r + ((a * (src_r-dst_r))>>8),
						dst_g + ((a * (src_g-dst_g))>>8),
						dst_b + ((a * (src_b-dst_b))>>8),
						dst_a + (unsigned char) (long((src_a) * (256-dst_a))>>8)
						) :
					MDE_RGBA16(
						dst_r,
						dst_g,
						dst_b,
						dst_a + (unsigned char) (long((src_a) * (256-dst_a))>>8)
						);
#endif // !USE_PREMULTIPLIED_ALPHA
			}
		}
		break;
	default:
		MDE_ASSERT(0);
		break;
	}
}
#ifdef MDE_BILINEAR_BLITSTRETCH
struct MDE_COLOR_RGBA16
{
	UINT16 red : 4;
	UINT16 green : 4;
	UINT16 blue : 4;
	UINT16 alpha : 4;
};
// this is a compiletime assert to verify that the size of the
// MDE_COLOR_RGBA16 struct is as expected. if this line generates a
// compilation error the interpolation code will have to be rewritten.
typedef char MDE_COLOR_RGBA16_ASSERT[sizeof(MDE_COLOR_RGBA16) == 2 ? 1 : -1];
void MDE_BilinearInterpolationX_RGBA16(void* dst, void* src, int dstlen, int srclen, MDE_F1616 sx, MDE_F1616 dx)
{
	MDE_COLOR_RGBA16* dstdata = (MDE_COLOR_RGBA16*)dst;
	MDE_COLOR_RGBA16* srcdata = (MDE_COLOR_RGBA16*)src;
	MDE_BILINEAR_INTERPOLATION_X_IMPL(dstdata, srcdata, dstlen, srclen, sx, dx)
}
void MDE_BilinearInterpolationY_RGBA16(void* dst, void* src1, void* src2, int len, MDE_F1616 sy)
{
	MDE_COLOR_RGBA16* dstdata = (MDE_COLOR_RGBA16*)dst;
	MDE_COLOR_RGBA16* src1data = (MDE_COLOR_RGBA16*)src1;
	MDE_COLOR_RGBA16* src2data = (MDE_COLOR_RGBA16*)src2;
	MDE_BILINEAR_INTERPOLATION_Y_IMPL(dstdata, src1data, src2data, len, sy)
}
#endif // MDE_BILINEAR_BLITSTRETCH
#endif // MDE_SUPPORT_RGBA16

// == BGR24 ==============================================================

void MDE_Scanline_InvColor_BGR24(void *dst, int len, unsigned int dummy)
{
	unsigned char *dst8 = (unsigned char *) dst;
	for(int x = 0; x < len; x++)
	{
		dst8[0] = 255 - dst8[0];
		dst8[1] = 255 - dst8[1];
		dst8[2] = 255 - dst8[2];
		dst8 += 3;
	}
}

void MDE_Scanline_SetColor_BGR24_NoBlend(void *dst, int len, unsigned int col)
{
	unsigned char *dst8 = (unsigned char *) dst;
	for(int x = 0; x < len; x++)
	{
		dst8[0] = MDE_COL_B(col);
		dst8[1] = MDE_COL_G(col);
		dst8[2] = MDE_COL_R(col);
		dst8 += 3;
	}
}
void MDE_Scanline_SetColor_BGR24_Blend(void *dst, int len, unsigned int col)
{
	unsigned char *dst8 = (unsigned char *) dst;
	if (MDE_COL_A(col) > 0)
	{
		unsigned char *src8 = (unsigned char *) &col;
		for(int x = 0; x < len; x++)
		{
			// No need for premultiplied alpha case here since the color is not premultiplied
			unsigned char dst_r = dst8[2];
			unsigned char dst_g = dst8[1];
			unsigned char dst_b = dst8[0];
			dst8[0]= dst_b+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_B]-dst_b)) >> 8);
			dst8[1] = dst_g+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_G]-dst_g)) >> 8);
			dst8[2] = dst_r+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_R]-dst_r)) >> 8);
			dst8 += 3;
		}
	}
}

bool MDE_Scanline_BlitNormal_BGR24_To_BGRA32(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;

	case MDE_METHOD_COMPONENT_BLEND:
	{
		unsigned char *dst8 = (unsigned char *)dst;
		unsigned char *src8 = (unsigned char *)src;
		unsigned char* col8 = (unsigned char*)&srcinf->col;
		for (x = 0; x < len; ++x)
		{
			dst8[MDE_COL_OFS_B] += src8[MDE_COL_OFS_B] * ( (int)col8[MDE_COL_OFS_B] - (int)dst8[MDE_COL_OFS_B] ) >> 8;
			dst8[MDE_COL_OFS_G] += src8[MDE_COL_OFS_G] * ( (int)col8[MDE_COL_OFS_G] - (int)dst8[MDE_COL_OFS_G] ) >> 8;
			dst8[MDE_COL_OFS_R] += src8[MDE_COL_OFS_R] * ( (int)col8[MDE_COL_OFS_R] - (int)dst8[MDE_COL_OFS_R] ) >> 8;
// 			dst8[MDE_COL_OFS_A] = 255;
			dst8 += 4;
			src8 += 3;
		}
		break;
	}

	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				dst8[MDE_COL_OFS_B] = src8[0];
				dst8[MDE_COL_OFS_G] = src8[1];
				dst8[MDE_COL_OFS_R] = src8[2];
				dst8[MDE_COL_OFS_A] = 255;
				dst8 += 4;
				src8 += 3;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				if (!(src8[0] == ((unsigned char*)&(srcinf->mask))[0] &&
					  src8[1] == ((unsigned char*)&(srcinf->mask))[1] &&
					  src8[2] == ((unsigned char*)&(srcinf->mask))[2]))
				{
					dst8[MDE_COL_OFS_B] = src8[0];
					dst8[MDE_COL_OFS_G] = src8[1];
					dst8[MDE_COL_OFS_R] = src8[2];
					dst8[MDE_COL_OFS_A] = 255;
				}
				dst8 += 4;
				src8 += 3;
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_BGR24_To_BGRA32(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned int ofs = (sx >> 16) * 3;
				dst8[MDE_COL_OFS_B] = src8[ofs + 0];
				dst8[MDE_COL_OFS_G] = src8[ofs + 1];
				dst8[MDE_COL_OFS_R] = src8[ofs + 2];
				dst8[MDE_COL_OFS_A] = 255;
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned int ofs = (sx >> 16) * 3;
				if (!(src8[ofs + 0] == ((unsigned char*)&(srcinf->mask))[0] &&
					  src8[ofs + 1] == ((unsigned char*)&(srcinf->mask))[1] &&
					  src8[ofs + 2] == ((unsigned char*)&(srcinf->mask))[2]))
				{
					dst8[MDE_COL_OFS_B] = src8[ofs + 0];
					dst8[MDE_COL_OFS_G] = src8[ofs + 1];
					dst8[MDE_COL_OFS_R] = src8[ofs + 2];
					dst8[MDE_COL_OFS_A] = 255;
				}
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	}
}

#ifdef MDE_SUPPORT_SRGB16

bool MDE_Scanline_BlitNormal_BGR24_To_SRGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				*dst16 = MDE_SRGB16(src8[2], src8[1], src8[0]);
				++dst16;
				src8 += 3;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				if (!(src8[0] == ((unsigned char*)&(srcinf->mask))[0] &&
					  src8[1] == ((unsigned char*)&(srcinf->mask))[1] &&
					  src8[2] == ((unsigned char*)&(srcinf->mask))[2]))
				{
					*dst16 = MDE_SRGB16(src8[2], src8[1], src8[0]);
				}
				++dst16;
				src8 += 3;
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_BGR24_To_SRGB16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned int ofs = (sx >> 16) * 3;
				*dst16 = MDE_SRGB16(src8[ofs + 2], src8[ofs + 1], src8[ofs]);
				++dst16;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned int ofs = (sx >> 16) * 3;
				if (!(src8[ofs + 0] == ((unsigned char*)&(srcinf->mask))[0] &&
					  src8[ofs + 1] == ((unsigned char*)&(srcinf->mask))[1] &&
					  src8[ofs + 2] == ((unsigned char*)&(srcinf->mask))[2]))
				{
					*dst16 = MDE_SRGB16(src8[ofs + 2], src8[ofs + 1], src8[ofs + 0]);
				}
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}

#endif // MDE_SUPPORT_SRGB16

// == RGBA16 ==============================================================
#ifdef MDE_SUPPORT_RGBA16

void MDE_Scanline_InvColor_RGBA16(void *dst, int len, unsigned int dummy)
{
	unsigned short *dst16 = (unsigned short *) dst;
	for(int x = 0; x < len; x++)
	{
#ifdef USE_PREMULTIPLIED_ALPHA
		*dst16 = MDE_RGBA16(MDE_COL_A16A(*dst16)-MDE_COL_R16A(*dst16),
				MDE_COL_A16A(*dst16)-MDE_COL_G16A(*dst16),
				MDE_COL_A16A(*dst16)-MDE_COL_B16A(*dst16),
				MDE_COL_A16A(*dst16));
#else // !USE_PREMULTIPLIED_ALPHA
		*dst16 = MDE_RGBA16(255-MDE_COL_R16A(*dst16),
				255-MDE_COL_G16A(*dst16),
				255-MDE_COL_B16A(*dst16),
				MDE_COL_A16A(*dst16));
#endif // !USE_PREMULTIPLIED_ALPHA
		++dst16;
	}
}

void MDE_Scanline_SetColor_RGBA16_NoBlend(void *dst, int len, unsigned int col)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	// Premultiply the color
	int alpha = MDE_COL_A(col)+1;
	int red = (MDE_COL_R(col)*alpha)>>8;
	int green = (MDE_COL_G(col)*alpha)>>8;
	int blue = (MDE_COL_B(col)*alpha)>>8;
	col = MDE_RGBA(red, green, blue, alpha-1);
#endif // USE_PREMULTIPLIED_ALPHA
	unsigned short col16 = MDE_RGBA16(MDE_COL_R(col), MDE_COL_G(col),
									  MDE_COL_B(col), MDE_COL_A(col));
	memset16bit((unsigned short*)dst, col16, len);
}
void MDE_Scanline_SetColor_RGBA16_Blend(void *dst, int len, unsigned int col)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	// Premultiply the color
	int alpha = MDE_COL_A(col)+1;
	int red = (MDE_COL_R(col)*alpha)>>8;
	int green = (MDE_COL_G(col)*alpha)>>8;
	int blue = (MDE_COL_B(col)*alpha)>>8;
	col = MDE_RGBA(red, green, blue, alpha-1);
#endif // USE_PREMULTIPLIED_ALPHA
	if (MDE_COL_A(col) > 0)
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) &col;
		for(int x = 0; x < len; x++)
		{
			unsigned char dst_r = MDE_COL_R16A(dst16[x]);
			unsigned char dst_g = MDE_COL_G16A(dst16[x]);
			unsigned char dst_b = MDE_COL_B16A(dst16[x]);
			unsigned char dst_a = MDE_COL_A16A(dst16[x]);
#ifdef USE_PREMULTIPLIED_ALPHA
			dst16[x] = MDE_RGBA16(
				((dst_r*(256-src8[MDE_COL_OFS_A]))>>8) + src8[MDE_COL_OFS_R],
				((dst_g*(256-src8[MDE_COL_OFS_A]))>>8) + src8[MDE_COL_OFS_G],
				((dst_b*(256-src8[MDE_COL_OFS_A]))>>8) + src8[MDE_COL_OFS_B],
				((dst_a*(256-src8[MDE_COL_OFS_A]))>>8) + src8[MDE_COL_OFS_A]
				);
#else // !USE_PREMULTIPLIED_ALPHA
			int a = src8[MDE_COL_OFS_A] + 1;
			dst16[x] = a > 1 ?
				MDE_RGBA16(
					dst_r+((a * (src8[MDE_COL_OFS_R]-dst_r)) >> 8),
					dst_g+((a * (src8[MDE_COL_OFS_G]-dst_g)) >> 8),
					dst_b+((a * (src8[MDE_COL_OFS_B]-dst_b)) >> 8),
					dst_a
					) :
				MDE_RGBA16(
					dst_r,
					dst_g,
					dst_b,
					dst_a
					);
#endif // !USE_PREMULTIPLIED_ALPHA
		}
	}
}

bool MDE_Scanline_BlitNormal_RGBA16_To_RGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
			OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for (x = 0; x < len; ++x)
			{
				*dst16 = (unsigned short)MDE_RGB16(MDE_COL_R16A(*src16),
							MDE_COL_G16A(*src16),
							MDE_COL_B16A(*src16));
				++src16;
				++dst16;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char r, g, b;
				r = (unsigned char)MDE_COL_R16(*dst16);
				g = (unsigned char)MDE_COL_G16(*dst16);
				b = (unsigned char)MDE_COL_B16(*dst16);

				unsigned int sa, sr, sg, sb;
				sa = MDE_COL_A16A(*src16);
				sr = MDE_COL_R16A(*src16);
				sg = MDE_COL_G16A(*src16);
				sb = MDE_COL_B16A(*src16);
#ifdef MDE_BRIGHT_WHITE_BLEND
				sa |= sa>>4;
				sr |= sr>>4;
				sg |= sg>>4;
				sb |= sb>>4;
#endif
#ifdef USE_PREMULTIPLIED_ALPHA
				*dst16 = (unsigned short)MDE_RGB16(
					((r*(256-sa))>>8)+sr,
					((g*(256-sa))>>8)+sg,
					((b*(256-sa))>>8)+sb);
#else // !USE_PREMULTIPLIED_ALPHA
				*dst16 = (unsigned short)MDE_RGB16(
					(r+(sa*(sr-r)>>8)),
					(g+(sa*(sg-g)>>8)),
					(b+(sa*(sb-b)>>8)));
#endif // !USE_PREMULTIPLIED_ALPHA
				++dst16;
				++src16;
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_RGBA16_To_RGB16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
			OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				*dst16 = (unsigned short)MDE_RGB16(MDE_COL_R16A(c),
							MDE_COL_G16A(c),
							MDE_COL_B16A(c));
				++dst16;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char r, g, b;
				unsigned short c = src16[sx>>16];
				r = (unsigned char)MDE_COL_R16(*dst16);
				g = (unsigned char)MDE_COL_G16(*dst16);
				b = (unsigned char)MDE_COL_B16(*dst16);

				unsigned int sa, sr, sg, sb;
				sa = MDE_COL_A16A(c);
				sr = MDE_COL_R16A(c);
				sg = MDE_COL_G16A(c);
				sb = MDE_COL_B16A(c);
#ifdef MDE_BRIGHT_WHITE_BLEND
				sa |= sa>>4;
				sr |= sr>>4;
				sg |= sg>>4;
				sb |= sb>>4;
#endif
#ifdef USE_PREMULTIPLIED_ALPHA
				*dst16 = (unsigned short)MDE_RGB16(
					((r*(256-sa))>>8)+sr,
					((g*(256-sa))>>8)+sg,
					((b*(256-sa))>>8)+sb);
#else // !USE_PREMULTIPLIED_ALPHA
				*dst16 = (unsigned short)MDE_RGB16(
					r+(sa*(sr-r)>>8),
					g+(sa*(sg-g)>>8),
					b+(sa*(sb-b)>>8));
#endif // !USE_PREMULTIPLIED_ALPHA
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}

bool MDE_Scanline_BlitNormal_RGBA16_To_BGRA32(void *dst, void *src, int len,
											  MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned int sr, sg, sb, sa;
				sb = MDE_COL_B16A(*src16);
				sg = MDE_COL_G16A(*src16);
				sr = MDE_COL_R16A(*src16);
				sa = MDE_COL_A16A(*src16);
#ifdef MDE_BRIGHT_WHITE_BLEND
				sb |= sb>>4;
				sg |= sg>>4;
				sr |= sr>>4;
				sa |= sa>>4;
#endif // MDE_BRIGHT_WHITE_BLEND
				dst8[MDE_COL_OFS_B] = sb;
				dst8[MDE_COL_OFS_G] = sg;
				dst8[MDE_COL_OFS_R] = sr;
				dst8[MDE_COL_OFS_A] = sa;
				++src16;
				dst8 += 4;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char src_bgra[4];
				src_bgra[MDE_COL_OFS_B] = MDE_COL_B16A(*src16);
				src_bgra[MDE_COL_OFS_G] = MDE_COL_G16A(*src16);
				src_bgra[MDE_COL_OFS_R] = MDE_COL_R16A(*src16);
				src_bgra[MDE_COL_OFS_A] = MDE_COL_A16A(*src16);
#ifdef MDE_BRIGHT_WHITE_BLEND
				src_bgra[MDE_COL_OFS_B] |= src_bgra[MDE_COL_OFS_B]>>4;
				src_bgra[MDE_COL_OFS_G] |= src_bgra[MDE_COL_OFS_G]>>4;
				src_bgra[MDE_COL_OFS_R] |= src_bgra[MDE_COL_OFS_R]>>4;
				src_bgra[MDE_COL_OFS_A] |= src_bgra[MDE_COL_OFS_A]>>4;
#endif // MDE_BRIGHT_WHITE_BLEND
				if (dstinf->method == MDE_METHOD_ALPHA)
				    SetPixelWithDestAlpha_BGRA32( dst8, src_bgra, MDE_COL_A16A(*src16) );
				else
				    SetPixel_BGRA32( dst8, src_bgra, MDE_COL_A16A(*src16) );
			    dst8 += 4;
			    src16++;
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_RGBA16_To_BGRA32(void *dst, void *src, int len,
											   MDE_F1616 sx, MDE_F1616 dx,
											   MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned int sr, sg, sb, sa;
				unsigned short c = src16[sx>>16];
				sb = MDE_COL_B16A(c);
				sg = MDE_COL_G16A(c);
				sr = MDE_COL_R16A(c);
				sa = MDE_COL_A16A(c);
#ifdef MDE_BRIGHT_WHITE_BLEND
				sb |= sb>>4;
				sg |= sg>>4;
				sr |= sr>>4;
				sa |= sa>>4;
#endif // MDE_BRIGHT_WHITE_BLEND
				dst8[MDE_COL_OFS_B] = sb;
				dst8[MDE_COL_OFS_G] = sg;
				dst8[MDE_COL_OFS_R] = sr;
				dst8[MDE_COL_OFS_A] = sa;
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for(x = 0; x < len; x++)
			{
				unsigned short c = src16[sx>>16];
				unsigned char src_bgra[4];
				src_bgra[MDE_COL_OFS_B] = MDE_COL_B16A(c);
				src_bgra[MDE_COL_OFS_G] = MDE_COL_G16A(c);
				src_bgra[MDE_COL_OFS_R] = MDE_COL_R16A(c);
				src_bgra[MDE_COL_OFS_A] = MDE_COL_A16A(c);
#ifdef MDE_BRIGHT_WHITE_BLEND
				src_bgra[MDE_COL_OFS_B] |= src_bgra[MDE_COL_OFS_B]>>4;
				src_bgra[MDE_COL_OFS_G] |= src_bgra[MDE_COL_OFS_G]>>4;
				src_bgra[MDE_COL_OFS_R] |= src_bgra[MDE_COL_OFS_R]>>4;
				src_bgra[MDE_COL_OFS_A] |= src_bgra[MDE_COL_OFS_A]>>4;
#endif // MDE_BRIGHT_WHITE_BLEND
				if (dstinf->method == MDE_METHOD_ALPHA)
				    SetPixelWithDestAlpha_BGRA32( dst8, src_bgra, MDE_COL_A16A(c) );
				else
				    SetPixel_BGRA32( dst8, src_bgra, MDE_COL_A16A(c) );
			    dst8 += 4;
				sx += dx;
			}
		}
		break;
	}
}



#ifdef MDE_SUPPORT_SRGB16
bool MDE_Scanline_BlitNormal_RGBA16_To_SRGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
			OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for (x = 0; x < len; ++x)
			{
				*dst16 = (unsigned short)MDE_SRGB16(MDE_COL_R16A(*src16),
							MDE_COL_G16A(*src16),
							MDE_COL_B16A(*src16));
				++src16;
				++dst16;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char r, g, b;
				r = (unsigned char)MDE_COL_SR16(*dst16);
				g = (unsigned char)MDE_COL_SG16(*dst16);
				b = (unsigned char)MDE_COL_SB16(*dst16);

				unsigned int sa, sr, sg, sb;
				sa = MDE_COL_A16A(*src16);
				sr = MDE_COL_R16A(*src16);
				sg = MDE_COL_G16A(*src16);
				sb = MDE_COL_B16A(*src16);
#ifdef MDE_BRIGHT_WHITE_BLEND
				sa |= sa>>4;
				sr |= sr>>4;
				sg |= sg>>4;
				sb |= sb>>4;
#endif
#ifdef USE_PREMULTIPLIED_ALPHA
				*dst16 = (unsigned short)MDE_SRGB16(
					((r*(256-sa))>>8)+sr,
					((g*(256-sa))>>8)+sg,
					((b*(256-sa))>>8)+sb);
#else // !USE_PREMULTIPLIED_ALPHA
				*dst16 = (unsigned short)MDE_SRGB16(
					r+(sa*(sr-r)>>8),
					g+(sa*(sg-g)>>8),
					b+(sa*(sb-b)>>8));
#endif // !USE_PREMULTIPLIED_ALPHA
				++dst16;
				++src16;
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_RGBA16_To_SRGB16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
			OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				*dst16 = (unsigned short)MDE_SRGB16(MDE_COL_R16A(c),
							MDE_COL_G16A(c),
							MDE_COL_B16A(c));
				++dst16;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char r, g, b;
				unsigned short c = src16[sx>>16];
				r = (unsigned char)MDE_COL_SR16(*dst16);
				g = (unsigned char)MDE_COL_SB16(*dst16);
				b = (unsigned char)MDE_COL_SG16(*dst16);

				unsigned int sa, sr, sg, sb;
				sa = MDE_COL_A16A(c);
				sr = MDE_COL_R16A(c);
				sg = MDE_COL_G16A(c);
				sb = MDE_COL_B16A(c);
#ifdef MDE_BRIGHT_WHITE_BLEND
				sa |= sa>>4;
				sr |= sr>>4;
				sg |= sg>>4;
				sb |= sb>>4;
#endif
#ifdef USE_PREMULTIPLIED_ALPHA
				*dst16 = (unsigned short)MDE_SRGB16(
					((r*(256-sa))>>8)+sr,
					((g*(256-sa))>>8)+sg,
					((b*(256-sa))>>8)+sb);
#else // !USE_PREMULTIPLIED_ALPHA
				*dst16 = (unsigned short)MDE_SRGB16(
					r+(sa*(sr-r)>>8),
					g+(sa*(sg-g)>>8),
					b+(sa*(sb-b)>>8));
#endif // !USE_PREMULTIPLIED_ALPHA
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}

#endif //MDE_SUPPORT_SRGB16

#ifdef MDE_SUPPORT_MBGR16

bool MDE_Scanline_BlitNormal_RGBA16_To_MBGR16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
			OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for (x = 0; x < len; ++x)
			{
				*dst16 = (unsigned short)MDE_MBGR16(MDE_COL_R16A(*src16),
							MDE_COL_G16A(*src16),
							MDE_COL_B16A(*src16));
				++src16;
				++dst16;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char r, g, b;
				r = (unsigned char)MDE_COL_R16M(*dst16);
				g = (unsigned char)MDE_COL_G16M(*dst16);
				b = (unsigned char)MDE_COL_B16M(*dst16);

				unsigned int sa, sr, sg, sb;
				sa = MDE_COL_A16A(*src16);
				if (sa > 0)
				{
					sr = MDE_COL_R16A(*src16);
					sg = MDE_COL_G16A(*src16);
					sb = MDE_COL_B16A(*src16);
#ifdef MDE_BRIGHT_WHITE_BLEND
					sa |= sa>>4;
					sr |= sr>>4;
					sg |= sg>>4;
					sb |= sb>>4;
#endif
#ifdef USE_PREMULTIPLIED_ALPHA
					*dst16 = (unsigned short)MDE_MBGR16(
						((r*(256-sa))>>8)+sr,
						((g*(256-sa))>>8)+sg,
						((b*(256-sa))>>8)+sb);
#else // !USE_PREMULTIPLIED_ALPHA
					*dst16 = (unsigned short)MDE_MBGR16(
						r+(sa*(sr-r)>>8),
						g+(sa*(sg-g)>>8),
						b+(sa*(sb-b)>>8));
#endif // !USE_PREMULTIPLIED_ALPHA
				}
				++dst16;
				++src16;
			}
		}
		break;
	}
	return true;
}
void MDE_Scanline_BlitStretch_RGBA16_To_MBGR16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
			OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				*dst16 = (unsigned short)MDE_MBGR16(MDE_COL_R16A(c),
							MDE_COL_G16A(c),
							MDE_COL_B16A(c));
				++dst16;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char r, g, b;
				unsigned short c = src16[sx>>16];
				r = (unsigned char)MDE_COL_R16M(*dst16);
				g = (unsigned char)MDE_COL_G16M(*dst16);
				b = (unsigned char)MDE_COL_B16M(*dst16);

				unsigned int sa, sr, sg, sb;
				sa = MDE_COL_A16A(c);
				sr = MDE_COL_R16A(c);
				sg = MDE_COL_G16A(c);
				sb = MDE_COL_B16A(c);
#ifdef MDE_BRIGHT_WHITE_BLEND
				sa |= sa>>4;
				sr |= sr>>4;
				sg |= sg>>4;
				sb |= sb>>4;
#endif
#ifdef USE_PREMULTIPLIED_ALPHA
				*dst16 = (unsigned short)MDE_MBGR16(
					((r*(256-sa))>>8)+sr,
					((g*(256-sa))>>8)+sg,
					((b*(256-sa))>>8)+sb);
#else // !USE_PREMULTIPLIED_ALPHA
				*dst16 = (unsigned short)MDE_MBGR16(
					r+(sa*(sr-r)>>8),
					g+(sa*(sg-g)>>8),
					b+(sa*(sb-b)>>8));
#endif // !USE_PREMULTIPLIED_ALPHA
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}
#endif // MDE_SUPPORT_MBGR16

#endif // MDE_SUPPORT_RGBA16

// == RGB16 ==============================================================

#ifdef MDE_SUPPORT_RGB16

void MDE_Scanline_InvColor_RGB16(void *dst, int len, unsigned int dummy)
{
	unsigned short *dst16 = (unsigned short *) dst;
	for(int x = 0; x < len; x++)
	{
		*dst16 = MDE_RGB16(255-MDE_COL_R16(*dst16),
				255-MDE_COL_G16(*dst16),
				255-MDE_COL_B16(*dst16));
		++dst16;
	}
}
void MDE_Scanline_SetColor_RGB16_NoBlend(void *dst, int len, unsigned int col)
{
	memset16bit((unsigned short*)dst, MDE_RGB16(MDE_COL_R(col), MDE_COL_G(col), MDE_COL_B(col)), len);
}

void MDE_Scanline_SetColor_RGB16_Blend(void *dst, int len, unsigned int col)
{
	if (MDE_COL_A(col) > 0)
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) &col;
		for(int x = 0; x < len; x++)
		{
			unsigned char dst_r = MDE_COL_R16(dst16[x]);
			unsigned char dst_g = MDE_COL_G16(dst16[x]);
			unsigned char dst_b = MDE_COL_B16(dst16[x]);
			// No need to use premultiplied alpha since the color is not premultiplied
			dst16[x] = MDE_RGB16(
				dst_r+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_R]-dst_r)) >> 8),
				dst_g+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_G]-dst_g)) >> 8),
				dst_b+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_B]-dst_b)) >> 8)
				);
		}
	}
}

bool MDE_Scanline_BlitNormal_RGB16_To_RGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COLOR:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			unsigned char *col8 = (unsigned char *) &srcinf->col;
			unsigned char color_r = col8[MDE_COL_OFS_R];
			unsigned char color_g = col8[MDE_COL_OFS_G];
			unsigned char color_b = col8[MDE_COL_OFS_B];
			for (x = 0; x < len; ++x)
			{
				unsigned char dst_r = MDE_COL_R16(dst16[x]);
				unsigned char dst_g = MDE_COL_G16(dst16[x]);
				unsigned char dst_b = MDE_COL_B16(dst16[x]);
				unsigned char src_r = MDE_COL_R16(src16[x]);
				unsigned char src_g = MDE_COL_G16(src16[x]);
				unsigned char src_b = MDE_COL_B16(src16[x]);
				unsigned char alpha_c = ( src_r + src_g + src_b ) / 3;
				dst16[x] = MDE_RGB16(
					dst_r+((alpha_c * (color_r - dst_r)) >> 8),
					dst_g+((alpha_c * (color_g - dst_g)) >> 8),
					dst_b+((alpha_c * (color_b - dst_b)) >> 8)
					);
			}
		}
		break;
	case MDE_METHOD_COPY:
	case MDE_METHOD_ALPHA:
	case MDE_METHOD_OPACITY:
		op_memcpy(dst, src, len<<1);
		break;
	case MDE_METHOD_MASK:
		for (x = 0; x < len; x++)
		{
			if (((unsigned short *)src)[x] != (unsigned short)(srcinf->mask))
			{
				((unsigned short *)dst)[x] = ((unsigned short *)src)[x];
			}
		}
		break;
	}
	return true;
}
void MDE_Scanline_BlitStretch_RGB16_To_RGB16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
	case MDE_METHOD_ALPHA:
	case MDE_METHOD_OPACITY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				*dst16 = (unsigned short)MDE_RGB16(MDE_COL_R16(c),
							MDE_COL_G16(c),
							MDE_COL_B16(c));
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}

#ifdef MDE_SUPPORT_RGBA16

bool MDE_Scanline_BlitNormal_RGB16_To_RGBA16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				*dst16 = (unsigned short)MDE_RGBA16(MDE_COL_R16(*src16),
							MDE_COL_G16(*src16),
							MDE_COL_B16(*src16),
							255);
				++src16;
				++dst16;
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_RGB16_To_RGBA16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				*dst16 = (unsigned short)MDE_RGBA16(MDE_COL_R16(c),
							MDE_COL_G16(c),
							MDE_COL_B16(c),
							255);
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}

void MDE_Scanline_BlitStretch_RGBA16_To_RGBA16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned short *src16 = (unsigned short *) src;
		for(x = 0; x < len; x++)
		{
			dst16[x] = src16[sx >> 16];
			sx += dx;
		}
		break;
	}
}

#endif // MDE_SUPPORT_RGBA16

#endif // MDE_SUPPORT_RGB16

#ifdef MDE_SUPPORT_BGR24
bool MDE_Scanline_BlitNormal_RGB16_To_BGR24(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *)dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				dst8[MDE_COL_OFS_B] = MDE_COL_B16(*src16);
				dst8[MDE_COL_OFS_G] = MDE_COL_G16(*src16);
				dst8[MDE_COL_OFS_R] = MDE_COL_R16(*src16);
				++src16;
				dst8 += 3;
			}
		}
		break;
	case MDE_METHOD_COLOR:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			unsigned char *col8 = (unsigned char *) &srcinf->col;
			unsigned char color_r = col8[MDE_COL_OFS_R];
			unsigned char color_g = col8[MDE_COL_OFS_G];
			unsigned char color_b = col8[MDE_COL_OFS_B];
			for (x = 0; x < len; ++x)
			{
				dst8[MDE_COL_OFS_B] += (MDE_COL_B16(*src16) * (color_b - dst8[MDE_COL_OFS_B])) >> 8;
				dst8[MDE_COL_OFS_G] += (MDE_COL_G16(*src16) * (color_g - dst8[MDE_COL_OFS_G])) >> 8;
				dst8[MDE_COL_OFS_R] += (MDE_COL_R16(*src16) * (color_r - dst8[MDE_COL_OFS_R])) >> 8;
				++src16;
				dst8 += 3;
			}
		}
		break;
	default:
		MDE_ASSERT(len == 0);
		return false;
	}
	return true;
}

bool MDE_Scanline_BlitNormal_BGR24_To_RGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				*dst16 = MDE_RGB16(src8[2], src8[1], src8[0]);
				++dst16;
				src8 += 3;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				if (!(src8[0] == ((unsigned char*)&(srcinf->mask))[0] &&
					  src8[1] == ((unsigned char*)&(srcinf->mask))[1] &&
					  src8[2] == ((unsigned char*)&(srcinf->mask))[2]))
				{
					*dst16 = MDE_RGB16(src8[2], src8[1], src8[0]);
				}
				++dst16;
				src8 += 3;
			}
		}
		break;
	}
	return true;
}

#endif // MDE_SUPPORT_BGR24

// == SRGB16 ==============================================================

#ifdef MDE_SUPPORT_SRGB16

void MDE_Scanline_InvColor_SRGB16(void *dst, int len, unsigned int dummy)
{
	unsigned short *dst16 = (unsigned short *) dst;
	for(int x = 0; x < len; x++)
	{
		*dst16 = MDE_SRGB16(255-MDE_COL_SR16(*dst16),
				255-MDE_COL_SG16(*dst16),
				255-MDE_COL_SB16(*dst16));
		++dst16;
	}
}

void MDE_Scanline_SetColor_SRGB16_NoBlend(void *dst, int len, unsigned int col)
{
	unsigned short col16 = MDE_SRGB16(MDE_COL_R(col), MDE_COL_G(col),
									  MDE_COL_B(col));
	memset16bit((unsigned short*)dst, col16, len);
}
void MDE_Scanline_SetColor_SRGB16_Blend(void *dst, int len, unsigned int col)
{
	if (MDE_COL_A(col) > 0)
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) &col;
		for(int x = 0; x < len; x++)
		{
			unsigned char dst_r = MDE_COL_SR16(dst16[x]);
			unsigned char dst_g = MDE_COL_SG16(dst16[x]);
			unsigned char dst_b = MDE_COL_SB16(dst16[x]);
			// No need to use premultiplied alpha since the color is not premultiplied
			dst16[x] = MDE_SRGB16(
				dst_r+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_R]-dst_r)) >> 8),
				dst_g+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_G]-dst_g)) >> 8),
				dst_b+((src8[MDE_COL_OFS_A] * (src8[MDE_COL_OFS_B]-dst_b)) >> 8)
				);
		}
	}
}

bool MDE_Scanline_BlitNormal_SRGB16_To_SRGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		op_memcpy(dst, src, len<<1);
		break;
	}
	return true;
}
void MDE_Scanline_BlitStretch_SRGB16_To_SRGB16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				*dst16 = (unsigned short)MDE_SRGB16(MDE_COL_SR16(c),
							MDE_COL_SG16(c),
							MDE_COL_SB16(c));
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}

bool MDE_Scanline_BlitNormal_SRGB16_To_RGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned short *src16 = (unsigned short *) src;
		for (x = 0; x < len; ++x)
		{
			*dst16 = (((*src16)>>8)|((*src16)<<8));
			++src16;
			++dst16;
		}
	}
	break;
	}
	return true;
}
void MDE_Scanline_BlitStretch_SRGB16_To_RGB16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				*dst16 = (unsigned short)MDE_RGB16(MDE_COL_SR16(c),
							MDE_COL_SG16(c),
							MDE_COL_SB16(c));
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}

#endif // MDE_SUPPORT_SRGB16


#ifdef MDE_SUPPORT_MBGR16

bool MDE_Scanline_BlitNormal_RGB16_To_MBGR16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned short c = *src16;
				*dst16 = (unsigned short)MDE_MBGR16(MDE_COL_R16(c),
							MDE_COL_G16(c),
							MDE_COL_B16(c));
				++dst16;
				++src16;
			}
		}
		break;
	}

	return true;
}
void MDE_Scanline_BlitStretch_RGB16_To_MBGR16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				*dst16 = (unsigned short)MDE_MBGR16(MDE_COL_R16(c),
							MDE_COL_G16(c),
							MDE_COL_B16(c));
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}
#endif // MDE_SUPPORT_MBGR16

#ifdef MDE_SUPPORT_RGB16

bool MDE_Scanline_BlitNormal_RGB16_To_BGRA32(void *dst, void *src, int len,
											 MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				dst8[MDE_COL_OFS_B] = MDE_COL_B16(*src16);
				dst8[MDE_COL_OFS_G] = MDE_COL_G16(*src16);
				dst8[MDE_COL_OFS_R] = MDE_COL_R16(*src16);
				dst8[MDE_COL_OFS_A] = 255;
				++src16;
				dst8 += 4;
			}
		}
		break;
	case MDE_METHOD_COLOR:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			unsigned char *col8 = (unsigned char *) &srcinf->col;
			unsigned char color_r = col8[MDE_COL_OFS_R];
			unsigned char color_g = col8[MDE_COL_OFS_G];
			unsigned char color_b = col8[MDE_COL_OFS_B];
			for (x = 0; x < len; ++x)
			{
				unsigned char alpha_c = ( MDE_COL_R16(*src16) + MDE_COL_G16(*src16) + MDE_COL_B16(*src16) ) / 3 ;
//				if (!alpha_c)
//					continue;
				dst8[MDE_COL_OFS_B] += (MDE_COL_B16(*src16) * (color_b - dst8[MDE_COL_OFS_B])) >> 8;
				dst8[MDE_COL_OFS_G] += (MDE_COL_G16(*src16) * (color_g - dst8[MDE_COL_OFS_G])) >> 8;
				dst8[MDE_COL_OFS_R] += (MDE_COL_R16(*src16) * (color_r - dst8[MDE_COL_OFS_R])) >> 8;
				dst8[MDE_COL_OFS_A] = alpha_c;
				++src16;
				dst8 += 4;
			}
		}
		break;
	default:
		MDE_ASSERT(len == 0);
		return false;
	}
	return true;
}

void MDE_Scanline_BlitStretch_RGB16_To_BGRA32(void *dst, void *src, int len,
											  MDE_F1616 sx, MDE_F1616 dx,
											  MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				dst8[MDE_COL_OFS_B] = MDE_COL_B16(c);
				dst8[MDE_COL_OFS_G] = MDE_COL_G16(c);
				dst8[MDE_COL_OFS_R] = MDE_COL_R16(c);
				dst8[MDE_COL_OFS_A] = 255;
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	}
}

#ifdef MDE_SUPPORT_RGBA32

bool MDE_Scanline_BlitNormal_RGB16_To_RGBA32(void *dst, void *src, int len,
											 MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				dst8[MDE_COL_OFS_B] = MDE_COL_R16(*src16);
				dst8[MDE_COL_OFS_G] = MDE_COL_G16(*src16);
				dst8[MDE_COL_OFS_R] = MDE_COL_B16(*src16);
				dst8[MDE_COL_OFS_A] = 255;
				++src16;
				dst8 += 4;
			}
		}
		break;
	}
	return true;
}

#endif // MDE_SUPPORT_RGBA32

#ifdef MDE_SUPPORT_SRGB16

bool MDE_Scanline_BlitNormal_RGB16_To_SRGB16(void *dst, void *src, int len,
											 MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				*dst16 = MDE_SRGB16(MDE_COL_R16(*src16), MDE_COL_G16(*src16), MDE_COL_B16(*src16));
				++src16;
				++dst16;
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_RGB16_To_SRGB16(void *dst, void *src, int len,
											  MDE_F1616 sx, MDE_F1616 dx,
											  MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				*dst16 = MDE_SRGB16(MDE_COL_R16(c), MDE_COL_G16(c), MDE_COL_B16(c));
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}
#endif //MDE_SUPPORT_SRGB16


#endif // MDE_SUPPORT_RGB16

#ifdef MDE_SUPPORT_SRGB16

bool MDE_Scanline_BlitNormal_SRGB16_To_BGRA32(void *dst, void *src, int len,
											 MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				dst8[MDE_COL_OFS_B] = MDE_COL_SB16(*src16);
				dst8[MDE_COL_OFS_G] = MDE_COL_SG16(*src16);
				dst8[MDE_COL_OFS_R] = MDE_COL_SR16(*src16);
				dst8[MDE_COL_OFS_A] = 255;
				++src16;
				dst8 += 4;
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_SRGB16_To_BGRA32(void *dst, void *src, int len,
											  MDE_F1616 sx, MDE_F1616 dx,
											  MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				dst8[MDE_COL_OFS_B] = MDE_COL_SB16(c);
				dst8[MDE_COL_OFS_G] = MDE_COL_SG16(c);
				dst8[MDE_COL_OFS_R] = MDE_COL_SR16(c);
				dst8[MDE_COL_OFS_A] = 255;
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	}
}

#ifdef MDE_SUPPORT_RGBA32

bool MDE_Scanline_BlitNormal_SRGB16_To_RGBA32(void *dst, void *src, int len,
											 MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				dst8[MDE_COL_OFS_B] = MDE_COL_SR16(*src16);
				dst8[MDE_COL_OFS_G] = MDE_COL_SG16(*src16);
				dst8[MDE_COL_OFS_R] = MDE_COL_SB16(*src16);
				dst8[MDE_COL_OFS_A] = 255;
				++src16;
				dst8 += 4;
			}
		}
		break;
	}
	return true;
}

#endif // MDE_SUPPORT_RGBA32

#endif // MDE_SUPPORT_SRGB16


// == RGB16 ==============================================================

#ifdef MDE_SUPPORT_MBGR16

void MDE_Scanline_InvColor_MBGR16(void *dst, int len, unsigned int dummy)
{
	unsigned short *dst16 = (unsigned short *) dst;
	for(int x = 0; x < len; x++)
	{
		*dst16 = MDE_MBGR16(255-MDE_COL_R16M(*dst16),
				255-MDE_COL_G16M(*dst16),
				255-MDE_COL_B16M(*dst16));
		++dst16;
	}
}

void MDE_Scanline_SetColor_MBGR16_NoBlend(void *dst, int len, unsigned int col)
{
	unsigned short col16 = MDE_MBGR16(MDE_COL_R(col), MDE_COL_G(col),
									  MDE_COL_B(col));
	memset16bit((unsigned short*)dst, col16, len);
}
void MDE_Scanline_SetColor_MBGR16_Blend(void *dst, int len, unsigned int col)
{
	if (MDE_COL_A(col) > 0)
	{
		unsigned char r = MDE_COL_R(col);
		unsigned char g = MDE_COL_G(col);
		unsigned char b = MDE_COL_B(col);
		unsigned char alpha = MDE_COL_A(col);
		unsigned short* dst16 = (unsigned short*)dst;
		while (len)
		{
			unsigned char dr = MDE_COL_R16M(*dst16);
			unsigned char dg = MDE_COL_G16M(*dst16);
			unsigned char db = MDE_COL_B16M(*dst16);

			// No need to use premultiplied alpha since the color is not premultiplied
			*dst16 = (unsigned short)MDE_MBGR16(
					dr+(alpha*(r-dr)>>8),
					dg+(alpha*(g-dg)>>8),
					db+(alpha*(b-db)>>8));
			--len;
			++dst16;
		}
	}
}

bool MDE_Scanline_BlitNormal_MBGR16_To_MBGR16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		op_memcpy(dst, src, len<<1);
		break;
	case MDE_METHOD_COLOR:
	{
		// FIXME: this is a hack to alow alphamaps which are aligned to two.
		// lower of the 2 bytes is the upper half of the image, the higher
		// of the two bytes is the lower half of the image.
		// First the upper half is drawn with mask = 0, then the lower with
		// mask = 8
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned short *src16 = (unsigned short *) src;
		unsigned char mask = srcinf->mask;
		unsigned char *col8 = (unsigned char *) &srcinf->col;
		unsigned char color_r = col8[MDE_COL_OFS_R];
		unsigned char color_g = col8[MDE_COL_OFS_G];
		unsigned char color_b = col8[MDE_COL_OFS_B];
		int alpha = MDE_COL_A(srcinf->col);
		if (alpha == 255)
		{
			for(int x = 0; x < len; x++)
			{
				unsigned char alpha = (src16[x]>>mask)&0xf;
				if (alpha)
				{
					unsigned char r, g, b;
					r = MDE_COL_R16M(*dst16);
					g = MDE_COL_G16M(*dst16);
					b = MDE_COL_B16M(*dst16);
					*dst16 = MDE_MBGR16(
						r + (alpha * (color_r-r)>>4),
						g + (alpha * (color_g-g)>>4),
						b + (alpha * (color_b-b)>>4));
				}
				++dst16;
			}
		}
		else if (alpha > 0)
		{
			++alpha;
			for(int x = 0; x < len; x++)
			{
				int a = (src16[x]>>mask)&0xf;
				if (a)
				{
					a = (a*alpha)>>8;
					unsigned char r, g, b;
					r = MDE_COL_R16M(*dst16);
					g = MDE_COL_G16M(*dst16);
					b = MDE_COL_B16M(*dst16);
					*dst16 = MDE_MBGR16(
						r + (a * (color_r-r)>>4),
						g + (a * (color_g-g)>>4),
						b + (a * (color_b-b)>>4));
				}
				++dst16;
			}
		}
	}
	break;
	}

	return true;
}
void MDE_Scanline_BlitStretch_MBGR16_To_MBGR16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				*dst16 = src16[sx>>16];
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}
#endif // MDE_SUPPORT_MBGR16

// == MDE_FORMAT_INDEX8 ==============================================================

void MDE_Scanline_SetColor_INDEX8_NoBlend(void *dst, int len, unsigned int col)
{
	op_memset(dst, col, len);
}
#define MDE_Scanline_SetColor_INDEX8_Blend MDE_Scanline_SetColor_INDEX8_NoBlend

bool MDE_Scanline_BlitNormal_INDEX8_To_BGRA32(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	    default:
			MDE_ASSERT(len == 0);
			return false;
		case MDE_METHOD_COPY:
			{
				unsigned char *dst8 = (unsigned char *) dst;
				unsigned char *src8 = (unsigned char *) src;
				for(x = 0; x < len; x++)
				{
					dst8[MDE_COL_OFS_B] = srcinf->pal[src8[x] * 3 + 2];
					dst8[MDE_COL_OFS_G] = srcinf->pal[src8[x] * 3 + 1];
					dst8[MDE_COL_OFS_R] = srcinf->pal[src8[x] * 3];
					dst8[MDE_COL_OFS_A] = 255;
					dst8 += 4;
				}
			}
			break;
		case MDE_METHOD_MASK:
			{
				unsigned char *dst8 = (unsigned char *) dst;
				unsigned char *src8 = (unsigned char *) src;
				for(x = 0; x < len; x++)
				{
					if (src8[x] != srcinf->mask)
					{
						dst8[MDE_COL_OFS_B] = srcinf->pal[src8[x] * 3 + 2];
						dst8[MDE_COL_OFS_G] = srcinf->pal[src8[x] * 3 + 1];
						dst8[MDE_COL_OFS_R] = srcinf->pal[src8[x] * 3];
						dst8[MDE_COL_OFS_A] = 255;
					}
					dst8 += 4;
				}
			}
			break;
		case MDE_METHOD_COLOR:
			{
				unsigned char *dst8 = (unsigned char *) dst;
				unsigned char *src8 = (unsigned char *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
				int alpha = MDE_COL_A(srcinf->col);
				if (alpha > 0)
				{
					++alpha;
					UINT32 c;
					unsigned char *col8 = (unsigned char *) &c;
					for(x = 0; x < len; x++)
					{
						c = g_opera->libgogi_module.m_color_lookup[src8[x]];
						SetPixel_BGRA32( dst8, col8, col8[MDE_COL_OFS_A] );
						dst8 += 4;
					}
				}
#else // !USE_PREMULTIPLIED_ALPHA
				unsigned char *col8 = (unsigned char *) &srcinf->col;
				int alpha = MDE_COL_A(srcinf->col);
				if (alpha == 255)
				{
					if (dstinf->method == MDE_METHOD_ALPHA)
						for(x = 0; x < len; x++)
						{
							SetPixelWithDestAlpha_BGRA32( dst8, col8, src8[x] );
							dst8 += 4;
						}
					else
						for(x = 0; x < len; x++)
						{
							SetPixel_BGRA32( dst8, col8, src8[x] );
							dst8 += 4;
						}
				}
				else if (alpha > 0)
				{
					if (dstinf->method == MDE_METHOD_ALPHA)
						for(x = 0; x < len; x++)
						{
							SetPixelWithDestAlpha_BGRA32( dst8, col8, (alpha*src8[x])>>8 );
							dst8 += 4;
						}
					else
						for(x = 0; x < len; x++)
						{
							SetPixel_BGRA32( dst8, col8, (alpha*src8[x])>>8 );
							dst8 += 4;
						}
				}
#endif // !USE_PREMULTIPLIED_ALPHA
			}
			break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_INDEX8_To_BGRA32(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char idx = src8[sx >> 16];
				dst8[MDE_COL_OFS_B] = srcinf->pal[idx * 3 + 2];
				dst8[MDE_COL_OFS_G] = srcinf->pal[idx * 3 + 1];
				dst8[MDE_COL_OFS_R] = srcinf->pal[idx * 3];
				dst8[MDE_COL_OFS_A] = 255;
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char idx = src8[sx >> 16];
				if (idx != srcinf->mask)
				{
					dst8[MDE_COL_OFS_B] = srcinf->pal[idx * 3 + 2];
					dst8[MDE_COL_OFS_G] = srcinf->pal[idx * 3 + 1];
					dst8[MDE_COL_OFS_R] = srcinf->pal[idx * 3];
					dst8[MDE_COL_OFS_A] = 255;
				}
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	}
}

#ifdef MDE_SUPPORT_BGR24

bool MDE_Scanline_BlitNormal_INDEX8_To_BGR24(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++)
		{
			dst8[0] = srcinf->pal[src8[x]*3+2];
			dst8[1] = srcinf->pal[src8[x]*3+1];
			dst8[2] = srcinf->pal[src8[x]*3];

			dst8 += 3;
		}
		break;
	}
	case MDE_METHOD_MASK:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++)
		{
			if (src8[x] != srcinf->mask)
			{
				dst8[0] = srcinf->pal[src8[x]*3+2];
				dst8[1] = srcinf->pal[src8[x]*3+1];
				dst8[2] = srcinf->pal[src8[x]*3];
			}
			dst8 += 3;
		}
		break;
	}
	case MDE_METHOD_COLOR:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		unsigned char *col8 = (unsigned char *) &srcinf->col;
		unsigned char color_r = col8[MDE_COL_OFS_R];
		unsigned char color_g = col8[MDE_COL_OFS_G];
		unsigned char color_b = col8[MDE_COL_OFS_B];
		int alpha = MDE_COL_A(srcinf->col);
		if (alpha == 255)
		{
			for(x = 0; x < len; x++)
			{
				if (src8[x])
				{
					unsigned char r, g, b;
					r = dst8[2];
					g = dst8[1];
					b = dst8[0];
					dst8[0]= b + (src8[x] * (color_b-b)>>8);
					dst8[1]= g + (src8[x] * (color_g-g)>>8);
					dst8[2]= r + (src8[x] * (color_r-r)>>8);
				}
				dst8 += 3;
			}
		}
		else if (alpha > 0)
		{
			++alpha;
			for(x = 0; x < len; x++)
			{
				if (src8[x])
				{
					int a = (src8[x]*alpha)>>8;
					unsigned char r, g, b;
					r = dst8[2];
					g = dst8[1];
					b = dst8[0];
					dst8[0]= b + (a * (color_b-b)>>8);
					dst8[1]= g + (a * (color_g-g)>>8);
					dst8[2]= r + (a * (color_r-r)>>8);
				}
				dst8 += 3;
			}
		}
		break;
	}
	}
	return true;
}


void MDE_Scanline_BlitStretch_INDEX8_To_BGR24(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	case MDE_METHOD_COPY:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++)
		{
			unsigned int idx = src8[sx >> 16]*3;
			dst8[2] = srcinf->pal[idx];
			dst8[1] = srcinf->pal[idx+1];
			dst8[0] = srcinf->pal[idx+2];
			dst8 += 3;
			sx += dx;
		}
		break;
	}
	case MDE_METHOD_MASK:
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++)
		{
			unsigned int idx = src8[sx >> 16];
			if (idx != srcinf->mask)
			{
				idx *= 3;
				dst8[2] = srcinf->pal[idx];
				dst8[1] = srcinf->pal[idx+1];
				dst8[0] = srcinf->pal[idx+2];
			}
			dst8 += 3;
			sx += dx;
		}
		break;
	}
	}
}

#endif // MDE_SUPPORT_BGR24


#ifdef MDE_SUPPORT_RGB16

bool MDE_Scanline_BlitNormal_INDEX8_To_RGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char *pal = srcinf->pal + src8[x] * 3;
				dst16[x] = MDE_RGB16(pal[0], pal[1], pal[2]);
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				if (src8[x] != srcinf->mask)
				{
					*dst16 = MDE_RGB16(srcinf->pal[src8[x]*3],
						srcinf->pal[src8[x]*3+1],
						srcinf->pal[src8[x]*3+2]);
				}
				++dst16;
			}
		}
		break;
	case MDE_METHOD_COLOR:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			unsigned char *col8 = (unsigned char *) &srcinf->col;
			unsigned char color_r = col8[MDE_COL_OFS_R];
			unsigned char color_g = col8[MDE_COL_OFS_G];
			unsigned char color_b = col8[MDE_COL_OFS_B];
			int alpha = MDE_COL_A(srcinf->col);
			if (alpha == 255)
			{
				for(x = 0; x < len; x++)
				{
					if (src8[x])
					{
						unsigned char r, g, b;
						r = MDE_COL_R16(*dst16);
						g = MDE_COL_G16(*dst16);
						b = MDE_COL_B16(*dst16);
						*dst16 = MDE_RGB16(
							r + (src8[x] * (color_r-r)>>8),
							g + (src8[x] * (color_g-g)>>8),
							b + (src8[x] * (color_b-b)>>8));
					}
					++dst16;
				}
			}
			else if (alpha > 0)
			{
				++alpha;
				for(x = 0; x < len; x++)
				{
					if (src8[x])
					{
						int a = (src8[x]*alpha)>>8;
						unsigned char r, g, b;
						r = MDE_COL_R16(*dst16);
						g = MDE_COL_G16(*dst16);
						b = MDE_COL_B16(*dst16);
						*dst16 = MDE_RGB16(
							r + (a * (color_r-r)>>8),
							g + (a * (color_g-g)>>8),
							b + (a * (color_b-b)>>8));
					}
					++dst16;
				}
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_INDEX8_To_RGB16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned int idx = src8[sx >> 16]*3;
				*dst16 = MDE_RGB16(srcinf->pal[idx],
						srcinf->pal[idx+1],
						srcinf->pal[idx+2]);
				++dst16;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned int idx = src8[sx >> 16];
				if (idx != srcinf->mask)
				{
					idx *= 3;
					*dst16 = MDE_RGB16(srcinf->pal[idx],
							srcinf->pal[idx+1],
							srcinf->pal[idx+2]);
				}
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}

#endif // MDE_SUPPORT_RGB16


#ifdef MDE_SUPPORT_SRGB16

bool MDE_Scanline_BlitNormal_INDEX8_To_SRGB16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				*dst16 = MDE_SRGB16(srcinf->pal[src8[x]*3],
						srcinf->pal[src8[x]*3+1],
						srcinf->pal[src8[x]*3+2]);
				++dst16;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				if (src8[x] != srcinf->mask)
				{
					*dst16 = MDE_SRGB16(srcinf->pal[src8[x]*3],
						srcinf->pal[src8[x]*3+1],
						srcinf->pal[src8[x]*3+2]);
				}
				++dst16;
			}
		}
		break;
	case MDE_METHOD_COLOR:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			unsigned char *col8 = (unsigned char *) &srcinf->col;
			unsigned char color_r = col8[MDE_COL_OFS_R];
			unsigned char color_g = col8[MDE_COL_OFS_G];
			unsigned char color_b = col8[MDE_COL_OFS_B];
			int alpha = MDE_COL_A(srcinf->col);
			if (alpha == 255)
			{
				for(x = 0; x < len; x++)
				{
					if (src8[x])
					{
						unsigned char r, g, b;
						r = MDE_COL_SR16(*dst16);
						g = MDE_COL_SG16(*dst16);
						b = MDE_COL_SB16(*dst16);
						*dst16 = MDE_SRGB16(
							r + (src8[x] * (color_r-r)>>8),
							g + (src8[x] * (color_g-g)>>8),
							b + (src8[x] * (color_b-b)>>8));
					}
					++dst16;
				}
			}
			else if (alpha > 0)
			{
				++alpha;
				for(x = 0; x < len; x++)
				{
					if (src8[x])
					{
						int a = (src8[x]*alpha)>>8;
						unsigned char r, g, b;
						r = MDE_COL_SR16(*dst16);
						g = MDE_COL_SG16(*dst16);
						b = MDE_COL_SB16(*dst16);
						*dst16 = MDE_SRGB16(
							r + (a * (color_r-r)>>8),
							g + (a * (color_g-g)>>8),
							b + (a * (color_b-b)>>8));
					}
					++dst16;
				}
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_INDEX8_To_SRGB16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned int idx = src8[sx >> 16]*3;
				*dst16 = MDE_SRGB16(srcinf->pal[idx],
						srcinf->pal[idx+1],
						srcinf->pal[idx+2]);
				++dst16;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned int idx = src8[sx >> 16];
				if (idx != srcinf->mask)
				{
					idx *= 3;
					*dst16 = MDE_SRGB16(srcinf->pal[idx],
							srcinf->pal[idx+1],
							srcinf->pal[idx+2]);
				}
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}

#endif // MDE_SUPPORT_SRGB16

#ifdef MDE_SUPPORT_MBGR16

bool MDE_Scanline_BlitNormal_INDEX8_To_MBGR16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				*dst16 = MDE_MBGR16(srcinf->pal[src8[x]*3],
						srcinf->pal[src8[x]*3+1],
						srcinf->pal[src8[x]*3+2]);
				++dst16;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				if (src8[x] != srcinf->mask)
				{
					*dst16 = MDE_MBGR16(srcinf->pal[src8[x]*3],
						srcinf->pal[src8[x]*3+1],
						srcinf->pal[src8[x]*3+2]);
				}
				++dst16;
			}
		}
		break;
	case MDE_METHOD_COLOR:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			unsigned char *col8 = (unsigned char *) &srcinf->col;
			unsigned char color_r = col8[MDE_COL_OFS_R];
			unsigned char color_g = col8[MDE_COL_OFS_G];
			unsigned char color_b = col8[MDE_COL_OFS_B];
			int alpha = MDE_COL_A(srcinf->col);
			if (alpha == 255)
			{
				for(x = 0; x < len; x++)
				{
					if (src8[x])
					{
						unsigned char r, g, b;
						r = MDE_COL_R16M(*dst16);
						g = MDE_COL_G16M(*dst16);
						b = MDE_COL_B16M(*dst16);
						*dst16 = MDE_MBGR16(
							r + (src8[x] * (color_r-r)>>8),
							g + (src8[x] * (color_g-g)>>8),
							b + (src8[x] * (color_b-b)>>8));
					}
					++dst16;
				}
			}
			else if (alpha > 0)
			{
				++alpha;
				for(x = 0; x < len; x++)
				{
					if (src8[x])
					{
						int a = (src8[x]*alpha)>>8;
						unsigned char r, g, b;
						r = MDE_COL_R16M(*dst16);
						g = MDE_COL_G16M(*dst16);
						b = MDE_COL_B16M(*dst16);
						*dst16 = MDE_MBGR16(
							r + (a * (color_r-r)>>8),
							g + (a * (color_g-g)>>8),
							b + (a * (color_b-b)>>8));
					}
					++dst16;
				}
			}
		}
		break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_INDEX8_To_MBGR16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned int idx = src8[sx >> 16]*3;
				*dst16 = MDE_MBGR16(srcinf->pal[idx],
						srcinf->pal[idx+1],
						srcinf->pal[idx+2]);
				++dst16;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned int idx = src8[sx >> 16];
				if (idx != srcinf->mask)
				{
					idx *= 3;
					*dst16 = MDE_MBGR16(srcinf->pal[idx],
							srcinf->pal[idx+1],
							srcinf->pal[idx+2]);
				}
				++dst16;
				sx += dx;
			}
		}
		break;
	}
}
#endif // MDE_SUPPORT_MBGR16

#ifdef MDE_SUPPORT_RGBA16

bool MDE_Scanline_BlitNormal_INDEX8_To_RGBA16(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				*dst16 = MDE_RGBA16(srcinf->pal[src8[x]*3],
						srcinf->pal[src8[x]*3+1],
						srcinf->pal[src8[x]*3+2],
						255);
				++dst16;
			}
		}
		break;

	case MDE_METHOD_COLOR:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
		unsigned char dst_b, dst_g, dst_r, dst_a;
#ifdef USE_PREMULTIPLIED_ALPHA
		int alpha = MDE_COL_A(srcinf->col);
		if (alpha > 0)
		{
			++alpha;
			UINT32 c;
			unsigned char *col8 = (unsigned char *) &c;
			for(x = 0; x < len; x++)
			{
				c = PremultiplyColor(srcinf->col, (src8[x]*alpha)>>8);
				dst_r = MDE_COL_R16A(*dst16);
				dst_g = MDE_COL_G16A(*dst16);
				dst_b = MDE_COL_B16A(*dst16);
				dst_a = MDE_COL_A16A(*dst16);
				dst_r = ((dst_r*(256-col8[MDE_COL_OFS_A]))>>8) + col8[MDE_COL_OFS_R];
				dst_g = ((dst_g*(256-col8[MDE_COL_OFS_A]))>>8) + col8[MDE_COL_OFS_G];
				dst_b = ((dst_b*(256-col8[MDE_COL_OFS_A]))>>8) + col8[MDE_COL_OFS_B];
				dst_a = ((dst_a*(256-col8[MDE_COL_OFS_A]))>>8) + col8[MDE_COL_OFS_A];
				*dst16 = MDE_RGBA16(dst_r,dst_g,dst_b,dst_a);
				++dst16;
			}
		}
#else // !USE_PREMULTIPLIED_ALPHA
		unsigned char *col8 = (unsigned char *) &srcinf->col;
		int alpha = MDE_COL_A(srcinf->col);
		if (alpha == 255)
		{
			if (dstinf->method == MDE_METHOD_ALPHA)
				for(x = 0; x < len; x++)
				{
					if (src8[x])
					{
						if (MDE_COL_A16A(*dst16) == 0)
						{
							dst_r = col8[MDE_COL_OFS_R];
							dst_g = col8[MDE_COL_OFS_G];
							dst_b = col8[MDE_COL_OFS_B];
							dst_a = src8[x];
						}
						else
						{
							int total_alpha = MDE_MIN(MDE_COL_A16A(*dst16) + src8[x], 255);
							int a = (src8[x]<<8) / (total_alpha + 1);
							dst_r = MDE_COL_R16A(*dst16);
							dst_g = MDE_COL_G16A(*dst16);
							dst_b = MDE_COL_B16A(*dst16);
							dst_a = MDE_COL_A16A(*dst16);
							dst_r += a * (col8[MDE_COL_OFS_R] - MDE_COL_R16A(*dst16)) >> 8;
							dst_g += a * (col8[MDE_COL_OFS_G] - MDE_COL_G16A(*dst16)) >> 8;
							dst_b += a * (col8[MDE_COL_OFS_B] - MDE_COL_B16A(*dst16)) >> 8;
							dst_a += (unsigned char) (long((src8[x]) * (256 - MDE_COL_A16A(*dst16))) >> 8);
						}
						*dst16 = MDE_RGBA16(dst_r,dst_g,dst_b,dst_a);
					}
					++dst16;
				}
			else
				for(x = 0; x < len; x++)
				{
					if (src8[x]) {
						unsigned char dst_b = MDE_COL_B16A(*dst16) + (src8[x] * (col8[MDE_COL_OFS_B] - MDE_COL_B16A(*dst16)) >> 8);
						unsigned char dst_g = MDE_COL_G16A(*dst16) + (src8[x] * (col8[MDE_COL_OFS_G] - MDE_COL_G16A(*dst16)) >> 8);
						unsigned char dst_r = MDE_COL_R16A(*dst16) + (src8[x] * (col8[MDE_COL_OFS_R] - MDE_COL_R16A(*dst16)) >> 8);
						*dst16 = (unsigned short) MDE_RGBA16(dst_r,dst_g,dst_b,255);
					}
					++dst16;
				}
		}
		else if (alpha > 0)
		{
			++alpha;
			if (dstinf->method == MDE_METHOD_ALPHA)
				for(x = 0; x < len; x++)
				{
					if (src8[x])
					{
						int sa = (src8[x]*alpha)>>8;
						if (MDE_COL_A16A(*dst16) == 0)
						{
							dst_r = col8[MDE_COL_OFS_R];
							dst_g = col8[MDE_COL_OFS_G];
							dst_b = col8[MDE_COL_OFS_B];
							dst_a = sa;
						}
						else
						{
							int total_alpha = MDE_MIN(MDE_COL_A16A(*dst16) + src8[x], 255);
							int a = (sa<<8) / (total_alpha + 1);
							dst_r = MDE_COL_R16A(*dst16);
							dst_g = MDE_COL_G16A(*dst16);
							dst_b = MDE_COL_B16A(*dst16);
							dst_a = MDE_COL_A16A(*dst16);
							dst_r += a * (col8[MDE_COL_OFS_R] - MDE_COL_R16A(*dst16)) >> 8;
							dst_g += a * (col8[MDE_COL_OFS_G] - MDE_COL_G16A(*dst16)) >> 8;
							dst_b += a * (col8[MDE_COL_OFS_B] - MDE_COL_B16A(*dst16)) >> 8;
							dst_a += (unsigned char) (long((sa) * (256 - MDE_COL_A16A(*dst16))) >> 8);
						}
						*dst16 = MDE_RGBA16(dst_r,dst_g,dst_b,dst_a);
					}
					++dst16;
				}
			else
				for(x = 0; x < len; x++)
				{
					if (src8[x]) {
						int a = (src8[x]*alpha)>>8;
						unsigned char dst_b = MDE_COL_B16A(*dst16) + (a * (col8[MDE_COL_OFS_B] - MDE_COL_B16A(*dst16)) >> 8);
						unsigned char dst_g = MDE_COL_G16A(*dst16) + (a * (col8[MDE_COL_OFS_G] - MDE_COL_G16A(*dst16)) >> 8);
						unsigned char dst_r = MDE_COL_R16A(*dst16) + (a * (col8[MDE_COL_OFS_R] - MDE_COL_R16A(*dst16)) >> 8);
						*dst16 = (unsigned short) MDE_RGBA16(dst_r,dst_g,dst_b,255);
					}
					++dst16;
				}
		}
#endif // !USE_PREMULTIPLIED_ALPHA
	}
	break;
	case MDE_METHOD_MASK:
	{
		unsigned short *dst16 = (unsigned short *) dst;
		unsigned char *src8 = (unsigned char *) src;
		for(x = 0; x < len; x++)
		{
			if (src8[x] != srcinf->mask)
			{
				*dst16 = (unsigned short) MDE_RGBA16(srcinf->pal[src8[x] * 3],     //R
									srcinf->pal[src8[x] * 3 + 1], //G
									srcinf->pal[src8[x] * 3 + 2], //B
									255);
			}
			++dst16;
		}
	}
	break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_INDEX8_To_RGBA16(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char idx = src8[sx >> 16];
				*dst16 = (unsigned short) MDE_RGBA16(srcinf->pal[idx * 3],     //R
									srcinf->pal[idx * 3 + 1], //G
									srcinf->pal[idx * 3 + 2], //B
									255);
				++dst16;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char idx = src8[sx >> 16];
				if (idx != srcinf->mask)
				{
					*dst16 = (unsigned short) MDE_RGBA16(srcinf->pal[idx * 3],     //R
										srcinf->pal[idx * 3 + 1], //G
										srcinf->pal[idx * 3 + 2], //B
										255);
				}
				++dst16;
				sx += dx;
			}
		}
		break;
	default:
		MDE_ASSERT(0);
		break;
	}
}


#endif // MDE_SUPPORT_RGBA16

#ifdef MDE_SUPPORT_RGBA32

bool MDE_Scanline_BlitNormal_INDEX8_To_RGBA32(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	    default:
			MDE_ASSERT(len == 0);
			return false;
		case MDE_METHOD_COPY:
			{
				unsigned char *dst8 = (unsigned char *) dst;
				unsigned char *src8 = (unsigned char *) src;
				for(x = 0; x < len; x++)
				{
					dst8[MDE_COL_OFS_B] = srcinf->pal[src8[x] * 3];
					dst8[MDE_COL_OFS_G] = srcinf->pal[src8[x] * 3 + 1];
					dst8[MDE_COL_OFS_R] = srcinf->pal[src8[x] * 3 + 2];
					dst8[MDE_COL_OFS_A] = 255;
					dst8 += 4;
				}
			}
			break;
		case MDE_METHOD_MASK:
			{
				unsigned char *dst8 = (unsigned char *) dst;
				unsigned char *src8 = (unsigned char *) src;
				for(x = 0; x < len; x++)
				{
					if (src8[x] != srcinf->mask)
					{
						dst8[MDE_COL_OFS_B] = srcinf->pal[src8[x] * 3];
						dst8[MDE_COL_OFS_G] = srcinf->pal[src8[x] * 3 + 1];
						dst8[MDE_COL_OFS_R] = srcinf->pal[src8[x] * 3 + 2];
						dst8[MDE_COL_OFS_A] = 255;
					}
					dst8 += 4;
				}
			}
			break;
		case MDE_METHOD_COLOR:
			{
				unsigned char *dst8 = (unsigned char *) dst;
				unsigned char *src8 = (unsigned char *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
				int alpha = MDE_COL_A(srcinf->col);
				if (alpha > 0)
				{
					++alpha;
					UINT32 c;
					unsigned char *col8 = (unsigned char *) &c;
					for(x = 0; x < len; x++)
					{
						c = PremultiplyColor(srcinf->col, (src8[x]*alpha)>>8);
						SetPixel_RGBA32( dst8, col8, col8[MDE_COL_OFS_A] );
						dst8 += 4;
					}
				}
#else // !USE_PREMULTIPLIED_ALPHA
				unsigned char *col8 = (unsigned char *) &srcinf->col;
				int alpha = MDE_COL_A(srcinf->col);
				if (alpha == 255)
				{
					if (dstinf->method == MDE_METHOD_ALPHA)
						for(x = 0; x < len; x++)
						{
							SetPixelWithDestAlpha_RGBA32( dst8, col8, src8[x] );
							dst8 += 4;
						}
					else
						for(x = 0; x < len; x++)
						{
							SetPixel_RGBA32( dst8, col8, src8[x] );
							dst8 += 4;
						}
				}
				else if (alpha > 0)
				{
					++alpha;
					if (dstinf->method == MDE_METHOD_ALPHA)
						for(x = 0; x < len; x++)
						{
							SetPixelWithDestAlpha_RGBA32( dst8, col8, (src8[x]*alpha)>>8 );
							dst8 += 4;
						}
					else
						for(x = 0; x < len; x++)
						{
							SetPixel_RGBA32( dst8, col8, (src8[x]*alpha)>>8 );
							dst8 += 4;
						}
				}
#endif // !USE_PREMULTIPLIED_ALPHA
			}
			break;
	}
	return true;
}

#endif // MDE_SUPPORT_RGBA32

// == MDE_FORMAT_RGBA32 =====================================================

#ifdef MDE_SUPPORT_RGBA32

// This one has the exact same implementation as BGRA32_To_BGRA32
#define MDE_Scanline_BlitNormal_RGBA32_To_RGBA32 MDE_Scanline_BlitNormal_BGRA32_To_BGRA32

#ifdef MDE_SUPPORT_RGB16

bool MDE_Scanline_BlitNormal_RGBA32_To_RGB16(void *dst, void *src, int len,
											 MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
			OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for(x = 0; x < len; x++, src8+=4)
			{
				dst16[x] = MDE_RGB16(src8[MDE_COL_OFS_B], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_R]);
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++, src8 += 4)
			{
				if (!(src8[MDE_COL_OFS_B] == MDE_COL_R(srcinf->mask) &&
					  src8[MDE_COL_OFS_G] == MDE_COL_G(srcinf->mask) &&
					  src8[MDE_COL_OFS_R] == MDE_COL_B(srcinf->mask)))
				{
#ifdef USE_PREMULTIPLIED_ALPHA
					OP_ASSERT(src8[MDE_COL_OFS_A] == 255);
#endif // USE_PREMULTIPLIED_ALPHA
					dst16[x] = MDE_RGB16(src8[MDE_COL_OFS_B], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_R]);
				}
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++, src8+=4)
			{
				int a = src8[MDE_COL_OFS_A];
				if (a == 0)
				{
					continue;
				}
				if (a == 255)
				{
					dst16[x] = MDE_RGB16(src8[MDE_COL_OFS_B], src8[MDE_COL_OFS_G], src8[MDE_COL_OFS_R]);
					continue;
				}
				unsigned char dst_r = MDE_COL_R16(dst16[x]);
				unsigned char dst_g = MDE_COL_G16(dst16[x]);
				unsigned char dst_b = MDE_COL_B16(dst16[x]);
#ifdef USE_PREMULTIPLIED_ALPHA
				dst16[x] = MDE_RGB16(
					((dst_r*(256-a))>>8)+src8[MDE_COL_OFS_B],
					((dst_g*(256-a))>>8)+src8[MDE_COL_OFS_G],
					((dst_b*(256-a))>>8)+src8[MDE_COL_OFS_R]);
#else // !USE_PREMULTIPLIED_ALPHA
				++a;
				dst16[x] = MDE_RGB16(
							dst_r+((a * (src8[MDE_COL_OFS_B]-dst_r)) >> 8),
							dst_g+((a * (src8[MDE_COL_OFS_G]-dst_g)) >> 8),
							dst_b+((a * (src8[MDE_COL_OFS_R]-dst_b)) >> 8)
							);
#endif // !USE_PREMULTIPLIED_ALPHA
			}
		}
		break;
	}
	return true;
}


void MDE_Scanline_BlitStretch_RGBA32_To_RGB16(void *dst, void *src, int len,
											  MDE_F1616 sx, MDE_F1616 dx,
											  MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned short *dst16 = (unsigned short *) dst;
#ifdef USE_PREMULTIPLIED_ALPHA
			OP_ASSERT(!"Copy a premultiplied bitmap to a non premultiplied bitmap should be avoided");
#endif // !USE_PREMULTIPLIED_ALPHA
			for (x = 0; x < len; ++x, ++dst16, sx+=dx)
			{
				unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
				unsigned char src_r = src8[MDE_COL_OFS_B];
				unsigned char src_g = src8[MDE_COL_OFS_G];
				unsigned char src_b = src8[MDE_COL_OFS_R];
				*dst16 = (unsigned short)MDE_RGB16(src_r, src_g, src_b);
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned short *dst16 = (unsigned short *) dst;
//			unsigned char* src8 = static_cast<unsigned int*>(src);
			for(x = 0; x < len; x++, sx+=dx)
			{
				unsigned char* src8 = (unsigned char*) (((unsigned int*)src)+(sx>>16));
				unsigned char src_r = src8[MDE_COL_OFS_B];
				unsigned char src_g = src8[MDE_COL_OFS_G];
				unsigned char src_b = src8[MDE_COL_OFS_R];
				unsigned char src_a = src8[MDE_COL_OFS_A];

				if (src_a == 0)
				{
					continue;
				}
				if (src_a == 255)
				{
					dst16[x] = MDE_RGB16(src_r, src_g, src_b);
					continue;
				}
				unsigned char dst_r = MDE_COL_R16(dst16[x]);
				unsigned char dst_g = MDE_COL_G16(dst16[x]);
				unsigned char dst_b = MDE_COL_B16(dst16[x]);
#ifdef USE_PREMULTIPLIED_ALPHA
				dst16[x] = MDE_RGB16(
					((dst_r*(256-src_a))>>8)+src_r,
					((dst_g*(256-src_a))>>8)+src_g,
					((dst_b*(256-src_a))>>8)+src_b);
#else // !USE_PREMULTIPLIED_ALPHA
				int a = src_a + 1;
				dst16[x] = MDE_RGB16(
							dst_r+((a * (src_r-dst_r)) >> 8),
							dst_g+((a * (src_g-dst_g)) >> 8),
							dst_b+((a * (src_b-dst_b)) >> 8)
							);
#endif // !USE_PREMULTIPLIED_ALPHA
			}
		}
		break;
	}
}

void MDE_Scanline_BlitStretch_RGB16_To_RGBA32(void *dst, void *src, int len,
											  MDE_F1616 sx, MDE_F1616 dx,
											  MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned short *src16 = (unsigned short *) src;
			for (x = 0; x < len; ++x)
			{
				unsigned short c = src16[sx>>16];
				dst8[MDE_COL_OFS_B] = MDE_COL_R16(c);
				dst8[MDE_COL_OFS_G] = MDE_COL_G16(c);
				dst8[MDE_COL_OFS_R] = MDE_COL_B16(c);
				dst8[MDE_COL_OFS_A] = 255;
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	}
}

#endif // MDE_SUPPORT_RGB16

void MDE_Scanline_BlitStretch_INDEX8_To_RGBA32(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char idx = src8[sx >> 16];
				dst8[MDE_COL_OFS_B] = srcinf->pal[idx * 3];
				dst8[MDE_COL_OFS_G] = srcinf->pal[idx * 3 + 1];
				dst8[MDE_COL_OFS_R] = srcinf->pal[idx * 3 + 2];
				dst8[MDE_COL_OFS_A] = 255;
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char idx = src8[sx >> 16];
				if (idx != srcinf->mask)
				{
					dst8[MDE_COL_OFS_B] = srcinf->pal[idx * 3];
					dst8[MDE_COL_OFS_G] = srcinf->pal[idx * 3 + 1];
					dst8[MDE_COL_OFS_R] = srcinf->pal[idx * 3 + 2];
					dst8[MDE_COL_OFS_A] = 255;
				}
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	}
}

// These have the exact same implementations
#define MDE_Scanline_BlitStretch_BGRA32_To_RGBA32 MDE_Scanline_BlitStretch_RGBA32_To_BGRA32

void MDE_Scanline_BlitStretch_RGBA32_To_BGRA32(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			for(x = 0; x < len; x++)
			{
				int idx = sx >> 16;
				unsigned char* src8 = ((unsigned char*)src)+4*idx;
				dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_R];
				dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_G];
				dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_B];
				dst8[MDE_COL_OFS_A] = src8[MDE_COL_OFS_A];
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			for(x = 0; x < len; x++)
			{
				int idx = sx >> 16;
				unsigned char* src8 = ((unsigned char*)src)+4*idx;
				if (*((unsigned int*)src8) != srcinf->mask)
				{
					dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_R];
					dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_G];
					dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_B];
					dst8[MDE_COL_OFS_A] = src8[MDE_COL_OFS_A];
				}
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			for(x = 0; x < len; x++)
			{
				int idx = sx >> 16;
				unsigned char* src8 = ((unsigned char*)src)+4*idx;
				unsigned char src_bgra[4]; // ARRAY OK 2009-04-24 wonko
				src_bgra[MDE_COL_OFS_B] = src8[MDE_COL_OFS_R];
				src_bgra[MDE_COL_OFS_G] = src8[MDE_COL_OFS_G];
				src_bgra[MDE_COL_OFS_R] = src8[MDE_COL_OFS_B];
				src_bgra[MDE_COL_OFS_A] = src8[MDE_COL_OFS_A];
				if (dstinf->method == MDE_METHOD_ALPHA)
				    SetPixelWithDestAlpha_BGRA32( dst8, src_bgra, src8[MDE_COL_OFS_A] );
				else
				    SetPixel_BGRA32( dst8, src_bgra, src8[MDE_COL_OFS_A] );
			    dst8 += 4;
				sx += dx;
			}
		}
		break;
	}
}

// These have the exact same implementations
#define MDE_Scanline_BlitNormal_BGRA32_To_RGBA32 MDE_Scanline_BlitNormal_RGBA32_To_BGRA32

bool MDE_Scanline_BlitNormal_RGBA32_To_BGRA32(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_R];
				dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_G];
				dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_B];
				dst8[MDE_COL_OFS_A] = src8[MDE_COL_OFS_A];
				dst8 += 4;
				src8 += 4;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				if (*((unsigned int*)src8) != srcinf->mask)
				{
					dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_R];
					dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_G];
					dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_B];
					dst8[MDE_COL_OFS_A] = src8[MDE_COL_OFS_A];
				}
				dst8 += 4;
				src8 += 4;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char src_bgra[4]; // ARRAY OK 2009-04-24 wonko
				src_bgra[MDE_COL_OFS_B] = src8[MDE_COL_OFS_R];
				src_bgra[MDE_COL_OFS_G] = src8[MDE_COL_OFS_G];
				src_bgra[MDE_COL_OFS_R] = src8[MDE_COL_OFS_B];
				src_bgra[MDE_COL_OFS_A] = src8[MDE_COL_OFS_A];
				if (dstinf->method == MDE_METHOD_ALPHA)
				    SetPixelWithDestAlpha_BGRA32( dst8, src_bgra, src8[MDE_COL_OFS_A] );
				else
				    SetPixel_BGRA32( dst8, src_bgra, src8[MDE_COL_OFS_A] );
			    dst8 += 4;
			    src8 += 4;
			}
		}
		break;
	}
	return true;
}

// The exact same implementation
#define MDE_Scanline_InvColor_RGBA32 MDE_Scanline_InvColor_BGRA32

void MDE_Scanline_SetColor_RGBA32_NoBlend(void *dst, int len, unsigned int col)
{
	union
	{
		unsigned char color[4]; // ARRAY OK 2009-06-04 wonko
		unsigned int col_as_int;
	};
	color[MDE_COL_OFS_B] = MDE_COL_R(col);
	color[MDE_COL_OFS_G] = MDE_COL_G(col);
	color[MDE_COL_OFS_R] = MDE_COL_B(col);
	color[MDE_COL_OFS_A] = MDE_COL_A(col);
	unsigned int* dst_as_ints = (unsigned int*)dst;
	for(int x = 0; x < len; x++)
		dst_as_ints[x] = col_as_int;
}
void MDE_Scanline_SetColor_RGBA32_Blend(void *dst, int len, unsigned int col)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	// Premultiply the color
	const unsigned char alpha = MDE_COL_A(col);
# ifdef MDE_USE_ALPHA_LOOKUPTABLE
	const unsigned char* ltab = g_opera->libgogi_module.lutbl_alpha + (alpha << 8);
	const unsigned char red = ltab[MDE_COL_R(col)];
	const unsigned char green = ltab[MDE_COL_G(col)];
	const unsigned char blue = ltab[MDE_COL_B(col)];
# else // MDE_USE_ALPHA_LOOKUPTABLE
	int a = alpha + 1;
	int red = (MDE_COL_R(col)*a)>>8;
	int green = (MDE_COL_G(col)*a)>>8;
	int blue = (MDE_COL_B(col)*a)>>8;
# endif // MDE_USE_ALPHA_LOOKUPTABLE
	col = MDE_RGBA(red, green, blue, alpha);
#endif // USE_PREMULTIPLIED_ALPHA
	if (MDE_COL_A(col) > 0)
	{
		unsigned char *dst8 = (unsigned char *) dst;
		unsigned char *col8 = (unsigned char *) &col;
		for(int x = 0; x < len; x++)
		{
			SetPixelWithDestAlpha_RGBA32(dst8, col8, col8[MDE_COL_OFS_A]);
			dst8 += 4;
		}
	}
}

// These have the exact same implementations
#define MDE_Scanline_BlitStretch_RGBA32_To_RGBA32 MDE_Scanline_BlitStretch_BGRA32_To_BGRA32

#endif // MDE_SUPPORT_RGBA32

#ifdef MDE_SUPPORT_ARGB32

void MDE_Scanline_SetColor_ARGB32_NoBlend(void *dst, int len, unsigned int col)
{
	union
	{
		unsigned char color[4]; // ARRAY OK 2009-06-04 wonko
		unsigned int col_as_int;
	};
	color[MDE_COL_OFS_B] = MDE_COL_A(col);
	color[MDE_COL_OFS_G] = MDE_COL_R(col);
	color[MDE_COL_OFS_R] = MDE_COL_G(col);
	color[MDE_COL_OFS_A] = MDE_COL_B(col);
	unsigned int* dst_as_ints = (unsigned int*)dst;
	for(int x = 0; x < len; x++)
		dst_as_ints[x] = col_as_int;
}

void MDE_Scanline_SetColor_ARGB32_Blend(void *dst, int len, unsigned int col)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	// Premultiply the color
	int alpha = MDE_COL_A(col)+1;
	int red = (MDE_COL_R(col)*alpha)>>8;
	int green = (MDE_COL_G(col)*alpha)>>8;
	int blue = (MDE_COL_B(col)*alpha)>>8;
	col = MDE_RGBA(red, green, blue, alpha-1);
#endif // USE_PREMULTIPLIED_ALPHA
	unsigned char color[4]; // ARRAY OK 2009-06-04 wonko
	color[MDE_COL_OFS_B] = MDE_COL_A(col);
	color[MDE_COL_OFS_G] = MDE_COL_R(col);
	color[MDE_COL_OFS_R] = MDE_COL_G(col);
	color[MDE_COL_OFS_A] = MDE_COL_B(col);
	if (MDE_COL_A(col) > 0)
	{
		unsigned char *dst8 = (unsigned char *) dst;
		for(int x = 0; x < len; x++)
		{
			SetPixelWithDestAlpha_ARGB32(dst8, color, MDE_COL_A(col) );
		    dst8 += 4;
		}
	}
}

#define MDE_Scanline_InvColor_ARGB32 MDE_Scanline_InvColor_BGRA32

void MDE_Scanline_BlitStretch_BGRA32_To_ARGB32(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			for(x = 0; x < len; x++)
			{
				int idx = sx >> 16;
				unsigned char* src8 = ((unsigned char*)src)+4*idx;
				dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_A];
				dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_R];
				dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_G];
				dst8[MDE_COL_OFS_A] = src8[MDE_COL_OFS_B];
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			for(x = 0; x < len; x++)
			{
				int idx = sx >> 16;
				unsigned char* src8 = ((unsigned char*)src)+4*idx;
				if (*((unsigned int*)src8) != srcinf->mask)
				{
					dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_A];
					dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_R];
					dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_G];
					dst8[MDE_COL_OFS_A] = src8[MDE_COL_OFS_B];
				}
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			for(x = 0; x < len; x++)
			{
				// The SetPixel methods expect a BGRA32 pixel, so no need for
				// conversion before calling them.
				int idx = sx >> 16;
				unsigned char* src8 = ((unsigned char*)src)+4*idx;
				if (dstinf->method == MDE_METHOD_ALPHA)
				    SetPixelWithDestAlpha_ARGB32( dst8, src8, src8[MDE_COL_OFS_A] );
				else
				    SetPixel_ARGB32( dst8, src8, src8[MDE_COL_OFS_A] );
			    dst8 += 4;
				sx += dx;
			}
		}
		break;
	}
}

// Note that the implementation of this function is not the same as
// ARGB32_To_BGRA32. The problem being that SetPixelWithDestAlpha uses
// MDE_COL_OFS_X to get the destination buffer's alpha channel.
bool MDE_Scanline_BlitNormal_BGRA32_To_ARGB32(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(len == 0);
		return false;
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_A];
				dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_R];
				dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_G];
				dst8[MDE_COL_OFS_A] = src8[MDE_COL_OFS_B];
				dst8 += 4;
				src8 += 4;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				if (*((unsigned int*)src8) != srcinf->mask)
				{
					dst8[MDE_COL_OFS_B] = src8[MDE_COL_OFS_A];
					dst8[MDE_COL_OFS_G] = src8[MDE_COL_OFS_R];
					dst8[MDE_COL_OFS_R] = src8[MDE_COL_OFS_G];
					dst8[MDE_COL_OFS_A] = src8[MDE_COL_OFS_B];
				}
				dst8 += 4;
				src8 += 4;
			}
		}
		break;
	case MDE_METHOD_ALPHA:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				// The SetPixel methods expect a BGRA32 pixel, so no need for
				// conversion before calling them.
				if (dstinf->method == MDE_METHOD_ALPHA)
				    SetPixelWithDestAlpha_ARGB32( dst8, src8, src8[MDE_COL_OFS_A] );
				else
				    SetPixel_ARGB32( dst8, src8, src8[MDE_COL_OFS_A] );
			    dst8 += 4;
			    src8 += 4;
			}
		}
		break;
	}
	return true;
}

bool MDE_Scanline_BlitNormal_INDEX8_To_ARGB32(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	    default:
			MDE_ASSERT(len == 0);
			return false;
		case MDE_METHOD_COPY:
			{
				unsigned char *dst8 = (unsigned char *) dst;
				unsigned char *src8 = (unsigned char *) src;
				for(x = 0; x < len; x++)
				{
					dst8[MDE_COL_OFS_A] = srcinf->pal[src8[x] * 3 + 2]; // Blue
					dst8[MDE_COL_OFS_R] = srcinf->pal[src8[x] * 3 + 1]; // Green
					dst8[MDE_COL_OFS_G] = srcinf->pal[src8[x] * 3];     // Red
					dst8[MDE_COL_OFS_B] = 255;
					dst8 += 4;
				}
			}
			break;
		case MDE_METHOD_MASK:
			{
				unsigned char *dst8 = (unsigned char *) dst;
				unsigned char *src8 = (unsigned char *) src;
				for(x = 0; x < len; x++)
				{
					if (src8[x] != srcinf->mask)
					{
						dst8[MDE_COL_OFS_A] = srcinf->pal[src8[x] * 3 + 2]; // Blue
						dst8[MDE_COL_OFS_R] = srcinf->pal[src8[x] * 3 + 1]; // Green
						dst8[MDE_COL_OFS_G] = srcinf->pal[src8[x] * 3];     // Red
						dst8[MDE_COL_OFS_B] = 255;
					}
					dst8 += 4;
				}
			}
			break;
		case MDE_METHOD_COLOR:
			{
				unsigned char *dst8 = (unsigned char *) dst;
				unsigned char *src8 = (unsigned char *) src;
#ifdef USE_PREMULTIPLIED_ALPHA
				int alpha = MDE_COL_A(srcinf->col);
				if (alpha > 0)
				{
					++alpha;
					UINT32 c;
					unsigned char *col8 = (unsigned char *) &c;
					for(x = 0; x < len; x++)
					{
						c = PremultiplyColor(srcinf->col, (src8[x]*alpha)>>8);
						SetPixel_ARGB32( dst8, col8, col8[MDE_COL_OFS_A] );
						dst8 += 4;
					}
				}
#else // !USE_PREMULTIPLIED_ALPHA
				unsigned char *col8 = (unsigned char *) &srcinf->col;
				int alpha = MDE_COL_A(srcinf->col);
				if (alpha == 255)
				{
					if (dstinf->method == MDE_METHOD_ALPHA)
						for(x = 0; x < len; x++)
						{
							SetPixelWithDestAlpha_ARGB32( dst8, col8, src8[x] );
							dst8 += 4;
						}
					else
						for(x = 0; x < len; x++)
						{
							SetPixel_ARGB32( dst8, col8, src8[x] );
							dst8 += 4;
						}
				}
				else if (alpha > 0)
				{
					if (dstinf->method == MDE_METHOD_ALPHA)
						for(x = 0; x < len; x++)
						{
							SetPixelWithDestAlpha_ARGB32( dst8, col8, (src8[x]*alpha)>>8 );
							dst8 += 4;
						}
					else
						for(x = 0; x < len; x++)
						{
							SetPixel_ARGB32( dst8, col8, (src8[x]*alpha)>>8 );
							dst8 += 4;
						}
				}
#endif // !USE_PREMULTIPLIED_ALPHA
			}
			break;
	}
	return true;
}

void MDE_Scanline_BlitStretch_INDEX8_To_ARGB32(void *dst, void *src, int len, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf)
{
	int x;
	switch(srcinf->method)
	{
	default:
		MDE_ASSERT(0);
	case MDE_METHOD_COPY:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char idx = src8[sx >> 16];
				dst8[MDE_COL_OFS_A] = srcinf->pal[idx * 3 + 2];
				dst8[MDE_COL_OFS_R] = srcinf->pal[idx * 3 + 1];
				dst8[MDE_COL_OFS_G] = srcinf->pal[idx * 3];
				dst8[MDE_COL_OFS_B] = 255;
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	case MDE_METHOD_MASK:
		{
			unsigned char *dst8 = (unsigned char *) dst;
			unsigned char *src8 = (unsigned char *) src;
			for(x = 0; x < len; x++)
			{
				unsigned char idx = src8[sx >> 16];
				if (idx != srcinf->mask)
				{
					dst8[MDE_COL_OFS_A] = srcinf->pal[idx * 3 + 2];
					dst8[MDE_COL_OFS_R] = srcinf->pal[idx * 3 + 1];
					dst8[MDE_COL_OFS_G] = srcinf->pal[idx * 3];
					dst8[MDE_COL_OFS_B] = 255;
				}
				dst8 += 4;
				sx += dx;
			}
		}
		break;
	}
}

#endif // MDE_SUPPORT_ARGB32



// ========================================================================

MDE_SCANLINE_SETCOLOR MDE_GetScanline_InvertColor(MDE_BUFFER *dstbuf, bool blend/*=true*/)
{
	switch(dstbuf->format)
	{
	case MDE_FORMAT_BGRA32:			return MDE_Scanline_InvColor_BGRA32;
#ifdef MDE_SUPPORT_RGBA32
	case MDE_FORMAT_RGBA32:			return MDE_Scanline_InvColor_RGBA32;
#endif
#ifdef MDE_SUPPORT_ARGB32
	case MDE_FORMAT_ARGB32:			return MDE_Scanline_InvColor_ARGB32;
#endif
	case MDE_FORMAT_BGR24:			return MDE_Scanline_InvColor_BGR24;
#ifdef MDE_SUPPORT_RGBA16
	case MDE_FORMAT_RGBA16:			return MDE_Scanline_InvColor_RGBA16;
#endif
#ifdef MDE_SUPPORT_RGB16
	case MDE_FORMAT_RGB16:			return MDE_Scanline_InvColor_RGB16;
#endif
#ifdef MDE_SUPPORT_MBGR16
	case MDE_FORMAT_MBGR16:			return MDE_Scanline_InvColor_MBGR16;
#endif
#ifdef MDE_SUPPORT_SRGB16
	case MDE_FORMAT_SRGB16:			return MDE_Scanline_InvColor_SRGB16;
#endif
	default:						return MDE_Scanline_InvColor_unsupported;
	}
}

MDE_SCANLINE_SETCOLOR MDE_GetScanline_SetColor(MDE_BUFFER *dstbuf, bool blend/*=true*/)
{
	blend &= MDE_COL_A(dstbuf->col) != 255;

#define RETURN_MDE_SCANLINE_SETCOLOR_F(color)				\
	if (blend)												\
		return MDE_Scanline_SetColor_##color##_Blend;		\
	else													\
		return MDE_Scanline_SetColor_##color##_NoBlend;

	switch(dstbuf->format)
	{
	case MDE_FORMAT_BGRA32:	RETURN_MDE_SCANLINE_SETCOLOR_F(BGRA32);
	case MDE_FORMAT_BGR24:	RETURN_MDE_SCANLINE_SETCOLOR_F(BGR24);
	case MDE_FORMAT_INDEX8:	RETURN_MDE_SCANLINE_SETCOLOR_F(INDEX8);

#ifdef MDE_SUPPORT_RGB16
	case MDE_FORMAT_RGB16:  RETURN_MDE_SCANLINE_SETCOLOR_F(RGB16);
#endif // MDE_SUPPORT_RGB16
#ifdef MDE_SUPPORT_RGBA16
	case MDE_FORMAT_RGBA16:	RETURN_MDE_SCANLINE_SETCOLOR_F(RGBA16);
#endif // MDE_SUPPORT_RGBA16
#ifdef MDE_SUPPORT_RGBA24
	case MDE_FORMAT_RGBA24: RETURN_MDE_SCANLINE_SETCOLOR_F(RGBA24);
#endif
#ifdef MDE_SUPPORT_SRGB16
	case MDE_FORMAT_SRGB16: RETURN_MDE_SCANLINE_SETCOLOR_F(SRGB16);
#endif // MDE_SUPPORT_SRGB16
#ifdef MDE_SUPPORT_MBGR16
	case MDE_FORMAT_MBGR16: RETURN_MDE_SCANLINE_SETCOLOR_F(MBGR16);
#endif // MDE_SUPPORT_MBGR16

#ifdef MDE_SUPPORT_RGBA32
	case MDE_FORMAT_RGBA32: RETURN_MDE_SCANLINE_SETCOLOR_F(RGBA32);
#endif // MDE_SUPPORT_RGBA32
#ifdef MDE_SUPPORT_ARGB32
	case MDE_FORMAT_ARGB32: RETURN_MDE_SCANLINE_SETCOLOR_F(ARGB32);
#endif // MDE_SUPPORT_ARGB32
#undef RETURN_MDE_SCANLINE_SETCOLOR

	default:				return MDE_Scanline_SetColor_unsupported;
	}
}

#define MDE_Scanline_BlitNormal_RGBA16_To_RGBA16 MDE_Scanline_BlitNormal_RGB16_To_RGB16;

MDE_SCANLINE_BLITNORMAL MDE_GetScanline_BlitNormal(MDE_BUFFER *dstbuf, MDE_FORMAT src_format)
{
	switch(dstbuf->format)
	{
	case MDE_FORMAT_BGRA32:
		switch(src_format)
		{
			case MDE_FORMAT_BGRA32:	return MDE_Scanline_BlitNormal_BGRA32_To_BGRA32;
			case MDE_FORMAT_BGR24:	return MDE_Scanline_BlitNormal_BGR24_To_BGRA32;
			case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitNormal_INDEX8_To_BGRA32;
#ifdef MDE_SUPPORT_RGBA32
			case MDE_FORMAT_RGBA32: return MDE_Scanline_BlitNormal_RGBA32_To_BGRA32;
#endif
#ifdef MDE_SUPPORT_RGBA24
			case MDE_FORMAT_RGBA24: return MDE_Scanline_BlitNormal_RGBA24_To_BGRA32;
#endif // MDE_SUPPORT_RGBA24
#ifdef MDE_SUPPORT_RGB16
			case MDE_FORMAT_RGB16: return MDE_Scanline_BlitNormal_RGB16_To_BGRA32;
			case MDE_FORMAT_RGBA16: return MDE_Scanline_BlitNormal_RGBA16_To_BGRA32;
#endif
#ifdef MDE_SUPPORT_SRGB16
			case MDE_FORMAT_SRGB16: return MDE_Scanline_BlitNormal_SRGB16_To_BGRA32;
#endif
		}
		break;
#ifdef MDE_SUPPORT_RGB16
	case MDE_FORMAT_RGB16:
		switch(src_format)
		{
#ifdef MDE_SUPPORT_RGBA16
			case MDE_FORMAT_RGBA16: return MDE_Scanline_BlitNormal_RGBA16_To_RGB16;
#endif
#ifdef MDE_SUPPORT_RGBA24
			case MDE_FORMAT_RGBA24: return MDE_Scanline_BlitNormal_RGBA24_To_RGB16;
#endif // MDE_SUPPORT_RGBA24
#ifdef MDE_SUPPORT_SRGB16
			case MDE_FORMAT_SRGB16: return MDE_Scanline_BlitNormal_SRGB16_To_RGB16;
#endif
			case MDE_FORMAT_RGB16: return MDE_Scanline_BlitNormal_RGB16_To_RGB16;
			case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitNormal_INDEX8_To_RGB16;
			case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitNormal_BGRA32_To_RGB16;
#ifdef MDE_SUPPORT_BGR24
			case MDE_FORMAT_BGR24: return MDE_Scanline_BlitNormal_BGR24_To_RGB16;
#endif
#ifdef MDE_SUPPORT_RGBA32
			case MDE_FORMAT_RGBA32: return MDE_Scanline_BlitNormal_RGBA32_To_RGB16;
#endif
		}
		break;
#endif
#ifdef MDE_SUPPORT_RGBA24
	case MDE_FORMAT_RGBA24:
		switch (src_format)
		{
		case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitNormal_INDEX8_To_RGBA24;
		case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitNormal_BGRA32_To_RGBA24;
		case MDE_FORMAT_RGBA24: return MDE_Scanline_BlitNormal_RGBA24_To_RGBA24;
		}
		break;
#endif // MDE_SUPPORT_RGBA24
#ifdef MDE_SUPPORT_SRGB16
	case MDE_FORMAT_SRGB16:
		switch(src_format)
		{
		    case MDE_FORMAT_RGB16: return MDE_Scanline_BlitNormal_RGB16_To_SRGB16;
			case MDE_FORMAT_SRGB16: return MDE_Scanline_BlitNormal_SRGB16_To_SRGB16;
			case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitNormal_INDEX8_To_SRGB16;
			case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitNormal_BGRA32_To_SRGB16;
#ifdef MDE_SUPPORT_BGR24
		    case MDE_FORMAT_BGR24: return MDE_Scanline_BlitNormal_BGR24_To_SRGB16;
#endif //MDE_SUPPORT_BGR24
		}
		break;
#endif
#ifdef MDE_SUPPORT_MBGR16
	case MDE_FORMAT_MBGR16:
		switch(src_format)
		{
#ifdef MDE_SUPPORT_RGBA16
			case MDE_FORMAT_RGBA16: return MDE_Scanline_BlitNormal_RGBA16_To_MBGR16;
#endif
#ifdef MDE_SUPPORT_RGB16
			case MDE_FORMAT_RGB16: return MDE_Scanline_BlitNormal_RGB16_To_MBGR16;
#endif
			case MDE_FORMAT_MBGR16: return MDE_Scanline_BlitNormal_MBGR16_To_MBGR16;
			case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitNormal_INDEX8_To_MBGR16;
		}
		break;
#endif
#ifdef MDE_SUPPORT_RGBA16
	case MDE_FORMAT_RGBA16:
		switch(src_format)
		{
			case MDE_FORMAT_RGBA16: return MDE_Scanline_BlitNormal_RGBA16_To_RGBA16;
			case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitNormal_BGRA32_To_RGBA16;
			case MDE_FORMAT_RGB16: return MDE_Scanline_BlitNormal_RGB16_To_RGBA16;
			// Needed on BREW to create tiled OpBitmap
			case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitNormal_INDEX8_To_RGBA16;
		}
		break;
#endif
#ifdef MDE_SUPPORT_RGBA32
	case MDE_FORMAT_RGBA32:
		switch(src_format)
		{
		case MDE_FORMAT_RGBA32: return MDE_Scanline_BlitNormal_RGBA32_To_RGBA32;
		case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitNormal_BGRA32_To_RGBA32;
#ifdef MDE_SUPPORT_RGB16
		case MDE_FORMAT_RGB16: return MDE_Scanline_BlitNormal_RGB16_To_RGBA32;
#endif
		case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitNormal_INDEX8_To_RGBA32;
		}
		break;
#endif
#ifdef MDE_SUPPORT_ARGB32
	case MDE_FORMAT_ARGB32:
		switch(src_format)
		{
			//case MDE_FORMAT_ARGB32: return MDE_Scanline_BlitNormal_ARGB32_To_ARGB32;
			case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitNormal_BGRA32_To_ARGB32;
			case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitNormal_INDEX8_To_ARGB32;
		}
		break;
#endif
#ifdef MDE_SUPPORT_BGR24
	case MDE_FORMAT_BGR24:
		switch(src_format)
		{
		case MDE_FORMAT_RGBA32: return MDE_Scanline_BlitNormal_RGBA32_To_BGR24;
		case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitNormal_BGRA32_To_BGR24;
		case MDE_FORMAT_BGR24: return MDE_Scanline_BlitNormal_BGR24_To_BGR24;
		case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitNormal_INDEX8_To_BGR24;
		}
		break;
#endif
	default: break;
	}
	return MDE_Scanline_BlitNormal_unsupported;
}

MDE_SCANLINE_BLITSTRETCH MDE_GetScanline_BlitStretch(MDE_BUFFER *dstbuf, MDE_FORMAT src_format)
{
	switch(dstbuf->format)
	{
#ifdef MDE_SUPPORT_RGBA32
	case MDE_FORMAT_RGBA32:
	{
		switch(src_format)
		{
		case MDE_FORMAT_RGBA32: return MDE_Scanline_BlitStretch_RGBA32_To_RGBA32;
		case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitStretch_BGRA32_To_RGBA32;
#ifdef MDE_SUPPORT_RGBA24
		case MDE_FORMAT_RGBA24: return MDE_Scanline_BlitStretch_RGBA24_To_BGRA32;
#endif // MDE_SUPPORT_RGBA24
#ifdef MDE_SUPPORT_RGB16
		case MDE_FORMAT_RGB16: return MDE_Scanline_BlitStretch_RGB16_To_RGBA32;
#endif
		case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitStretch_INDEX8_To_RGBA32;
		}
		break;
	}
#endif
#ifdef MDE_SUPPORT_ARGB32
	case MDE_FORMAT_ARGB32:
	{
		switch(src_format)
		{
		case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitStretch_BGRA32_To_ARGB32;
		case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitStretch_INDEX8_To_ARGB32;
		}
		break;
	}
#endif
	case MDE_FORMAT_BGRA32:
		switch(src_format)
		{
			case MDE_FORMAT_BGRA32:	return MDE_Scanline_BlitStretch_BGRA32_To_BGRA32;
			case MDE_FORMAT_BGR24:	return MDE_Scanline_BlitStretch_BGR24_To_BGRA32;
			case MDE_FORMAT_INDEX8:	return MDE_Scanline_BlitStretch_INDEX8_To_BGRA32;
#ifdef MDE_SUPPORT_RGBA32
			case MDE_FORMAT_RGBA32: return MDE_Scanline_BlitStretch_RGBA32_To_BGRA32;
#endif
#ifdef MDE_SUPPORT_RGB16
			case MDE_FORMAT_RGB16: return MDE_Scanline_BlitStretch_RGB16_To_BGRA32;
			case MDE_FORMAT_RGBA16: return MDE_Scanline_BlitStretch_RGBA16_To_BGRA32;
#endif
#ifdef MDE_SUPPORT_SRGB16
			case MDE_FORMAT_SRGB16: return MDE_Scanline_BlitStretch_SRGB16_To_BGRA32;
#endif
#ifdef MDE_SUPPORT_RGBA24
			case MDE_FORMAT_RGBA24: return MDE_Scanline_BlitStretch_RGBA24_To_BGRA32;
#endif // MDE_SUPPORT_RGBA24
		}
		break;
#ifdef MDE_SUPPORT_RGBA16
	case MDE_FORMAT_RGBA16:
		switch(src_format)
		{
			case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitStretch_BGRA32_To_RGBA16;
			case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitStretch_INDEX8_To_RGBA16;
			case MDE_FORMAT_RGB16: return MDE_Scanline_BlitStretch_RGB16_To_RGBA16;
			case MDE_FORMAT_RGBA16: return MDE_Scanline_BlitStretch_RGBA16_To_RGBA16;
		}
		break;
#endif // MDE_SUPPORT_RGBA16

#ifdef MDE_SUPPORT_RGB16
	case MDE_FORMAT_RGB16:
		switch(src_format)
		{
#ifdef MDE_SUPPORT_RGBA16
			case MDE_FORMAT_RGBA16: return MDE_Scanline_BlitStretch_RGBA16_To_RGB16;
#endif
			case MDE_FORMAT_RGB16: return MDE_Scanline_BlitStretch_RGB16_To_RGB16;
#ifdef MDE_SUPPORT_RGBA24
			case MDE_FORMAT_RGBA24: return MDE_Scanline_BlitStretch_RGBA24_To_RGB16;
#endif // MDE_SUPPORT_RGBA24
#ifdef MDE_SUPPORT_SRGB16
			case MDE_FORMAT_SRGB16: return MDE_Scanline_BlitStretch_SRGB16_To_RGB16;
#endif
			case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitStretch_INDEX8_To_RGB16;
#ifdef MDE_SUPPORT_RGBA32
			case MDE_FORMAT_RGBA32: return MDE_Scanline_BlitStretch_RGBA32_To_RGB16;
#endif
			case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitStretch_BGRA32_To_RGB16;
		}
		break;
#endif

#ifdef MDE_SUPPORT_RGBA24
	case MDE_FORMAT_RGBA24:
		switch (src_format)
		{
		case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitStretch_BGRA32_To_RGBA24;
		case MDE_FORMAT_RGBA24: return MDE_Scanline_BlitStretch_RGBA24_To_RGBA24;
#ifdef MDE_SUPPORT_RGB16
		case MDE_FORMAT_RGB16: return MDE_Scanline_BlitStretch_RGBA24_To_RGB16;
#endif // MDE_SUPPORT_RGB16
		}
		break;
#endif // MDE_SUPPORT_RGBA24

#ifdef MDE_SUPPORT_SRGB16
	case MDE_FORMAT_SRGB16:
		switch(src_format)
		{
			case MDE_FORMAT_SRGB16: return MDE_Scanline_BlitStretch_SRGB16_To_SRGB16;
#ifdef MDE_SUPPORT_RGB16
			case MDE_FORMAT_RGB16: return MDE_Scanline_BlitStretch_RGB16_To_SRGB16;
#endif
			case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitStretch_INDEX8_To_SRGB16;
			case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitStretch_BGRA32_To_SRGB16;
#ifdef MDE_SUPPORT_BGR24
		    case MDE_FORMAT_BGR24: return MDE_Scanline_BlitStretch_BGR24_To_SRGB16;
#endif
		}
		break;
#endif

#ifdef MDE_SUPPORT_MBGR16
	case MDE_FORMAT_MBGR16:
		switch(src_format)
		{
#ifdef MDE_SUPPORT_RGBA16
			case MDE_FORMAT_RGBA16: return MDE_Scanline_BlitStretch_RGBA16_To_MBGR16;
#endif
#ifdef MDE_SUPPORT_RGB16
			case MDE_FORMAT_RGB16: return MDE_Scanline_BlitStretch_RGB16_To_MBGR16;
#endif
			case MDE_FORMAT_MBGR16: return MDE_Scanline_BlitStretch_MBGR16_To_MBGR16;
			case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitStretch_INDEX8_To_MBGR16;
		}
		break;
#endif
#ifdef MDE_SUPPORT_BGR24
	case MDE_FORMAT_BGR24:
		switch (src_format)
		{
		case MDE_FORMAT_RGBA32: return MDE_Scanline_BlitStretch_RGBA32_To_BGR24;
		case MDE_FORMAT_BGRA32: return MDE_Scanline_BlitStretch_BGRA32_To_BGR24;
		case MDE_FORMAT_BGR24: return MDE_Scanline_BlitStretch_BGR24_To_BGR24;
		case MDE_FORMAT_INDEX8: return MDE_Scanline_BlitStretch_INDEX8_To_BGR24;
		}
		break;
#endif

	default: break;
	}
	return MDE_Scanline_BlitStretch_unsupported;
}

#ifdef MDE_BILINEAR_BLITSTRETCH
MDE_BILINEAR_INTERPOLATION_X MDE_GetBilinearInterpolationX(MDE_FORMAT fmt)
{
	switch (fmt)
	{
#ifdef MDE_SUPPORT_RGBA32
	case MDE_FORMAT_RGBA32:
		break;
#endif
#ifdef MDE_SUPPORT_ARGB32
	case MDE_FORMAT_ARGB32:
		break;
#endif
	case MDE_FORMAT_BGRA32:
		return MDE_BilinearInterpolationX_BGRA32;
#ifdef MDE_SUPPORT_RGBA16
	case MDE_FORMAT_RGBA16:
		return MDE_BilinearInterpolationX_RGBA16;
#endif // MDE_SUPPORT_RGBA16

#ifdef MDE_SUPPORT_RGB16
	case MDE_FORMAT_RGB16:
		return MDE_BilinearInterpolationX_RGB16;
#endif

#ifdef MDE_SUPPORT_SRGB16
	case MDE_FORMAT_SRGB16:
		break;
#endif

#ifdef MDE_SUPPORT_MBGR16
	case MDE_FORMAT_MBGR16:
		break;
#endif
#ifdef MDE_SUPPORT_BGR24
	case MDE_FORMAT_BGR24:
		return MDE_BilinearInterpolationX_BGR24;
#endif
	case MDE_FORMAT_INDEX8:
		return NULL; // There is no way to interpolate indexed images, so just ignore it
	default:
		break;
	}
	OP_ASSERT(!"Linear interpolation is not implemented for this color format");
	return NULL;
}
MDE_BILINEAR_INTERPOLATION_Y MDE_GetBilinearInterpolationY(MDE_FORMAT fmt)
{
	switch (fmt)
	{
#ifdef MDE_SUPPORT_RGBA32
	case MDE_FORMAT_RGBA32:
		break;
#endif
#ifdef MDE_SUPPORT_ARGB32
	case MDE_FORMAT_ARGB32:
		break;
#endif
	case MDE_FORMAT_BGRA32:
		return MDE_BilinearInterpolationY_BGRA32;
#ifdef MDE_SUPPORT_RGBA16
	case MDE_FORMAT_RGBA16:
		return MDE_BilinearInterpolationY_RGBA16;
#endif // MDE_SUPPORT_RGBA16

#ifdef MDE_SUPPORT_RGB16
	case MDE_FORMAT_RGB16:
		return MDE_BilinearInterpolationY_RGB16;
#endif

#ifdef MDE_SUPPORT_SRGB16
	case MDE_FORMAT_SRGB16:
		break;
#endif

#ifdef MDE_SUPPORT_MBGR16
	case MDE_FORMAT_MBGR16:
		break;
#endif
#ifdef MDE_SUPPORT_BGR24
		return MDE_BilinearInterpolationY_BGR24;
		break;
#endif
	case MDE_FORMAT_INDEX8:
		return NULL; // There is no way to interpolate indexed images, so just ignore it
	default:
		break;
	}
	OP_ASSERT(!"Linear interpolation is not implemented for this color format");
	return NULL;
}
#endif // MDE_BILINEAR_BLITSTRETCH


bool MDE_GetBlitMethodSupported(MDE_METHOD method, MDE_FORMAT src_format, MDE_FORMAT dst_format)
{
	MDE_BUFFER dstbuf;
	MDE_InitializeBuffer(0, 0, 0, dst_format, NULL, NULL, &dstbuf);

	MDE_BUFFER_INFO srcinf;
	srcinf.method=method;

	MDE_SCANLINE_BLITNORMAL scanline = MDE_GetScanline_BlitNormal(&dstbuf, src_format);

	return scanline(NULL, NULL, 0, &srcinf, &dstbuf);
}
