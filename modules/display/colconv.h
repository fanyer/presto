/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef COLOR_CONVERTER_H
#define COLOR_CONVERTER_H

// A bunch of functions to make it easier to convert colors in ImgBlt.cpp

// The 565 convertions
#define COL_8888_TO_565(col) (  ( (((UINT8*)&col)[0]>>3) ) | \
				( (((UINT8*)&col)[1]>>2)<<5 ) | \
				( (((UINT8*)&col)[2]>>3)<<11 ) )
#define COL_565_R(col) (((col & 0xF800) >> 11) << 3)
#define COL_565_G(col) (((col & 0x7E0) >> 5) << 2)
#define COL_565_B(col) ((col & 0x1F) << 3)

void Convert8888To565(UINT8* src, UINT8* dst, UINT32 len);

void Convert565To8888(UINT8* src, UINT8* dst, UINT32 len);

void Convert8888To888(UINT8* src, UINT8* dst, UINT32 len);

void Convert888To8888(UINT8* src, UINT8* dst, UINT32 len);

void Convert8888To8888(UINT8* src, UINT8* dst, UINT32 len, BOOL* dst_got_alpha);

void ConvertColor(UINT8* src, UINT8* dst, UINT32 len, UINT32 src_bpp, UINT32 dst_bpp, BOOL* dst_got_alpha);

// == Indexed functions ====================

void Convert8To32(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor, BOOL* dst_got_alpha);

void Convert1To32(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor, BOOL* dst_got_alpha);

void Convert4To32(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor, BOOL* dst_got_alpha);

void Convert8To4(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor);

void Convert4To8(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor);

void Convert8To1(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor);

void Convert1To8(UINT8* src, UINT8* dst, UINT32 len, UINT8* pal, UINT32 transpcolor);

/** This function is not used in core and will be removed very soon. If you need it in your platform code, you need to move the implementation there. */
void DEPRECATED(Convert8ToMask(UINT8* src, UINT8* dst, UINT32 len, UINT32 transpcolor, BOOL* dst_got_alpha = NULL));

/** This function is not used in core and will be removed very soon. If you need it in your platform code, you need to move the implementation there. */
void DEPRECATED(Convert32ToMask(UINT8* src, UINT8* dst, UINT32 len, BOOL* dst_got_alpha = NULL));

// The palette should be aligned to 4 bytes for each component (BGR_BGR_BGR_BGR_...)
void ConvertColorIndexed(UINT8* src, UINT8* dst, UINT32 len, UINT32 src_bpp, UINT32 dst_bpp, UINT8* pal, UINT32 transpcolor, BOOL* dst_got_alpha);

#endif // !COLOR_CONVERTER_H
