/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

//
// Planar format:
// Plane 0: Color buffer in RGB565 format
// Plane 1: Alpha/transparency buffer in A8 format
//
// VEGA_PIXEL:s will contain a composite value like: (alpha << 16) | color
//

#define VEGA565_GET_ALPHA_FROM_POINTER(a) ((a)?(*(a)):255)
#define VEGA565_GET_ALPHA_FROM_POINTER_INC(a) ((a)?(*a++):255)
#define VEGA565_GET_ALPHA_FROM_POINTER_OFS(a, ofs) ((a)?((a)[ofs]):255)

struct VEGAPixelFormat_RGB565A8
{
	enum { BytesPerPixel = 2 }; // FIXME: Confusing for planar formats

	static inline bool IsCompatible(VEGAPixelStoreFormat fmt) { return fmt == VPSF_RGB565; }

	// Packing / unpacking
	static inline VEGA_PIXEL PackRGB(unsigned r, unsigned g, unsigned b)
	{
		return (0xffu<<16)|((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3);
	}
	static inline VEGA_PIXEL PackARGB(unsigned a, unsigned r, unsigned g, unsigned b)
	{
		return (a<<16)|((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3);
	}
	static inline unsigned UnpackA(VEGA_PIXEL pix) { return pix >> 16; }
	static inline unsigned UnpackR(VEGA_PIXEL pix) { return ((pix & 0xf800) >> 8) | ((pix & 0xf800) >> 13); }
	static inline unsigned UnpackG(VEGA_PIXEL pix) { return ((pix & 0x7e0) >> 3) | ((pix & 0x7e0) >> 9); }
	static inline unsigned UnpackB(VEGA_PIXEL pix) { return ((pix & 0x1f) << 3) | ((pix & 0x1f) >> 2); }

	// Premultiply / unpremultiply
	static inline VEGA_PIXEL Premultiply(VEGA_PIXEL pix)
	{
		unsigned a = UnpackA(pix);
		if (a == 0)
			return 0;
		if (a == 255)
			return pix;

		unsigned r = UnpackR(pix);
		unsigned g = UnpackG(pix);
		unsigned b = UnpackB(pix);

		r = (r * a) / 255;
		g = (g * a) / 255;
		b = (b * a) / 255;

		return PackARGB(a, r, g, b);
	}
	// Same as Premultiply except that it uses an approximative
	// calculation for some values, with the intent of being faster
	// with (potentially) slightly worse quality as a result
	static inline VEGA_PIXEL PremultiplyFast(VEGA_PIXEL pix)
	{
		unsigned a = UnpackA(pix);
		if (a == 0)
			return 0;
		if (a == 255)
			return pix;

		unsigned r = (((pix & 0xf800) >> 11) * (a+1)) >> 8;
		unsigned g = (((pix & 0x7e0) >> 5) * (a+1)) >> 8;
		unsigned b = ((pix & 0x1f) * (a+1)) >> 8;

		return (a<<16)|(r<<11)|(g<<5)|b;
	}
	static inline VEGA_PIXEL UnpremultiplyFast(VEGA_PIXEL pix)
	{
		unsigned a = UnpackA(pix);
		if (a == 0 || a == 255)
			return pix;

		unsigned recip_a = (255u << 24) / a;
		unsigned r = ((((pix & 0xf800) >> 11) * recip_a) + (1 << 23)) >> 24;
		unsigned g = ((((pix & 0x7e0) >> 5) * recip_a) + (1 << 23)) >> 24;
		unsigned b = (((pix & 0x1f) * recip_a) + (1 << 23)) >> 24;

		return (a<<16)|(r<<11)|(g<<5)|b;
	}

	struct Pointer
	{
		UINT16* rgb;
		UINT8* a;
	};

	// Constant (readonly) pixel accessor
	class ConstAccessor
	{
	public:
		ConstAccessor(const Pointer& p) { Reset(p); }

		const Pointer& Ptr() const { return ptr; }
		void Reset(const Pointer& p) { ptr = p; }

		inline void LoadUnpack(int& a, int& r, int& g, int& b) const
		{
			a = VEGA565_GET_ALPHA_FROM_POINTER(ptr.a);
			r = VEGAPixelFormat_RGB565A8::UnpackR(*ptr.rgb);
			g = VEGAPixelFormat_RGB565A8::UnpackG(*ptr.rgb);
			b = VEGAPixelFormat_RGB565A8::UnpackB(*ptr.rgb);
		}
		inline void LoadUnpack(unsigned& a, unsigned& r, unsigned& g, unsigned& b) const
		{
			a = VEGA565_GET_ALPHA_FROM_POINTER(ptr.a);
			r = VEGAPixelFormat_RGB565A8::UnpackR(*ptr.rgb);
			g = VEGAPixelFormat_RGB565A8::UnpackG(*ptr.rgb);
			b = VEGAPixelFormat_RGB565A8::UnpackB(*ptr.rgb);
		}
		inline void LoadUnpack(unsigned& a) const
		{
			a = VEGA565_GET_ALPHA_FROM_POINTER(ptr.a);
		}
		inline void LoadUnpack(unsigned& a, unsigned &l) const
		{
			a = VEGA565_GET_ALPHA_FROM_POINTER(ptr.a);
			l = VEGAPixelFormat_RGB565A8::UnpackR(*ptr.rgb);
		}
		inline VEGA_PIXEL Load() const { return *ptr.rgb | (VEGA565_GET_ALPHA_FROM_POINTER(ptr.a) << 16); }
		inline VEGA_PIXEL LoadRel(int ofs) const { return *(ptr.rgb+ofs) | (VEGA565_GET_ALPHA_FROM_POINTER_OFS(ptr.a, ofs) << 16); }

		inline void operator+=(int pix_disp) { ptr.rgb += pix_disp; if (ptr.a)ptr.a += pix_disp; }
		inline void operator-=(int pix_disp) { ptr.rgb -= pix_disp; if (ptr.a)ptr.a -= pix_disp; }

		inline void operator++() { ++ptr.rgb; if(ptr.a)++ptr.a; }
		inline void operator++(int) { ptr.rgb++; if(ptr.a)ptr.a++; }

	private:
		Pointer ptr;
	};

	class Accessor
	{
	public:
		Accessor(const Pointer& p) { Reset(p); }

		const Pointer& Ptr() const { return ptr; }
		void Reset(const Pointer& p) { ptr = p; }

		inline void LoadUnpack(int& a, int& r, int& g, int& b) const
		{
			a = VEGA565_GET_ALPHA_FROM_POINTER(ptr.a);
			r = VEGAPixelFormat_RGB565A8::UnpackR(*ptr.rgb);
			g = VEGAPixelFormat_RGB565A8::UnpackG(*ptr.rgb);
			b = VEGAPixelFormat_RGB565A8::UnpackB(*ptr.rgb);
		}
		inline void LoadUnpack(unsigned& a, unsigned& r, unsigned& g, unsigned& b) const
		{
			a = VEGA565_GET_ALPHA_FROM_POINTER(ptr.a);
			r = VEGAPixelFormat_RGB565A8::UnpackR(*ptr.rgb);
			g = VEGAPixelFormat_RGB565A8::UnpackG(*ptr.rgb);
			b = VEGAPixelFormat_RGB565A8::UnpackB(*ptr.rgb);
		}
		inline VEGA_PIXEL Load() const { return *ptr.rgb | (VEGA565_GET_ALPHA_FROM_POINTER(ptr.a) << 16); }
		inline VEGA_PIXEL LoadRel(int ofs) const { return *(ptr.rgb+ofs) | (VEGA565_GET_ALPHA_FROM_POINTER_OFS(ptr.a, ofs) << 16); }

		inline void Store(VEGA_PIXEL px) { *ptr.rgb = (UINT16)px; if (ptr.a)*ptr.a = (UINT8)(px >> 16); }
		inline void Store(VEGA_PIXEL px, unsigned len)
		{
			// Store in color buffer
			VEGAPixelFormatUtils_memset16(ptr.rgb, (UINT16)px, len);

			if (ptr.a)
			{
				// Store in alpha buffer
				UINT8* p8 = ptr.a;
				UINT8 v8 = (UINT8)(px >> 16);
				while (len-- > 0)
					*p8++ = v8;
			}
		}
		inline void StoreRGB(unsigned r, unsigned g, unsigned b)
		{
			*ptr.rgb = ((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3);
			if (ptr.a)
				*ptr.a = 0xff;
		}
		inline void StoreARGB(unsigned a, unsigned r, unsigned g, unsigned b)
		{
			*ptr.rgb = ((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3);
			if (ptr.a)
				*ptr.a = (UINT8)a;
		}
		inline void CopyFrom(const Pointer& src, unsigned pix_count)
		{
			op_memcpy(ptr.rgb, src.rgb, pix_count * sizeof(*ptr.rgb));
			if (ptr.a)
			{
				if (src.a)
					op_memcpy(ptr.a, src.a, pix_count);
				else
					op_memset(ptr.a, 255, pix_count);
			}
		}
		inline void MoveFrom(const Pointer& src, unsigned pix_count)
		{
			op_memmove(ptr.rgb, src.rgb, pix_count * sizeof(*ptr.rgb));
			if (ptr.a)
			{
				if (src.a)
					op_memmove(ptr.a, src.a, pix_count);
				else
					op_memset(ptr.a, 255, pix_count);
			}
		}

		inline void operator+=(int pix_disp) { ptr.rgb += pix_disp; if (ptr.a)ptr.a += pix_disp; }
		inline void operator-=(int pix_disp) { ptr.rgb -= pix_disp; if (ptr.a)ptr.a -= pix_disp; }

		inline void operator++() { ++ptr.rgb; if (ptr.a)++ptr.a; }
		inline void operator++(int) { ptr.rgb++; if (ptr.a)ptr.a++; }

	private:
		Pointer ptr;
	};

	// Buffer creation / deletion
	static inline OP_STATUS CreateBuffer(Pointer& p, unsigned w, unsigned h, bool opaque)
	{
		// Calculate (w * h * 3 + 1) / 2 = w * h + (w * h + 1) / 2
		unsigned prod = w * h;
		if (w && h && prod / w != h)
			return OpStatus::ERR_NO_MEMORY;

		if (opaque)
		{
			p.rgb = OP_NEWA(UINT16, prod);
			if (!p.rgb)
				return OpStatus::ERR_NO_MEMORY;
			p.a = NULL;
			return OpStatus::OK;
		}

		unsigned alloc_size = prod + prod / 2 + (prod & 1);
		if (alloc_size < prod)
			return OpStatus::ERR_NO_MEMORY;

		p.rgb = OP_NEWA(UINT16, alloc_size);
		if (!p.rgb)
			return OpStatus::ERR_NO_MEMORY;
		p.a = (UINT8*)(p.rgb + prod);
		return OpStatus::OK;
	}
	static inline OP_STATUS CreateBufferFromData(Pointer& ptr, void* data, unsigned w, unsigned h, bool opaque)
	{
		// FIXME: This assumes the same allocation convention as in CreateBuffer
		ptr.rgb = (UINT16*)data;
		if (opaque)
			ptr.a = NULL;
		else
			ptr.a = (UINT8*)(ptr.rgb + w * h);
		return OpStatus::OK;
	}
	static inline void DeleteBuffer(Pointer& p)
	{
		OP_DELETEA(p.rgb);
		p.rgb = NULL;
		p.a = NULL;
	}
	static inline void ClearBuffer(Pointer& p)
	{
		p.rgb = NULL;
		p.a = NULL;
	}

	static inline OP_STATUS Bind(Pointer& ptr, VEGAPixelStore* ps, bool opaque)
	{
		OP_ASSERT(ps->format == VPSF_RGB565);
		if (opaque)
			ptr.a = NULL;
		else
		{
			ptr.a = OP_NEWA_WH(UINT8, ps->width, ps->height);
			if (!ptr.a)
				return OpStatus::ERR;
		}
		ptr.rgb = (UINT16*)ps->buffer;
		return OpStatus::OK;
	}
	static inline void Unbind(Pointer& ptr, VEGAPixelStore* ps)
	{
		OP_ASSERT(ps->format == VPSF_RGB565);
		OP_DELETEA(ptr.a);
		ptr.rgb = NULL;
		ptr.a = NULL;
	}

#ifdef USE_PREMULTIPLIED_ALPHA
#define RET_PREMULT(v)		v
#define RET_UNPREMULT(v)	VEGAPixelFormat_RGB565A8::PremultiplyFast(v)
#else
#define RET_PREMULT(v)		VEGAPixelFormat_RGB565A8::UnpremultiplyFast(v)
#define RET_UNPREMULT(v)	v
#endif // USE_PREMULTIPLIED_ALPHA

	static op_force_inline void SamplerNearestX_Opaque(Accessor& color, const Pointer& data, unsigned cnt, INT32 csx, INT32 cdx);
	static op_force_inline void SamplerNearestX_CompOver(Accessor& color, const Pointer& data, unsigned cnt, INT32 csx, INT32 cdx);
	static op_force_inline void SamplerNearestX_CompOverMask(Accessor& color, const Pointer& data, const UINT8*& mask, unsigned cnt, INT32 csx, INT32 cdx);
	static op_force_inline void SamplerNearestX_CompOverConstMask(Accessor& color, const Pointer& data, UINT8 mask, unsigned cnt, INT32 csx, INT32 cdx);

	static op_force_inline void SamplerNearestXY_CompOver(Accessor& color, const Pointer& data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	static op_force_inline void SamplerNearestXY_CompOverMask(Accessor& color, const Pointer& data, const UINT8*& mask, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);

	// Sample using bilinear interpolation from a buffer containing premultiplied pixels
	static inline VEGA_PIXEL SamplerBilerpPM(const Pointer& data, unsigned dataPixelStride,
											 INT32 csx, INT32 csy);

	// Perform linear interpolation in x direction from <srcdata> position <csx>, step <cdx>
	static inline void SamplerLerpXPM(Pointer dstdata, const Pointer& srcdata, INT32 csx, INT32 cdx,
									  unsigned dstlen, unsigned srclen);

	// Perform linear interpolation between <srcdata1> and <srcdata2> by factor <frc_y>
	static inline void SamplerLerpYPM(Pointer dstdata, const Pointer& srcdata1, const Pointer& srcdata2,
									  INT32 frc_y, unsigned len);

	static op_force_inline void SamplerLerpXY_Opaque(Accessor& color, const Pointer& data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	static op_force_inline void SamplerLerpXY_CompOver(Accessor& color, const Pointer& data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	static op_force_inline void SamplerLerpXY_CompOverMask(Accessor& color, const Pointer& data, const UINT8*& mask, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);

	// Sample using nearest neighbour from a buffer containing premultiplied pixels
	static inline VEGA_PIXEL SamplerNearestPM(const Pointer& data, unsigned dataPixelStride,
											  INT32 csx, INT32 csy)
	{
		unsigned offset = VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx);
		return RET_PREMULT(data.rgb[offset] | (VEGA565_GET_ALPHA_FROM_POINTER_OFS(data.a, offset) << 16));
	}

	// Sample using nearest neighbour from a buffer containing non-premultiplied pixels
	static inline VEGA_PIXEL SamplerNearest(const Pointer& data, unsigned dataPixelStride,
											INT32 csx, INT32 csy)
	{
		unsigned offset = VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx);
		return RET_UNPREMULT(data.rgb[offset] | (VEGA565_GET_ALPHA_FROM_POINTER_OFS(data.a, offset) << 16));
	}

	// Sample in one dimension using nearest neighbour from a buffer containing premultiplied pixels
	static inline VEGA_PIXEL SamplerNearestPM_1D(const Pointer& data, INT32 csx)
	{
		unsigned offset = VEGA_SAMPLE_INT(csx);
		return RET_PREMULT(data.rgb[offset] | (VEGA565_GET_ALPHA_FROM_POINTER_OFS(data.a, offset) << 16));
	}

	// Sample in one dimension using nearest neighbour from a buffer containing non-premultiplied pixels
	static inline VEGA_PIXEL SamplerNearest_1D(const Pointer& data, INT32 csx)
	{
		unsigned offset = VEGA_SAMPLE_INT(csx);
		return RET_UNPREMULT(data.rgb[offset] | (VEGA565_GET_ALPHA_FROM_POINTER_OFS(data.a, offset) << 16));
	}

	// Composite operators
	static inline VEGA_PIXEL Modulate(VEGA_PIXEL s, UINT32 m);
	static inline VEGA_PIXEL CompositeOver(VEGA_PIXEL d, VEGA_PIXEL s);
	static inline VEGA_PIXEL CompositeOverIn(VEGA_PIXEL d, VEGA_PIXEL s, UINT32 m);
	static inline VEGA_PIXEL CompositeOverUnpacked(VEGA_PIXEL dst, int sa, int sr, int sg, int sb);

	static inline void CompositeOver(Pointer db, Pointer sb, unsigned len);
	static inline void CompositeOverPM(Pointer db, Pointer sb, unsigned len);
	static inline void CompositeOverIn(Pointer db, Pointer sb, const UINT8* mask, unsigned len);
	static inline void CompositeOverIn(Pointer db, Pointer sb, UINT8 mask, unsigned len);
	static inline void CompositeOverInPM(Pointer db, Pointer sb, UINT8 mask, unsigned len);

	static inline void CompositeOver(Pointer db, VEGA_PIXEL src, unsigned len);
	static inline void CompositeOverIn(Pointer db, VEGA_PIXEL src, const UINT8* mask, unsigned len);
};

static inline VEGAPixelFormat_RGB565A8::Pointer
operator+(const VEGAPixelFormat_RGB565A8::Pointer& p, int offset)
{
	VEGAPixelFormat_RGB565A8::Pointer tp = p;
	tp.rgb += offset;
	if (tp.a)
		tp.a += offset;
	return tp;
}

// Relying on the ordering of the color buffer
static inline BOOL
operator<(const VEGAPixelFormat_RGB565A8::Pointer& p1, const VEGAPixelFormat_RGB565A8::Pointer& p2)
{
	return p1.rgb < p2.rgb;
}

static inline BOOL
operator>=(const VEGAPixelFormat_RGB565A8::Pointer& p1, const VEGAPixelFormat_RGB565A8::Pointer& p2)
{
	return p1.rgb >= p2.rgb;
}

// Helper macros
#define ExtractA(p) ((p) >> 16)
#define ExtractR(c) (((c) >> 8) & 0xf8)
#define ExtractG(c) (((c) >> 3) & 0xfc)
#define ExtractB(c) (((c) << 3) & 0xf8)

#define ExtractAG(c,a) (((a) << 16) | ExtractG(c))
#define ExtractBR(c) ((ExtractB(c) << 16) | ExtractR(c))

#define MaskIsOpaque(v) ((v) == 255)
#define MaskIsTransp(v) ((v) == 0)

// a + s * b, where s is [0,256], and a,b is on the form 0x00HH00LL
#define PackedMacc(a,b,s) (((a) + (((s) * (b)) >> 8)) & 0x00ff00ff)
// (a * s) << 8, where s is [0,256], and a is on the form 0x00HH00LL
#define PackedScaleHi(a,s) (((a) * (s)) & 0xff00ff00)
#define RepackToDest(ag,br) (((ag)&0xff0000)|(((ag)&0xfc)<<3)|(((br)&0xf80000)>>19)|(((br)&0xf8) << 8))

#ifndef USE_PREMULTIPLIED_ALPHA
// Blend src with destination, destination is assumed opaque
static inline VEGA_PIXEL RGB565A8_BlendOneOpaque(int sa, int sr, int sg, int sb,
												 int dr, int dg, int db)
{
	dr = dr + ((sa * (sr - dr)) >> 8);
	dg = dg + ((sa * (sg - dg)) >> 8);
	db = db + ((sa * (sb - db)) >> 8);
	return (0xffu<<16) | ((dr&0xf8)<<8) | ((dg&0xfc)<<3) | (db>>3);
}

// Blend src with destination
static inline VEGA_PIXEL RGB565A8_BlendOne(int sa, int sr, int sg, int sb,
										   int da, int dr, int dg, int db)
{
	dr *= da;
	dg *= da;
	db *= da;
	da += sa-((da*sa)>>8);
	dr = ((dr + (sa*(sr-(dr>>8))))/da) & 0xf8;
	dg = ((dg + (sa*(sg-(dg>>8))))/da) & 0xfc;
	db = ((db + (sa*(sb-(db>>8))))/da) & 0xf8;
	if (da > 0xff)
		da = 0xff;
	OP_ASSERT(da >= 0 && da <= 0xff);
	return (da<<16) | (dr<<8) | (dg<<3) | (db>>3);
}
#endif // !USE_PREMULTIPLIED_ALPHA

/* static */
op_force_inline void
VEGAPixelFormat_RGB565A8::SamplerNearestX_Opaque(Accessor& color, const Pointer& data, unsigned cnt, INT32 csx, INT32 cdx)
{
	while (cnt-- > 0)
	{
		VEGA_PIXEL c = SamplerNearestPM_1D(data, csx);
		color.Store(c);

		csx += cdx;

		color++;
	}
}

/* static */
op_force_inline void
VEGAPixelFormat_RGB565A8::SamplerNearestX_CompOver(Accessor& color, const Pointer& data, unsigned cnt, INT32 csx, INT32 cdx)
{
	while (cnt-- > 0)
	{
		VEGA_PIXEL c = SamplerNearestPM_1D(data, csx);
		color.Store(CompositeOver(color.Load(), c));

		csx += cdx;

		color++;
	}
}

/* static */
op_force_inline void
VEGAPixelFormat_RGB565A8::SamplerNearestX_CompOverMask(Accessor& color, const Pointer& data, const UINT8*& mask, unsigned cnt, INT32 csx, INT32 cdx)
{
	while (cnt-- > 0)
	{
		int a = *mask++;
		if (a)
		{
			VEGA_PIXEL c = SamplerNearestPM_1D(data, csx);
			color.Store(CompositeOverIn(color.Load(), c, a));
		}

		csx += cdx;

		color++;
	}
}

/* static */
op_force_inline void
VEGAPixelFormat_RGB565A8::SamplerNearestX_CompOverConstMask(Accessor& color, const Pointer& data, UINT8 mask, unsigned cnt, INT32 csx, INT32 cdx)
{
	while (cnt-- > 0)
	{
		color.Store(CompositeOverIn(color.Load(), SamplerNearestPM_1D(data, csx), mask));
		csx += cdx;
		color++;
	}
}

/* static */
op_force_inline void
VEGAPixelFormat_RGB565A8::SamplerNearestXY_CompOver(Accessor& color, const Pointer& data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	while (cnt-- > 0)
	{
		VEGA_PIXEL c = SamplerNearestPM(data, dataPixelStride, csx, csy);
		color.Store(CompositeOver(color.Load(), c));

		csx += cdx;
		csy += cdy;

		color++;
	}
}

/* static */
op_force_inline void
VEGAPixelFormat_RGB565A8::SamplerNearestXY_CompOverMask(Accessor& color, const Pointer& data, const UINT8*& mask, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	while (cnt-- > 0)
	{
		int a = *mask++;
		if (a)
		{
			VEGA_PIXEL c = SamplerNearestPM(data, dataPixelStride, csx, csy);
			color.Store(CompositeOverIn(color.Load(), c, a));
		}

		csx += cdx;
		csy += cdy;

		color++;
	}
}

inline void
VEGAPixelFormat_RGB565A8::SamplerLerpXPM(Pointer dstdata, const Pointer& srcdata, INT32 csx, INT32 cdx, unsigned dstlen, unsigned srclen)
{
	const UINT16* spixc = srcdata.rgb;
	const UINT8* spixa = srcdata.a;
	UINT16* dpixc = dstdata.rgb;
	UINT8* dpixa = dstdata.a;
	while (dstlen && csx < 0)
	{
		*dpixc++ = spixc[0];
		if (dpixa)
			*dpixa++ = VEGA565_GET_ALPHA_FROM_POINTER(spixa);

		csx += cdx;
		dstlen--;
	}
	OP_ASSERT(!dstlen || csx >= 0);
	while (dstlen > 0 && ((unsigned)csx) < VEGA_INTTOSAMPLE(srclen-1))
	{
		INT32 int_px = VEGA_SAMPLE_INT(csx);
		INT32 frc_px = VEGA_SAMPLE_FRAC(csx);

		UINT32 c = spixc[int_px];
		UINT32 ca = VEGA565_GET_ALPHA_FROM_POINTER_OFS(spixa, int_px);
		if (frc_px)
		{
			UINT32 c_ag = ExtractAG(c, ca);
			UINT32 c_br = ExtractBR(c);

			// merge c and next c+1
			UINT32 nc = spixc[int_px + 1];
			UINT32 nca = VEGA565_GET_ALPHA_FROM_POINTER_OFS(spixa, int_px + 1);

			// Merge
			c_ag = PackedMacc(c_ag, ExtractAG(nc, nca) - c_ag, frc_px);
			c_br = PackedMacc(c_br, ExtractBR(nc) - c_br, frc_px);

			c = RepackToDest(c_ag, c_br);
			ca = (UINT8)(c >> 16);
		}

		*dpixc++ = (UINT16)c;
		if (dpixa)
			*dpixa++ = ca;

		csx += cdx;
		dstlen--;
	}
	while (dstlen > 0)
	{
		*dpixc++ = (UINT16)spixc[srclen-1];
		if (dpixa)
			*dpixa++ = VEGA565_GET_ALPHA_FROM_POINTER_OFS(spixa, srclen-1);

		csx += cdx;
		dstlen--;
	}
}

inline void
VEGAPixelFormat_RGB565A8::SamplerLerpYPM(Pointer dstdata, const Pointer& srcdata1, const Pointer& srcdata2, INT32 frc_y, unsigned len)
{
	const UINT16* spix1c = srcdata1.rgb;
	const UINT8* spix1a = srcdata1.a;
	const UINT16* spix2c = srcdata2.rgb;
	const UINT8* spix2a = srcdata2.a;
	UINT16* dpixc = dstdata.rgb;
	UINT8* dpixa = dstdata.a;
	while (len-- > 0)
	{
		UINT32 c = *spix1c++;
		UINT32 ca = VEGA565_GET_ALPHA_FROM_POINTER_INC(spix1a);
		UINT32 c_ag = ExtractAG(c, ca);
		UINT32 c_br = ExtractBR(c);

		// merge c and next c+1
		UINT32 nc = *spix2c++;
		UINT32 nca = VEGA565_GET_ALPHA_FROM_POINTER_INC(spix2a);

		// Merge
		c_ag = PackedMacc(c_ag, ExtractAG(nc, nca) - c_ag, frc_y);
		c_br = PackedMacc(c_br, ExtractBR(nc) - c_br, frc_y);

		UINT32 d_rgba = RepackToDest(c_ag, c_br);
		*dpixc++ = (UINT16)d_rgba;
		if (dpixa)
			*dpixa++ = (UINT8)(d_rgba >> 16);
	}
}

/* static */ op_force_inline void
VEGAPixelFormat_RGB565A8::SamplerLerpXY_Opaque(Accessor& color, const Pointer& data,
											   unsigned cnt, unsigned dataPixelStride,
											   INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	while (cnt-- > 0)
	{
		color.Store(SamplerBilerpPM(data, dataPixelStride, csx, csy));

		csx += cdx;
		csy += cdy;

		color++;
	}
}

/* static */ op_force_inline void
VEGAPixelFormat_RGB565A8::SamplerLerpXY_CompOver(Accessor& color, const Pointer& data,
												 unsigned cnt, unsigned dataPixelStride,
												 INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	while (cnt-- > 0)
	{
		VEGA_PIXEL c = SamplerBilerpPM(data, dataPixelStride, csx, csy);
		color.Store(CompositeOver(color.Load(), c));

		csx += cdx;
		csy += cdy;

		color++;
	}
}

/* static */ op_force_inline void
VEGAPixelFormat_RGB565A8::SamplerLerpXY_CompOverMask(Accessor& color, const Pointer& data, const UINT8*& mask,
													 unsigned cnt, unsigned dataPixelStride,
													 INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	while (cnt-- > 0)
	{
		int a = *mask++;
		if (a)
		{
			VEGA_PIXEL c = SamplerBilerpPM(data, dataPixelStride, csx, csy);
			color.Store(CompositeOverIn(color.Load(), c, a));
		}

		csx += cdx;
		csy += cdy;

		color++;
	}
}

inline VEGA_PIXEL
VEGAPixelFormat_RGB565A8::SamplerBilerpPM(const Pointer& data, unsigned dataPixelStride,
										  INT32 csx, INT32 csy)
{
	unsigned offset = VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx);
	const UINT16* color = data.rgb + offset;
	const UINT8* alpha = data.a?(data.a + offset):NULL;
	UINT32 c = *color;
	int weight2 = VEGA_SAMPLE_FRAC(csx);
	int yweight2 = VEGA_SAMPLE_FRAC(csy);
	UINT32 c_ag = ExtractAG(c, VEGA565_GET_ALPHA_FROM_POINTER(alpha));
	UINT32 c_br = ExtractBR(c);
	if (weight2)
	{
		// merge c and next c+1
		UINT32 nc = *(color + 1);
		// Merge
		c_ag = PackedMacc(c_ag, ExtractAG(nc, VEGA565_GET_ALPHA_FROM_POINTER_OFS(alpha, 1)) - c_ag, weight2);
		c_br = PackedMacc(c_br, ExtractBR(nc) - c_br, weight2);

		if (yweight2)
		{
			color += dataPixelStride;
			if (alpha)
				alpha += dataPixelStride;

			// merge with next scanline
			UINT32 c2 = *color;
			UINT32 c2_ag = ExtractAG(c2, VEGA565_GET_ALPHA_FROM_POINTER(alpha));
			UINT32 c2_br = ExtractBR(c2);
			// merge c and next c+1
			UINT32 nc2 = *(color + 1);
			// Merge
			c2_ag = PackedMacc(c2_ag, ExtractAG(nc2, VEGA565_GET_ALPHA_FROM_POINTER_OFS(alpha, 1)) - c2_ag, weight2);
			c2_br = PackedMacc(c2_br, ExtractBR(nc2) - c2_br, weight2);

			// Merge
			c_ag = PackedMacc(c_ag, c2_ag - c_ag, yweight2);
			c_br = PackedMacc(c_br, c2_br - c_br, yweight2);
		}
	}
	else if (yweight2)
	{
		color += dataPixelStride;
		if (alpha)
			alpha += dataPixelStride;

		// merge with next scanline
		UINT32 c2 = *color;
		// Merge
		c_ag = PackedMacc(c_ag, ExtractAG(c2, VEGA565_GET_ALPHA_FROM_POINTER(alpha)) - c_ag, yweight2);
		c_br = PackedMacc(c_br, ExtractBR(c2) - c_br, yweight2);
	}
	c = RepackToDest(c_ag, c_br);
	return RET_PREMULT(c);
}

#undef RET_PREMULT
#undef RET_UNPREMULT

inline VEGA_PIXEL
VEGAPixelFormat_RGB565A8::Modulate(VEGA_PIXEL s, UINT32 m)
{
	if (m == 0)
		return 0;
	if (m == 255)
		return s;

#ifdef USE_PREMULTIPLIED_ALPHA
	unsigned s_ag = PackedScaleHi(ExtractAG(s, ExtractA(s)), m+1) >> 8;
	unsigned s_br = PackedScaleHi(ExtractBR(s), m+1) >> 8;

	return RepackToDest(s_ag, s_br);
#else
	unsigned sa = (UnpackA(s) * (m+1)) >> 8;

	return (sa << 16) | (s & 0x00ffff);
#endif // USE_PREMULTIPLIED_ALPHA
}

// r = s over d
inline VEGA_PIXEL
VEGAPixelFormat_RGB565A8::CompositeOver(VEGA_PIXEL d, VEGA_PIXEL s)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	if (s == 0)
		return d;

	if (ExtractA(s) == 255 || d == 0)
		return s;

	UINT32 s_ag = ExtractAG(s, ExtractA(s));
	UINT32 s_br = ExtractBR(s);

	UINT32 d_ag = ExtractAG(d, ExtractA(d));
	UINT32 d_br = ExtractBR(d);

	UINT32 inv_sa = 256 - ExtractA(s);

	d_ag = PackedMacc(s_ag, d_ag, inv_sa);
	d_br = PackedMacc(s_br, d_br, inv_sa);

	return RepackToDest(d_ag, d_br);
#else
	int sa = ExtractA(s);
	if (sa == 255)
		return s;

	if (sa == 0)
		return d;

	int da = ExtractA(d);
	if (da == 0)
		return s;

	int dr = ExtractR(d);
	int dg = ExtractG(d);
	int db = ExtractB(d);

	int sr = ExtractR(s);
	int sg = ExtractG(s);
	int sb = ExtractB(s);

	if (da == 255)
		d = RGB565A8_BlendOneOpaque(sa, sr, sg, sb, dr, dg, db);
	else
		d = RGB565A8_BlendOne(sa, sr, sg, sb, da, dr, dg, db);

	return d;
#endif // USE_PREMULTIPLIED_ALPHA
}

// r = (s in m) over d
inline VEGA_PIXEL
VEGAPixelFormat_RGB565A8::CompositeOverIn(VEGA_PIXEL d, VEGA_PIXEL s, UINT32 m)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	if (MaskIsTransp(m) || s == 0)
		return d;

	UINT32 s_ag, s_br;
	if (MaskIsOpaque(m))
	{
		if (ExtractA(s) == 255 || d == 0)
			return s;

		s_ag = ExtractAG(s, ExtractA(s));
		s_br = ExtractBR(s);
	}
	else // 0 < m < 255
	{
		s_ag = PackedScaleHi(ExtractAG(s, ExtractA(s)), m+1);
		s_br = PackedScaleHi(ExtractBR(s), m+1);
		s_ag >>= 8;
		s_br >>= 8;

		if (d == 0)
			return RepackToDest(s_ag, s_br);
	}

	UINT32 d_ag = ExtractAG(d, ExtractA(d));
	UINT32 d_br = ExtractBR(d);

	UINT32 inv_sa = 256 - (s_ag >> 16);

	d_ag = PackedMacc(s_ag, d_ag, inv_sa);
	d_br = PackedMacc(s_br, d_br, inv_sa);

	return RepackToDest(d_ag, d_br);
#else
	int sa = ExtractA(s);
	if (MaskIsTransp(m) || sa == 0)
		return d;

	if (MaskIsOpaque(m))
	{
		if (sa == 255)
			return s;
	}
	else // 0 < m < 0xf0
	{
		sa *= m+1;
		sa >>= 8;
		if (sa == 0)
			return d;
	}

	int da = ExtractA(d);
	if (da == 0)
		return (sa << 16) | (s & 0xffff);

	int dr = ExtractR(d);
	int dg = ExtractG(d);
	int db = ExtractB(d);

	int sr = ExtractR(s);
	int sg = ExtractG(s);
	int sb = ExtractB(s);

	if (da == 255)
		d = RGB565A8_BlendOneOpaque(sa, sr, sg, sb, dr, dg, db);
	else
		d = RGB565A8_BlendOne(sa, sr, sg, sb, da, dr, dg, db);

	return d;
#endif // USE_PREMULTIPLIED_ALPHA
}

inline void
VEGAPixelFormat_RGB565A8::CompositeOver(Pointer dp, Pointer sp, unsigned len)
{
	UINT16* dst_rgb = dp.rgb;
	UINT8* dst_a = dp.a;
	const UINT16* src_rgb = sp.rgb;
	const UINT8* src_a = sp.a;
	if (!src_a)
	{
		unsigned cnt = len;
		while (cnt-- > 0)
			*dst_rgb++ = *src_rgb++;

		if (dst_a)
			while (len-- > 0)
				*dst_a++ = 255;

		return;
	}
#ifdef USE_PREMULTIPLIED_ALPHA
	while (len-- > 0)
	{
		UINT32 s_rgb = *src_rgb++;
		UINT8 s_a = *src_a++;

		if (s_a == 0)
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}

		if (s_a == 255 || VEGA565_GET_ALPHA_FROM_POINTER(dst_a) == 0)
		{
			*dst_rgb++ = s_rgb;
			if (dst_a)
				*dst_a++ = s_a;
			continue;
		}

		UINT32 s_ag = ExtractAG(s_rgb, s_a);
		UINT32 s_br = ExtractBR(s_rgb);

		UINT32 d_rgb = *dst_rgb;
		UINT8 d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);

		UINT32 d_ag = ExtractAG(d_rgb, d_a);
		UINT32 d_br = ExtractBR(d_rgb);

		UINT32 inv_sa = 256 - s_a;

		d_ag = PackedMacc(s_ag, d_ag, inv_sa);
		d_br = PackedMacc(s_br, d_br, inv_sa);

		UINT32 d_rgba = RepackToDest(d_ag, d_br);
		*dst_rgb++ = (UINT16)d_rgba;
		if (dst_a)
			*dst_a++ = (UINT8)(d_rgba >> 16);
	}
#else
	while (len-- > 0)
	{
		UINT32 s_rgb = *src_rgb++;
		int s_a = *src_a++;

		if (s_a == 255)
		{
			*dst_rgb++ = s_rgb;
			if (dst_a)
				*dst_a++ = s_a;
			continue;
		}

		if (s_a == 0)
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}

