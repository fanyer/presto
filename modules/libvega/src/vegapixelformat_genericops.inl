//
// Generic implementations of composite operators and bilinear interpolation
//
// To use, define LOCAL_PXCLASS appropriately, and define one or more of:
//
//    PXGENOPS_USE_MODULATE			Modulate pixel
//    PXGENOPS_USE_OVER_1			1-pixel 'over'
//    PXGENOPS_USE_OVER_N			N-pixel 'over'
//    PXGENOPS_USE_OVER_UNPACKED_1  1-pixel unpacked source 'over'
//    PXGENOPS_USE_OVERIN_1			1-pixel 'in' + 'over' ("masked over")
//    PXGENOPS_USE_OVERIN_N			N-pixel 'in' + 'over'
//    PXGENOPS_USE_OVERCIN_N		N-pixel 'in' with constant mask ("opacity")
//    PXGENOPS_USE_OVER_CSRC_N		N-pixel 'over' with constant source
//    PXGENOPS_USE_OVERIN_CSRC_N	N-pixel 'in' + 'over' with constant source
//
//    PXGENOPS_USE_SAMPLER_BILERP	Bilinear interpolation
//    PXGENOPS_USE_SAMPLER_LERPXY	Bilinear interpolation - separate X/Y
//

#ifdef PXGENOPS_USE_MODULATE
inline VEGA_PIXEL
LOCAL_PXCLASS::Modulate(VEGA_PIXEL s, UINT32 m)
{
	if (m == 0)
		return 0;
	if (m == 255)
		return s;

	unsigned sa = UnpackA(s);
	unsigned sr = UnpackR(s);
	unsigned sg = UnpackG(s);
	unsigned sb = UnpackB(s);

	++m;

	sa = (sa * m) >> 8;
#ifdef USE_PREMULTIPLIED_ALPHA
	sr = (sr * m) >> 8;
	sg = (sg * m) >> 8;
	sb = (sb * m) >> 8;
#endif // USE_PREMULTIPLIED_ALPHA
	return PackARGB(sa, sr, sg, sb);
}
#endif // PXGENOPS_USE_MODULATE

#ifdef PXGENOPS_USE_OVER_1
inline VEGA_PIXEL
LOCAL_PXCLASS::CompositeOver(VEGA_PIXEL d, VEGA_PIXEL s)
{
	unsigned sa = UnpackA(s);
	if (sa == 255)
		return s;
	if (sa == 0)
		return d;

	unsigned da = UnpackA(d);
	if (da == 0)
		return s;

	unsigned sr = UnpackR(s);
	unsigned sg = UnpackG(s);
	unsigned sb = UnpackB(s);

	unsigned dr = UnpackR(d);
	unsigned dg = UnpackG(d);
	unsigned db = UnpackB(d);

#ifndef USE_PREMULTIPLIED_ALPHA
	if (da < 255)
	{
		dr = (dr * (da+1)) >> 8;
		dg = (dg * (da+1)) >> 8;
		db = (db * (da+1)) >> 8;
	}

	if (sa < 255)
	{
		sr = (sr * (sa+1)) >> 8;
		sg = (sg * (sa+1)) >> 8;
		sb = (sb * (sa+1)) >> 8;
	}
#endif // !USE_PREMULTIPLIED_ALPHA

	unsigned inv_sa = 256 - sa;

	dr = sr + ((inv_sa * dr) >> 8);
	dg = sg + ((inv_sa * dg) >> 8);
	db = sb + ((inv_sa * db) >> 8);
	da = sa + ((inv_sa * da) >> 8);

#ifndef USE_PREMULTIPLIED_ALPHA
	if (da == 0)
	{
		dr = dg = db = 0;
	}
	else if (da < 255)
	{
		dr = 255 * dr / da;
		dg = 255 * dg / da;
		db = 255 * db / da;
	}
#endif // !USE_PREMULTIPLIED_ALPHA

	return PackARGB(da, dr, dg, db);
}
#endif // PXGENOPS_USE_OVER_1

#ifdef PXGENOPS_USE_OVERIN_1
inline VEGA_PIXEL
LOCAL_PXCLASS::CompositeOverIn(VEGA_PIXEL d, VEGA_PIXEL s, UINT32 m)
{
	return CompositeOver(d, Modulate(s, m));
}
#endif // PXGENOPS_USE_OVERIN_1

#ifdef PXGENOPS_USE_OVER_UNPACKED_1
inline VEGA_PIXEL
LOCAL_PXCLASS::CompositeOverUnpacked(VEGA_PIXEL dst, int sa, int sr, int sg, int sb)
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
#endif // USE_PREMULTIPLIED_ALPHA

	return CompositeOver(dst, PackARGB(sa, sr, sg, sb));
}
#endif // PXGENOPS_USE_OVER_UNPACKED_1

