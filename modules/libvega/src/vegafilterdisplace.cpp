/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegafilterdisplace.h"
#include "modules/libvega/src/vegapixelformat.h"
#include "modules/libvega/src/vegabackend.h"

#if defined(VEGA_INTERNAL_FORMAT_RGB565A8) && !defined(VEGA_INTERNAL_FORMAT_BGRA8888)
#define USE_565_DISPLACEMENT
#define DISP_565_BLUR_RADIUS 5
#define DISP_565_BLUR_LEFT ((DISP_565_BLUR_RADIUS-1)/2)
#define DISP_565_BLUR_RIGHT (DISP_565_BLUR_RADIUS-DISP_565_BLUR_LEFT-1)
#endif

#ifdef USE_PREMULTIPLIED_ALPHA
// When using premultiplied alpha we can easily do bilinear interpolation of the displacement
#define DISPLACE_LINEAR
#endif // USE_PREMULTIPLIED_ALPHA

VEGAFilterDisplace::VEGAFilterDisplace()
	: scale(0), x_comp(COMP_A), y_comp(COMP_A)
{
	mapstore = NULL;
#ifdef VEGA_3DDEVICE
	dispmapTex = NULL;
#endif // VEGA_3DDEVICE
	dispmap.Reset();
}

#ifdef VEGA_3DDEVICE

#include "modules/libvega/src/vegabackend_hw3d.h"