		UINT32 d_rgb = *dst_rgb;
		int d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);
		if (d_a == 0)
		{
			*dst_rgb++ = s_rgb;
			// d_a can't be 0 if dst_a is NULL
			*dst_a++ = s_a;
			continue;
		}

		int dr = ExtractR(d_rgb);
		int dg = ExtractG(d_rgb);
		int db = ExtractB(d_rgb);

		int sr = ExtractR(s_rgb);
		int sg = ExtractG(s_rgb);
		int sb = ExtractB(s_rgb);

		UINT32 d;
		if (d_a == 255)
			d = RGB565A8_BlendOneOpaque(s_a, sr, sg, sb, dr, dg, db);
		else
			d = RGB565A8_BlendOne(s_a, sr, sg, sb, d_a, dr, dg, db);

		*dst_rgb++ = (UINT16)d;
		if (dst_a)
			*dst_a++ = (UINT8)(d >> 16);
	}
#endif // USE_PREMULTIPLIED_ALPHA
}
inline void
VEGAPixelFormat_RGB565A8::CompositeOverPM(Pointer dp, Pointer sp, unsigned len)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	CompositeOver(dp, sp, len);
#else
	UINT16* dst_rgb = dp.rgb;
	UINT8* dst_a = dp.a;
	const UINT16* src_rgb = sp.rgb;
	const UINT8* src_a = sp.a;
	if (!src_a)
	{
		unsigned cnt = len;
		while (cnt-- > 0)
			*dst_rgb++ = *src_rgb++;

		if (dst_a)
			while (len-- > 0)
				*dst_a++ = 255;

		return;
	}
	while (len-- > 0)
	{
		UINT32 s_rgb = *src_rgb++;
		int s_a = VEGA565_GET_ALPHA_FROM_POINTER_INC(src_a);

		if (s_a == 255)
		{
			*dst_rgb++ = s_rgb;
			if (dst_a)
				*dst_a++ = s_a;
			continue;
		}

		if (s_a == 0)
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}
		s_rgb = UnpremultiplyFast(s_rgb|(s_a<<16));

		UINT32 d_rgb = *dst_rgb;
		int d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);
		if (d_a == 0)
		{
			*dst_rgb++ = s_rgb;
			// d_a can't be 0 if dst_a is NULL
			*dst_a++ = s_a;
			continue;
		}

		int dr = ExtractR(d_rgb);
		int dg = ExtractG(d_rgb);
		int db = ExtractB(d_rgb);

		int sr = ExtractR(s_rgb);
		int sg = ExtractG(s_rgb);
		int sb = ExtractB(s_rgb);

		UINT32 d;
		if (d_a == 255)
			d = RGB565A8_BlendOneOpaque(s_a, sr, sg, sb, dr, dg, db);
		else
			d = RGB565A8_BlendOne(s_a, sr, sg, sb, d_a, dr, dg, db);

		*dst_rgb++ = (UINT16)d;
		if (dst_a)
			*dst_a++ = (UINT8)(d >> 16);
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