#ifdef PXGENOPS_USE_OVER_N
inline void
LOCAL_PXCLASS::CompositeOver(Pointer dp, Pointer sp, unsigned len)
{
	Accessor dst(dp);
	ConstAccessor src(sp);

	while (len-- > 0)
	{
		dst.Store(CompositeOver(dst.Load(), src.Load()));

		src++;
		dst++;
	}
}
inline void
LOCAL_PXCLASS::CompositeOverPM(Pointer dp, Pointer sp, unsigned len)
{
	Accessor dst(dp);
	ConstAccessor src(sp);

	while (len-- > 0)
	{
#ifdef USE_PREMULTIPLIED_ALPHA
		dst.Store(CompositeOver(dst.Load(), src.Load()));
#else
		dst.Store(CompositeOver(dst.Load(), UnpremultiplyFast(src.Load())));
#endif

		src++;
		dst++;
	}
}
#endif // PXGENOPS_USE_OVER_N

#ifdef PXGENOPS_USE_OVERIN_N
inline void
LOCAL_PXCLASS::CompositeOverIn(Pointer dp, Pointer sp, const UINT8* mask, unsigned len)
{
	Accessor dst(dp);
	ConstAccessor src(sp);

	while (len-- > 0)
	{
		dst.Store(CompositeOverIn(dst.Load(), src.Load(), *mask));

		mask++;
		src++;
		dst++;
	}
}
#endif // PXGENOPS_USE_OVERIN_N

#ifdef PXGENOPS_USE_OVERCIN_N
inline void
LOCAL_PXCLASS::CompositeOverIn(Pointer dp, Pointer sp, UINT8 mask, unsigned len)
{
	Accessor dst(dp);
	ConstAccessor src(sp);

	while (len-- > 0)
	{
		dst.Store(CompositeOverIn(dst.Load(), src.Load(), mask));

		src++;
		dst++;
	}
}
inline void
LOCAL_PXCLASS::CompositeOverInPM(Pointer dp, Pointer sp, UINT8 mask, unsigned len)
{
	Accessor dst(dp);
	ConstAccessor src(sp);

	while (len-- > 0)
	{
#ifdef USE_PREMULTIPLIED_ALPHA
		dst.Store(CompositeOverIn(dst.Load(), src.Load(), mask));
#else
		dst.Store(CompositeOverIn(dst.Load(), UnpremultiplyFast(src.Load()), mask));
#endif

		src++;
		dst++;
	}
}
#endif // PXGENOPS_USE_OVERCIN_N

#ifdef PXGENOPS_USE_OVER_CSRC_N
inline void
LOCAL_PXCLASS::CompositeOver(Pointer dp, VEGA_PIXEL src, unsigned len)
{
	Accessor dst(dp);

	while (len-- > 0)
	{
		dst.Store(CompositeOver(dst.Load(), src));

		dst++;
	}
}
#endif // PXGENOPS_USE_OVER_CSRC_N

#ifdef PXGENOPS_USE_OVERIN_CSRC_N
inline void
LOCAL_PXCLASS::CompositeOverIn(Pointer dp, VEGA_PIXEL src, const UINT8* mask, unsigned len)
{
	Accessor dst(dp);

	while (len-- > 0)
	{
		dst.Store(CompositeOverIn(dst.Load(), src, *mask));

		mask++;
		dst++;
	}
}
#endif // PXGENOPS_USE_OVERIN_CSRC_N

#ifdef PXGENOPS_USE_SAMPLER_LERPXY
inline void
LOCAL_PXCLASS::SamplerLerpXPM(Pointer dstdata, const Pointer& srcdata, INT32 csx, INT32 cdx, unsigned dstlen, unsigned srclen)
{
	ConstAccessor src(srcdata);
	Accessor dst(dstdata);

	while (dstlen && csx < 0)
	{
		dst.Store(src.LoadRel(0));

		++dst;
		csx += cdx;
		dstlen--;
	}
	while (dstlen > 0 && csx < VEGA_INTTOSAMPLE(srclen-1))
	{
		INT32 int_px = VEGA_SAMPLE_INT(csx);
		INT32 frc_px = VEGA_SAMPLE_FRAC(csx);

		VEGA_PIXEL c = src.LoadRel(int_px);
		if (frc_px)
		{
			int c0a = UnpackA(c);
			int c0r = UnpackR(c);
			int c0g = UnpackG(c);
			int c0b = UnpackB(c);

			VEGA_PIXEL nc = src.LoadRel(int_px + 1);
		
			c0a += (((UnpackA(nc) - c0a) * frc_px) >> 8);
			c0r += (((UnpackR(nc) - c0r) * frc_px) >> 8);
			c0g += (((UnpackG(nc) - c0g) * frc_px) >> 8);
			c0b += (((UnpackB(nc) - c0b) * frc_px) >> 8);

			c = PackARGB(c0a, c0r, c0g, c0b);
		}

		dst.Store(c);

		++dst;
		csx += cdx;
		dstlen--;  
	}
	while (dstlen > 0)
	{
		dst.Store(src.LoadRel(srclen-1));

		++dst;
		csx += cdx;
		dstlen--;
	}
}

