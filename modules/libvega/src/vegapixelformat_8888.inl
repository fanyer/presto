/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#define RED_MASK (255u << RED_SHIFT)
#define GREEN_MASK (255u << GREEN_SHIFT)
#define BLUE_MASK (255u << BLUE_SHIFT)
#define ALPHA_MASK (255u << ALPHA_SHIFT)

#define RGB_MASK (RED_MASK | GREEN_MASK | BLUE_MASK)

// Extracts the components pairwise from a VEGA_PIXEL for packed multiply accumulate.
#define EXTRACT_HI(p) (((p) & 0xff00ff00) >> 8)
#define EXTRACT_LO(p) ( (p) & 0x00ff00ff)

// a + b * s, where s is [0,256], and a, b is on the form 0x00HH00LL
// leaves the result upshifted 8 bits.
#define PACKED_MACC(a, b, s) (((a) << 8) + (b) * (s))

// Combines upshifted component pair into a VEGA_PIXEL (e.g. after PACKED_MACC).
#define COMBINE_ARGB(hi, lo) (((hi) & 0xff00ff00) | (((lo) >> 8) & 0x00ff00ff))

// Combines upshifted component pair, discarding alpha, into a VEGA_PIXEL (e.g. after PACKED_MACC).
#define COMBINE_RGB(hi, lo) (ALPHA_MASK | ((hi) & (0xff00ff00 & RGB_MASK)) | (((lo) >> 8) & (0x00ff00ff & RGB_MASK)))

// Fetch a specific component from VEGA_PIXEL.
#define UNPACK_R(p) (((p) >> RED_SHIFT) & 0xff)
#define UNPACK_G(p) (((p) >> GREEN_SHIFT) & 0xff)
#define UNPACK_B(p) (((p) >> BLUE_SHIFT) & 0xff)
#define UNPACK_A(p) (((p) >> ALPHA_SHIFT) & 0xff)

// Pack components into a VEGA_PIXEL.
#define PACK_RGB(r, g, b) (ALPHA_MASK | ((r) << RED_SHIFT) | ((g) << GREEN_SHIFT) | ((b) << BLUE_SHIFT))
#define PACK_ARGB(a, r, g, b) (((a) << ALPHA_SHIFT) | ((r) << RED_SHIFT) | ((g) << GREEN_SHIFT) | ((b) << BLUE_SHIFT))

#ifdef VEGA_USE_ASM
#define VEGA_DISPATCH(NAME, PARAMS) g_vegaDispatchTable->NAME ## _8888 PARAMS
#else
#define VEGA_DISPATCH(NAME, PARAMS) NAME PARAMS
#endif // VEGA_USE_ASM

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
struct VEGAPixelFormat_8888
{
	enum { BytesPerPixel = 4 };

	static op_force_inline bool IsCompatible(VEGAPixelStoreFormat fmt);

	// Packing / unpacking
	static op_force_inline VEGA_PIXEL PackRGB(unsigned r, unsigned g, unsigned b)
	{
		return PACK_RGB(r, g, b);
	}
	static op_force_inline VEGA_PIXEL PackARGB(unsigned a, unsigned r, unsigned g, unsigned b)
	{
		return PACK_ARGB(a, r, g, b);
	}
	static op_force_inline unsigned UnpackA(VEGA_PIXEL pix) { return UNPACK_A(pix); }
	static op_force_inline unsigned UnpackR(VEGA_PIXEL pix) { return UNPACK_R(pix); }
	static op_force_inline unsigned UnpackG(VEGA_PIXEL pix) { return UNPACK_G(pix); }
	static op_force_inline unsigned UnpackB(VEGA_PIXEL pix) { return UNPACK_B(pix); }

	// Premultiply / unpremultiply
	static op_force_inline VEGA_PIXEL Premultiply(VEGA_PIXEL pix)
	{
		unsigned a = UNPACK_A(pix);
		if (a == 0)
			return 0;
		if (a == 255)
			return pix;

		unsigned r = UNPACK_R(pix);
		unsigned g = UNPACK_G(pix);
		unsigned b = UNPACK_B(pix);

		r = (r * a + 127) / 255;
		g = (g * a + 127) / 255;
		b = (b * a + 127) / 255;

		return PACK_ARGB(a, r, g, b);
	}
	// Same as Premultiply except that it uses an approximative
	// calculation for some values, with the intent of being faster
	// with (potentially) slightly worse quality as a result
	static op_force_inline VEGA_PIXEL PremultiplyFast(VEGA_PIXEL pix)
	{
		unsigned a = UNPACK_A(pix);
		if (a == 0)
			return 0;
		if (a == 255)
			return pix;

		UINT32 rb = EXTRACT_LO(pix);
		UINT32 ag = EXTRACT_HI(pix);

		rb *= a+1;
		ag *= a+1;
		rb >>= 8;

		return (a << ALPHA_SHIFT) | (rb & (0x00ff00ff & RGB_MASK)) | (ag & (0xff00ff00 & RGB_MASK));
	}
	static op_force_inline VEGA_PIXEL UnpremultiplyFast(VEGA_PIXEL pix)
	{
		unsigned a = UNPACK_A(pix);
		if (a == 0 || a == 255)
			return pix;

		unsigned recip_a = (255u << 24) / a;
		unsigned r = ((UNPACK_R(pix) * recip_a) + (1 << 23)) >> 24;
		unsigned g = ((UNPACK_G(pix) * recip_a) + (1 << 23)) >> 24;
		unsigned b = ((UNPACK_B(pix) * recip_a) + (1 << 23)) >> 24;

		return PACK_ARGB(a, r, g, b);
	}

	struct Pointer
	{
		UINT32* rgba;

		op_force_inline Pointer operator+(int offset) const
		{
			Pointer p;
			p.rgba = rgba + offset;
			return p;
		}

		op_force_inline BOOL operator<(const Pointer &p) const
		{
			return rgba < p.rgba;
		}

		op_force_inline BOOL operator>=(const Pointer &p) const
		{
			return rgba >= p.rgba;
		}
	};

	// Constant (readonly) pixel accessor
	class ConstAccessor
	{
	public:
		ConstAccessor(const Pointer& p) { Reset(p); }

		const Pointer& Ptr() const { return ptr; }
		void Reset(const Pointer& p) { ptr = p; }

		op_force_inline void LoadUnpack(int& a, int& r, int& g, int& b) const
		{
			a = UNPACK_A(*ptr.rgba);
			r = UNPACK_R(*ptr.rgba);
			g = UNPACK_G(*ptr.rgba);
			b = UNPACK_B(*ptr.rgba);
		}
		op_force_inline void LoadUnpack(unsigned& a, unsigned& r, unsigned& g, unsigned& b) const
		{
			a = UNPACK_A(*ptr.rgba);
			r = UNPACK_R(*ptr.rgba);
			g = UNPACK_G(*ptr.rgba);
			b = UNPACK_B(*ptr.rgba);
		}
		op_force_inline void LoadUnpack(unsigned& a) const
		{
			a = UNPACK_A(*ptr.rgba);
		}
		op_force_inline void LoadUnpack(unsigned& a, unsigned &l) const
		{
			a = UNPACK_A(*ptr.rgba);
			l = UNPACK_R(*ptr.rgba);
		}
		op_force_inline VEGA_PIXEL Load() const { return *ptr.rgba; }
		op_force_inline VEGA_PIXEL LoadRel(int ofs) const { return *(ptr.rgba+ofs); }

		op_force_inline void operator+=(int pix_disp) { ptr.rgba += pix_disp; }
		op_force_inline void operator-=(int pix_disp) { ptr.rgba -= pix_disp; }

		op_force_inline void operator++() { ++ptr.rgba; }
		op_force_inline void operator++(int) { ptr.rgba++; }

	private:
		Pointer ptr;
	};

	class Accessor
	{
	public:
		Accessor(const Pointer& p) { Reset(p); }

		const Pointer& Ptr() const { return ptr; }
		void Reset(const Pointer& p) { ptr = p; }

		op_force_inline void LoadUnpack(int& a, int& r, int& g, int& b) const
		{
			a = UNPACK_A(*ptr.rgba);
			r = UNPACK_R(*ptr.rgba);
			g = UNPACK_G(*ptr.rgba);
			b = UNPACK_B(*ptr.rgba);
		}
		op_force_inline void LoadUnpack(unsigned& a, unsigned& r, unsigned& g, unsigned& b) const
		{
			a = UNPACK_A(*ptr.rgba);
			r = UNPACK_R(*ptr.rgba);
			g = UNPACK_G(*ptr.rgba);
			b = UNPACK_B(*ptr.rgba);
		}
		op_force_inline VEGA_PIXEL Load() const { return *ptr.rgba; }
		op_force_inline VEGA_PIXEL LoadRel(int ofs) const { return *(ptr.rgba+ofs); }

		op_force_inline void Store(VEGA_PIXEL px) { *ptr.rgba = px; }
		op_force_inline void Store(VEGA_PIXEL px, unsigned len)
		{
			VEGA_DISPATCH(StoreTo, (ptr.rgba, px, len));
		}

