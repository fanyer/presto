/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/display/colconv.h"

void Convert8888To565(UINT8* src, UINT8* dst, UINT32 len)
{
	UINT16* dst16 = (UINT16*) dst;
	UINT32 i;
	for(i = 0; i < len; i++)
	{
#if 0
	  *dst16 = (UINT16) (src[0]>>3) + ( int(src[1]>>3)<<5 ) + ( int(src[2]>>3)<<10 );
#elif 0
	  *dst16 = (UINT16) (src[0]>>3) | ( (src[1]>>2)<<5 ) | ( (src[2]>>3)<<11 );
#elif 0
	  dst16 = (UINT16)
#if 0
	    (src[0]>>3) + ( (src[1]<<2) & 0x00FF00 ) +
#endif
	    ( (src[2]<<8) & 0xFF0000 );
#else // inner 0
		*dst16 = COL_8888_TO_565(*src);
#endif // outer 0, 0, 0.
		dst16++;
		src += 4;
	}
}

void Convert565To8888(UINT8* src, UINT8* dst, UINT32 len)
{
	UINT16* src16 = (UINT16*) src;
	UINT32 i;
	for(i = 0; i < len; i++)
	{
		dst[0] = COL_565_R(*src16);
		dst[1] = COL_565_G(*src16);
		dst[2] = COL_565_B(*src16);
		dst[3] = 255;
		dst += 4;
		src16++;
	}
}

void Convert8888To888(UINT8* src, UINT8* dst, UINT32 len)
{
	UINT32 i;
	for(i = 0; i < len; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst += 3;
		src += 4;
	}
}

void Convert888To8888(UINT8* src, UINT8* dst, UINT32 len)
{
	UINT32 i;
	for(i = 0; i < len; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = 255;
		dst += 4;
		src += 3;
	}
}

void Convert8888To8888(UINT8* src, UINT8* dst, UINT32 len, BOOL* dst_got_alpha)
{
	for(UINT32 i = 0; i < len; i++)
	{
		*((UINT32*)dst) = *((UINT32*)src);

		if (src[3] != 0xff)
		{
			*dst_got_alpha = TRUE;
		}

		src += 4;
		dst += 4;
	}
}

void ConvertColor(UINT8* src, UINT8* dst, UINT32 len, UINT32 src_bpp, UINT32 dst_bpp, BOOL* dst_got_alpha)
{
	BOOL dummy;

	if (dst_got_alpha)
	{
		*dst_got_alpha = FALSE;
	}
	else
	{
		dst_got_alpha = &dummy;
	}

	if (src_bpp == 15)
	{
		if (dst_bpp == 15)
			op_memcpy(dst, src, len * 2);
	}
	else if (src_bpp == 16)
	{
		if (dst_bpp == 16)
			op_memcpy(dst, src, len * 2);
		else if (dst_bpp == 32)
			Convert565To8888(src, dst, len);
	}
	else if (src_bpp == 24)
	{
		if (dst_bpp == 24)
			op_memcpy(dst, src, len * 3);
		else if (dst_bpp == 32)
			Convert888To8888(src, dst, len);
	}
	else if (src_bpp == 32)
	{
		if (dst_bpp == 32)
			Convert8888To8888(src, dst, len, dst_got_alpha);
		else if (dst_bpp == 16)
			Convert8888To565(src, dst, len);
		else if (dst_bpp == 24)
			Convert8888To888(src, dst, len);
	}
}

// == Indexed functions ====================

void Convert8To32(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor, BOOL* dst_got_alpha)
{
	UINT32 i;
	for(i = 0; i < len; i++)
	{
		if(src[i] == transpcolor)
		{
			*((UINT32*)dst) = 0;
			*dst_got_alpha = TRUE;
		}
		else
		{
			*((UINT32*)dst) = 0xFF000000 | (pal[(src[i] << 2) + 2] << 16) | (pal[(src[i] << 2) + 1] << 8) | pal[(src[i] << 2)];
		}
		dst += 4;
	}
}

