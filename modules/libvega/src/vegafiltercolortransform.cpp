/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegafiltercolortransform.h"
#include "modules/libvega/src/vegapixelformat.h"
#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"
#endif // VEGA_3DDEVICE

#define VEGA_CLAMP_U8(v) (((v) <= 255) ? (((v) >= 0) ? (v) : 0) : 255)

#ifdef USE_PREMULTIPLIED_ALPHA
#define VEGA_COND_PREMULT(a,r,g,b) \
do{\
	(r) = (((a) + 1) * (r)) >> 8;\
	(g) = (((a) + 1) * (g)) >> 8;\
	(b) = (((a) + 1) * (b)) >> 8;\
}while(0)

#define VEGA_COND_UNPREMULT(a,r,g,b) \
do{\
	if ((a))\
	{\
		(r) = 255 * (r) / (a);\
		(g) = 255 * (g) / (a);\
		(b) = 255 * (b) / (a);\
	}\
	else\
		(r) = (g) = (b) = 0;\
}while(0)

#else
#define VEGA_COND_PREMULT(a,r,g,b) do{}while(0)
#define VEGA_COND_UNPREMULT(a,r,g,b) do{}while(0)
#endif // USE_PREMULTIPLIED_ALPHA

VEGAFilterColorTransform::VEGAFilterColorTransform(VEGAColorTransformType type, VEGA_FIX* mat)
	: transform_type(type)
{
	setMatrix(mat);
#ifdef VEGA_3DDEVICE
	xlat_table = NULL;
	xlat_table_needs_update = TRUE;
#endif // VEGA_3DDEVICE
}

VEGAFilterColorTransform::~VEGAFilterColorTransform()
{
#ifdef VEGA_3DDEVICE
	VEGARefCount::DecRef(xlat_table);
#endif // VEGA_3DDEVICE
}

void VEGAFilterColorTransform::setMatrix(VEGA_FIX* mat)
{
	if (!mat)
		return;

	for (int i = 0; i < 20; i++)
		matrix[i] = mat[i];
}

void VEGAFilterColorTransform::setCompIdentity(VEGAComponent comp)
{
	for (unsigned int i = 0; i < 256; i++)
		lut[comp][i] = i;
#ifdef VEGA_3DDEVICE
	xlat_table_needs_update = TRUE;
#endif // VEGA_3DDEVICE
}

void VEGAFilterColorTransform::setCompTable(VEGAComponent comp,
											VEGA_FIX* c_table, unsigned int c_table_size)
{
	for (unsigned int i = 0; i < 256; i++)
	{
		VEGA_FIX fix_c = VEGA_INTTOFIX(i) / 255;

		unsigned int tidx = VEGA_FIXTOINT(VEGA_FLOOR(fix_c * (c_table_size - 1)));

		int val;
		if (tidx < c_table_size-1)
		{
			VEGA_FIX frac = fix_c * (c_table_size - 1) - VEGA_INTTOFIX(tidx);
			VEGA_FIX fix_val = c_table[tidx] + VEGA_FIXMUL(frac, c_table[tidx+1] - c_table[tidx]);
			val = VEGA_FIXTOINT(255 * fix_val);
		}
		else
		{
			val = VEGA_FIXTOINT(255 * c_table[c_table_size-1]);
		}

		lut[comp][i] = (unsigned char)VEGA_CLAMP_U8(val);
	}
#ifdef VEGA_3DDEVICE
	xlat_table_needs_update = TRUE;
#endif // VEGA_3DDEVICE
}

void VEGAFilterColorTransform::setCompDiscrete(VEGAComponent comp,
											   VEGA_FIX* c_table, unsigned int c_table_size)
{
	for (unsigned int i = 0; i < 256; i++)
	{
		unsigned int tbl_idx = (i * c_table_size) / 255;

		if (tbl_idx >= c_table_size)
			tbl_idx = c_table_size - 1;

		int val = VEGA_FIXTOINT(255 * c_table[tbl_idx]);

		lut[comp][i] = (unsigned char)VEGA_CLAMP_U8(val);
	}
#ifdef VEGA_3DDEVICE
	xlat_table_needs_update = TRUE;
#endif // VEGA_3DDEVICE
}

void VEGAFilterColorTransform::setCompLinear(VEGAComponent comp,
											 VEGA_FIX slope, VEGA_FIX intercept)
{
	for (unsigned int i = 0; i < 256; i++)
	{
		VEGA_FIX norm_i = VEGA_INTTOFIX(i) / 255;
		VEGA_FIX lin = VEGA_FIXMUL(slope, norm_i) + intercept;
		int val = VEGA_FIXTOINT(lin * 255);

		lut[comp][i] = (unsigned char)VEGA_CLAMP_U8(val);
	}
#ifdef VEGA_3DDEVICE
	xlat_table_needs_update = TRUE;
#endif // VEGA_3DDEVICE
}