		static op_force_inline void StoreTo(UINT32* ptr, UINT32 px, unsigned len)
		{
#if defined(SIXTY_FOUR_BIT) && defined(HAVE_UINT64)
			UINT64* p = reinterpret_cast<UINT64*>(ptr);
			unsigned len8 = len / 2;
			UINT64 px8 = (UINT64)px << 32 | px;
			while (len8-- > 0)
				*p++ = px8;
			if (len % 2)
				*reinterpret_cast<UINT32*>(p) = px;
#else
			UINT32* p = ptr;
			if (len & ~0x7)
			{
				UINT32* p_end = ptr + (len & ~0x7);
				while (p < p_end)
				{
					*(p+0) = px;
					*(p+1) = px;
					*(p+2) = px;
					*(p+3) = px;

					*(p+4) = px;
					*(p+5) = px;
					*(p+6) = px;
					*(p+7) = px;

					p += 8;
				}

				len &= 0x7;
			}
			while (len-- > 0)
				*p++ = px;
#endif
		}
		op_force_inline void StoreRGB(unsigned r, unsigned g, unsigned b)
		{
			*ptr.rgba = PACK_RGB(r,g,b);
		}
		op_force_inline void StoreARGB(unsigned a, unsigned r, unsigned g, unsigned b)
		{
			*ptr.rgba = PACK_ARGB(a,r,g,b);
		}
		op_force_inline void CopyFrom(const Pointer& src, unsigned pix_count)
		{
			op_memcpy(ptr.rgba, src.rgba, pix_count * 4);
		}
		op_force_inline void MoveFrom(const Pointer& src, unsigned pix_count)
		{
			VEGA_DISPATCH(MoveFromTo, (ptr.rgba, src.rgba, pix_count));
		}
		static op_force_inline void MoveFromTo(UINT32* dst, const UINT32* src, unsigned pix_count)
		{
			op_memmove(dst, src, pix_count * 4);
		}

		op_force_inline void operator+=(int pix_disp) { ptr.rgba += pix_disp; }
		op_force_inline void operator-=(int pix_disp) { ptr.rgba -= pix_disp; }

		op_force_inline void operator++() { ++ptr.rgba; }
		op_force_inline void operator++(int) { ptr.rgba++; }

	private:
		Pointer ptr;
	};

	// Buffer creation / deletion
	static op_force_inline OP_STATUS CreateBuffer(Pointer& p, unsigned w, unsigned h, bool opaque)
	{
		p.rgba = OP_NEWA_WH(UINT32, w, h);
		if (!p.rgba)
			return OpStatus::ERR_NO_MEMORY;
		return OpStatus::OK;
	}
	static op_force_inline OP_STATUS CreateBufferFromData(Pointer& ptr, void* data, unsigned w, unsigned h, bool opaque)
	{
		ptr.rgba = (UINT32*)data;
		return OpStatus::OK;
	}
	static op_force_inline void DeleteBuffer(Pointer& p)
	{
		OP_DELETEA(p.rgba);
		p.rgba = NULL;
	}
	static op_force_inline void ClearBuffer(Pointer& p)
	{
		p.rgba = NULL;
	}
	static op_force_inline OP_STATUS Bind(Pointer& ptr, VEGAPixelStore* ps, bool opaque)
	{
		ptr.rgba = (UINT32*)ps->buffer;
		return OpStatus::OK;
	}
	static op_force_inline void Unbind(Pointer& ptr, VEGAPixelStore* ps)
	{
		ptr.rgba = NULL;
	}

#ifdef USE_PREMULTIPLIED_ALPHA
#define RET_PREMULT(v)		v
#define RET_UNPREMULT(v)	PremultiplyFast(v)
#else
#define RET_PREMULT(v)		UnpremultiplyFast(v)
#define RET_UNPREMULT(v)	v
#endif // USE_PREMULTIPLIED_ALPHA


	static op_force_inline void Sampler_NearestX_Opaque(UINT32* color, const UINT32* data, unsigned cnt, INT32 csx, INT32 cdx);
	static op_force_inline void Sampler_NearestX_CompOver(UINT32* color, const UINT32* data, unsigned cnt, INT32 csx, INT32 cdx);
	static op_force_inline void Sampler_NearestX_CompOverMask(UINT32* color, const UINT32* data, const UINT8* mask, unsigned cnt, INT32 csx, INT32 cdx);
	static op_force_inline void Sampler_NearestX_CompOverConstMask(UINT32* color, const UINT32* data, UINT32 mask, unsigned cnt, INT32 csx, INT32 cdx);