void Convert1To32(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor, BOOL* dst_got_alpha)
{
	UINT32 i, j;
	UINT32 rest = len % 8;
	len /= 8;
	for (i = 0; i < len; i ++)
	{
		UINT8 mask = 0x80;
		for (j = 0; j < 8; j++, mask >>= 1)
		{
			UINT32 c1 = (src[i] & mask) != 0;
			if (c1 == transpcolor)
			{
				*((UINT32*)dst) = 0;
				*dst_got_alpha = TRUE;
			}
			else
			{
				*((UINT32*)dst) = 0xFF000000 | (pal[(c1 << 2) + 2] << 16) | (pal[(c1 << 2) + 1] << 8) | pal[(c1 << 2)];
			}
			dst += 4;
		}
	}
	UINT8 mask = 0x80;
	for (j = 0; j < rest; j++, mask >>= 1)
	{
		UINT32 c1 = (src[i] & mask) != 0;
		if (c1 == transpcolor)
		{
			*((UINT32*)dst) = 0;
			*dst_got_alpha = TRUE;
		}
		else
		{
			*((UINT32*)dst) = 0xFF000000 | (pal[(c1 << 2) + 2] << 16) | (pal[(c1 << 2) + 1] << 8) | pal[(c1 << 2)];
		}
		dst += 4;
	}
}

void Convert4To8(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor, BOOL* dst_got_alpha)
{
	UINT32 i;
	int rest = len - int(len/2)*2;
	len /= 2;
	for(i = 0; i < len; i ++)
	{
		*dst++ = src[i] >> 4;
		*dst++ = src[i] & 0x0F;
	}
	if(rest)
		*dst = src[i] >> 4;
}

void Convert4To32(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor, BOOL* dst_got_alpha)
{
	UINT32 i;
	int rest = len - int(len/2)*2;
	len /= 2;
	for(i = 0; i < len; i ++)
	{
		UINT32 c1 = src[i] >> 4;
		UINT32 c2 = src[i] & 0x0F;
		// First pixel
		if(c1 == transpcolor)
		{
			*((UINT32*)dst) = 0;
			*dst_got_alpha = TRUE;
		}
		else
		{
			*((UINT32*)dst) = 0xFF000000 | (pal[(c1 << 2) + 2] << 16) | (pal[(c1 << 2) + 1] << 8) | pal[(c1 << 2)];
		}
		dst += 4;
		// Second
		if(c2 == transpcolor)
		{
			*((UINT32*)dst) = 0;
			*dst_got_alpha = TRUE;
		}
		else
		{
			*((UINT32*)dst) = 0xFF000000 | (pal[(c2 << 2) + 2] << 16) | (pal[(c2 << 2) + 1] << 8) | pal[(c2 << 2)];
		}
		dst += 4;
	}
	if(rest)
	{
		UINT32 c1 = src[i] >> 4;
		// First pixel
		if(c1 == transpcolor)
		{
			*((UINT32*)dst) = 0;
			*dst_got_alpha = TRUE;
		}
		else
		{
			*((UINT32*)dst) = 0xFF000000 | (pal[(c1 << 2) + 2] << 16) | (pal[(c1 << 2) + 1] << 8) | pal[(c1 << 2)];
		}
	}
}

void Convert8To4(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor)
{
	UINT32 i;
	int rest = len - int(len/2)*2;
	len /= 2;
	for(i = 0; i < len; i ++)
	{
		*dst = (src[0] << 4) + src[1];
		dst ++;
		src += 2;
	}
	if(rest)
	{
		*dst = (src[0] << 4);
	}
}

void Convert8To1(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor)
{
	for (UINT32 x = 0; x < len; dst++)
	{
		UINT8	dst_pixel	= 0x80;
		UINT8	dst_byte	= *dst;

		for (; x < len && dst_pixel; x++, dst_pixel >>= 1, src++)
		{
			if (*src)
			{
				dst_byte |= dst_pixel;
			}
			else
			{
				dst_byte &= ~dst_pixel;
			}
		}

		*dst = dst_byte;
	}
}