void VEGAFilterColorTransform::setCompGamma(VEGAComponent comp,
											VEGA_FIX ampl, VEGA_FIX exp, VEGA_FIX offs)
{
	for (unsigned int i = 0; i < 256; i++)
	{
		VEGA_FIX norm_i = VEGA_INTTOFIX(i) / 255;
		VEGA_FIX corr = VEGA_FIXMUL(ampl, VEGA_FIXPOW(norm_i, exp)) + offs;
		int val = VEGA_FIXTOINT(255 * corr);

		lut[comp][i] = (unsigned char)VEGA_CLAMP_U8(val);
	}
#ifdef VEGA_3DDEVICE
	xlat_table_needs_update = TRUE;
#endif // VEGA_3DDEVICE
}

void VEGAFilterColorTransform::setColorSpaceConversion(VEGACSConversionType type)
{
	switch (type)
	{
	case VEGACSCONV_SRGB_TO_LINRGB:
	{
		// linRGB = (sRGB <= 0.04045) ? sRGB / 12.92 : ((sRGB + 0.055) / 1.055)^(2.4)
		VEGA_FIX exp = VEGA_INTTOFIX(24) / 10; /* 2.4 */
		for (unsigned int i = 0; i < 256; i++)
		{
			VEGA_FIX norm_i = VEGA_INTTOFIX(i) / 255;

			VEGA_FIX corr;
			if (i < 11) // 11/255 ~= 0.043
			{
				corr = norm_i * 100 / 1292; /* sRGB/12.92 */
			}
			else
			{
				VEGA_FIX base = (norm_i * 1000 + VEGA_INTTOFIX(55)) / 1055; /* (sRGB+0.055)/1.055 */
				corr = VEGA_FIXPOW(base, exp);
			}

			int val = VEGA_FIXTOINT(255 * corr);
			lut[0][i] = (unsigned char)VEGA_CLAMP_U8(val);
		}
		break;
	}

	case VEGACSCONV_LINRGB_TO_SRGB:
	{
		// sRGB = (linRGB <= 0.00313008) ? linRGB * 12.92 : 1.055*linRGB^(1/2.4)-0.055
		VEGA_FIX exp = VEGA_INTTOFIX(10) / 24; /* 1 / 2.4 */
		for (unsigned int i = 0; i < 256; i++)
		{
			if (i == 0) // 1/255 ~= 0.0039
			{
				lut[0][i] = 0;
			}
			else
			{
				VEGA_FIX norm_i = VEGA_INTTOFIX(i) / 255;
				VEGA_FIX corr = (VEGA_FIXPOW(norm_i, exp) * 1055 - VEGA_INTTOFIX(55)) / 1000;

				int val = VEGA_FIXTOINT(255 * corr);
				lut[0][i] = (unsigned char)VEGA_CLAMP_U8(val);
			}
		}
		break;
	}
	default:
		OP_ASSERT(0);
		break;
	}

#ifdef VEGA_3DDEVICE
	xlat_table_needs_update = TRUE;
	csconv_type = type;
#endif // VEGA_3DDEVICE
}