inline void
VEGAPixelFormat_RGB565A8::CompositeOverIn(Pointer dp, Pointer sp, const UINT8* mask, unsigned len)
{
	UINT16* dst_rgb = dp.rgb;
	UINT8* dst_a = dp.a;
	const UINT16* src_rgb = sp.rgb;
	const UINT8* src_a = sp.a;
#ifdef USE_PREMULTIPLIED_ALPHA
	while (len-- > 0)
	{
		UINT32 m = *mask++;
		UINT8 s_a = VEGA565_GET_ALPHA_FROM_POINTER_INC(src_a);
		UINT32 s_rgb = *src_rgb++;

		if (MaskIsTransp(m) || s_a == 0)
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}

		UINT32 s_ag, s_br;
		if (MaskIsOpaque(m))
		{
			if (s_a == 255 || VEGA565_GET_ALPHA_FROM_POINTER(dst_a) == 0)
			{
				*dst_rgb++ = s_rgb;
				if (dst_a)
					*dst_a++ = s_a;
				continue;
			}

			s_ag = ExtractAG(s_rgb, s_a);
			s_br = ExtractBR(s_rgb);
		}
		else // 0 < m < 255
		{
			s_ag = PackedScaleHi(ExtractAG(s_rgb, s_a), m+1);
			s_br = PackedScaleHi(ExtractBR(s_rgb), m+1);

			s_ag >>= 8;
			s_br >>= 8;

			if (VEGA565_GET_ALPHA_FROM_POINTER(dst_a) == 0)
			{
				UINT32 d = RepackToDest(s_ag, s_br);
				*dst_rgb++ = (UINT16)d;
				// *dst_a can't be 0 if dst_a is NULL
				*dst_a++ = (UINT8)(d >> 16);
				continue;
			}
		}

		UINT32 d_rgb = *dst_rgb;
		UINT8 d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);

		UINT32 d_ag = ExtractAG(d_rgb, d_a);
		UINT32 d_br = ExtractBR(d_rgb);

		UINT32 inv_sa = 256 - (s_ag >> 16);

		d_ag = PackedMacc(s_ag, d_ag, inv_sa);
		d_br = PackedMacc(s_br, d_br, inv_sa);

		UINT32 d = RepackToDest(d_ag, d_br);
		*dst_rgb++ = (UINT16)d;
		if (dst_a)
			*dst_a++ = (UINT8)(d >> 16);
	}