inline void
LOCAL_PXCLASS::SamplerLerpYPM(Pointer dstdata, const Pointer& srcdata1, const Pointer& srcdata2, INT32 frc_y, unsigned len)
{
	ConstAccessor src1(srcdata1);
	ConstAccessor src2(srcdata2);
	Accessor dst(dstdata);

	while (len-- > 0)
	{
		int c0a, c0r, c0g, c0b;
		src1.LoadUnpack(c0a, c0r, c0g, c0b);

		int c1a, c1r, c1g, c1b;
		src2.LoadUnpack(c1a, c1r, c1g, c1b);
		
		c0a += (((c1a - c0a) * frc_y) >> 8);
		c0r += (((c1r - c0r) * frc_y) >> 8);
		c0g += (((c1g - c0g) * frc_y) >> 8);
		c0b += (((c1b - c0b) * frc_y) >> 8);

		VEGA_PIXEL curr = PackARGB(c0a, c0r, c0g, c0b);

		dst.Store(curr);

		++dst;
		++src1;
		++src2;
	}
}
#endif // PXGENOPS_USE_SAMPLER_LERPXY

#ifdef PXGENOPS_USE_SAMPLER_BILERP
inline VEGA_PIXEL
LOCAL_PXCLASS::SamplerBilerpPM(const Pointer& data, unsigned dataPixelStride,
							   INT32 csx, INT32 csy)
{
	ConstAccessor samp(data + VEGA_SAMPLE_INT(csy) * dataPixelStride + VEGA_SAMPLE_INT(csx));

	int weight2 = VEGA_SAMPLE_FRAC(csx);
	int yweight2 = VEGA_SAMPLE_FRAC(csy);

	VEGA_PIXEL curr;
	if (weight2)
	{
		// Sample from current line
		int c0a, c0r, c0g, c0b;
		samp.LoadUnpack(c0a, c0r, c0g, c0b);

		samp++;

		int c1a, c1r, c1g, c1b;
		samp.LoadUnpack(c1a, c1r, c1g, c1b);
		
		c0a += (((c1a - c0a) * weight2) >> 8);
		c0r += (((c1r - c0r) * weight2) >> 8);
		c0g += (((c1g - c0g) * weight2) >> 8);
		c0b += (((c1b - c0b) * weight2) >> 8);

		if (yweight2)
		{
			samp += dataPixelStride - 1;

			// Sample from next line
			int c2a, c2r, c2g, c2b;
			samp.LoadUnpack(c2a, c2r, c2g, c2b);

			samp++;

			int c3a, c3r, c3g, c3b;
			samp.LoadUnpack(c3a, c3r, c3g, c3b);

			c2a += (((c3a - c2a) * weight2) >> 8);
			c2r += (((c3r - c2r) * weight2) >> 8);
			c2g += (((c3g - c2g) * weight2) >> 8);
			c2b += (((c3b - c2b) * weight2) >> 8);

			// Merge line samples
			c0a += (((c2a - c0a) * yweight2) >> 8);
			c0r += (((c2r - c0r) * yweight2) >> 8);
			c0g += (((c2g - c0g) * yweight2) >> 8);
			c0b += (((c2b - c0b) * yweight2) >> 8);
		}

		curr = PackARGB(c0a, c0r, c0g, c0b);
	}
	else if (yweight2)
	{
		// Sample from column
		int c0a, c0r, c0g, c0b;
		samp.LoadUnpack(c0a, c0r, c0g, c0b);

		samp += dataPixelStride;

		int c2a, c2r, c2g, c2b;
		samp.LoadUnpack(c2a, c2r, c2g, c2b);

		c0a += (((c2a - c0a) * yweight2) >> 8);
		c0r += (((c2r - c0r) * yweight2) >> 8);
		c0g += (((c2g - c0g) * yweight2) >> 8);
		c0b += (((c2b - c0b) * yweight2) >> 8);

		curr = PackARGB(c0a, c0r, c0g, c0b);
	}
	else
	{
		curr = samp.Load();
	}

#ifndef USE_PREMULTIPLIED_ALPHA
	curr = UnpremultiplyFast(curr);
#endif // !USE_PREMULTIPLIED_ALPHA
	return curr;
}
#endif // PXGENOPS_USE_SAMPLER_BILERP