#ifdef VEGA_3DDEVICE
OP_STATUS VEGAFilterColorTransform::getShader(VEGA3dDevice* device, VEGA3dShaderProgram** out_shader, VEGA3dTexture* srcTex)
{
	VEGA3dShaderProgram::ShaderType shdtype;
	switch (transform_type)
	{
	default:
	case VEGACOLORTRANSFORM_MATRIX:
		shdtype = VEGA3dShaderProgram::SHADER_COLORMATRIX;
		break;
	case VEGACOLORTRANSFORM_LUMINANCETOALPHA:
		shdtype = VEGA3dShaderProgram::SHADER_LUMINANCE_TO_ALPHA;
		break;
	case VEGACOLORTRANSFORM_COMPONENTTRANSFER:
		shdtype = VEGA3dShaderProgram::SHADER_COMPONENTTRANSFER;
		break;
	case VEGACOLORTRANSFORM_COLORSPACECONV:
		if (csconv_type == VEGACSCONV_SRGB_TO_LINRGB)
			shdtype = VEGA3dShaderProgram::SHADER_SRGB_TO_LINEARRGB;
		else
			shdtype = VEGA3dShaderProgram::SHADER_LINEARRGB_TO_SRGB;
		break;
	}

	if (transform_type == VEGACOLORTRANSFORM_COMPONENTTRANSFER &&
		!xlat_table)
	{
		// Allocate a 256x1 texture (or maybe a 256x4 lum. - or maybe
		// a 1D even?) and fill it from the relevant tables
		RETURN_IF_ERROR(device->createTexture(&xlat_table, 256, 4,
											  VEGA3dTexture::FORMAT_ALPHA8));
		xlat_table->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);
	}

	VEGA3dShaderProgram* shader = NULL;
	RETURN_IF_ERROR(device->createShaderProgram(&shader, shdtype, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP));

	device->setShaderProgram(shader);

	switch (transform_type)
	{
	case VEGACOLORTRANSFORM_MATRIX:
	{
		int i, j;

		float mat[16];
		for (i = 0; i < 4; ++i)
			for (j = 0; j < 4; ++j)
				mat[i*4+j] = VEGA_FIXTOFLT(matrix[i*5+j]);

		shader->setMatrix4(shader->getConstantLocation("colormat"), mat);

		for (i = 0; i < 4; ++i)
			mat[i] = VEGA_FIXTOFLT(matrix[i*5+4]);

		shader->setVector4(shader->getConstantLocation("colorbias"), mat);
	}
	break;

	case VEGACOLORTRANSFORM_COMPONENTTRANSFER:
		if (xlat_table_needs_update)
		{
			xlat_table->update(0, 0, 256, 1, lut[VEGACOMP_R]);
			xlat_table->update(0, 1, 256, 1, lut[VEGACOMP_G]);
			xlat_table->update(0, 2, 256, 1, lut[VEGACOMP_B]);
			xlat_table->update(0, 3, 256, 1, lut[VEGACOMP_A]);
			xlat_table_needs_update = FALSE;
		}

		device->setTexture(1, xlat_table);
		break;
	}

	device->setTexture(0, srcTex);

	*out_shader = shader;

	return OpStatus::OK;
}

void VEGAFilterColorTransform::putShader(VEGA3dDevice* device, VEGA3dShaderProgram* shader)
{
	OP_ASSERT(device && shader);

	VEGARefCount::DecRef(shader);
}

OP_STATUS VEGAFilterColorTransform::apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame)
{
	OP_STATUS status = applyShader(destStore, region, frame);
	if (OpStatus::IsError(status))
		status = applyFallback(destStore, region);

	return status;
}
#endif // VEGA_3DDEVICE