#else
	while (len-- > 0)
	{
		UINT32 m = *mask++;
		UINT32 s_rgb = *src_rgb++;
		int s_a = VEGA565_GET_ALPHA_FROM_POINTER(src_a);

		if (MaskIsTransp(m) || s_a == 0)
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}

		if (MaskIsOpaque(m))
		{
			if (s_a == 255)
			{
				*dst_rgb++ = s_rgb;
				if (dst_a)
					*dst_a++ = s_a;
				continue;
			}
		}
		else // 0 < m < 255
		{
			s_a *= m+1;
			s_a >>= 8;
			if (s_a == 0)
			{
				dst_rgb++;
				if (dst_a)
					dst_a++;
				continue;
			}
		}

		UINT32 d_rgb = *dst_rgb;
		int d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);
		if (d_a == 0)
		{
			*dst_rgb++ = s_rgb;
			// d_a can't be 0 if dst_a is NULL
			*dst_a++ = s_a;
			continue;
		}

		int dr = ExtractR(d_rgb);
		int dg = ExtractG(d_rgb);
		int db = ExtractB(d_rgb);

		int sr = ExtractR(s_rgb);
		int sg = ExtractG(s_rgb);
		int sb = ExtractB(s_rgb);

		UINT32 d;
		if (d_a == 255)
			d = RGB565A8_BlendOneOpaque(s_a, sr, sg, sb, dr, dg, db);
		else
			d = RGB565A8_BlendOne(s_a, sr, sg, sb, d_a, dr, dg, db);

		*dst_rgb++ = (UINT16)d;
		if (dst_a)
			*dst_a++ = (UINT8)(d >> 16);
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