OP_STATUS VEGAFilterDisplace::getShader(VEGA3dDevice* device, VEGA3dShaderProgram** out_shader, VEGA3dTexture* srcTex)
{
	VEGA3dShaderProgram* shader = NULL;
	RETURN_IF_ERROR(device->createShaderProgram(&shader, VEGA3dShaderProgram::SHADER_DISPLACEMENT, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP));

	device->setShaderProgram(shader);

	int selx_idx = (x_comp + 3) & 3; // Want the order {r,g,b,a}, not {a,r,g,b}
	int sely_idx = (y_comp + 3) & 3;

	float selector[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	selector[selx_idx] = 1.0f;
	shader->setVector4(shader->getConstantLocation("xselector"), selector);

	selector[selx_idx] = 0.0f; // Reset
	selector[sely_idx] = 1.0f;
	shader->setVector4(shader->getConstantLocation("yselector"), selector);

	float src_scale[2] = { VEGA_FIXTOFLT(scale) / srcTex->getWidth(),
						   VEGA_FIXTOFLT(scale) / srcTex->getHeight() };
	shader->setVector2(shader->getConstantLocation("src_scale"), src_scale);

	device->setTexture(0, srcTex);
	device->setTexture(1, dispmapTex);

	*out_shader = shader;

	return OpStatus::OK;
}

void VEGAFilterDisplace::putShader(VEGA3dDevice* device, VEGA3dShaderProgram* shader)
{
	OP_ASSERT(device && shader);

	VEGARefCount::DecRef(shader);
}

OP_STATUS VEGAFilterDisplace::setupVertexBuffer(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout, unsigned int* out_startIndex, VEGA3dTexture* tex, VEGA3dShaderProgram* sprog,
												const VEGAFilterRegion& region)
{
	return createVertexBuffer_Binary(device, out_vbuf, out_vlayout, out_startIndex, tex, dispmapTex, sprog, region);
}

OP_STATUS VEGAFilterDisplace::apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame)
{
	OP_ASSERT(mapstore);

	if (mapstore->IsA(VEGABackingStore::FRAMEBUFFEROBJECT))
	{
		VEGA3dRenderTarget* rt = static_cast<VEGABackingStore_FBO*>(mapstore)->GetReadRenderTarget();
		if (!rt || rt->getType() != VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
			return OpStatus::ERR;

		dispmapTex = static_cast<VEGA3dFramebufferObject*>(rt)->getAttachedColorTexture();
	}
	else if (mapstore->IsA(VEGABackingStore::TEXTURE))
		dispmapTex = static_cast<VEGABackingStore_Texture*>(mapstore)->GetTexture();
	else
		return OpStatus::ERR;
	if (!dispmapTex)
		return OpStatus::ERR;

	// FIXME: HW codepath needs to be updated to match the SW codepath, see CORE-22199.
	OP_STATUS status = applyShader(destStore, region, frame);
	if (OpStatus::IsError(status))
		status = applyFallback(destStore, region);

	return status;
}
#endif // VEGA_3DDEVICE

#if 0
// Boilerplate for interpolation of sourceimage -
// needs (tiny) API change
void VEGAFilterDisplace::applyLerp()
{
	for (yp = 0; yp < height; ++yp)
	{
		for (xp = 0; xp < width; ++xp)
		{
			dmap.LoadUnpack(comp[0], comp[1], comp[2], comp[3]);

			int fix_diff_x = fix_scale * comp[x_comp] + fix_ofs;
			int fix_diff_y = fix_scale * comp[y_comp] + fix_ofs;

			int fix_sx = (xp << 8) + fix_diff_x + 0x80;
			int fix_sy = (yp << 8) + fix_diff_y + 0x80;

			int floor_sx = fix_sx & (~0xff);
			int floor_sy = fix_sy & (~0xff);

			int isx = fix_sx >> 8;
			int isy = fix_sy >> 8;
			unsigned int fsx = fix_sx - floor_sx;
			unsigned int fsy = fix_sy - floor_sy;

			unsigned int srcofs = isx + isy * srcPixelStride;

 			unsigned int inside = INSIDE(isx, width) ? 1 : 0;
			inside |= INSIDE(isx+1, width) ? 2 : 0;
			inside |= INSIDE(isy, height) ? 4 : 0;
			inside |= INSIDE(isy+1, height) ? 8 : 0;

			unsigned int c1, c2, c3, c4;
			if ((inside & 0xf) != 0xf)
			{
				if (!inside ||
					!(inside & (1|2)) ||
					!(inside & (4|8)))
				{
					dst.Store(0);

					++dmap;
					++dst;
					continue;
				}

				c1 = c2 = c3 = c4 = 0;
				if ((inside & (1|4)) == (1|4)) // 0,0
					c1 = src.LoadRel(srcofs);
				if ((inside & (2|4)) == (2|4)) // 1,0
					c2 = src.LoadRel(srcofs + 1);
				if ((inside & (1|8)) == (1|8)) // 0,1
					c3 = src.LoadRel(srcofs + srcPixelStride);
				if ((inside & (2|8)) == (2|8)) // 1,1
					c4 = src.LoadRel(srcofs + srcPixelStride + 1);

				// The pixels should be premultiplied
				// to yield the correct visual appearance
			}
			else
			{
				c1 = src.LoadRel(srcofs);
				c2 = src.LoadRel(srcofs + 1);
				c3 = src.LoadRel(srcofs + srcPixelStride);
				c4 = src.LoadRel(srcofs + srcPixelStride + 1);
			}

			unsigned int c1rb = c1 & 0xff00ff;
			unsigned int c1ag = (c1 >> 8) & 0xff00ff;

			unsigned int c2rb = c2 & 0xff00ff;
			unsigned int c2ag = (c2 >> 8) & 0xff00ff;

			c1rb *= 255-fsx;
			c1ag *= 255-fsx;
			c2rb *= fsx;
			c2ag *= fsx;

			c1rb >>= 8;
			c1ag >>= 8;
			c2rb >>= 8;
			c2ag >>= 8;

			c1rb += c2rb;
			c1ag += c2ag;

			c1rb &= 0xff00ff;
			c1ag &= 0xff00ff;

			c2rb = c3 & 0xff00ff;
			c2ag = (c3 >> 8) & 0xff00ff;

			unsigned int c3rb = c4 & 0xff00ff;
			unsigned int c3ag = (c4 >> 8) & 0xff00ff;

			c2rb *= 255-fsx;
			c2ag *= 255-fsx;
			c3rb *= fsx;
			c3ag *= fsx;

			c2rb >>= 8;
			c2ag >>= 8;
			c3rb >>= 8;
			c3ag >>= 8;

			c2rb += c3rb;
			c2ag += c3ag;

			c2rb &= 0xff00ff;
			c2ag &= 0xff00ff;

			c1rb *= 255-fsy;
			c1ag *= 255-fsy;
			c2rb *= fsy;
			c2ag *= fsy;

			c1rb >>= 8;
			c1ag >>= 8;
			c2rb >>= 8;
			c2ag >>= 8;

			c1rb += c2rb;
			c1ag += c2ag;

			dst.Store((c1rb & 0xff00ff) | ((c1ag & 0xff00ff) << 8));

			++dmap;
			++dst;
		}

		dmap += dmapPixelStride - width;
		dst += dstPixelStride - width;
	}
}
#endif // 0

OP_STATUS VEGAFilterDisplace::apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region)
{
	OP_ASSERT(mapstore);

	VEGASWBuffer* dmbuf = mapstore->BeginTransaction(VEGABackingStore::ACC_READ_ONLY);
	if (!dmbuf)
		return OpStatus::ERR_NO_MEMORY;

	dispmap = *dmbuf;

	VEGAConstPixelAccessor dmap = dispmap.GetConstAccessor(region.sx, region.sy);
	VEGAConstPixelAccessor src = source.GetConstAccessor(0, 0);
	VEGAPixelAccessor dst = dest.GetAccessor(region.dx, region.dy);

	unsigned int dstPixelStride = dest.GetPixelStride();
	unsigned int srcPixelStride = source.GetPixelStride();
	unsigned int dmapPixelStride = dispmap.GetPixelStride();

	// Perform calculations in an 24.8 fixed point format
	int fix_scale = VEGA_FIXTOSCALEDINT(scale / 255, 8);
	int fix_ofs = -VEGA_FIXTOSCALEDINT(VEGA_FIXDIV2(scale), 8);

// [v >= 0 && v < m] ==> [(unsigned)v < (unsigned)m]
#define INSIDE(v,max) ((unsigned int)(v) < (unsigned int)(max))

	int comp[4]; /* ARRAY OK 2008-10-27 fs */

	for (unsigned yp = 0; yp < region.height; ++yp)
	{
#ifdef USE_565_DISPLACEMENT
		int disp_map_offsets_y[DISP_565_BLUR_RADIUS];
		for (int i = 0; i < DISP_565_BLUR_RADIUS; ++i)
		{
			disp_map_offsets_y[i] = i-DISP_565_BLUR_LEFT;
			if ((int)yp + disp_map_offsets_y[i] < 0)
				disp_map_offsets_y[i] = -(int)yp;
			else if ((int)yp + disp_map_offsets_y[i] >= (int)region.height)
				disp_map_offsets_y[i] = region.height-(int)yp-1;
		}
		unsigned int sum_x_comp = 0;
		unsigned int sum_y_comp = 0;
		for (unsigned int blur_yp = 0; blur_yp < DISP_565_BLUR_RADIUS; ++blur_yp)
		{
			VEGA_PIXEL d = dmap.LoadRel(disp_map_offsets_y[blur_yp]*(int)dmapPixelStride);
#ifdef USE_PREMULTIPLIED_ALPHA
			d = VEGAPixelUnpremultiplyFast(d);
#endif
			comp[0] = VEGA_UNPACK_A(d);
			comp[1] = VEGA_UNPACK_R(d);
			comp[2] = VEGA_UNPACK_G(d);
			comp[3] = VEGA_UNPACK_B(d);

			// All samples on the left of current and current are 0
			// Since current is really -1, first on the right is also 0
			sum_x_comp += comp[x_comp]*(DISP_565_BLUR_LEFT+2);
			sum_y_comp += comp[y_comp]*(DISP_565_BLUR_LEFT+2);
			const int y_offset = disp_map_offsets_y[blur_yp]*(int)dmapPixelStride;
			for (unsigned int blur_xp = 1; blur_xp < DISP_565_BLUR_RIGHT; ++blur_xp)
			{
				int xo = blur_xp;
				if (xo >= (int)region.width)
					xo = region.width-1;
				VEGA_PIXEL d = dmap.LoadRel(y_offset + xo);
#ifdef USE_PREMULTIPLIED_ALPHA
				d = VEGAPixelUnpremultiplyFast(d);
#endif
				comp[0] = VEGA_UNPACK_A(d);
				comp[1] = VEGA_UNPACK_R(d);
				comp[2] = VEGA_UNPACK_G(d);
				comp[3] = VEGA_UNPACK_B(d);

				sum_x_comp += comp[x_comp];
				sum_y_comp += comp[y_comp];
			}
		}
#endif // USE_565_DISPLACEMENT
		for (unsigned xp = 0; xp < region.width; ++xp)
		{
#ifdef USE_565_DISPLACEMENT
			int x_offset_remove = -(DISP_565_BLUR_RADIUS/2);
			int x_offset_add = (DISP_565_BLUR_RADIUS/2);
			if ((int)xp+x_offset_remove < 0)
				x_offset_remove = -(int)xp;
			if ((int)xp+x_offset_add >= (int)region.width)
				x_offset_add = region.width-1-(int)xp;
			for (unsigned int blur_yp = 0; blur_yp < DISP_565_BLUR_RADIUS; ++blur_yp)
			{
				const int y_offset = disp_map_offsets_y[blur_yp]*(int)dmapPixelStride;

				VEGA_PIXEL d = dmap.LoadRel(y_offset + x_offset_remove);
#ifdef USE_PREMULTIPLIED_ALPHA
				d = VEGAPixelUnpremultiplyFast(d);
#endif
				comp[0] = VEGA_UNPACK_A(d);
				comp[1] = VEGA_UNPACK_R(d);
				comp[2] = VEGA_UNPACK_G(d);
				comp[3] = VEGA_UNPACK_B(d);

				sum_x_comp -= comp[x_comp];
				sum_y_comp -= comp[y_comp];

				d = dmap.LoadRel(y_offset + x_offset_add);
#ifdef USE_PREMULTIPLIED_ALPHA
				d = VEGAPixelUnpremultiplyFast(d);
#endif
				comp[0] = VEGA_UNPACK_A(d);
				comp[1] = VEGA_UNPACK_R(d);
				comp[2] = VEGA_UNPACK_G(d);
				comp[3] = VEGA_UNPACK_B(d);

				sum_x_comp += comp[x_comp];
				sum_y_comp += comp[y_comp];
			}

			int fix_diff_x = fix_scale * sum_x_comp/(DISP_565_BLUR_RADIUS*DISP_565_BLUR_RADIUS) + fix_ofs;
			int fix_diff_y = fix_scale * sum_y_comp/(DISP_565_BLUR_RADIUS*DISP_565_BLUR_RADIUS) + fix_ofs;
#else // !USE_565_DISPLACEMENT
#ifdef USE_PREMULTIPLIED_ALPHA
			VEGA_PIXEL d = VEGAPixelUnpremultiplyFast(dmap.Load());

			comp[0] = VEGA_UNPACK_A(d);
			comp[1] = VEGA_UNPACK_R(d);
			comp[2] = VEGA_UNPACK_G(d);
			comp[3] = VEGA_UNPACK_B(d);
#else
			dmap.LoadUnpack(comp[0], comp[1], comp[2], comp[3]);
#endif
			// The computation below is
			// fix_scale * 0x100*(comp[x/y_comp]/0xFF) + fix_ofs,
			// slightly rearranged for integer arithmetic. This is to linearly
			// map the color range 0x0-0xFF onto the fixed-point range 0.0-1.0,
			// represented by the range 0x0-0x100.
			int fix_diff_x = fix_scale * ((comp[x_comp] << 8)/0xFF) + fix_ofs;
			int fix_diff_y = fix_scale * ((comp[y_comp] << 8)/0xFF) + fix_ofs;
#endif

			VEGA_PIXEL cval;

			int disp_x;
			int disp_y;

#ifdef DISPLACE_LINEAR
			disp_x = region.sx + xp + (fix_diff_x >> 8);
			disp_y = region.sy + yp + (fix_diff_y >> 8);
			if (INSIDE(disp_x, source.width) && INSIDE(disp_y, source.height) &&
				INSIDE(disp_x+1, source.width) && INSIDE(disp_y+1, source.height))
			{
#if VEGA_SAMPLER_FRACBITS > 8
				disp_x = (xp<<VEGA_SAMPLER_FRACBITS) + (fix_diff_x<<(VEGA_SAMPLER_FRACBITS-8));
				disp_y = (yp<<VEGA_SAMPLER_FRACBITS) + (fix_diff_y<<(VEGA_SAMPLER_FRACBITS-8));
#elif VEGA_SAMPLER_FRACBITS < 8
				disp_x = (xp<<VEGA_SAMPLER_FRACBITS) + (fix_diff_x>>(8-VEGA_SAMPLER_FRACBITS));
				disp_y = (yp<<VEGA_SAMPLER_FRACBITS) + (fix_diff_y>>(8-VEGA_SAMPLER_FRACBITS));
#else
				disp_x = (xp<<VEGA_SAMPLER_FRACBITS) + fix_diff_x;
				disp_y = (yp<<VEGA_SAMPLER_FRACBITS) + fix_diff_y;
#endif
				cval = VEGASamplerBilerpPM(src.Ptr(), srcPixelStride, disp_x, disp_y);
			}
			else
#endif
			{
				disp_x = region.sx + xp + ((fix_diff_x + 0x80) >> 8);
				disp_y = region.sy + yp + ((fix_diff_y + 0x80) >> 8);
				if (INSIDE(disp_x, source.width) && INSIDE(disp_y, source.height))
				{
					cval = src.LoadRel(disp_x + disp_y * srcPixelStride);
				}
				else
				{
					cval = 0;
				}
			}

			dst.Store(cval);

			++dmap;
			++dst;
		}

		dmap += dmapPixelStride - region.width;
		dst += dstPixelStride - region.width;
	}

	mapstore->EndTransaction(FALSE);

	return OpStatus::OK;
}

#undef INSIDE

#endif // VEGA_SUPPORT