	static op_force_inline void Sampler_NearestXY_CompOver(UINT32* color, const UINT32* data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	static op_force_inline void Sampler_NearestXY_CompOverMask(UINT32* color, const UINT32* data, const UINT8* mask, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);

	static op_force_inline void SamplerNearestX_Opaque(Accessor& color, const Pointer& data, unsigned cnt, INT32 csx, INT32 cdx)
	{
		VEGA_DISPATCH(Sampler_NearestX_Opaque, (color.Ptr().rgba, data.rgba, cnt, csx, cdx));
		color += cnt;
	}

	static op_force_inline void SamplerNearestX_CompOver(Accessor& color, const Pointer& data, unsigned cnt, INT32 csx, INT32 cdx)
	{
		VEGA_DISPATCH(Sampler_NearestX_CompOver, (color.Ptr().rgba, data.rgba, cnt, csx, cdx));
		color += cnt;
	}

	static op_force_inline void SamplerNearestX_CompOverMask(Accessor& color, const Pointer& data, const UINT8*& mask, unsigned cnt, INT32 csx, INT32 cdx)
	{
		VEGA_DISPATCH(Sampler_NearestX_CompOverMask, (color.Ptr().rgba, data.rgba, mask, cnt, csx, cdx));
		color += cnt;
		mask += cnt;
	}

	static op_force_inline void SamplerNearestX_CompOverConstMask(Accessor& color, const Pointer& data, UINT8 mask, unsigned cnt, INT32 csx, INT32 cdx)
	{
		VEGA_DISPATCH(Sampler_NearestX_CompOverConstMask, (color.Ptr().rgba, data.rgba, mask, cnt, csx, cdx));
		color += cnt;
	}

	static op_force_inline void SamplerNearestXY_CompOver(Accessor& color, const Pointer& data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
	{
		VEGA_DISPATCH(Sampler_NearestXY_CompOver, (color.Ptr().rgba, data.rgba, cnt, dataPixelStride, csx, cdx, csy, cdy));
		color += cnt;
	}

	static op_force_inline void SamplerNearestXY_CompOverMask(Accessor& color, const Pointer& data, const UINT8*& mask, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
	{
		VEGA_DISPATCH(Sampler_NearestXY_CompOverMask, (color.Ptr().rgba, data.rgba, mask, cnt, dataPixelStride, csx, cdx, csy, cdy));
		color += cnt;
		mask += cnt;
	}

	// Sample using nearest neighbour from a buffer containing premultiplied pixels
	static op_force_inline VEGA_PIXEL SamplerNearestPM(const UINT32* data, unsigned dataPixelStride,
													   INT32 csx, INT32 csy)
	{
		return RET_PREMULT(data[VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx)]);
	}
	static op_force_inline VEGA_PIXEL SamplerNearestPM(const Pointer& data, unsigned dataPixelStride,
													   INT32 csx, INT32 csy)
	{
		return SamplerNearestPM(data.rgba, dataPixelStride, csx, csy);
	}

	// Sample using nearest neighbour from a buffer containing non-premultiplied pixels
	static op_force_inline VEGA_PIXEL SamplerNearest(const Pointer& data, unsigned dataPixelStride,
													 INT32 csx, INT32 csy)
	{
		return RET_UNPREMULT(data.rgba[VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx)]);
	}

	// Sample in one dimension using nearest neighbour from a buffer containing premultiplied pixels
	static op_force_inline VEGA_PIXEL SamplerNearestPM_1D(const UINT32* data, INT32 csx)
	{
		return RET_PREMULT(data[VEGA_SAMPLE_INT(csx)]);
	}
	static op_force_inline VEGA_PIXEL SamplerNearestPM_1D(const Pointer& data, INT32 csx)
	{
		return SamplerNearestPM_1D(data.rgba, csx);
	}

	// Sample in one dimension using nearest neighbour from a buffer containing non-premultiplied pixels
	static op_force_inline VEGA_PIXEL SamplerNearest_1D(const Pointer& data, INT32 csx)
	{
		return RET_UNPREMULT(data.rgba[VEGA_SAMPLE_INT(csx)]);
	}

	// Sample using bilinear interpolation from a buffer containing premultiplied pixels
	static op_force_inline VEGA_PIXEL SamplerBilerpPM(const Pointer& data, unsigned dataPixelStride,
													  INT32 csx, INT32 csy)
	{
		return SamplerBilerpPM(data.rgba, dataPixelStride, csx, csy);
	}

	static op_force_inline UINT32 SamplerBilerpPM(const UINT32* data, unsigned dataPixelStride,
												  INT32 csx, INT32 csy);

	static op_force_inline void Sampler_LerpX(UINT32* dstdata, const UINT32* srcdata, INT32 csx, INT32 cdx,
											  unsigned dstlen, unsigned srclen);

	static op_force_inline void Sampler_LerpY(UINT32* dstdata, const UINT32* srcdata1, const UINT32* srcdata2,
											  INT32 frc_y, unsigned len);

	static op_force_inline void Sampler_LerpXY_Opaque(UINT32* color, const UINT32* data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	static op_force_inline void Sampler_LerpXY_CompOver(UINT32* color, const UINT32* data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);
	static op_force_inline void Sampler_LerpXY_CompOverMask(UINT32* color, const UINT32* data, const UINT8* mask, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy);

	// Perform linear interpolation in x direction from <srcdata> position <csx>, step <cdx>
	static op_force_inline void SamplerLerpXPM(Pointer dstdata, const Pointer& srcdata, INT32 csx, INT32 cdx,
											   unsigned dstlen, unsigned srclen)
	{
		VEGA_DISPATCH(Sampler_LerpX, (dstdata.rgba, srcdata.rgba, csx, cdx, dstlen, srclen));
	}

	// Perform linear interpolation between <srcdata1> and <srcdata2> by factor <frc_y>
	static op_force_inline void SamplerLerpYPM(Pointer dstdata, const Pointer& srcdata1, const Pointer& srcdata2,
											   INT32 frc_y, unsigned len)
	{
		VEGA_DISPATCH(Sampler_LerpY, (dstdata.rgba, srcdata1.rgba, srcdata2.rgba, frc_y, len));
	}

	static op_force_inline void SamplerLerpXY_Opaque(Accessor& color, const Pointer& data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
	{
		VEGA_DISPATCH(Sampler_LerpXY_Opaque, (color.Ptr().rgba, data.rgba, cnt, dataPixelStride, csx, cdx, csy, cdy));
		color += cnt;
	}

	static op_force_inline void SamplerLerpXY_CompOver(Accessor& color, const Pointer& data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
	{
		VEGA_DISPATCH(Sampler_LerpXY_CompOver, (color.Ptr().rgba, data.rgba, cnt, dataPixelStride, csx, cdx, csy, cdy));
		color += cnt;
	}

	static op_force_inline void SamplerLerpXY_CompOverMask(Accessor& color, const Pointer& data, const UINT8*& mask, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
	{
		VEGA_DISPATCH(Sampler_LerpXY_CompOverMask, (color.Ptr().rgba, data.rgba, mask, cnt, dataPixelStride, csx, cdx, csy, cdy));
		color += cnt;
		mask += cnt;
	}

	// Composite operators
	static op_force_inline VEGA_PIXEL Modulate(VEGA_PIXEL s, UINT32 m);
	static op_force_inline VEGA_PIXEL CompositeOver(VEGA_PIXEL d, VEGA_PIXEL s);
#ifdef USE_PREMULTIPLIED_ALPHA
	static op_force_inline VEGA_PIXEL CompositeOverPremultiplied(VEGA_PIXEL d, VEGA_PIXEL s);
#endif // USE_PREMULTIPLIED_ALPHA
	static op_force_inline VEGA_PIXEL CompositeOverIn(VEGA_PIXEL d, VEGA_PIXEL s, UINT32 m);
	static op_force_inline VEGA_PIXEL CompositeOverUnpacked(VEGA_PIXEL dst, int sa, int sr, int sg, int sb);

	static op_force_inline void CompOver(UINT32* dp, const UINT32* sp, unsigned len);
	static op_force_inline void CompOverConstMask(UINT32* dp, const UINT32* sp, UINT32 mask, unsigned len);
	static op_force_inline void CompConstOver(UINT32* dp, UINT32 src, unsigned len);
	static op_force_inline void CompConstOverMask(UINT32* dp, UINT32 src, const UINT8* mask, unsigned len);

	static op_force_inline void CompositeOver(Pointer dp, Pointer sp, unsigned len);
	static op_force_inline void CompositeOverPM(Pointer dp, Pointer sp, unsigned len);
	static op_force_inline void CompositeOverIn(Pointer dp, Pointer sp, const UINT8* mask, unsigned len);
	static op_force_inline void CompositeOverIn(Pointer dp, Pointer sp, UINT8 mask, unsigned len);
	static op_force_inline void CompositeOverInPM(Pointer dp, Pointer sp, UINT8 mask, unsigned len);

	static op_force_inline void CompositeOver(Pointer dp, VEGA_PIXEL src, unsigned len);
	static op_force_inline void CompositeOverIn(Pointer dp, VEGA_PIXEL src, const UINT8* mask, unsigned len);

	typedef void (*Convert)(UINT32* dst, const void* src, unsigned cnt);

	template<Convert CONVERT, int BPP>
	static void UnpackFormatConvert(VEGAPixelStore* store, Accessor dst,
									unsigned dstPixelPadding, unsigned width, unsigned height)
	{
		const UINT8* sp = static_cast<UINT8*>(store->buffer);
		UINT32* dp = dst.Ptr().rgba;
		unsigned dstPixelStride = dstPixelPadding + width;

		for (unsigned y = 0; y < height; ++y)
		{
			CONVERT(dp, sp, width);
			sp += store->stride;
			dp += dstPixelStride;
		}
	}

#ifdef VEGA_USE_ASM

	typedef void (*UnpackRGBA)(const UINT8* p, unsigned &a, unsigned &r, unsigned &g, unsigned &b);
	typedef void (*UnpackRGB)(const UINT8* p, unsigned &r, unsigned &g, unsigned &b);

	template<UnpackRGB UNPCK, int BPP>
	static void UnpackFormatConvertRGB(UINT32* dst, const void* src, unsigned cnt)
	{
		const UINT8* s = reinterpret_cast<const UINT8*>(src);
		unsigned r, g, b;

		while (cnt-- > 0)
		{
			UNPCK(s, r, g, b);
			*dst = PACK_RGB(r, g, b);
			s += BPP;
			dst++;
		}
	}

	template<UnpackRGBA UNPCK, int BPP>
	static void UnpackFormatConvertRGBA(UINT32* dst, const void* src, unsigned cnt)
	{
		const UINT8* s = reinterpret_cast<const UINT8*>(src);
		unsigned a, r, g, b;

		while (cnt-- > 0)
		{
			UNPCK(s, a, r, g, b);
			*dst = PACK_ARGB(a, r, g, b);
			s += BPP;
			dst++;
		}
	}

#define DEFINE_DISPATCH_CONVERSION(FMT) \
	static void FormatConvertDispatch_ ## FMT(UINT32* dst, const void* src, unsigned cnt)	\
	{																						\
		g_vegaDispatchTable->ConvertFrom_ ## FMT(dst, src, cnt);							\
	}

	DEFINE_DISPATCH_CONVERSION(BGRA8888);
	DEFINE_DISPATCH_CONVERSION(RGBA8888);
	DEFINE_DISPATCH_CONVERSION(ABGR8888);
	DEFINE_DISPATCH_CONVERSION(ARGB8888);
	DEFINE_DISPATCH_CONVERSION(BGRA4444);
	DEFINE_DISPATCH_CONVERSION(RGBA4444);
	DEFINE_DISPATCH_CONVERSION(ABGR4444);
	DEFINE_DISPATCH_CONVERSION(RGB565);
	DEFINE_DISPATCH_CONVERSION(BGR565);
	DEFINE_DISPATCH_CONVERSION(RGB888);
#undef DEFINE_CONVERSION

#define FORMAT_CONVERT_RGBA(FMT, BPP) \
	UnpackFormatConvert<VEGAPixelFormat_8888::FormatConvertDispatch_ ## FMT, BPP>(store, dst, dstPixelPadding, width, height);
#define FORMAT_CONVERT_RGB(FMT, BPP) \
	UnpackFormatConvert<VEGAPixelFormat_8888::FormatConvertDispatch_ ## FMT, BPP>(store, dst, dstPixelPadding, width, height);

#define DEFINE_FORMAT_CONVERSION(FMT, TYPE, BPP)																\
	static inline void UnpackFormatConvert_ ## FMT(VEGAPixelStore* store, Accessor dst,							\
												   unsigned dstPixelPadding, unsigned width, unsigned height)	\
	{																											\
		FORMAT_CONVERT_ ## TYPE(FMT, BPP);																		\
	}

#else

#define DEFINE_FORMAT_CONVERSION_LINE_RGB(FMT, BPP)													\
	static inline void UnpackFormatConvert_ ## FMT(UINT32* dst, const void* src, unsigned cnt)		\
	{																								\
		const UINT8* s = reinterpret_cast<const UINT8*>(src);										\
		unsigned r, g, b;																			\
																									\
		while (cnt-- > 0)																			\
		{																							\
			VEGAFormatUnpack::FMT ## _Unpack(s, r, g, b);											\
			*dst = PACK_RGB(r, g, b);																\
			s += BPP;																				\
			dst++;																					\
		}																							\
	}

#define DEFINE_FORMAT_CONVERSION_LINE_RGBA(FMT, BPP)												\
	static inline void UnpackFormatConvert_ ## FMT(UINT32* dst, const void* src, unsigned cnt)		\
	{																								\
		const UINT8* s = reinterpret_cast<const UINT8*>(src);										\
		unsigned a, r, g, b;																		\
																									\
		while (cnt-- > 0)																			\
		{																							\
			VEGAFormatUnpack::FMT ## _Unpack(s, a, r, g, b);										\
			*dst = PACK_ARGB(a, r, g, b);															\
			s += BPP;																				\
			dst++;																					\
		}																							\
	}

#define FORMAT_CONVERT_RGBA(FMT, BPP) \
	UnpackFormatConvert<VEGAPixelFormat_8888::UnpackFormatConvert_ ## FMT, BPP>(store, dst, dstPixelPadding, width, height);
#define FORMAT_CONVERT_RGB(FMT, BPP) \
	UnpackFormatConvert<VEGAPixelFormat_8888::UnpackFormatConvert_ ## FMT, BPP>(store, dst, dstPixelPadding, width, height);

#define DEFINE_FORMAT_CONVERSION(FMT, TYPE, BPP)														\
	DEFINE_FORMAT_CONVERSION_LINE_ ## TYPE(FMT, BPP)													\
	static void UnpackFormatConvert_ ## FMT(VEGAPixelStore* store, Accessor dst,						\
											unsigned dstPixelPadding, unsigned width, unsigned height)	\
	{																									\
		FORMAT_CONVERT_ ## TYPE(FMT, BPP);																\
	}

#endif // VEGA_USE_ASM

	DEFINE_FORMAT_CONVERSION(RGBA8888, RGBA, 4);
	DEFINE_FORMAT_CONVERSION(BGRA8888, RGBA, 4);
	DEFINE_FORMAT_CONVERSION(ABGR8888, RGBA, 4);
	DEFINE_FORMAT_CONVERSION(ARGB8888, RGBA, 4);
	DEFINE_FORMAT_CONVERSION(BGRA4444, RGBA, 4);
	DEFINE_FORMAT_CONVERSION(RGBA4444, RGBA, 2);
	DEFINE_FORMAT_CONVERSION(ABGR4444, RGBA, 2);
	DEFINE_FORMAT_CONVERSION(RGB565, RGB, 2);
	DEFINE_FORMAT_CONVERSION(BGR565, RGB, 2);
	DEFINE_FORMAT_CONVERSION(RGB888, RGB, 3);

#undef DEFINE_FORMAT_CONVERSION
#undef FORMAT_CONVERT_RGBA
#undef FORMAT_CONVERT_RGB
};

template <>
/* static */
op_force_inline bool VEGAPixelFormat_8888< 0,  8, 16, 24>::IsCompatible(VEGAPixelStoreFormat fmt)
{
	return fmt == VPSF_RGBA8888;
}

template <>
/* static */
op_force_inline bool VEGAPixelFormat_8888<16,  8,  0, 24>::IsCompatible(VEGAPixelStoreFormat fmt)
{
	return fmt == VPSF_BGRA8888;
}

/* static */
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_NearestX_Opaque(UINT32* color, const UINT32* data, unsigned cnt, INT32 csx, INT32 cdx)
{
	while (cnt-- > 0)
	{
		*color = SamplerNearestPM_1D(data, csx);

		csx += cdx;
		color++;
	}
}

/* static */
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_NearestX_CompOver(UINT32* color, const UINT32* data, unsigned cnt, INT32 csx, INT32 cdx)
{
	while (cnt-- > 0)
	{
		*color = CompositeOver(*color, SamplerNearestPM_1D(data, csx));

		csx += cdx;
		color++;
	}
}

/* static */
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_NearestX_CompOverMask(UINT32* color, const UINT32* data, const UINT8* mask, unsigned cnt, INT32 csx, INT32 cdx)
{
	while (cnt-- > 0)
	{
		int a = *mask++;
		if (a)
			*color = CompositeOverIn(*color, SamplerNearestPM_1D(data, csx), a);

		csx += cdx;
		color++;
	}
}

/* static */
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_NearestX_CompOverConstMask(UINT32* color, const UINT32* data, UINT32 mask, unsigned cnt, INT32 csx, INT32 cdx)
{
	while (cnt-- > 0)
	{
		*color = CompositeOverIn(*color, SamplerNearestPM_1D(data, csx), mask);
		csx += cdx;
		color++;
	}
}

/* static */
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_NearestXY_CompOver(UINT32* color, const UINT32* data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	while (cnt-- > 0)
	{
		VEGA_PIXEL c = SamplerNearestPM(data, dataPixelStride, csx, csy);
		*color = CompositeOver(*color, c);

		csx += cdx;
		csy += cdy;
		color++;
	}
}

/* static */
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_NearestXY_CompOverMask(UINT32* color, const UINT32* data, const UINT8* mask, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	while (cnt-- > 0)
	{
		int a = *mask++;
		if (a)
		{
			VEGA_PIXEL c = SamplerNearestPM(data, dataPixelStride, csx, csy);
			*color = CompositeOverIn(*color, c, a);
		}

		csx += cdx;
		csy += cdy;
		color++;
	}
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_LerpX(UINT32* dpix, const UINT32* spix, INT32 csx, INT32 cdx, unsigned dstlen, unsigned srclen)
{
	while (dstlen && csx < 0)
	{
		*dpix++ = spix[0];

		csx += cdx;
		dstlen--;
	}
	OP_ASSERT(!dstlen || csx >= 0);
	while (dstlen > 0 && ((unsigned)csx) < VEGA_INTTOSAMPLE(srclen-1))
	{
		INT32 int_px = VEGA_SAMPLE_INT(csx);
		INT32 frc_px = VEGA_SAMPLE_FRAC(csx);

		UINT32 c = spix[int_px];
		if (frc_px)
		{
			UINT32 c_ag = EXTRACT_HI(c);
			UINT32 c_rb = EXTRACT_LO(c);

			// merge c and next c+1
			UINT32 nc = spix[int_px + 1];

			// Merge
			c_ag = PACKED_MACC(c_ag, EXTRACT_HI(nc) - c_ag, frc_px);
			c_rb = PACKED_MACC(c_rb, EXTRACT_LO(nc) - c_rb, frc_px);

			c = COMBINE_ARGB(c_ag, c_rb);
		}

		*dpix++ = c;

		csx += cdx;
		dstlen--;
	}
	while (dstlen > 0)
	{
		*dpix++ = spix[srclen - 1];

		csx += cdx;
		dstlen--;
	}
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_LerpY(UINT32* dpix, const UINT32* spix1, const UINT32* spix2, INT32 frc_y, unsigned len)
{
	while (len-- > 0)
	{
		UINT32 c = *spix1++;
		UINT32 c_ag = EXTRACT_HI(c);
		UINT32 c_rb = EXTRACT_LO(c);

		// merge c and next c+1
		UINT32 nc = *spix2++;

		// Merge
		c_ag = PACKED_MACC(c_ag, EXTRACT_HI(nc) - c_ag, frc_y);
		c_rb = PACKED_MACC(c_rb, EXTRACT_LO(nc) - c_rb, frc_y);

		*dpix++ = COMBINE_ARGB(c_ag, c_rb);
	}
}

/* static */
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_LerpXY_Opaque(UINT32* color, const UINT32* data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	while (cnt-- > 0)
	{
		*color = SamplerBilerpPM(data, dataPixelStride, csx, csy);

		csx += cdx;
		csy += cdy;
		color++;
	}
}

/* static */
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_LerpXY_CompOver(UINT32* color, const UINT32* data, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	while (cnt-- > 0)
	{
		UINT32 c = SamplerBilerpPM(data, dataPixelStride, csx, csy);
		*color = CompositeOver(*color, c);

		csx += cdx;
		csy += cdy;
		color++;
	}
}

/* static */
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Sampler_LerpXY_CompOverMask(UINT32* color, const UINT32* data, const UINT8* mask, unsigned cnt, unsigned dataPixelStride, INT32 csx, INT32 cdx, INT32 csy, INT32 cdy)
{
	while (cnt-- > 0)
	{
		int a = *mask++;
		if (a)
		{
			UINT32 c = SamplerBilerpPM(data, dataPixelStride, csx, csy);
			*color = CompositeOverIn(*color, c, a);
		}

		csx += cdx;
		csy += cdy;
		color++;
	}
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline UINT32
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::SamplerBilerpPM(const UINT32* data, unsigned dataPixelStride,
										  INT32 csx, INT32 csy)
{
	const UINT32* pix = data + VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx);
	UINT32 c = *pix;
	int weight2 = VEGA_SAMPLE_FRAC(csx);
	int yweight2 = VEGA_SAMPLE_FRAC(csy);
	UINT32 c_ag = EXTRACT_HI(c);
	UINT32 c_rb = EXTRACT_LO(c);
	if (weight2)
	{
		// merge c and next c+1
		UINT32 nc = *(pix + 1);
		// Merge
		UINT32 diff_ag = EXTRACT_HI(nc) - c_ag;
		UINT32 diff_rb = EXTRACT_LO(nc) - c_rb;
		diff_ag = (diff_ag*weight2)>>8;
		diff_rb = (diff_rb*weight2)>>8;
		c_ag += diff_ag;
		c_rb += diff_rb;
		c_ag &= 0xff00ff;
		c_rb &= 0xff00ff;

		if (yweight2)
		{
			pix += dataPixelStride;

			// merge with next scanline
			UINT32 c2 = *pix;
			UINT32 c2_ag = EXTRACT_HI(c2);
			UINT32 c2_rb = EXTRACT_LO(c2);
			// merge c and next c+1
			UINT32 nc2 = *(pix + 1);
			// Merge
			diff_ag = EXTRACT_HI(nc2) - c2_ag;
			diff_rb = EXTRACT_LO(nc2) - c2_rb;
			diff_ag = (diff_ag*weight2)>>8;
			diff_rb = (diff_rb*weight2)>>8;
			c2_ag += diff_ag;
			c2_rb += diff_rb;
			c2_ag &= 0xff00ff;
			c2_rb &= 0xff00ff;

			// Merge
			diff_ag = c2_ag-c_ag;
			diff_rb = c2_rb-c_rb;
			diff_ag = (diff_ag*yweight2)>>8;
			diff_rb = (diff_rb*yweight2)>>8;
			c_ag += diff_ag;
			c_rb += diff_rb;
			c_ag &= 0xff00ff;
			c_rb &= 0xff00ff;
		}
	}
	else if (yweight2)
	{
		pix += dataPixelStride;

		// merge with next scanline
		UINT32 c2 = *pix;
		UINT32 c2_ag = EXTRACT_HI(c2);
		UINT32 c2_rb = EXTRACT_LO(c2);
		// Merge
		UINT32 diff_ag = c2_ag-c_ag;
		UINT32 diff_rb = c2_rb-c_rb;
		diff_ag = (diff_ag*yweight2)>>8;
		diff_rb = (diff_rb*yweight2)>>8;
		c_ag += diff_ag;
		c_rb += diff_rb;
		c_ag &= 0xff00ff;
		c_rb &= 0xff00ff;
	}
	c = (c_ag << 8) | c_rb;
	return RET_PREMULT(c);
}

#undef RET_PREMULT
#undef RET_UNPREMULT

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline VEGA_PIXEL
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::Modulate(VEGA_PIXEL s, UINT32 m)
{
	if (m == 0)
		return 0;
	if (m == 255)
		return s;

#ifdef USE_PREMULTIPLIED_ALPHA
	UINT32 s_ag = EXTRACT_HI(s);
	UINT32 s_rb = EXTRACT_LO(s);

	s_ag = s_ag * (m + 1);
	s_rb = s_rb * (m + 1);

	return COMBINE_ARGB(s_ag, s_rb);
#else
	UINT32 sa = ((UNPACK_A(s) * (m + 1)) >> 8) & 0xff;

	return (sa << ALPHA_SHIFT) | (s & RGB_MASK);
#endif // USE_PREMULTIPLIED_ALPHA
}

// r = s over d
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline VEGA_PIXEL
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOver(VEGA_PIXEL d, VEGA_PIXEL s)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	if (s == 0)
		return d;

	if (UNPACK_A(s) == 255 || d == 0)
		return s;

	return CompositeOverPremultiplied(d, s);
#else
	int sa = UNPACK_A(s);
	if (sa == 255)
		return s;

	if (sa == 0)
		return d;

	int da = UNPACK_A(d);
	if (da == 0)
		return s;

	if (da == 255)
	{
		UINT32 dag = EXTRACT_HI(d);
		UINT32 drb = EXTRACT_LO(d);

		UINT32 sag = EXTRACT_HI(s);
		UINT32 srb = EXTRACT_LO(s);

		dag = PACKED_MACC(dag, sag - dag, sa);
		drb = PACKED_MACC(drb, srb - drb, sa);

		d = COMBINE_RGB(dag, drb);
	}
	else
	{
		int dr = UNPACK_R(d);
		int dg = UNPACK_G(d);
		int db = UNPACK_B(d);

		int sr = UNPACK_R(s);
		int sb = UNPACK_B(s);
		int sg = UNPACK_G(s);

		dr *= da;
		dg *= da;
		db *= da;
		da += sa-((da*sa)>>8);
		dr = ((dr + (sa*(sr-(dr>>8))))/da)&0xff;
		dg = ((dg + (sa*(sg-(dg>>8))))/da)&0xff;
		db = ((db + (sa*(sb-(db>>8))))/da)&0xff;
		if (da > 0xff)
			da = 0xff;
		OP_ASSERT(da >= 0);
		d = PACK_ARGB(da, dr, dg, db);
	}
	return d;
#endif // USE_PREMULTIPLIED_ALPHA
}

// r = (s in m) over d
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline VEGA_PIXEL
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOverIn(VEGA_PIXEL d, VEGA_PIXEL s, UINT32 m)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	if (m == 0 || s == 0)
		return d;

	UINT32 s_ag, s_rb;
	if (m == 255)
	{
		if (UNPACK_A(s) == 255 || d == 0)
			return s;

		s_ag = EXTRACT_HI(s);
		s_rb = EXTRACT_LO(s);
	}
	else // 0 < m < 255
	{
		s_ag = EXTRACT_HI(s);
		s_rb = EXTRACT_LO(s);

		s_ag *= m + 1;
		s_rb *= m + 1;

		s_ag &= 0xff00ff00;
		s_rb &= 0xff00ff00;

		s_rb >>= 8;

		if (d == 0)
			return s_ag | s_rb;

		s_ag >>= 8;
	}

	UINT32 d_ag = EXTRACT_HI(d);
	UINT32 d_rb = EXTRACT_LO(d);
	UINT32 inv_sa;

	// This rather ugly bit expects the compiler to optimize away
	// the if-else mess since it's all constant expressions.
	if (ALPHA_SHIFT == 24)
		inv_sa = 256 - (s_ag >> 16);
	else if (ALPHA_SHIFT == 0)
		inv_sa = 256 - (s_rb & 0xff);
	else
		OP_ASSERT(FALSE && "Your pixel format is on crack!");

	d_ag = PACKED_MACC (s_ag, d_ag, inv_sa);
	d_rb = PACKED_MACC (s_rb, d_rb, inv_sa);

	return COMBINE_ARGB(d_ag, d_rb);
#else
	int sa = UNPACK_A(s);
	if (m == 0 || sa == 0)
		return d;

	if (m == 255)
	{
		if (sa == 255)
			return s;
	}
	else // 0 < m < 255
	{
		sa *= m+1;
		sa >>= 8;
		if (sa == 0)
			return d;
	}

	int da = UNPACK_A(d);
	if (da == 0)
		return (sa << ALPHA_SHIFT) | (s & RGB_MASK);

	if (da == 255)
	{
		UINT32 dag = EXTRACT_HI(d);
		UINT32 drb = EXTRACT_LO(d);

		UINT32 sag = EXTRACT_HI(s);
		UINT32 srb = EXTRACT_LO(s);

		dag = PACKED_MACC(dag, sag - dag, sa);
		drb = PACKED_MACC(drb, srb - drb, sa);

		d = COMBINE_RGB(dag, drb);
	}
	else
	{
		int dr = UNPACK_R(d);
		int dg = UNPACK_G(d);
		int db = UNPACK_B(d);

		int sr = UNPACK_R(s);
		int sg = UNPACK_G(s);
		int sb = UNPACK_B(s);

		dr *= da;
		dg *= da;
		db *= da;
		da += sa-((da*sa)>>8);
		dr = ((dr + (sa*(sr-(dr>>8))))/da)&0xff;
		dg = ((dg + (sa*(sg-(dg>>8))))/da)&0xff;
		db = ((db + (sa*(sb-(db>>8))))/da)&0xff;
		if (da > 0xff)
			da = 0xff;
		OP_ASSERT(da >= 0);
		d = PACK_ARGB(da, dr, dg, db);
	}
	return d;
#endif // USE_PREMULTIPLIED_ALPHA
}

#ifdef USE_PREMULTIPLIED_ALPHA
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline VEGA_PIXEL
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOverPremultiplied(VEGA_PIXEL d, VEGA_PIXEL s)
{
	UINT32 s_ag = EXTRACT_HI(s);
	UINT32 s_rb = EXTRACT_LO(s);

	UINT32 d_ag = EXTRACT_HI(d);
	UINT32 d_rb = EXTRACT_LO(d);

	UINT32 inv_sa = 256 - UNPACK_A(s);

	d_ag = PACKED_MACC(s_ag, d_ag, inv_sa);
	d_rb = PACKED_MACC(s_rb, d_rb, inv_sa);

	return COMBINE_ARGB(d_ag, d_rb);
}
#endif // USE_PREMULTIPLIED_ALPHA

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOver(Pointer dp, Pointer sp, unsigned len)
{
	VEGA_DISPATCH(CompOver, (dp.rgba, sp.rgba, len));
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompOver(UINT32* dp, const UINT32* sp, unsigned len)
{
#ifdef USE_PREMULTIPLIED_ALPHA
#if defined(SIXTY_FOUR_BIT) && defined(HAVE_UINT64_LITERAL)
	const UINT64* src64 = reinterpret_cast<const UINT64*>(sp);
	UINT64* dst64 = reinterpret_cast<UINT64*>(dp);
	unsigned len64 = len / 2;
	const UINT64 alpha_mask = OP_UINT64(0xff000000ff000000);

	while (len64-- > 0)
	{
		UINT64 s = *src64++;

		if ((s & alpha_mask) == alpha_mask || *dst64 == 0)
		{
			*dst64++ = s;
			continue;
		}

		if (s == 0)
		{
			dst64++;
			continue;
		}

		UINT64 d = *dst64;
		UINT64 d0 = CompositeOverPremultiplied(d >> 32, s >> 32);
		UINT64 d1 = CompositeOverPremultiplied(d, s);
		*dst64++ = d0 << 32 | d1;
	}

	const UINT32* src = reinterpret_cast<const UINT32*>(src64);
	UINT32* dst = reinterpret_cast<UINT32*>(dst64);
	len = len % 2;
#else
	const UINT32* src = sp;
	UINT32* dst = dp;
#endif // defined(SIXTY_FOUR_BIT) && defined(HAVE_UINT64_LITERAL)
	while (len-- > 0)
	{
		UINT32 s = *src++;

		if ((s & ALPHA_MASK) == ALPHA_MASK || *dst == 0)
		{
			*dst++ = s;
			continue;
		}

		if (s == 0)
		{
			dst++;
			continue;
		}

		UINT32 d = *dst;
		*dst++ = CompositeOverPremultiplied(d, s);
	}
#else
	const UINT32* src = sp;
	UINT32* dst = dp;

	while (len-- > 0)
	{
		UINT32 s = *src++;

		int sa = UNPACK_A(s);
		if (sa == 255)
		{
			*dst++ = s;
			continue;
		}

		if (sa == 0)
		{
			dst++;
			continue;
		}

		UINT32 d = *dst;
		int da = UNPACK_A(d);
		if (da == 0)
		{
			*dst++ = s;
			continue;
		}

		if (da == 255)
		{
			UINT32 dag = EXTRACT_HI(d);
			UINT32 drb = EXTRACT_LO(d);

			UINT32 sag = EXTRACT_HI(s);
			UINT32 srb = EXTRACT_LO(s);

			dag = PACKED_MACC(dag, sag - dag, sa);
			drb = PACKED_MACC(drb, srb - drb, sa);

			d = COMBINE_RGB(dag, drb);
		}
		else
		{
			int dr = UNPACK_R(d);
			int dg = UNPACK_G(d);
			int db = UNPACK_B(d);

			int sr = UNPACK_R(s);
			int sg = UNPACK_G(s);
			int sb = UNPACK_B(s);

			dr *= da;
			dg *= da;
			db *= da;
			da += sa-((da*sa)>>8);
			dr = ((dr + (sa*(sr-(dr>>8))))/da)&0xff;
			dg = ((dg + (sa*(sg-(dg>>8))))/da)&0xff;
			db = ((db + (sa*(sb-(db>>8))))/da)&0xff;
			if (da > 0xff)
				da = 0xff;
			OP_ASSERT(da >= 0);
			d = PACK_ARGB(da, dr, dg, db);
		}
		*dst++ = d;
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOverPM(Pointer dp, Pointer sp, unsigned len)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	CompositeOver(dp, sp, len);
#else
	UINT32* dst = dp.rgba;
	const UINT32* src = sp.rgba;
	while (len-- > 0)
	{
		UINT32 s = *src++;

		int sa = UNPACK_A(s);
		if (sa == 255)
		{
			*dst++ = s;
			continue;
		}

		if (sa == 0)
		{
			dst++;
			continue;
		}
		s = UnpremultiplyFast(s);

		UINT32 d = *dst;
		int da = UNPACK_A(d);
		if (da == 0)
		{
			*dst++ = s;
			continue;
		}

		if (da == 255)
		{
			UINT32 dag = EXTRACT_HI(d);
			UINT32 drb = EXTRACT_LO(d);

			UINT32 sag = EXTRACT_HI(s);
			UINT32 srb = EXTRACT_LO(s);

			dag = PACKED_MACC(dag, sag - dag, sa);
			drb = PACKED_MACC(drb, srb - drb, sa);

			d = COMBINE_RGB(dag, drb);
		}
		else
		{
			int dr = UNPACK_R(d);
			int dg = UNPACK_G(d);
			int db = UNPACK_B(d);

			int sr = UNPACK_R(s);
			int sg = UNPACK_G(s);
			int sb = UNPACK_B(s);

			dr *= da;
			dg *= da;
			db *= da;
			da += sa-((da*sa)>>8);
			dr = ((dr + (sa*(sr-(dr>>8))))/da)&0xff;
			dg = ((dg + (sa*(sg-(dg>>8))))/da)&0xff;
			db = ((db + (sa*(sb-(db>>8))))/da)&0xff;
			if (da > 0xff)
				da = 0xff;
			OP_ASSERT(da >= 0);
			d = PACK_ARGB(da, dr, dg, db);
		}
		*dst++ = d;
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOverIn(Pointer dp, Pointer sp, const UINT8* mask, unsigned len)
{
	UINT32* dst = dp.rgba;
	const UINT32* src = sp.rgba;
#ifdef USE_PREMULTIPLIED_ALPHA
	while (len-- > 0)
	{
		UINT32 m = *mask++;
		UINT32 s = *src++;

		if (m == 0 || s == 0)
		{
			dst++;
			continue;
		}

		UINT32 s_ag, s_rb;
		if (m == 255)
		{
			if ((s & ALPHA_MASK) == ALPHA_MASK || *dst == 0)
			{
				*dst++ = s;
				continue;
			}

			s_ag = EXTRACT_HI(s);
			s_rb = EXTRACT_LO(s);
		}
		else // 0 < m < 255
		{
			s_ag = EXTRACT_HI(s);
			s_rb = EXTRACT_LO(s);

			s_ag *= m + 1;
			s_rb *= m + 1;

			s_ag &= 0xff00ff00;
			s_rb &= 0xff00ff00;
			s_rb >>= 8;

			if (*dst == 0)
			{
				*dst++ = s_ag | s_rb;
				continue;
			}

			s_ag >>= 8;
		}

		UINT32 d = *dst;

		UINT32 d_ag = EXTRACT_HI(d);
		UINT32 d_rb = EXTRACT_LO(d);
		UINT32 inv_sa;

		// This rather ugly bit expects the compiler to optimize away
		// the if-else mess since it's all constant expressions.
		if (ALPHA_SHIFT == 24)
			inv_sa = 256 - (s_ag >> 16);
		else if (ALPHA_SHIFT == 0)
			inv_sa = 256 - (s_rb & 0xff);
		else
			OP_ASSERT(FALSE && "Your pixel format is on crack!");

		d_ag = PACKED_MACC(s_ag, d_ag, inv_sa);
		d_ag = PACKED_MACC(s_rb, d_rb, inv_sa);

		*dst++ = COMBINE_ARGB(d_ag, d_rb);
	}
#else
	while (len-- > 0)
	{
		UINT32 m = *mask++;
		UINT32 s = *src++;

		int sa = UNPACK_A(s);
		if (m == 0 || sa == 0)
		{
			dst++;
			continue;
		}

		if (m == 255)
		{
			if (sa == 255)
			{
				*dst++ = s;
				continue;
			}
		}
		else // 0 < m < 255
		{
			sa *= m+1;
			sa >>= 8;
			if (sa == 0)
			{
				dst++;
				continue;
			}
		}

		UINT32 d = *dst;
		int da = UNPACK_A(d);
		if (da == 0)
		{
			*dst++ = (sa << ALPHA_SHIFT) | (s & RGB_MASK);
			continue;
		}

		if (da == 255)
		{
			UINT32 dag = EXTRACT_HI(d);
			UINT32 drb = EXTRACT_LO(d);

			UINT32 sag = EXTRACT_HI(s);
			UINT32 srb = EXTRACT_LO(s);

			dag = PACKED_MACC(dag, sag - dag, sa);
			drb = PACKED_MACC(drb, srb - drb, sa);

			d = COMBINE_RGB(dag, drb);
		}
		else
		{
			int dr = UNPACK_R(d);
			int dg = UNPACK_G(d);
			int db = UNPACK_B(d);

			int sr = UNPACK_R(s);
			int sg = UNPACK_G(s);
			int sb = UNPACK_B(s);

			dr *= da;
			dg *= da;
			db *= da;
			da += sa-((da*sa)>>8);
			dr = ((dr + (sa*(sr-(dr>>8))))/da)&0xff;
			dg = ((dg + (sa*(sg-(dg>>8))))/da)&0xff;
			db = ((db + (sa*(sb-(db>>8))))/da)&0xff;
			if (da > 0xff)
				da = 0xff;
			OP_ASSERT(da >= 0);
			d = PACK_ARGB(da, dr, dg, db);
		}
		*dst++ = d;
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOverIn(Pointer dp, Pointer sp, UINT8 mask, unsigned len)
{
	VEGA_DISPATCH(CompOverConstMask, (dp.rgba, sp.rgba, static_cast<UINT32>(mask), len));
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompOverConstMask(UINT32* dp, const UINT32* sp, UINT32 mask, unsigned len)
{
	UINT32* dst = dp;
	const UINT32* src = sp;
	mask += 1;
#ifdef USE_PREMULTIPLIED_ALPHA
	while (len-- > 0)
	{
		UINT32 s = *src++;
		UINT32 s_ag = EXTRACT_HI(s);
		UINT32 s_rb = EXTRACT_LO(s);

		s_ag *= mask;
		s_rb *= mask;

		s_ag &= 0xff00ff00;
		s_rb &= 0xff00ff00;
		s_rb >>= 8;
		s_ag >>= 8;

		UINT32 d = *dst;

		UINT32 d_ag = EXTRACT_HI(d);
		UINT32 d_rb = EXTRACT_LO(d);
		UINT32 inv_sa;

		// This rather ugly bit expects the compiler to optimize away
		// the if-else mess since it's all constant expressions.
		if (ALPHA_SHIFT == 24)
			inv_sa = 256 - (s_ag >> 16);
		else if (ALPHA_SHIFT == 0)
			inv_sa = 256 - (s_rb & 0xff);
		else
			OP_ASSERT(FALSE && "Your pixel format is on crack!");

		d_ag = PACKED_MACC (s_ag, d_ag, inv_sa);
		d_rb = PACKED_MACC (s_rb, d_rb, inv_sa);

		*dst++ = COMBINE_ARGB(d_ag, d_rb);
	}
#else
	while (len-- > 0)
	{
		UINT32 s = *src++;

		int sa = UNPACK_A(s);
		if (sa == 0)
		{
			dst++;
			continue;
		}

		sa *= mask;
		sa >>= 8;
		if (sa == 0)
		{
			dst++;
			continue;
		}

		UINT32 d = *dst;
		int da = UNPACK_A(d);
		if (da == 0)
		{
			*dst++ = (sa << ALPHA_SHIFT) | (s & RGB_MASK);
			continue;
		}

		if (da == 255)
		{
			UINT32 dag = EXTRACT_HI(d);
			UINT32 drb = EXTRACT_LO(d);

			UINT32 sag = EXTRACT_HI(s);
			UINT32 srb = EXTRACT_LO(s);

			dag = PACKED_MACC(dag, sag - dag, sa);
			drb = PACKED_MACC(drb, srb - drb, sa);

			d = COMBINE_RGB(dag, drb);
		}
		else
		{
			int dr = UNPACK_R(d);
			int dg = UNPACK_G(d);
			int db = UNPACK_B(d);

			int sr = UNPACK_R(s);
			int sg = UNPACK_G(s);
			int sb = UNPACK_B(s);

			dr *= da;
			dg *= da;
			db *= da;
			da += sa-((da*sa)>>8);
			dr = ((dr + (sa*(sr-(dr>>8))))/da)&0xff;
			dg = ((dg + (sa*(sg-(dg>>8))))/da)&0xff;
			db = ((db + (sa*(sb-(db>>8))))/da)&0xff;
			if (da > 0xff)
				da = 0xff;
			OP_ASSERT(da >= 0);
			d = PACK_ARGB(da, dr, dg, db);
		}
		*dst++ = d;
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOverInPM(Pointer dp, Pointer sp, UINT8 mask, unsigned len)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	CompositeOverIn(dp, sp, mask, len);
#else
	UINT32* dst = dp.rgba;
	const UINT32* src = sp.rgba;
	UINT32 m = mask;
	m += 1;
	while (len-- > 0)
	{
		UINT32 s = *src++;

		int sa = UNPACK_A(s);
		if (sa == 0)
		{
			dst++;
			continue;
		}

		sa *= m;
		sa >>= 8;
		if (sa == 0)
		{
			dst++;
			continue;
		}
		s = UnpremultiplyFast(s);

		UINT32 d = *dst;
		int da = UNPACK_A(d);
		if (da == 0)
		{
			*dst++ = (sa << ALPHA_SHIFT) | (s & RGB_MASK);
			continue;
		}

		if (da == 255)
		{
			UINT32 dag = EXTRACT_HI(d);
			UINT32 drb = EXTRACT_LO(d);

			UINT32 sag = EXTRACT_HI(s);
			UINT32 srb = EXTRACT_LO(s);

			dag = PACKED_MACC(dag, sag - dag, sa);
			drb = PACKED_MACC(drb, srb - drb, sa);

			d = COMBINE_RGB(dag, drb);
		}
		else
		{
			int dr = UNPACK_R(d);
			int dg = UNPACK_G(d);
			int db = UNPACK_B(d);

			int sr = UNPACK_R(s);
			int sg = UNPACK_G(s);
			int sb = UNPACK_B(s);

			dr *= da;
			dg *= da;
			db *= da;
			da += sa-((da*sa)>>8);
			dr = ((dr + (sa*(sr-(dr>>8))))/da)&0xff;
			dg = ((dg + (sa*(sg-(dg>>8))))/da)&0xff;
			db = ((db + (sa*(sb-(db>>8))))/da)&0xff;
			if (da > 0xff)
				da = 0xff;
			OP_ASSERT(da >= 0);
			d = PACK_ARGB(da, dr, dg, db);
		}
		*dst++ = d;
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOver(Pointer dp, VEGA_PIXEL s, unsigned len)
{
	VEGA_DISPATCH(CompConstOver, (dp.rgba, s, len));
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompConstOver(UINT32* dp, UINT32 s, unsigned len)
{
	UINT32* dst = dp;
#ifdef USE_PREMULTIPLIED_ALPHA
	if (s == 0)
		return;

	UINT32 sa = UNPACK_A(s);
	if (sa == 255)
	{
		while (len-- > 0)
			*dst++ = s;
	}
	else
	{
		UINT32 s_ag = EXTRACT_HI(s);
		UINT32 s_rb = EXTRACT_LO(s);
		UINT32 inv_sa = 256 - sa;

		while (len-- > 0)
		{
			UINT32 d = *dst;
			if (d == 0)
			{
				*dst++ = s;
				continue;
			}

			UINT32 d_ag = EXTRACT_HI(d);
			UINT32 d_rb = EXTRACT_LO(d);

			d_ag = PACKED_MACC (s_ag, d_ag, inv_sa);
			d_rb = PACKED_MACC (s_rb, d_rb, inv_sa);

			*dst++ = COMBINE_ARGB(d_ag, d_rb);
		}
	}
#else
	int sa = UNPACK_A(s);
	if (sa == 0)
		return;

	if (sa == 255)
	{
		while (len-- > 0)
			*dst++ = s;
	}
	else
	{
		int sr = UNPACK_R(s);
		int sg = UNPACK_G(s);
		int sb = UNPACK_B(s);
		UINT32 sag = EXTRACT_HI(s);
		UINT32 srb = EXTRACT_LO(s);

		while (len-- > 0)
		{
			UINT32 d = *dst;
			int da = UNPACK_A(d);
			if (da == 0)
			{
				*dst++ = s;
				continue;
			}

			if (da == 255)
			{
				UINT32 dag = EXTRACT_HI(d);
				UINT32 drb = EXTRACT_LO(d);

				dag = PACKED_MACC(dag, sag - dag, sa);
				drb = PACKED_MACC(drb, srb - drb, sa);

				d = COMBINE_RGB(dag, drb);
			}
			else
			{
				int dr = UNPACK_R(d);
				int dg = UNPACK_G(d);
				int db = UNPACK_B(d);

				dr *= da;
				dg *= da;
				db *= da;
				da += sa-((da*sa)>>8);
				dr = ((dr + (sa*(sr-(dr>>8))))/da)&0xff;
				dg = ((dg + (sa*(sg-(dg>>8))))/da)&0xff;
				db = ((db + (sa*(sb-(db>>8))))/da)&0xff;
				if (da > 0xff)
					da = 0xff;
				OP_ASSERT(da >= 0);
				d = PACK_ARGB(da, dr, dg, db);
			}
			*dst++ = d;
		}
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOverIn(Pointer dp, VEGA_PIXEL s, const UINT8* mask, unsigned len)
{
	VEGA_DISPATCH(CompConstOverMask, (dp.rgba, s, mask, len));
}

template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline void
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompConstOverMask(UINT32* dp, UINT32 s, const UINT8* mask, unsigned len)
{
	UINT32* dst = dp;
#ifdef USE_PREMULTIPLIED_ALPHA
	if (s == 0)
		return;

	UINT32 sa = UNPACK_A(s);
	UINT32 s_ag = EXTRACT_HI(s);
	UINT32 s_rb = EXTRACT_LO(s);
	UINT32 inv_sa = 256 - sa;

	while (len-- > 0)
	{
		UINT32 m = *mask++;
		if (m == 0)
		{
			dst++;
			continue;
		}

		UINT32 ms_ag, ms_rb, inv_msa;
		if (m == 255)
		{
			if (sa == 255 || *dst == 0)
			{
				*dst++ = s;
				continue;
			}

			ms_ag = s_ag;
			ms_rb = s_rb;
			inv_msa = inv_sa;
		}
		else // 0 < m < 255
		{
			ms_ag = s_ag * (m+1);
			ms_rb = s_rb * (m+1);

			ms_ag &= 0xff00ff00;
			ms_rb &= 0xff00ff00;
			ms_rb >>= 8;

			if (*dst == 0)
			{
				*dst++ = ms_ag | ms_rb;
				continue;
			}

			ms_ag >>= 8;

			// This rather ugly bit expects the compiler to optimize away
			// the if-else mess since it's all constant expressions.
			if (ALPHA_SHIFT == 24)
				inv_msa = 256 - (ms_ag >> 16);
			else if (ALPHA_SHIFT == 0)
				inv_msa = 256 - (ms_rb & 0xff);
			else
				OP_ASSERT(FALSE && "Your pixel format is on crack!");
		}

		UINT32 d = *dst;

		UINT32 d_ag = EXTRACT_HI(d);
		UINT32 d_rb = EXTRACT_LO(d);

		d_ag = PACKED_MACC (ms_ag, d_ag, inv_msa);
		d_rb = PACKED_MACC (ms_rb, d_rb, inv_msa);

		*dst++ = COMBINE_ARGB(d_ag, d_rb);
	}
#else
	int sa = UNPACK_A(s);
	if (sa == 0)
		return;

	int sr = UNPACK_R(s);
	int sg = UNPACK_G(s);
	int sb = UNPACK_B(s);
	UINT32 sc = s & RGB_MASK;

	UINT32 sag = EXTRACT_HI(s);
	UINT32 srb = EXTRACT_LO(s);

	if (sa == 255)
	{
		while (len-- > 0)
		{
			UINT32 m = *mask++;
			if (m == 0)
			{
				dst++;
				continue;
			}

			if (m == 255)
			{
				*dst++ = s;
				continue;
			}

			UINT32 d = *dst;
			int da = UNPACK_A(d);
			if (da == 0)
			{
				*dst++ = (m << ALPHA_SHIFT) | sc;
				continue;
			}

			if (da == 255)
			{
				UINT32 dag = EXTRACT_HI(d);
				UINT32 drb = EXTRACT_LO(d);

				dag = PACKED_MACC(dag, sag - dag, m);
				drb = PACKED_MACC(drb, srb - drb, m);

				d = COMBINE_RGB(dag, drb);
			}
			else
			{
				int dr = UNPACK_R(d);
				int dg = UNPACK_G(d);
				int db = UNPACK_B(d);

				dr *= da;
				dg *= da;
				db *= da;
				da += m-((da*m)>>8);
				dr = ((dr + (m*(sr-(dr>>8))))/da)&0xff;
				dg = ((dg + (m*(sg-(dg>>8))))/da)&0xff;
				db = ((db + (m*(sb-(db>>8))))/da)&0xff;
				if (da > 0xff)
					da = 0xff;
				OP_ASSERT(da >= 0);
				d = PACK_ARGB(da, dr, dg, db);
			}
			*dst++ = d;
		}
	}
	else
	{
		while (len-- > 0)
		{
			UINT32 m = *mask++;

			if (m == 0)
			{
				dst++;
				continue;
			}

			int msa = sa;
			if (m < 255)
			{
				msa *= m+1;
				msa >>= 8;
				if (msa == 0)
				{
					dst++;
					continue;
				}
			}

			UINT32 d = *dst;
			int da = UNPACK_A(d);
			if (da == 0)
			{
				*dst++ = (msa << ALPHA_SHIFT) | sc;
				continue;
			}

			if (da == 255)
			{
				UINT32 dag = EXTRACT_HI(d);
				UINT32 drb = EXTRACT_LO(d);

				dag = PACKED_MACC(dag, sag - dag, msa);
				drb = PACKED_MACC(drb, srb - drb, msa);

				d = COMBINE_RGB(dag, drb);
			}
			else
			{
				int dr = UNPACK_R(d);
				int dg = UNPACK_G(d);
				int db = UNPACK_B(d);

				dr *= da;
				dg *= da;
				db *= da;
				da += msa-((da*msa)>>8);
				dr = ((dr + (msa*(sr-(dr>>8))))/da)&0xff;
				dg = ((dg + (msa*(sg-(dg>>8))))/da)&0xff;
				db = ((db + (msa*(sb-(db>>8))))/da)&0xff;
				if (da > 0xff)
					da = 0xff;
				OP_ASSERT(da >= 0);
				d = PACK_ARGB(da, dr, dg, db);
			}
			*dst++ = d;
		}
	}
#endif // USE_PREMULTIPLIED_ALPHA
}

// r = (src=[sa,sr,sg,sb]) over dst, where src is always unassociated (and 8bits/component)
template <int RED_SHIFT, int GREEN_SHIFT, int BLUE_SHIFT, int ALPHA_SHIFT>
op_force_inline VEGA_PIXEL
VEGAPixelFormat_8888<RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT, ALPHA_SHIFT>::CompositeOverUnpacked(VEGA_PIXEL dst, int sa, int sr, int sg, int sb)
{
	if (sa == 255)
		return PACK_ARGB(sa, sr, sg, sb);

	if (sa == 0)
		return dst;

#ifdef USE_PREMULTIPLIED_ALPHA
	// Premultiply
	UINT32 src_rb = (sr << 16) | sb;

	src_rb *= sa+1;
	sg *= sa+1;

	src_rb >>= 8;
	sg >>= 8;

	src_rb &= 0xff00ff;

	// Apply over operator
	UINT32 inv_sa = 256 - sa;
	UINT32 src_ag = (sa << 16) | sg;

	UINT32 dst_ag = (UNPACK_A(dst) << 16) | UNPACK_G(dst);
	UINT32 dst_rb = (UNPACK_R(dst) << 16) | UNPACK_B(dst);

	dst_ag = PACKED_MACC(src_ag, dst_ag, inv_sa);
	dst_rb = PACKED_MACC(src_rb, dst_rb, inv_sa);

	return PACK_ARGB(dst_ag >> 24, dst_rb >> 24, (dst_ag >> 8) & 0xff, (dst_rb >> 8) & 0xff);
#else
	int da = UNPACK_A(dst);
	if (da == 0)
		return PACK_ARGB(sa, sr, sg, sb);

	int dr = UNPACK_R(dst);
	int dg = UNPACK_G(dst);
	int db = UNPACK_B(dst);
	if (da == 255)
	{
		UINT32 dst_rb = (dr<<16) | db;
		UINT32 src_rb = (sr<<16) | sb;
		dst_rb = (dst_rb + ((sa*(src_rb-dst_rb))>>8))&0xff00ff;
		dg = (dg + ((sa*(sg-dg))>>8))&0xff;
		return PACK_RGB(dst_rb >> 16, dg, dst_rb & 0xff);
	}
	else
	{
		dr *= da;
		dg *= da;
		db *= da;
		da += sa-((da*sa)>>8);
		dr = ((dr + (sa*(sr-(dr>>8))))/da)&0xff;
		dg = ((dg + (sa*(sg-(dg>>8))))/da)&0xff;
		db = ((db + (sa*(sb-(db>>8))))/da)&0xff;
		if (da > 0xff)
			da = 0xff;
		OP_ASSERT(da >= 0 && da <= 0xff);
		return PACK_ARGB(da, dr, dg, db);
	}
#endif // !USE_PREMULTIPLIED_ALPHA
}

typedef VEGAPixelFormat_8888< 0,  8, 16, 24> VEGAPixelFormat_RGBA8888;
typedef VEGAPixelFormat_8888<16,  8,  0, 24> VEGAPixelFormat_BGRA8888;

#undef RED_MASK
#undef GREEN_MASK
#undef BLUE_MASK
#undef ALPHA_MASK

#undef RGB_MASK

#undef EXTRACT_HI
#undef EXTRACT_LO

#undef COMBINE_ARGB
#undef COMBINE_RGB

#undef UNPACK_R
#undef UNPACK_G
#undef UNPACK_B
#undef UNPACK_A

#undef PACK_RGB
#undef PACK_ARGB

#undef VEGA_DISPATCH