inline void
VEGAPixelFormat_RGB565A8::CompositeOverIn(Pointer dp, Pointer sp, UINT8 mask, unsigned len)
{
	UINT16* dst_rgb = dp.rgb;
	UINT8* dst_a = dp.a;
	const UINT16* src_rgb = sp.rgb;
	const UINT8* src_a = sp.a;
	if (mask == 0)
		return;
	if (mask == 255)
	{
		CompositeOver(dp, sp, len);
		return;
	}
	UINT32 m = mask + 1;
#ifdef USE_PREMULTIPLIED_ALPHA
	while (len-- > 0)
	{
		UINT8 s_a = VEGA565_GET_ALPHA_FROM_POINTER_INC(src_a);
		UINT32 s_rgb = *src_rgb++;

		if (s_a == 0)
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}

		UINT32 s_ag = PackedScaleHi(ExtractAG(s_rgb, s_a), m);
		UINT32 s_br = PackedScaleHi(ExtractBR(s_rgb), m);

		s_ag >>= 8;
		s_br >>= 8;

		if (VEGA565_GET_ALPHA_FROM_POINTER(dst_a) == 0)
		{
			UINT32 d = RepackToDest(s_ag, s_br);
			*dst_rgb++ = (UINT16)d;
			// *dst_a can't be 0 if dst_a is NULL
			*dst_a++ = (UINT8)(d >> 16);
			continue;
		}

		UINT32 d_rgb = *dst_rgb;
		UINT8 d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);

		UINT32 d_ag = ExtractAG(d_rgb, d_a);
		UINT32 d_br = ExtractBR(d_rgb);

		UINT32 inv_sa = 256 - (s_ag >> 16);

		d_ag = PackedMacc(s_ag, d_ag, inv_sa);
		d_br = PackedMacc(s_br, d_br, inv_sa);

		UINT32 d = RepackToDest(d_ag, d_br);
		*dst_rgb++ = (UINT16)d;
		if (dst_a)
			*dst_a++ = (UINT8)(d >> 16);
	}