void Convert1To8(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor)
{
	int rest = len - int(len/8)*8;
	len /= 8;
	for (UINT32 x = 0; x < len; x++)
	{
		*dst = (src[x] & 128) != 0; dst++;
		*dst = (src[x] & 64) != 0; dst++;
		*dst = (src[x] & 32) != 0; dst++;
		*dst = (src[x] & 16) != 0; dst++;
		*dst = (src[x] & 8) != 0; dst++;
		*dst = (src[x] & 4) != 0; dst++;
		*dst = (src[x] & 2) != 0; dst++;
		*dst = (src[x] & 1) != 0; dst++;
	}
	int mask = 128;
	for (int k = 0; k < rest; k++)
	{
		*dst = (src[len] & mask) != 0; dst++;
		mask = mask >> 1;
	}
}

void Convert8ToMask(UINT8* src, UINT8* dst, UINT32 len, UINT32 transpcolor, BOOL* dst_got_alpha)
{
	if (dst_got_alpha)
		*dst_got_alpha = FALSE;

	for (UINT32 x = 0; x < len; dst++)
	{
		UINT8	dst_pixel	= 0x80;
		UINT8	dst_byte	= *dst;

		for (; x < len && dst_pixel; x++, dst_pixel >>= 1, src++)
		{
			if (*src == transpcolor)
			{
				dst_byte |= dst_pixel;
				if (dst_got_alpha)
					*dst_got_alpha = TRUE;
			}
			else
			{
				dst_byte &= ~dst_pixel;
			}
		}

		*dst = dst_byte;
	}
}

void Convert32ToMask(UINT8* src, UINT8* dst, UINT32 len, BOOL* dst_got_alpha)
{
	if (dst_got_alpha)
		*dst_got_alpha = FALSE;

	for (UINT32 x = 0; x < len; dst++)
	{
		UINT8	dst_pixel	= 0x80;
		UINT8	dst_byte	= *dst;

		for (; x < len && dst_pixel; x++, dst_pixel >>= 1, src+=4)
		{
			if (src[3] == 0)
			{
				dst_byte |= dst_pixel;
				if (dst_got_alpha)
					*dst_got_alpha = TRUE;
			}
			else
			{
				dst_byte &= ~dst_pixel;
			}
		}

		*dst = dst_byte;
	}
}

// The palette should be aligned to 4 bytes for each component (BGR_BGR_BGR_BGR_...)
void ConvertColorIndexed(UINT8* src, UINT8* dst, UINT32 len, UINT32 src_bpp, UINT32 dst_bpp, UINT8* pal, UINT32 transpcolor, BOOL* dst_got_alpha)
{
	BOOL dummy;

	if (dst_got_alpha)
	{
		*dst_got_alpha = FALSE;
	}
	else
	{
		dst_got_alpha = &dummy;
	}

	switch (src_bpp)
	{
	case 1:
		{
			switch (dst_bpp)
			{
			case 1:
				{
					int	copy_len = ((len + 7) >> 3);
					op_memcpy(dst, src, copy_len);
					break;
				}
			case 8:
				{
					Convert1To8(src, dst, len, pal, transpcolor);
					break;
				}

			case 32:
				{
					Convert1To32(src, dst, len, pal, transpcolor, dst_got_alpha);
					break;
				}
			default:
				{
					OP_ASSERT(FALSE);
					break;
				}
			}
			break;
		}
	case 4:
		{
			switch (dst_bpp)
			{
			case 4:
				{
					int	copy_len = ((len + 1) >> 1);
					op_memcpy(dst, src, copy_len);
					break;
				}
			case 8:
				{
					Convert4To8(src, dst, len, pal, transpcolor, dst_got_alpha);
					break;
				}
			case 32:
				{
					Convert4To32(src, dst, len, pal, transpcolor, dst_got_alpha);
					break;
				}
			default:
				{
					OP_ASSERT(FALSE);
					break;
				}
			}
			break;
		}
	case 8:
		{
			switch (dst_bpp)
			{
			case 1:
				{
					Convert8To1(src, dst, len, pal, transpcolor);
					break;
				}
			case 4:
				{
					Convert8To4(src, dst, len, pal, transpcolor);
					break;
				}
			case 8:
				{
					op_memcpy(dst, src, len);
					break;
				}
			case 32:
				{
					Convert8To32(src, dst, len, pal, transpcolor, dst_got_alpha);
					break;
				}
			}
			break;
		}
	default:
		{
			OP_ASSERT(FALSE);
			break;
		}
	}
}