OP_STATUS VEGAFilterColorTransform::apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region)
{
	// Alpha conserving transform
	BOOL conservesAlpha = FALSE;

	unsigned int yp;

	VEGAConstPixelAccessor src = source.GetConstAccessor(region.sx, region.sy);
	VEGAPixelAccessor dst = dest.GetAccessor(region.dx, region.dy);

	unsigned int srcPixelStride = source.GetPixelStride();
	unsigned int dstPixelStride = dest.GetPixelStride();

	srcPixelStride -= region.width;
	dstPixelStride -= region.width;

	switch (transform_type)
	{
	case VEGACOLORTRANSFORM_MATRIX:
		/* alpha identity mapping (row 4 == [0 0 0 1 0]) */
		if (matrix[15] == 0 && matrix[16] == 0 && matrix[17] == 0 &&
			matrix[18] == 1 && matrix[19] == 0)
			conservesAlpha = TRUE;

		if (conservesAlpha)
		{
			VEGA_FIX m4_255 = matrix[4] * 255;
			VEGA_FIX m9_255 = matrix[9] * 255;
			VEGA_FIX m14_255 = matrix[14] * 255;

			for (yp = 0; yp < region.height; ++yp)
			{
				unsigned cnt = region.width;
				while (cnt-- > 0)
				{
					int sa, sr, sg, sb;
					src.LoadUnpack(sa, sr, sg, sb);

					VEGA_COND_UNPREMULT(sa, sr, sg, sb);

					// Apply matrix
					int dr = VEGA_FIXTOINT(sr * matrix[0] + sg * matrix[1] +
										   sb * matrix[2] + sa * matrix[3] + m4_255);
					int dg = VEGA_FIXTOINT(sr * matrix[5] + sg * matrix[6] +
										   sb * matrix[7] + sa * matrix[8] + m9_255);
					int db = VEGA_FIXTOINT(sr * matrix[10] + sg * matrix[11] +
										   sb * matrix[12] + sa * matrix[13] + m14_255);

					// Clip color components
					dr = VEGA_CLAMP_U8(dr);
					dg = VEGA_CLAMP_U8(dg);
					db = VEGA_CLAMP_U8(db);

					VEGA_COND_PREMULT(sa, dr, dg, db);

					dst.StoreARGB(sa, dr, dg, db);

					++src;
					++dst;
				}

				src += srcPixelStride;
				dst += dstPixelStride;
			}
		}
		else
		{
			VEGA_FIX m4_255 = matrix[4] * 255;
			VEGA_FIX m9_255 = matrix[9] * 255;
			VEGA_FIX m14_255 = matrix[14] * 255;
			VEGA_FIX m19_255 = matrix[19] * 255;

			for (yp = 0; yp < region.height; ++yp)
			{
				unsigned cnt = region.width;
				while (cnt-- > 0)
				{
					int sa, sr, sg, sb;
					src.LoadUnpack(sa, sr, sg, sb);

					VEGA_COND_UNPREMULT(sa, sr, sg, sb);

					// Apply matrix
					int dr = VEGA_FIXTOINT(sr * matrix[0] + sg * matrix[1] +
										   sb * matrix[2] + sa * matrix[3] + m4_255);
					int dg = VEGA_FIXTOINT(sr * matrix[5] + sg * matrix[6] +
										   sb * matrix[7] + sa * matrix[8] + m9_255);
					int db = VEGA_FIXTOINT(sr * matrix[10] + sg * matrix[11] +
										   sb * matrix[12] + sa * matrix[13] + m14_255);
					int da = VEGA_FIXTOINT(sr * matrix[15] + sg * matrix[16] +
										   sb * matrix[17] + sa * matrix[18] + m19_255);

					// Clip color components
					dr = VEGA_CLAMP_U8(dr);
					dg = VEGA_CLAMP_U8(dg);
					db = VEGA_CLAMP_U8(db);
					da = VEGA_CLAMP_U8(da);

					VEGA_COND_PREMULT(da, dr, dg, db);

					dst.StoreARGB(da, dr, dg, db);

					++src;
					++dst;
				}

				src += srcPixelStride;
				dst += dstPixelStride;
			}
		}
		break;

	case VEGACOLORTRANSFORM_LUMINANCETOALPHA:
		for (yp = 0; yp < region.height; ++yp)
		{
			unsigned cnt = region.width;
			while (cnt-- > 0)
			{
				unsigned sa, sr, sg, sb;
				src.LoadUnpack(sa, sr, sg, sb);

#ifdef USE_PREMULTIPLIED_ALPHA
				// Premultiplied
				sa = sa ? ((sr*108 + sg*365 + sb*37) / sa) >> 1 : 0;
				sa = sa > 255 ? 255 : sa;
#else
				sa = (sr*54 + sg*183 + sb*18) >> 8;
#endif // USE_PREMULTIPLIED_ALPHA

				dst.StoreARGB(sa, 0, 0, 0);

				++src;
				++dst;
			}

			src += srcPixelStride;
			dst += dstPixelStride;
		}
		break;

	case VEGACOLORTRANSFORM_COMPONENTTRANSFER:
		for (yp = 0; yp < region.height; ++yp)
		{
			unsigned cnt = region.width;
			while (cnt-- > 0)
			{
				unsigned da, dr, dg, db;
				src.LoadUnpack(da, dr, dg, db);

				VEGA_COND_UNPREMULT(da, dr, dg, db);

				da = lut[VEGACOMP_A][da];
				dr = lut[VEGACOMP_R][dr&0xff];
				dg = lut[VEGACOMP_G][dg&0xff];
				db = lut[VEGACOMP_B][db&0xff];

				VEGA_COND_PREMULT(da, dr, dg, db);

				dst.StoreARGB(da, dr, dg, db);

				++src;
				++dst;
			}

			src += srcPixelStride;
			dst += dstPixelStride;
		}
		break;

	case VEGACOLORTRANSFORM_COLORSPACECONV:
		for (yp = 0; yp < region.height; ++yp)
		{
			unsigned cnt = region.width;
			while (cnt-- > 0)
			{
				unsigned da, dr, dg, db;
				src.LoadUnpack(da, dr, dg, db);

				VEGA_COND_UNPREMULT(da, dr, dg, db);

				dr = lut[0][dr&0xff];
				dg = lut[0][dg&0xff];
				db = lut[0][db&0xff];

				VEGA_COND_PREMULT(da, dr, dg, db);

				dst.StoreARGB(da, dr, dg, db);

				++src;
				++dst;
			}

			src += srcPixelStride;
			dst += dstPixelStride;
		}
		break;
	}

	return OpStatus::OK;
}

#undef VEGA_COND_PREMULT
#undef VEGA_COND_UNPREMULT
#undef VEGA_CLAMP_U8

#endif // VEGA_SUPPORT