#else
	while (len-- > 0)
	{
		UINT32 s_rgb = *src_rgb++;
		int s_a = VEGA565_GET_ALPHA_FROM_POINTER_INC(src_a);

		if (s_a == 0)
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}

		s_a *= m;
		s_a >>= 8;
		if (s_a == 0)
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}

		UINT32 d_rgb = *dst_rgb;
		int d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);
		if (d_a == 0)
		{
			*dst_rgb++ = s_rgb;
			// d_a can't be 0 if dst_a is NULL
			*dst_a++ = s_a;
			continue;
		}

		int dr = ExtractR(d_rgb);
		int dg = ExtractG(d_rgb);
		int db = ExtractB(d_rgb);

		int sr = ExtractR(s_rgb);
		int sg = ExtractG(s_rgb);
		int sb = ExtractB(s_rgb);

		UINT32 d;
		if (d_a == 255)
			d = RGB565A8_BlendOneOpaque(s_a, sr, sg, sb, dr, dg, db);
		else
			d = RGB565A8_BlendOne(s_a, sr, sg, sb, d_a, dr, dg, db);

		*dst_rgb++ = (UINT16)d;
		if (dst_a)
			*dst_a++ = (UINT8)(d >> 16);
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

inline void
VEGAPixelFormat_RGB565A8::CompositeOverInPM(Pointer dp, Pointer sp, UINT8 mask, unsigned len)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	CompositeOverIn(dp, sp, mask, len);
#else
	UINT16* dst_rgb = dp.rgb;
	UINT8* dst_a = dp.a;
	const UINT16* src_rgb = sp.rgb;
	const UINT8* src_a = sp.a;
	if (mask == 0)
		return;
	if (mask == 255)
	{
		CompositeOverPM(dp, sp, len);
		return;
	}
	UINT32 m = mask + 1;
	while (len-- > 0)
	{
		UINT32 s_rgb = *src_rgb++;
		int s_a = VEGA565_GET_ALPHA_FROM_POINTER_INC(src_a);

		if (s_a == 0)
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}

		s_a *= m;
		s_a >>= 8;
		if (s_a == 0)
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}
		s_rgb = UnpremultiplyFast(s_rgb|(s_a<<16));

		UINT32 d_rgb = *dst_rgb;
		int d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);
		if (d_a == 0)
		{
			*dst_rgb++ = s_rgb;
			// d_a can't be 0 if dst_a is NULL
			*dst_a++ = s_a;
			continue;
		}

		int dr = ExtractR(d_rgb);
		int dg = ExtractG(d_rgb);
		int db = ExtractB(d_rgb);

		int sr = ExtractR(s_rgb);
		int sg = ExtractG(s_rgb);
		int sb = ExtractB(s_rgb);

		UINT32 d;
		if (d_a == 255)
			d = RGB565A8_BlendOneOpaque(s_a, sr, sg, sb, dr, dg, db);
		else
			d = RGB565A8_BlendOne(s_a, sr, sg, sb, d_a, dr, dg, db);

		*dst_rgb++ = (UINT16)d;
		if (dst_a)
			*dst_a++ = (UINT8)(d >> 16);
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

