/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegafiltermerge.h"
#include "modules/libvega/src/vegapixelformat.h"
#include "modules/libvega/src/vegacomposite.h"
#include "modules/libvega/src/vegabackend.h"
#ifdef VEGA_3DDEVICE
#include "modules/libvega/src/vegabackend_hw3d.h"
#endif // VEGA_3DDEVICE

VEGAFilterMerge::VEGAFilterMerge() : mergeType(VEGAMERGE_NORMAL), opacity(255)
{
}
VEGAFilterMerge::VEGAFilterMerge(VEGAMergeType mt) : mergeType(mt), opacity(255)
{
}

#define VEGA_CLAMP_U8(v) (((v) > 255) ? 255 : (((v) < 0) ? 0 : (v)))
#define PREMULTIPLY(a,r,g,b) \
{ \
(r) = ((r) * ((a) + 1)) >> 8;\
(g) = ((g) * ((a) + 1)) >> 8;\
(b) = ((b) * ((a) + 1)) >> 8;\
}

#ifdef USE_PREMULTIPLIED_ALPHA
static inline VEGA_PIXEL clamp_and_pack_rgba(int da, int dr, int dg, int db)
{
	dr = VEGA_CLAMP_U8(dr);
	dg = VEGA_CLAMP_U8(dg);
	db = VEGA_CLAMP_U8(db);
	return VEGA_PACK_ARGB(da, dr, dg, db);
}
#define CLAMP_AND_PACK(a,r,g,b) clamp_and_pack_rgba(a,r,g,b)
#else
static inline VEGA_PIXEL unpremult_clamp_and_pack_rgba(int da, int dr, int dg, int db)
{
	if (da == 0)
		return 0;

	if (da >= 0xff)
	{
		dr = VEGA_CLAMP_U8(dr);
		dg = VEGA_CLAMP_U8(dg);
		db = VEGA_CLAMP_U8(db);
		return VEGA_PACK_RGB(dr, dg, db);
	}

	dr = (dr*255)/da;
	dg = (dg*255)/da;
	db = (db*255)/da;

	dr = VEGA_CLAMP_U8(dr);
	dg = VEGA_CLAMP_U8(dg);
	db = VEGA_CLAMP_U8(db);
	return VEGA_PACK_ARGB(da, dr, dg, db);
}
#define CLAMP_AND_PACK(a,r,g,b) unpremult_clamp_and_pack_rgba(a,r,g,b)
#endif // USE_PREMULTIPLIED_ALPHA

//
// Legend:
// qr = Result opacity value
// cr = Result color (RGB) - premultiplied
// qa = Opacity value at a given pixel for image A
// qb = Opacity value at a given pixel for image B
// ca = Color (RGB) at a given pixel for image A - premultiplied
// cb = Color (RGB) at a given pixel for image B - premultiplied
//

