/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

static inline void VEGAPixelFormatUtils_memset16(UINT16* p, UINT16 v, unsigned len)
{
	if (len && (reinterpret_cast<UINTPTR>(p) & 0x3) != 0)
		*p++ = v, len--;

	unsigned len32 = len / 2;
	if (len32)
	{
		UINT32 px32 = v | (v << 16);
		UINT32* p32 = (UINT32*)p;
		while (len32-- > 0)
			*p32++ = px32, len -= 2;

		p = (UINT16*)p32;
	}
	if (len)
		*p = v;
}

struct VEGAFormatUnpack
{
	static op_force_inline void RGB565_Unpack(const UINT8* p, unsigned& r, unsigned& g, unsigned& b)
	{
		UINT32 pix = *(const UINT16*)p;
		r = ((pix & 0xf800) >> 8) | ((pix & 0xf800) >> 13);
		g = ((pix & 0x7e0) >> 3) | ((pix & 0x7e0) >> 9);
		b = ((pix & 0x1f) << 3) | ((pix & 0x1f) >> 2);
	}

	static op_force_inline void BGR565_Unpack(const UINT8* p, unsigned& r, unsigned& g, unsigned& b)
	{
		UINT32 pix = *(const UINT16*)p;
		b = ((pix & 0xf800) >> 8) | ((pix & 0xf800) >> 13);
		g = ((pix & 0x7e0) >> 3) | ((pix & 0x7e0) >> 9);
		r = ((pix & 0x1f) << 3) | ((pix & 0x1f) >> 2);
	}

	static op_force_inline void BGRA8888_Unpack(const UINT8* p, unsigned& a, unsigned& r, unsigned& g, unsigned& b)
	{
		UINT32 pix = *(const UINT32*)p;
		a = pix >> 24;
		r = (pix >> 16) & 0xff;
		g = (pix >> 8) & 0xff;
		b = pix & 0xff;
	}

	static op_force_inline void RGBA8888_Unpack(const UINT8* p, unsigned& a, unsigned& r, unsigned& g, unsigned& b)
	{
		UINT32 pix = *(const UINT32*)p;
		a = pix >> 24;
		b = (pix >> 16) & 0xff;
		g = (pix >> 8) & 0xff;
		r = pix & 0xff;
	}

	static op_force_inline void BGRX8888_Unpack(const UINT8* p, unsigned& r, unsigned& g, unsigned& b)
	{
		UINT32 pix = *(const UINT32*)p;
		r = (pix >> 16) & 0xff;
		g = (pix >> 8) & 0xff;
		b = pix & 0xff;
	}

	static op_force_inline void ARGB8888_Unpack(const UINT8* p, unsigned& a, unsigned& r, unsigned& g, unsigned& b)
	{
		UINT32 pix = *(const UINT32*)p;
		b = pix >> 24;
		g = (pix >> 16) & 0xff;
		r = (pix >> 8) & 0xff;
		a = pix & 0xff;
	}

	static op_force_inline void ABGR8888_Unpack(const UINT8* p, unsigned& a, unsigned& r, unsigned& g, unsigned& b)
	{
		UINT32 pix = *(const UINT32*)p;
		r = pix >> 24;
		g = (pix >> 16) & 0xff;
		b = (pix >> 8) & 0xff;
		a = pix & 0xff;
	}

	static op_force_inline void ABGR4444_Unpack(const UINT8* p, unsigned& a, unsigned& r, unsigned& g, unsigned& b)
	{
		UINT32 pix = *(const UINT16*)p;
		r = (pix >> 12) * 255 / 15;
		g = ((pix >> 8) & 0xf) * 255 / 15;
		b = ((pix >> 4) & 0xf) * 255 / 15;
		a = (pix & 0xf) * 255 / 15;
	}

	static op_force_inline void BGRA4444_Unpack(const UINT8* p, unsigned& a, unsigned& r, unsigned& g, unsigned& b)
	{
        const UINT32 pix = *(const UINT16*)p;

        // Multiply by 0x11 to translate into [0,255] range

        r = ((pix & 0x0f00) >> 4) | ((pix & 0x0f00) >> 8);
        g = (pix & 0x00f0)        | ((pix & 0x00f0) >> 4);
        b = ((pix & 0x000f) << 4) | (pix & 0x000f);
        a = (pix >> 12)           | ((pix >> 12) << 4);
	}

	static op_force_inline void RGBA4444_Unpack(const UINT8* p, unsigned& a, unsigned& r, unsigned& g, unsigned& b)
	{
		const UINT32 pix = *(const UINT16*)p;

		// Multiply by 0x11 to translate into [0,255] range

		r = ((pix & 0x000f) << 4) | (pix & 0x000f);
		g = (pix & 0x00f0)        | ((pix & 0x00f0) >> 4);
		b = ((pix & 0x0f00) >> 4) | ((pix & 0x0f00) >> 8);
		a = (pix >> 12)           | ((pix >> 12) << 4);
	}

	static op_force_inline void RGBA5551_Unpack(const UINT8* p, unsigned& a, unsigned& r, unsigned& g, unsigned& b)
	{
		UINT32 pix = *(const UINT16*)p;
		r = (pix >> 11) * 255 / 31;
		g = ((pix >> 6) & 0x1f) * 255 / 31;
		b = ((pix >> 1) & 0x1f) * 255 / 31;
		a = (pix & 0x1) * 255;
	}

	static op_force_inline void RGB888_Unpack(const UINT8* p, unsigned& r, unsigned& g, unsigned& b)
	{
		r = p[0];
		g = p[1];
		b = p[2];
	}
};