inline void
VEGAPixelFormat_RGB565A8::CompositeOver(Pointer dp, VEGA_PIXEL src, unsigned len)
{
	UINT16* dst_rgb = dp.rgb;
	UINT8* dst_a = dp.a;
#ifdef USE_PREMULTIPLIED_ALPHA
	if (src == 0)
		return;

	UINT32 sa = ExtractA(src);
	if (sa == 255)
	{
		VEGAPixelFormatUtils_memset16(dst_rgb, (UINT16)src, len);

		if (dst_a)
		{
			while (len-- > 0)
				*dst_a++ = 255;
		}
	}
	else
	{
		UINT32 s_ag = ExtractAG(src, sa);
		UINT32 s_br = ExtractBR(src);
		UINT32 inv_sa = 256 - sa;

		while (len-- > 0)
		{
			UINT32 d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);
			if (d_a == 0)
			{
				*dst_rgb++ = (UINT16)src;
				// d_a can't be 0 if dst_a is NULL
				*dst_a++ = (UINT8)sa;
				continue;
			}

			UINT32 d_rgb = *dst_rgb;

			UINT32 d_ag = ExtractAG(d_rgb, d_a);
			UINT32 d_br = ExtractBR(d_rgb);

			d_ag = PackedMacc(s_ag, d_ag, inv_sa);
			d_br = PackedMacc(s_br, d_br, inv_sa);

			UINT32 d = RepackToDest(d_ag, d_br);
			*dst_rgb++ = (UINT16)d;
			if (dst_a)
				*dst_a++ = (UINT8)(d >> 16);
		}
	}
#else
	int sa = ExtractA(src);
	if (sa == 0)
		return;

	if (sa == 255)
	{
		VEGAPixelFormatUtils_memset16(dst_rgb, (UINT16)src, len);

		if (dst_a)
		{
			while (len-- > 0)
				*dst_a++ = 255;
		}
	}
	else
	{
		int sr = ExtractR(src);
		int sg = ExtractG(src);
		int sb = ExtractB(src);

		while (len-- > 0)
		{
			UINT32 d_rgb = *dst_rgb;
			int d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);
			if (d_a == 0)
			{
				*dst_rgb++ = (UINT16)src;
				// d_a can't be 0 if dst_a is NULL
				*dst_a++ = sa;
				continue;
			}

			int dr = ExtractR(d_rgb);
			int dg = ExtractG(d_rgb);
			int db = ExtractB(d_rgb);

			UINT32 d;
			if (d_a == 255)
				d = RGB565A8_BlendOneOpaque(sa, sr, sg, sb, dr, dg, db);
			else
				d = RGB565A8_BlendOne(sa, sr, sg, sb, d_a, dr, dg, db);

			*dst_rgb++ = (UINT16)d;
			if (dst_a)
				*dst_a++ = (UINT8)(d >> 16);
		}
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