//
// Arithmetic (SVG: http://www.w3.org/TR/SVG11/filters.html#feComposite)
//
// cr = k1 * ca * cb + k2 * ca + k3 * cb + k4
// qr = k1 * qa * qb + k2 * qa + k3 * qb + k4
//
void VEGAFilterMerge::mergeArithmetic(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride() - dstbuf.width;
	unsigned dstPixelStride = dstbuf.GetPixelStride() - dstbuf.width;

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		unsigned cnt = dstbuf.width;
		while (cnt-- > 0)
		{
			int sa, sr, sg, sb;
			src.LoadUnpack(sa, sr, sg, sb);

			int da, dr, dg, db;
			dst.LoadUnpack(da, dr, dg, db);

#ifndef USE_PREMULTIPLIED_ALPHA
			PREMULTIPLY(da, dr, dg, db);
			PREMULTIPLY(sa, sr, sg, sb);
#endif // !USE_PREMULTIPLIED_ALPHA

			VEGA_FIX fda = k1*((da*sa)/255) + k2*sa + k3*da + k4*255;
			VEGA_FIX fdr = k1*((dr*sr)/255) + k2*sr + k3*dr + k4*255;
			VEGA_FIX fdg = k1*((dg*sg)/255) + k2*sg + k3*dg + k4*255;
			VEGA_FIX fdb = k1*((db*sb)/255) + k2*sb + k3*db + k4*255;

			fda = (fda > VEGA_INTTOFIX(255)) ? VEGA_INTTOFIX(255) :
				(fda < 0) ? 0 : fda;
			da = VEGA_FIXTOINT(fda);
			if (da)
			{
#ifndef USE_PREMULTIPLIED_ALPHA
				VEGA_FIX mf = VEGA_FIXDIV(VEGA_INTTOFIX(255), fda);
				dr = VEGA_FIXTOINT(VEGA_FIXMUL(fdr, mf));
				dg = VEGA_FIXTOINT(VEGA_FIXMUL(fdg, mf));
				db = VEGA_FIXTOINT(VEGA_FIXMUL(fdb, mf));
#else
				dr = VEGA_FIXTOINT(fdr);
				dg = VEGA_FIXTOINT(fdg);
				db = VEGA_FIXTOINT(fdb);
#endif // !USE_PREMULTIPLIED_ALPHA

				dr = VEGA_CLAMP_U8(dr);
				dg = VEGA_CLAMP_U8(dg);
				db = VEGA_CLAMP_U8(db);

				dst.StoreARGB(da, dr, dg, db);
			}
			else
				dst.Store(0);

			++src;
			++dst;
		}
		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// Multiply (SVG: http://www.w3.org/TR/SVG11/filters.html#feBlend)
//
// cr = (1 - qa) * cb + (1 - qb) * ca + ca * cb
// qr = 1 - (1-qa)*(1-qb)
//
void VEGAFilterMerge::mergeMultiply(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride() - dstbuf.width;
	unsigned dstPixelStride = dstbuf.GetPixelStride() - dstbuf.width;

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		unsigned cnt = dstbuf.width;
		while (cnt-- > 0)
		{
#ifdef USE_PREMULTIPLIED_ALPHA
			int sa, sr, sg, sb;
			src.LoadUnpack(sa, sr, sg, sb);

			int da, dr, dg, db;
			dst.LoadUnpack(da, dr, dg, db);

			dr = dr + sr + (((dr*sr) - (dr*sa) - (sr*da)) >> 8);
			dg = dg + sg + (((dg*sg) - (dg*sa) - (sg*da)) >> 8);
			db = db + sb + (((db*sb) - (db*sa) - (sb*da)) >> 8);
			da = sa + da - ((sa*da) >> 8);

			da = VEGA_CLAMP_U8(da);

			dst.Store(CLAMP_AND_PACK(da, dr, dg, db));
#else
			int sa, sr, sg, sb;
			src.LoadUnpack(sa, sr, sg, sb);

			int da, dr, dg, db;
			dst.LoadUnpack(da, dr, dg, db);

			if (sa >= 0xff)
			{
				if (da >= 0xff)
				{
					dr = ((dr * sr) >> 8);
					dg = ((dg * sg) >> 8);
					db = ((db * sb) >> 8);
				}
				else
				{
					dr = sr + ((sr*dr*da/255 - sr*da) >> 8);
					dg = sg + ((sg*dg*da/255 - sg*da) >> 8);
					db = sb + ((sb*db*da/255 - sb*da) >> 8);

					dr = VEGA_CLAMP_U8(dr);
					dg = VEGA_CLAMP_U8(dg);
					db = VEGA_CLAMP_U8(db);
				}
				dst.StoreRGB(dr, dg, db);
			}
			else
			{
				VEGA_PIXEL d;
				if (da >= 0xff)
				{
					dr = dr + ((dr*sr*sa/255 - dr*sa) >> 8);
					dg = dg + ((dg*sg*sa/255 - dg*sa) >> 8);
					db = db + ((db*sb*sa/255 - db*sa) >> 8);

					dr = VEGA_CLAMP_U8(dr);
					dg = VEGA_CLAMP_U8(dg);
					db = VEGA_CLAMP_U8(db);

					d = VEGA_PACK_RGB(dr, dg, db);
				}
				else
				{
					PREMULTIPLY(sa, sr, sg, sb);
					PREMULTIPLY(da, dr, dg, db);

					dr = dr + sr + (((dr*sr) - (dr*sa) - (sr*da)) >> 8);
					dg = dg + sg + (((dg*sg) - (dg*sa) - (sg*da)) >> 8);
					db = db + sb + (((db*sb) - (db*sa) - (sb*da)) >> 8);
					da = sa + da - ((sa*da) >> 8);

					da = VEGA_CLAMP_U8(da);

					d = CLAMP_AND_PACK(da, dr, dg, db);
				}
				dst.Store(d);
			}
#endif // USE_PREMULTIPLIED_ALPHA

			++src;
			++dst;
		}
		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// Screen (SVG: http://www.w3.org/TR/SVG11/filters.html#feBlend)
//
// cr = cb + ca - ca * cb
// qr = 1 - (1-qa)*(1-qb)
//
void VEGAFilterMerge::mergeScreen(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride() - dstbuf.width;
	unsigned dstPixelStride = dstbuf.GetPixelStride() - dstbuf.width;

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		unsigned cnt = dstbuf.width;
		while (cnt-- > 0)
		{
			int sa, sr, sg, sb;
			src.LoadUnpack(sa, sr, sg, sb);

			int da, dr, dg, db;
			dst.LoadUnpack(da, dr, dg, db);

#ifndef USE_PREMULTIPLIED_ALPHA
			PREMULTIPLY(sa, sr, sg, sb);
			PREMULTIPLY(da, dr, dg, db);
#endif // !USE_PREMULTIPLIED_ALPHA

			dr = dr + sr - ((dr*sr) >> 8);
			dg = dg + sg - ((dg*sg) >> 8);
			db = db + sb - ((db*sb) >> 8);
			da = da + sa - ((da*sa) >> 8);

			da = VEGA_CLAMP_U8(da);

			dst.Store(CLAMP_AND_PACK(da, dr, dg, db));

			++src;
			++dst;
		}
		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// Darken (SVG: http://www.w3.org/TR/SVG11/filters.html#feBlend)
//
// cr = Min ((1 - qa) * cb + ca, (1 - qb) * ca + cb)
// qr = 1 - (1-qa)*(1-qb)
//
void VEGAFilterMerge::mergeDarken(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride() - dstbuf.width;
	unsigned dstPixelStride = dstbuf.GetPixelStride() - dstbuf.width;

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		unsigned cnt = dstbuf.width;
		while (cnt-- > 0)
		{
#ifdef USE_PREMULTIPLIED_ALPHA
			unsigned sa, sr, sg, sb;
			src.LoadUnpack(sa, sr, sg, sb);

			unsigned da, dr, dg, db;
			dst.LoadUnpack(da, dr, dg, db);

			unsigned inv_sa = 255 - sa;
			unsigned inv_da = 255 - da;

			dr = MIN(sr + ((inv_sa * dr) >> 8), dr + ((inv_da * sr) >> 8));
			dg = MIN(sg + ((inv_sa * dg) >> 8), dg + ((inv_da * sg) >> 8));
			db = MIN(sb + ((inv_sa * db) >> 8), db + ((inv_da * sb) >> 8));
			da = sa + ((inv_sa * da) >> 8);

			dst.Store(CLAMP_AND_PACK(da, dr, dg, db));
#else
			int sa, sr, sg, sb;
			src.LoadUnpack(sa, sr, sg, sb);

			int da, dr, dg, db;
			dst.LoadUnpack(da, dr, dg, db);

			if (sa >= 0xff)
			{
				VEGA_PIXEL d;
				if (da >= 0xff)
				{
					dr = MIN(dr, sr);
					dg = MIN(dg, sg);
					db = MIN(db, sb);

					d = VEGA_PACK_RGB(dr, dg, db);
				}
				else
				{
					dr = sr + MIN(0, (da*(dr - sr)) >> 8);
					dg = sg + MIN(0, (da*(dg - sg)) >> 8);
					db = sb + MIN(0, (da*(db - sb)) >> 8);

					dr = VEGA_CLAMP_U8(dr);
					dg = VEGA_CLAMP_U8(dg);
					db = VEGA_CLAMP_U8(db);

					d = VEGA_PACK_RGB(dr, dg, db);
				}
				dst.Store(d);
			}
			else if (sa > 0)
			{
				VEGA_PIXEL d;
				if (da >= 0xff)
				{
					dr = dr + MIN(0, (sa*(sr - dr)) >> 8);
					dg = dg + MIN(0, (sa*(sg - dg)) >> 8);
					db = db + MIN(0, (sa*(sb - db)) >> 8);

					dr = VEGA_CLAMP_U8(dr);
					dg = VEGA_CLAMP_U8(dg);
					db = VEGA_CLAMP_U8(db);

					d = VEGA_PACK_RGB(dr, dg, db);
				}
				else
				{
					// General case
					PREMULTIPLY(sa, sr, sg, sb);
					PREMULTIPLY(da, dr, dg, db);

					dr = sr + dr - (MAX(dr*sa, sr*da) >> 8);
					dg = sg + dg - (MAX(dg*sa, sg*da) >> 8);
					db = sb + db - (MAX(db*sa, sb*da) >> 8);
					da = da + sa - ((da*sa) >> 8);

					d = CLAMP_AND_PACK(da, dr, dg, db);
				}
				dst.Store(d);
			}
#endif // USE_PREMULTIPLIED_ALPHA

			++src;
			++dst;
		}
		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// Lighten (SVG: http://www.w3.org/TR/SVG11/filters.html#feBlend)
//
// cr = Max ((1 - qa) * cb + ca, (1 - qb) * ca + cb)
// qr = 1 - (1-qa)*(1-qb)
//
void VEGAFilterMerge::mergeLighten(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride() - dstbuf.width;
	unsigned dstPixelStride = dstbuf.GetPixelStride() - dstbuf.width;

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		unsigned cnt = dstbuf.width;
		while (cnt-- > 0)
		{
#ifdef USE_PREMULTIPLIED_ALPHA
			unsigned sa, sr, sg, sb;
			src.LoadUnpack(sa, sr, sg, sb);

			unsigned da, dr, dg, db;
			dst.LoadUnpack(da, dr, dg, db);

			unsigned inv_sa = 255 - sa;
			unsigned inv_da = 255 - da;

			dr = MAX(sr + ((inv_sa * dr) >> 8), dr + ((inv_da * sr) >> 8));
			dg = MAX(sg + ((inv_sa * dg) >> 8), dg + ((inv_da * sg) >> 8));
			db = MAX(sb + ((inv_sa * db) >> 8), db + ((inv_da * sb) >> 8));
			da = sa + ((inv_sa * da) >> 8);

			dst.Store(CLAMP_AND_PACK(da, dr, dg, db));
#else
			int sa, sr, sg, sb;
			src.LoadUnpack(sa, sr, sg, sb);

			int da, dr, dg, db;
			dst.LoadUnpack(da, dr, dg, db);

			if (sa >= 0xff)
			{
				if (da >= 0xff)
				{
					dr = MAX(dr, sr);
					dg = MAX(dg, sg);
					db = MAX(db, sb);
				}
				else
				{
					dr = sr + MAX(0, (da*(dr - sr)) >> 8);
					dg = sg + MAX(0, (da*(dg - sg)) >> 8);
					db = sb + MAX(0, (da*(db - sb)) >> 8);

					dr = VEGA_CLAMP_U8(dr);
					dg = VEGA_CLAMP_U8(dg);
					db = VEGA_CLAMP_U8(db);
				}

				dst.StoreRGB(dr, dg, db);
			}
			else if (sa > 0)
			{
				VEGA_PIXEL d;
				if (da >= 0xff)
				{
					dr = dr + MAX(0, (sa*(sr - dr)) >> 8);
					dg = dg + MAX(0, (sa*(sg - dg)) >> 8);
					db = db + MAX(0, (sa*(sb - db)) >> 8);

					dr = VEGA_CLAMP_U8(dr);
					dg = VEGA_CLAMP_U8(dg);
					db = VEGA_CLAMP_U8(db);

					d = VEGA_PACK_RGB(dr, dg, db);
				}
				else
				{
					// General case
					PREMULTIPLY(sa, sr, sg, sb);
					PREMULTIPLY(da, dr, dg, db);

					dr = dr + sr - (MIN(dr*sa, sr*da) >> 8);
					dg = dg + sg - (MIN(dg*sa, sg*da) >> 8);
					db = db + sb - (MIN(db*sa, sb*da) >> 8);
					da = sa + da - ((sa*da) >> 8);

					d = CLAMP_AND_PACK(da, dr, dg, db);
				}
				dst.Store(d);
			}
#endif // USE_PREMULTIPLIED_ALPHA

			++src;
			++dst;
		}
		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// Plus/Add
//
// cr = ca + cb
// qr = qa + qb
//
void VEGAFilterMerge::mergePlus(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride() - dstbuf.width;
	unsigned dstPixelStride = dstbuf.GetPixelStride() - dstbuf.width;

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		unsigned cnt = dstbuf.width;
		while (cnt-- > 0)
		{
			unsigned sa, sr, sg, sb;
			src.LoadUnpack(sa, sr, sg, sb);

			unsigned da, dr, dg, db;
			dst.LoadUnpack(da, dr, dg, db);

#ifndef USE_PREMULTIPLIED_ALPHA
			if (sa == 0)
			{
				sr = sg = sb = 0;
			}
			else if (sa < 0xff)
			{
				PREMULTIPLY(sa, sr, sg, sb);
			}
			if (da == 0)
			{
				dr = dg = db = 0;
			}
			else if (da < 0xff)
			{
				PREMULTIPLY(da, dr, dg, db);
			}
#endif // !USE_PREMULTIPLIED_ALPHA

			da += sa;
			dr += sr;
			dg += sg;
			db += sb;

			da = da > 255 ? 255 : da;

			dst.Store(CLAMP_AND_PACK(da, dr, dg, db));

			++src;
			++dst;
		}
		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// Porter-Duff 'atop'
//
// cr = ca * qb + cb * (1 - qa)
// qr = qa * qb + qb * (1 - qa)
//
void VEGAFilterMerge::mergeAtop(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride() - dstbuf.width;
	unsigned dstPixelStride = dstbuf.GetPixelStride() - dstbuf.width;

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		unsigned cnt = dstbuf.width;
		while (cnt-- > 0)
		{
#ifdef USE_PREMULTIPLIED_ALPHA
			unsigned da, dr, dg, db;
			dst.LoadUnpack(da, dr, dg, db);

			unsigned sa, sr, sg, sb;
			src.LoadUnpack(sa, sr, sg, sb);

			dr = (sr * (da+1) + dr * (256 - sa)) >> 8;
			dg = (sg * (da+1) + dg * (256 - sa)) >> 8;
			db = (sb * (da+1) + db * (256 - sa)) >> 8;

			dr = (dr > 255) ? 255 : dr;
			dg = (dg > 255) ? 255 : dg;
			db = (db > 255) ? 255 : db;

			dst.StoreARGB(da, dr, dg, db);
#else
			VEGA_PIXEL d = dst.Load();
			int da = VEGA_UNPACK_A(d);

			if (da == 0)
			{
				dst.Store(0);
			}
			else
			{
				int sa, sr, sg, sb;
				src.LoadUnpack(sa, sr, sg, sb);

				if (sa >= 0xff)
				{
					dst.StoreARGB(da, sr, sg, sb);
				}
				else if (sa > 0)
				{
					int dr = VEGA_UNPACK_R(d);
					int dg = VEGA_UNPACK_G(d);
					int db = VEGA_UNPACK_B(d);

					// This is the general case
					dr = dr + ((sa*(sr - dr)) >> 8);
					dg = dg + ((sa*(sg - dg)) >> 8);
					db = db + ((sa*(sb - db)) >> 8);

					dr = VEGA_CLAMP_U8(dr);
					dg = VEGA_CLAMP_U8(dg);
					db = VEGA_CLAMP_U8(db);

					dst.StoreARGB(da, dr, dg, db);
				}
			}
#endif // USE_PREMULTIPLIED_ALPHA

			++src;
			++dst;
		}
		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// Porter-Duff 'xor'
//
// cr = ca * (1 - qb) + cb * (1 - qa)
// qr = qa * (1 - qb) + qb * (1 - qa)
//
void VEGAFilterMerge::mergeXor(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride() - dstbuf.width;
	unsigned dstPixelStride = dstbuf.GetPixelStride() - dstbuf.width;

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		unsigned cnt = dstbuf.width;
		while (cnt-- > 0)
		{
			VEGA_PIXEL s = src.Load();
			unsigned sa = VEGA_UNPACK_A(s);

			if (sa >= 0xff)
			{
				VEGA_PIXEL d = dst.Load();
				unsigned da = VEGA_UNPACK_A(d);

				if (da == 0)
				{
					dst.Store(s);
				}
				else if (da >= 0xff)
				{
					dst.Store(0);
				}
				else
				{
					da = 255 - da;

#ifdef USE_PREMULTIPLIED_ALPHA
					unsigned inv_da = da + 1;
					unsigned dr = (inv_da * VEGA_UNPACK_R(s)) >> 8;
					unsigned dg = (inv_da * VEGA_UNPACK_G(s)) >> 8;
					unsigned db = (inv_da * VEGA_UNPACK_B(s)) >> 8;

					dst.StoreARGB(da, dr, dg, db);
#else
					dst.StoreARGB(da, VEGA_UNPACK_R(s), VEGA_UNPACK_G(s), VEGA_UNPACK_B(s));
#endif // USE_PREMULTIPLIED_ALPHA
				}
			}
			else if (sa > 0)
			{
				unsigned da, dr, dg, db;
				dst.LoadUnpack(da, dr, dg, db);

				if (da >= 0xff)
				{
					da = 255 - sa;

#ifdef USE_PREMULTIPLIED_ALPHA
					unsigned inv_sa = da + 1;
					dr = (inv_sa * dr) >> 8;
					dg = (inv_sa * dg) >> 8;
					db = (inv_sa * db) >> 8;
#endif // USE_PREMULTIPLIED_ALPHA

					dst.StoreARGB(da, dr, dg, db);
				}
				else if (da > 0)
				{
					unsigned sr = VEGA_UNPACK_R(s);
					unsigned sg = VEGA_UNPACK_G(s);
					unsigned sb = VEGA_UNPACK_B(s);

					// General case
#ifndef USE_PREMULTIPLIED_ALPHA
					PREMULTIPLY(sa, sr, sg, sb);
					PREMULTIPLY(da, dr, dg, db);
#endif // !USE_PREMULTIPLIED_ALPHA

					dr = (dr*(255 - sa) + sr*(255 - da)) >> 8;
					dg = (dg*(255 - sa) + sg*(255 - da)) >> 8;
					db = (db*(255 - sa) + sb*(255 - da)) >> 8;
					da = (da*(255 - sa) + sa*(255 - da)) >> 8;

					dst.Store(CLAMP_AND_PACK(da, dr, dg, db));
				}
				else
				{
					dst.Store(s);
				}
			}

			++src;
			++dst;
		}
		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// Porter-Duff 'out'
//
// cr = ca * (1 - qb)
// qr = qa * (1 - qb)
//
void VEGAFilterMerge::mergeOut(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride() - dstbuf.width;
	unsigned dstPixelStride = dstbuf.GetPixelStride() - dstbuf.width;

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		unsigned cnt = dstbuf.width;
		while (cnt-- > 0)
		{
			VEGA_PIXEL d = dst.Load();
			unsigned da = VEGA_UNPACK_A(d);

			if (da >= 0xff)
			{
				dst.Store(0);
			}
			else if (da > 0)
			{
				VEGA_PIXEL s = src.Load();
				unsigned sa = VEGA_UNPACK_A(s);

				if (sa > 0)
				{
					unsigned inv_da = 256 - da;
					da = (sa * inv_da) >> 8;
#ifdef USE_PREMULTIPLIED_ALPHA
					unsigned dr = (inv_da * VEGA_UNPACK_R(s)) >> 8;
					unsigned dg = (inv_da * VEGA_UNPACK_G(s)) >> 8;
					unsigned db = (inv_da * VEGA_UNPACK_B(s)) >> 8;

					dst.StoreARGB(da, dr, dg, db);
#else
					dst.StoreARGB(da, VEGA_UNPACK_R(s), VEGA_UNPACK_G(s), VEGA_UNPACK_B(s));
#endif // USE_PREMULTIPLIED_ALPHA
				}
				else
				{
					dst.Store(0);
				}
			}
			else
			{
				dst.Store(src.Load());
			}

			++src;
			++dst;
		}
		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// Porter-Duff 'in'
//
// cr = ca * qb
// qr = qa * qb
//
void VEGAFilterMerge::mergeIn(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride() - dstbuf.width;
	unsigned dstPixelStride = dstbuf.GetPixelStride() - dstbuf.width;

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		unsigned cnt = dstbuf.width;
		while (cnt-- > 0)
		{
			VEGA_PIXEL s = src.Load();
			unsigned sa = VEGA_UNPACK_A(s);

			if (sa == 0)
			{
				dst.Store(0);
			}
			else if (sa > 0)
			{
				VEGA_PIXEL d = dst.Load();
				unsigned da = VEGA_UNPACK_A(d);

				if (da >= 0xff)
				{
					dst.Store(src.Load());
				}
				else if (da > 0)
				{
					unsigned alpha = da + 1;
#ifdef USE_PREMULTIPLIED_ALPHA
					unsigned dr = (alpha * VEGA_UNPACK_R(s)) >> 8;
					unsigned dg = (alpha * VEGA_UNPACK_G(s)) >> 8;
					unsigned db = (alpha * VEGA_UNPACK_B(s)) >> 8;
					da = (alpha * sa) >> 8;

					dst.StoreARGB(da, dr, dg, db);
#else
					da = (alpha * sa) >> 8;

					dst.StoreARGB(da, VEGA_UNPACK_R(s), VEGA_UNPACK_G(s), VEGA_UNPACK_B(s));
#endif // USE_PREMULTIPLIED_ALPHA
				}
				else
				{
					dst.Store(0);
				}
			}

			++src;
			++dst;
		}
		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// [Approximately: (A in O) over B, where O is the opacity]
//
// [o = opacity]
// cr = ca * o + cb * (1 - qa * o)
// qr = qa * o + qb * (1 - qa * o)
//
void VEGAFilterMerge::mergeOpacity(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride();
	unsigned dstPixelStride = dstbuf.GetPixelStride();

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		VEGACompOverIn(dst.Ptr(), src.Ptr(), opacity, dstbuf.width);

		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

//
// 'Copy'
//
// cr = ca
// qr = qa
//
void VEGAFilterMerge::mergeReplace(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride();
	unsigned dstPixelStride = dstbuf.GetPixelStride();

	if (sourceAlphaOnly)
	{
		srcPixelStride -= dstbuf.width;
		dstPixelStride -= dstbuf.width;

		for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
		{
			unsigned cnt = dstbuf.width;
			while (cnt-- > 0)
			{
				dst.StoreARGB(VEGA_UNPACK_A(src.Load()), 0, 0, 0);

				++src;
				++dst;
			}
			src += srcPixelStride;
			dst += dstPixelStride;
		}
	}
	else
	{
		for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
		{
			dst.CopyFrom(src.Ptr(), dstbuf.width);

			src += srcPixelStride;
			dst += dstPixelStride;
		}
	}
}

//
// Porter-Duff 'over'
//
// cr = ca + cb * (1 - qa)
// qr = qa + qb * (1 - qa)
//
void VEGAFilterMerge::mergeOver(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);

	unsigned srcPixelStride = srcbuf.GetPixelStride();
	unsigned dstPixelStride = dstbuf.GetPixelStride();

	for (unsigned int yp = 0; yp < dstbuf.height; ++yp)
	{
		VEGACompOver(dst.Ptr(), src.Ptr(), dstbuf.width);

		src += srcPixelStride;
		dst += dstPixelStride;
	}
}

OP_STATUS VEGAFilterMerge::apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region)
{
	VEGASWBuffer sub_src = source.CreateSubset(region.sx, region.sy, region.width, region.height);
	VEGASWBuffer sub_dst = dest.CreateSubset(region.dx, region.dy, region.width, region.height);

	switch (mergeType)
	{
	case VEGAMERGE_ARITHMETIC:
		mergeArithmetic(sub_dst, sub_src);
		break;
	case VEGAMERGE_MULTIPLY:
		mergeMultiply(sub_dst, sub_src);
		break;
	case VEGAMERGE_SCREEN:
		mergeScreen(sub_dst, sub_src);
		break;
	case VEGAMERGE_DARKEN:
		mergeDarken(sub_dst, sub_src);
		break;
	case VEGAMERGE_LIGHTEN:
		mergeLighten(sub_dst, sub_src);
		break;
	case VEGAMERGE_PLUS:
		mergePlus(sub_dst, sub_src);
		break;
	case VEGAMERGE_ATOP:
		mergeAtop(sub_dst, sub_src);
		break;
	case VEGAMERGE_XOR:
		mergeXor(sub_dst, sub_src);
		break;
	case VEGAMERGE_OUT:
		mergeOut(sub_dst, sub_src);
		break;
	case VEGAMERGE_IN:
		mergeIn(sub_dst, sub_src);
		break;
	case VEGAMERGE_OPACITY:
		mergeOpacity(sub_dst, sub_src);
		break;
	case VEGAMERGE_REPLACE:
		mergeReplace(sub_dst, sub_src);
		break;
	case VEGAMERGE_NORMAL:
	case VEGAMERGE_OVER:
	default:
		mergeOver(sub_dst, sub_src);
		break;
	}

	return OpStatus::OK;
}

#ifdef VEGA_3DDEVICE
bool VEGAFilterMerge::setBlendingFactors()
{
	switch (mergeType)
	{
	case VEGAMERGE_PLUS:
		// one, one
		srcBlend = VEGA3dRenderState::BLEND_ONE;
		dstBlend = VEGA3dRenderState::BLEND_ONE;
		break;
	case VEGAMERGE_ATOP:
		// dsta, 1-srca
		srcBlend = VEGA3dRenderState::BLEND_DST_ALPHA;
		dstBlend = VEGA3dRenderState::BLEND_ONE_MINUS_SRC_ALPHA;
		break;
	case VEGAMERGE_XOR:
		// 1-dsta, 1-srca
		srcBlend = VEGA3dRenderState::BLEND_ONE_MINUS_DST_ALPHA;
		dstBlend = VEGA3dRenderState::BLEND_ONE_MINUS_SRC_ALPHA;
		break;
	case VEGAMERGE_OUT:
		// 1-dsta, 0
		srcBlend = VEGA3dRenderState::BLEND_ONE_MINUS_DST_ALPHA;
		dstBlend = VEGA3dRenderState::BLEND_ZERO;
		break;
	case VEGAMERGE_IN:
		// dsta, 0
		srcBlend = VEGA3dRenderState::BLEND_DST_ALPHA;
		dstBlend = VEGA3dRenderState::BLEND_ZERO;
		break;
	case VEGAMERGE_OPACITY:
		// one, 1-srca
		// use color opacity,opacity,opacity,opacity
		srcBlend = VEGA3dRenderState::BLEND_ONE;
		dstBlend = VEGA3dRenderState::BLEND_ONE_MINUS_SRC_ALPHA;
		break;
	case VEGAMERGE_REPLACE:
		// one, zero
		srcBlend = VEGA3dRenderState::BLEND_ONE;
		dstBlend = VEGA3dRenderState::BLEND_ZERO;
		break;
	default:
	case VEGAMERGE_NORMAL:
	case VEGAMERGE_OVER:
		// one, 1-srca
		srcBlend = VEGA3dRenderState::BLEND_ONE;
		dstBlend = VEGA3dRenderState::BLEND_ONE_MINUS_SRC_ALPHA;
		break;
	case VEGAMERGE_ARITHMETIC:
	case VEGAMERGE_MULTIPLY:
	case VEGAMERGE_SCREEN:
	case VEGAMERGE_DARKEN:
	case VEGAMERGE_LIGHTEN:
		// Just set dev and let there be light
		break;
	}
	return true;
}

OP_STATUS VEGAFilterMerge::setupVertexBuffer(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout, unsigned int* out_startIndex, VEGA3dTexture* tex, VEGA3dShaderProgram* sprog,
											 const VEGAFilterRegion& region)
{
	VEGA3dTexture* tex1 = source2Tex;
	return createVertexBuffer_Binary(device, out_vbuf, out_vlayout, out_startIndex, tex, tex1, sprog, region);
}

OP_STATUS VEGAFilterMerge::getShader(VEGA3dDevice* device, VEGA3dShaderProgram** out_shader, VEGA3dTexture* srcTex)
{
	VEGA3dShaderProgram::ShaderType shdtype;
	switch (mergeType)
	{
	case VEGAMERGE_ARITHMETIC:
		shdtype = VEGA3dShaderProgram::SHADER_MERGE_ARITHMETIC;
		break;
	case VEGAMERGE_MULTIPLY:
		shdtype = VEGA3dShaderProgram::SHADER_MERGE_MULTIPLY;
		break;
	case VEGAMERGE_SCREEN:
		shdtype = VEGA3dShaderProgram::SHADER_MERGE_SCREEN;
		break;
	case VEGAMERGE_DARKEN:
		shdtype = VEGA3dShaderProgram::SHADER_MERGE_DARKEN;
		break;
	case VEGAMERGE_LIGHTEN:
		shdtype = VEGA3dShaderProgram::SHADER_MERGE_LIGHTEN;
		break;
	default:
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	VEGA3dShaderProgram* shader = NULL;
	RETURN_IF_ERROR(device->createShaderProgram(&shader, shdtype, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP));

	device->setShaderProgram(shader);

	if (mergeType == VEGAMERGE_ARITHMETIC)
	{
		shader->setScalar(shader->getConstantLocation("k1"), k1);
		shader->setScalar(shader->getConstantLocation("k2"), k2);
		shader->setScalar(shader->getConstantLocation("k3"), k3);
		shader->setScalar(shader->getConstantLocation("k4"), k4);
	}

	device->setTexture(0, srcTex);
	device->setTexture(1, source2Tex);

	*out_shader = shader;

	return OpStatus::OK;
}

void VEGAFilterMerge::putShader(VEGA3dDevice* device, VEGA3dShaderProgram* shader)
{
	OP_ASSERT(device && shader);

	VEGARefCount::DecRef(shader);
}

OP_STATUS VEGAFilterMerge::apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame)
{
	if (!setBlendingFactors())
		return applyFallback(destStore, region);

	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;

	VEGA3dRenderTarget* destRT = destStore->GetWriteRenderTarget(frame);
	if (sourceRT == destRT ||
		sourceRT->getType() != VEGA3dRenderTarget::VEGA3D_RT_TEXTURE ||
		!static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture())
	{
		if (mergeType == VEGAMERGE_REPLACE && destRT->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE &&
			static_cast<VEGA3dFramebufferObject*>(destRT)->getAttachedColorTexture())
		{
			dev->setRenderTarget(sourceRT);
			dev->setRenderState(dev->getDefault2dNoBlendNoScissorRenderState());
			dev->copyToTexture(static_cast<VEGA3dFramebufferObject*>(destRT)->getAttachedColorTexture(), VEGA3dTexture::CUBE_SIDE_NONE, 0, 
				region.dx, region.dy, region.sx, region.sy, region.width, region.height);
			return OpStatus::OK;
		}
		// merge filters can only be applied if they have an attached
		// color texture.
		OP_ASSERT(!"unhandled merge from window (or multi-sampled backbuffer)");
		return OpStatus::ERR;
	}

	// FIXME: A lot of checking done twice
	if (mergeType == VEGAMERGE_ARITHMETIC || mergeType == VEGAMERGE_MULTIPLY ||
		mergeType == VEGAMERGE_SCREEN || mergeType == VEGAMERGE_DARKEN ||
		mergeType == VEGAMERGE_LIGHTEN)
	{
		VEGA3dTexture* tmpTex = NULL;
		VEGA3dRenderTarget* destReadRT = destStore->GetReadRenderTarget();
		if (destReadRT != destRT && destReadRT->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
		{
			tmpTex = static_cast<VEGA3dFramebufferObject*>(destReadRT)->getAttachedColorTexture();
		}
		OP_STATUS status = OpStatus::OK;
		if (tmpTex)
			VEGARefCount::IncRef(tmpTex);
		else
		{
			// This assumes that applyShader does not use the temp texture. It only uses tempTexture
			// when creating clamp texure or source==dest, which is already checked for
			// source2Tex and destination must use same coordinates, so always clone from 0,0
			status = cloneToTempTexture(dev, destReadRT, region.dx, region.dy,
											  region.width, region.height, true, &tmpTex);
		}
		if (OpStatus::IsSuccess(status))
		{
			source2Tex = tmpTex;

			status = applyShader(destStore, region, frame);
		}

		VEGARefCount::DecRef(tmpTex);

		if (OpStatus::IsSuccess(status))
			return status;

		return applyFallback(destStore, region);
	}

	dev->setRenderTarget(destRT);

	if (srcBlend == VEGA3dRenderState::BLEND_ONE && dstBlend == VEGA3dRenderState::BLEND_ONE_MINUS_SRC_ALPHA)
	{
		VEGA3dRenderState* state = dev->getDefault2dRenderState();
		dev->setScissor(0, 0, destRT->getWidth(), destRT->getHeight());
		dev->setRenderState(state);
	}
	else if (srcBlend == VEGA3dRenderState::BLEND_ONE && dstBlend == VEGA3dRenderState::BLEND_ZERO)
	{
		VEGA3dRenderState* state = dev->getDefault2dNoBlendRenderState();
		dev->setScissor(0, 0, destRT->getWidth(), destRT->getHeight());
		dev->setRenderState(state);
	}
	else
	{
		VEGA3dRenderState state;
		state.enableBlend(true);
		state.setBlendFunc(srcBlend, dstBlend);

		dev->setRenderState(&state);
	}

	VEGA3dTexture* tex = static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture();
	dev->setTexture(0, tex);

	UINT32 color = 0xffffffff;
	if (mergeType == VEGAMERGE_OPACITY)
		color = (opacity<<24)|(opacity<<16)|(opacity<<8)|opacity;
	else if (mergeType == VEGAMERGE_REPLACE && sourceAlphaOnly)
		color = 0xff000000;

	VEGA3dShaderProgram* shd;
	OP_STATUS err = dev->createShaderProgram(&shd, VEGA3dShaderProgram::SHADER_VECTOR2D, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP);
	if (OpStatus::IsSuccess(err))
	{
		VEGA3dBuffer* vbuf = NULL;
		VEGA3dVertexLayout* vlayout = NULL;
		unsigned int vbufStartIndex = 0;
		err = createVertexBuffer_Unary(dev, &vbuf, &vlayout, &vbufStartIndex, tex, shd, region, color);

		if (OpStatus::IsSuccess(err))
		{
			dev->setShaderProgram(shd);
			shd->setOrthogonalProjection();
			err = dev->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, vlayout, vbufStartIndex, 4);
			VEGARefCount::DecRef(vlayout);
			VEGARefCount::DecRef(vbuf);
		}
		VEGARefCount::DecRef(shd);
	}

	return err;
}
#endif // VEGA_3DDEVICE

#ifdef VEGA_2DDEVICE
OP_STATUS VEGAFilterMerge::apply(VEGA2dSurface* destRT, const VEGAFilterRegion& region)
{
	VEGA2dDevice* dev2d = g_vegaGlobals.vega2dDevice;
	if (mergeType != VEGAMERGE_OPACITY ||
		!dev2d->supportsCompositeOperator(VEGA2dDevice::COMPOSITE_SRC_OPACITY))
		return VEGAFilter::apply(destRT, region);

	sourceStore->Validate();

	dev2d->setRenderTarget(destRT);
	dev2d->setColor(opacity, opacity, opacity, opacity);
	dev2d->setClipRect(0, 0, destRT->getWidth(), destRT->getHeight());
	dev2d->setCompositeOperator(VEGA2dDevice::COMPOSITE_SRC_OPACITY);

	dev2d->blit(sourceSurf, region.sx, region.sy, region.width, region.height, region.dx, region.dy);

	dev2d->setCompositeOperator(VEGA2dDevice::COMPOSITE_SRC_OVER);
	return OpStatus::OK;
}
#endif // VEGA_2DDEVICE

#undef VEGA_CLAMP_U8
#undef PREMULTIPLY
#undef CLAMP_AND_PACK

#endif // VEGA_SUPPORT