inline void
VEGAPixelFormat_RGB565A8::CompositeOverIn(Pointer dp, VEGA_PIXEL src, const UINT8* mask, unsigned len)
{
	UINT16* dst_rgb = dp.rgb;
	UINT8* dst_a = dp.a;
#ifdef USE_PREMULTIPLIED_ALPHA
	if (src == 0)
		return;

	UINT32 s_a = ExtractA(src);
	UINT32 s_ag = ExtractAG(src, s_a);
	UINT32 s_br = ExtractBR(src);
	UINT32 inv_sa = 256 - s_a;

	while (len-- > 0)
	{
		UINT32 m = *mask++;
		if (MaskIsTransp(m))
		{
			dst_rgb++;
			if (dst_a)
				dst_a++;
			continue;
		}

		UINT32 ms_ag, ms_br, inv_msa;
		if (MaskIsOpaque(m))
		{
			if (s_a == 255 || VEGA565_GET_ALPHA_FROM_POINTER(dst_a) == 0)
			{
				*dst_rgb++ = (UINT16)src;
				if (dst_a)
					*dst_a++ = s_a;
				continue;
			}

			ms_ag = s_ag;
			ms_br = s_br;
			inv_msa = inv_sa;
		}
		else // 0 < m < 255
		{
			ms_ag = PackedScaleHi(s_ag, m+1);
			ms_br = PackedScaleHi(s_br, m+1);

			ms_ag >>= 8;
			ms_br >>= 8;

			if (VEGA565_GET_ALPHA_FROM_POINTER(dst_a) == 0)
			{
				UINT32 d = RepackToDest(ms_ag, ms_br);
				*dst_rgb++ = (UINT16)d;
				// *dst_a can't be 0 if dst_a is NULL
				*dst_a++ = (UINT8)(d >> 16);
				continue;
			}

			inv_msa = 256 - (ms_ag >> 16);
		}

		UINT32 d_rgb = *dst_rgb;
		UINT32 d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);

		UINT32 d_ag = ExtractAG(d_rgb, d_a);
		UINT32 d_br = ExtractBR(d_rgb);

		d_ag = PackedMacc(ms_ag, d_ag, inv_msa);
		d_br = PackedMacc(ms_br, d_br, inv_msa);

		UINT32 d = RepackToDest(d_ag, d_br);
		*dst_rgb++ = (UINT16)d;
		if (dst_a)
			*dst_a++ = (UINT8)(d >> 16);
	}
#else
	int s_a = ExtractA(src);
	if (s_a == 0)
		return;

	int sr = ExtractR(src);
	int sg = ExtractG(src);
	int sb = ExtractB(src);
	UINT16 sc = (UINT16)src;

	if (s_a == 255)
	{
		while (len-- > 0)
		{
			UINT32 m = *mask++;
			if (MaskIsTransp(m))
			{
				*dst_rgb++;
				if (dst_a)
					*dst_a++;
				continue;
			}

			if (MaskIsOpaque(m))
			{
				*dst_rgb++ = sc;
				if (dst_a)
					*dst_a++ = 255;
				continue;
			}

			UINT32 d_rgb = *dst_rgb;
			int d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);
			if (d_a == 0)
			{
				*dst_rgb++ = sc;
				// d_a can't be 0 if dst_a is NULL
				*dst_a++ = m;
				continue;
			}

			int dr = ExtractR(d_rgb);
			int dg = ExtractG(d_rgb);
			int db = ExtractB(d_rgb);

			UINT32 d;
			if (d_a == 255)
				d = RGB565A8_BlendOneOpaque(m, sr, sg, sb, dr, dg, db);
			else
				d = RGB565A8_BlendOne(m, sr, sg, sb, d_a, dr, dg, db);

			*dst_rgb++ = (UINT16)d;
			if (dst_a)
				*dst_a++ = (UINT8)(d >> 16);
		}
	}
	else
	{
		while (len-- > 0)
		{
			UINT32 m = *mask++;

			if (MaskIsTransp(m))
			{
				dst_rgb++;
				if (dst_a)
					dst_a++;
				continue;
			}

			int msa = s_a;
			if (!MaskIsOpaque(m))
			{
				msa *= m+1;
				msa >>= 8;
				if (msa == 0)
				{
					dst_rgb++;
					if (dst_a)
						dst_a++;
					continue;
				}
			}

			UINT32 d_rgb = *dst_rgb;
			int d_a = VEGA565_GET_ALPHA_FROM_POINTER(dst_a);
			if (d_a == 0)
			{
				*dst_rgb++ = sc;
				// d_a can't be 0 if dst_a is NULL
				*dst_a++ = msa;
				continue;
			}

			int dr = ExtractR(d_rgb);
			int dg = ExtractG(d_rgb);
			int db = ExtractB(d_rgb);

			UINT32 d;
			if (d_a == 255)
				d = RGB565A8_BlendOneOpaque(msa, sr, sg, sb, dr, dg, db);
			else
				d = RGB565A8_BlendOne(msa, sr, sg, sb, d_a, dr, dg, db);

			*dst_rgb++ = (UINT16)d;
			if (dst_a)
				*dst_a++ = (UINT8)(d >> 16);
		}
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

// r = (src=[sa,sr,sg,sb]) over dst, where src is always unassociated (and 8bits/component)
inline VEGA_PIXEL
VEGAPixelFormat_RGB565A8::CompositeOverUnpacked(VEGA_PIXEL dst, int sa, int sr, int sg, int sb)
{
	if (sa == 255)
		return PackARGB(sa, sr, sg, sb);

	if (sa == 0)
		return dst;

#ifdef USE_PREMULTIPLIED_ALPHA
	// Premultiply
	sr = (sr * (sa+1)) >> 8;
	sg = (sg * (sa+1)) >> 8;
	sb = (sb * (sa+1)) >> 8;

	// Apply over operator
	UINT32 inv_sa = 256 - sa;
	UINT32 src_ag = (sa << 16) | sg;
	UINT32 src_br = (sb << 16) | sr;

	UINT32 dst_ag = ExtractAG(dst, ExtractA(dst));
	UINT32 dst_br = ExtractBR(dst);

	dst_ag = PackedMacc(src_ag, dst_ag, inv_sa);
	dst_br = PackedMacc(src_br, dst_br, inv_sa);

	return RepackToDest(dst_ag, dst_br);
#else
	int da = ExtractA(dst);
	if (da == 0)
		return PackARGB(sa, sr, sg, sb);

	int dr = ExtractR(dst);
	int dg = ExtractG(dst);
	int db = ExtractB(dst);

	if (da == 255)
		return RGB565A8_BlendOneOpaque(sa, sr, sg, sb, dr, dg, db);

	return RGB565A8_BlendOne(sa, sr, sg, sb, da, dr, dg, db);
#endif // !USE_PREMULTIPLIED_ALPHA
}

#undef ExtractAG
#undef ExtractBR

#undef ExtractA
#undef ExtractB
#undef ExtractG
#undef ExtractR

#undef MaskIsOpaque
#undef MaskIsTransp

#undef PackedMacc
#undef PackedScaleHi
#undef RepackToDest
