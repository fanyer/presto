/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegafilter.h"
#include "modules/libvega/vegafill.h"
#include "modules/libvega/src/vegaimage.h"

#include "modules/libvega/src/vegabackend_sw.h"

#ifdef VEGA_2DDEVICE
#include "modules/libvega/vega2ddevice.h"
#include "modules/libvega/src/vegabackend_hw2d.h"
#endif // VEGA_2DDEVICE

#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"
#include "modules/libvega/src/vegabackend_hw3d.h"
#endif // VEGA_3DDEVICE

VEGAFilter::VEGAFilter() :
#ifdef VEGA_2DDEVICE
	sourceSurf(NULL),
#endif // VEGA_2DDEVICE
#ifdef VEGA_3DDEVICE
	sourceRT(NULL),
	releaseSourceRT(FALSE),
#endif // VEGA_3DDEVICE
	sourceStore(NULL), sourceAlphaOnly(false)
{
	source.Reset();
}

VEGAFilter::~VEGAFilter()
{
#ifdef VEGA_3DDEVICE
	if (releaseSourceRT)
		VEGARefCount::DecRef(static_cast<VEGA3dFramebufferObject*>(sourceRT));
#endif // VEGA_3DDEVICE
}

void VEGAFilter::setSource(VEGARenderTarget* srcRT, bool alphaOnly)
{
	sourceStore = srcRT->GetBackingStore();

	setBackendSource();

	sourceAlphaOnly = alphaOnly;
}
void VEGAFilter::setSource(VEGAFill* srcImg, bool alphaOnly)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	OP_ASSERT(srcImg->isImage());
#endif // VEGA_OPPAINTER_SUPPORT

	sourceStore = static_cast<VEGAImage*>(srcImg)->GetBackingStore();

	setBackendSource();

	sourceAlphaOnly = alphaOnly;
}
void VEGAFilter::setBackendSource()
{
	OP_ASSERT(sourceStore);

#ifdef VEGA_3DDEVICE
	releaseSourceRT = FALSE;
	if (sourceStore->IsA(VEGABackingStore::TEXTURE))
	{
		VEGA3dFramebufferObject* fbo;
		RETURN_VOID_IF_ERROR(g_vegaGlobals.vega3dDevice->createFramebuffer(&fbo));
		fbo->attachColor(static_cast<VEGABackingStore_Texture*>(sourceStore)->GetTexture());
		sourceRT = fbo;
		releaseSourceRT = TRUE;
	}
	else if (sourceStore->IsA(VEGABackingStore::FRAMEBUFFEROBJECT))
		sourceRT = static_cast<VEGABackingStore_FBO*>(sourceStore)->GetReadRenderTarget();
	else
#endif // VEGA_3DDEVICE
#ifdef VEGA_2DDEVICE
	if (sourceStore->IsA(VEGABackingStore::SURFACE))
		sourceSurf = static_cast<VEGABackingStore_Surface*>(sourceStore)->GetSurface();
	else
#endif // VEGA_2DDEVICE
	if (sourceStore->IsA(VEGABackingStore::SWBUFFER))
		source = *static_cast<VEGABackingStore_SWBuffer*>(sourceStore)->GetBuffer();
}

OP_STATUS VEGAFilter::checkRegion(VEGAFilterRegion& region, unsigned int destWidth, unsigned int destHeight)
{
	unsigned src_width = sourceStore->GetWidth();
	unsigned src_height = sourceStore->GetHeight();

	// Make sure the start position is not out of bounds
	if (region.sx >= src_width || region.sy >= src_height ||
		region.dx >= destWidth || region.dy >= destHeight)
		return OpStatus::ERR;

	if (region.width + region.sx > src_width)
		region.width = src_width - region.sx;
	if (region.width + region.dx > destWidth)
		region.width = destWidth - region.dx;

	if (region.height + region.sy > src_height)
		region.height = src_height - region.sy;
	if (region.height + region.dy > destHeight)
		region.height = destHeight - region.dy;

	return OpStatus::OK;
}

#ifdef VEGA_2DDEVICE
OP_STATUS VEGAFilter::apply(VEGA2dSurface* destRT, const VEGAFilterRegion& region)
{
	// FIXME: Passing a backing store would make the code a bit
	// neater, and it would probably even allow merging this with
	// applyFallback.
	bool is_inplace = sourceStore->IsA(VEGABackingStore::SURFACE) &&
		static_cast<VEGABackingStore_Surface*>(sourceStore)->GetSurface() == destRT;

	OP_STATUS status;
	if (is_inplace)
	{
		// Source and dest are the same
		VEGASWBuffer* dstbuf = sourceStore->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
		if (!dstbuf)
			return OpStatus::ERR_NO_MEMORY;

		source = *dstbuf;

		status = apply(*dstbuf, region);

		sourceStore->EndTransaction(TRUE);
	}
	else
	{
		VEGASWBuffer* srcbuf = sourceStore->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
		if (!srcbuf)
			return OpStatus::ERR_NO_MEMORY;

		source = *srcbuf;

		VEGA2dDevice::SurfaceData dst;
		dst.x = 0;
		dst.y = 0;
		dst.w = destRT->getWidth();
		dst.h = destRT->getHeight();

		VEGA2dDevice* dev2d = g_vegaGlobals.vega2dDevice;
		dev2d->setRenderTarget(destRT);
		status = dev2d->lockRect(&dst, VEGA2dDevice::READ_INTENT | VEGA2dDevice::WRITE_INTENT);

		if (OpStatus::IsSuccess(status))
		{
			VEGASWBuffer dstbuf;
			dstbuf.CreateFromData(dst.pixels.buffer, dst.pixels.stride, dst.w, dst.h);
			status = apply(dstbuf, region);

			dev2d->setRenderTarget(destRT);
			dev2d->unlockRect(&dst);
		}

		sourceStore->EndTransaction(FALSE);
	}

	return status;
}
#endif // VEGA_2DDEVICE

#ifdef VEGA_3DDEVICE
/* static */
bool VEGAFilter::isSubRegion(const VEGAFilterRegion& region, VEGA3dTexture* tex)
{
	return
		region.sx != 0 || region.sy != 0 ||
		region.width != tex->getWidth() ||
		region.height != tex->getHeight();
}

OP_STATUS VEGAFilter::applyFallback(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region)
{
	VEGA3dDevice::FramebufferData srcFB;
	srcFB.x = region.sx;
	srcFB.y = region.sy;
	srcFB.w = region.width;
	srcFB.h = region.height;

	OP_STATUS status = OpStatus::OK;
	VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;
	dev3d->setRenderTarget(sourceRT);
	RETURN_IF_ERROR(dev3d->lockRect(&srcFB, true));

	status = source.Bind(&srcFB.pixels, true);
	if (OpStatus::IsError(status))
	{
		dev3d->unlockRect(&srcFB, false);
		return status;
	}

	VEGASWBuffer* destBuffer;
	if (destStore->IsReadRenderTarget(sourceRT))
	{
		destStore->GetReadRenderTarget();
		destStore->DisableMultisampling();
		destBuffer = &source;
	}
	else
	{
		destBuffer = destStore->BeginTransaction(OpRect(region.dx, region.dy, region.width, region.height), VEGABackingStore::ACC_WRITE_ONLY);
		if (!destBuffer)
		{
			dev3d->unlockRect(&srcFB, false);
			return OpStatus::ERR;
		}
	}

	// Apply filter in a local region (src is located at sx,sy and dst is located at dx,dy)
	VEGAFilterRegion localRegion;
	localRegion.sx = localRegion.dx = localRegion.sy = localRegion.dy = 0;
	localRegion.width = region.width;
	localRegion.height = region.height;

	status = apply(*destBuffer, localRegion);

	if (destStore->IsReadRenderTarget(sourceRT))
	{
		// Upload to the right position in the destination buffer
		srcFB.x = region.dx;
		srcFB.y = region.dy;

		source.Sync(&srcFB.pixels);
		dev3d->unlockRect(&srcFB, true);
	}
	else
	{
		dev3d->unlockRect(&srcFB, false);
		destStore->EndTransaction(TRUE);
	}

	destStore->Validate();
	source.Unbind(&srcFB.pixels);

	return status;
}

static inline void VEGA_SetVertex(VEGA3dDevice::Vega2dVertex& vtx, unsigned int x, unsigned int y,
								  VEGA3dTexture* tex, unsigned int s, unsigned int t,
								  UINT32 color = 0xffffffff)
{
	vtx.x = (float)x;
	vtx.y = (float)y;
	vtx.s = (float)s/tex->getWidth();
	vtx.t = (float)t/tex->getHeight();
	vtx.color = color;
}

static inline void VEGA_CopyOffsetVertex(VEGA3dDevice::Vega2dVertex& src, VEGA3dDevice::Vega2dVertex& tgt,
                                         int offs_x, int offs_y)
{
	tgt.x = src.x + offs_x;
	tgt.y = src.y + offs_y;
	tgt.s = src.s;
	tgt.t = src.t;
	tgt.color = src.color;
}

OP_STATUS VEGAFilter::createVertexBuffer_Unary(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout,
											   unsigned int* out_startIndex,
											   VEGA3dTexture* tex, VEGA3dShaderProgram* sprog,
											   const VEGAFilterRegion& rg, UINT32 color)
{
	color = device->convertToNativeColor(color);
	VEGA3dDevice::Vega2dVertex verts[4];
	VEGA_SetVertex(verts[0], rg.dx, rg.dy, tex, rg.sx, rg.sy, color);
	VEGA_SetVertex(verts[1], rg.dx, rg.dy+rg.height, tex, rg.sx, rg.sy+rg.height, color);
	VEGA_SetVertex(verts[2], rg.dx+rg.width, rg.dy, tex, rg.sx+rg.width, rg.sy, color);
	VEGA_SetVertex(verts[3], rg.dx+rg.width, rg.dy+rg.height, tex, rg.sx+rg.width, rg.sy+rg.height, color);

	VEGA3dBuffer* vbuf = NULL;
	VEGA3dVertexLayout* vlayout = NULL;
	OP_STATUS err = device->createVega2dVertexLayout(&vlayout, sprog->getShaderType());
	if (OpStatus::IsSuccess(err))
	{
		vbuf = device->getTempBuffer(4*sizeof(VEGA3dDevice::Vega2dVertex));
		if (vbuf)
			VEGARefCount::IncRef(vbuf);
		else
			err = OpStatus::ERR;
	}
	if (OpStatus::IsSuccess(err))
	{
		if (out_startIndex)
			err = vbuf->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, *out_startIndex);
		else
			err = vbuf->writeAtOffset(0, 4*sizeof(VEGA3dDevice::Vega2dVertex), verts);
	}
	*out_vbuf = vbuf;
	*out_vlayout = vlayout;
	return err;
}

struct VegaBinaryFilterVertex
{
	float x, y;
	float s0, t0;
	float s1, t1;
};

static inline void VEGA_SetVertex(VegaBinaryFilterVertex& vtx, unsigned int x, unsigned int y,
								  VEGA3dTexture* tex0, unsigned int s0, unsigned int t0,
								  VEGA3dTexture* tex1)
{
	vtx.x = (float)x;
	vtx.y = (float)y;
	vtx.s0 = (float)s0/tex0->getWidth();
	vtx.t0 = (float)t0/tex0->getHeight();
	vtx.s1 = (float)x/tex1->getWidth();
	vtx.t1 = (float)y/tex1->getHeight();
}

OP_STATUS VEGAFilter::createVertexBuffer_Binary(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout,
												unsigned int* out_startIndex,
												VEGA3dTexture* tex0, VEGA3dTexture* tex1, VEGA3dShaderProgram* sprog,
												const VEGAFilterRegion& rg)
{
	VegaBinaryFilterVertex verts[4];
	VEGA_SetVertex(verts[0], rg.dx, rg.dy, tex0, rg.sx, rg.sy, tex1);
	VEGA_SetVertex(verts[1], rg.dx, rg.dy+rg.height, tex0, rg.sx, rg.sy+rg.height, tex1);
	VEGA_SetVertex(verts[2], rg.dx+rg.width, rg.dy, tex0, rg.sx+rg.width, rg.sy, tex1);
	VEGA_SetVertex(verts[3], rg.dx+rg.width, rg.dy+rg.height, tex0, rg.sx+rg.width, rg.sy+rg.height, tex1);

	VEGA3dBuffer* vbuf = NULL;
	VEGA3dVertexLayout* vlayout = NULL;
	OP_STATUS err = OpStatus::OK;
	vbuf = device->getTempBuffer(4*sizeof(VegaBinaryFilterVertex));
	if (vbuf)
		VEGARefCount::IncRef(vbuf);
	else
		err = OpStatus::ERR;
	if (OpStatus::IsSuccess(err))
	{
		if (out_startIndex)
			err = vbuf->writeAnywhere(4, sizeof(VegaBinaryFilterVertex), verts, *out_startIndex);
		else
			err = vbuf->writeAtOffset(0, 4*sizeof(VegaBinaryFilterVertex), verts);
	}
	if (OpStatus::IsSuccess(err))
		err = device->createVertexLayout(&vlayout, sprog);
	if (OpStatus::IsSuccess(err))
		err = vlayout->addComponent(vbuf, 0, 0, sizeof(VegaBinaryFilterVertex), VEGA3dVertexLayout::FLOAT2, false);
	if (OpStatus::IsSuccess(err))
		err = vlayout->addComponent(vbuf, 1, 8, sizeof(VegaBinaryFilterVertex), VEGA3dVertexLayout::FLOAT2, false);
	if (OpStatus::IsSuccess(err))
		err = vlayout->addComponent(vbuf, 3, 16, sizeof(VegaBinaryFilterVertex), VEGA3dVertexLayout::FLOAT2, false);
	if (OpStatus::IsError(err))
	{
		VEGARefCount::DecRef(vbuf);
		vbuf = NULL;
		VEGARefCount::DecRef(vlayout);
		vlayout = NULL;
	}
	*out_vbuf = vbuf;
	*out_vlayout = vlayout;
	return err;
}

OP_STATUS VEGAFilter::setupVertexBuffer(VEGA3dDevice* device,
										VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout, unsigned int* out_startIndex, VEGA3dTexture* tex, VEGA3dShaderProgram* sprog,
										const VEGAFilterRegion& region)
{
	return createVertexBuffer_Unary(device, out_vbuf, out_vlayout, out_startIndex, tex, sprog, region);
}

OP_STATUS VEGAFilter::cloneToTempTexture(VEGA3dDevice* device, VEGA3dRenderTarget* copyRT,
									 unsigned int dx, unsigned int dy,
									 unsigned int width, unsigned int height,
									 bool keepPosition,
									 VEGA3dTexture** texclone)
{
	*texclone = NULL;

	device->setRenderTarget(copyRT);

	VEGA3dTexture* tex = device->getTempTexture(width, height);
	if (!tex)
		return OpStatus::ERR;
	VEGARefCount::IncRef(tex);

	OP_STATUS status = device->copyToTexture(tex, VEGA3dTexture::CUBE_SIDE_NONE, 0, keepPosition?dx:0, keepPosition?dy:0, dx, dy, width, height);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(tex);
	}

	*texclone = tex;
	return status;
}

// static
OP_STATUS VEGAFilter::createClampTexture(VEGA3dDevice* device, VEGA3dTexture* copyTex,
							 unsigned int& sx, unsigned int& sy,
							 unsigned int width, unsigned int height,
							 unsigned int borderWidth, VEGAFilterEdgeMode clampMode,
							 VEGA3dFramebufferObject* clampTexRT, VEGA3dTexture** clampTex)
{
	*clampTex = NULL;


	VEGA3dTexture* tex = NULL;
	unsigned int neww = width;
	unsigned int newh = height;
	if (clampMode == VEGAFILTEREDGE_WRAP)
	{
		// gist: create a bigger texture with original image data
		// in the middle, and add enough tiles of the original
		// image around it so that wrapping is not needed
		neww = width  + 2*borderWidth;
		newh = height + 2*borderWidth;
	}
	else if (clampMode == VEGAFILTEREDGE_NONE)
	{
		// gist: create a texture with the original image data in the
		// middle and a 1px transparent black border
		neww = width+2;
		newh = height+2;
	}
	RETURN_IF_ERROR(device->createTexture(&tex, neww, newh, VEGA3dTexture::FORMAT_RGBA8888));
	OP_STATUS err = clampTexRT->attachColor(tex);
	if (OpStatus::IsSuccess(err))
		err = updateClampTexture(device, copyTex, sx, sy, width, height, borderWidth, clampMode, clampTexRT);
	if (OpStatus::IsError(err))
	{
		clampTexRT->attachColor((VEGA3dTexture*)NULL);
		VEGARefCount::DecRef(tex);
		return err;
	}

	// newly created texture needs to have its wrap mode set as well
	switch (clampMode)
	{
	case VEGAFILTEREDGE_WRAP:
		tex->setWrapMode(VEGA3dTexture::WRAP_REPEAT, VEGA3dTexture::WRAP_REPEAT);
		break;
	case VEGAFILTEREDGE_DUPLICATE:
	case VEGAFILTEREDGE_NONE:
		tex->setWrapMode(VEGA3dTexture::WRAP_CLAMP_EDGE, VEGA3dTexture::WRAP_CLAMP_EDGE);
		break;
	}

	*clampTex = tex;
	return OpStatus::OK;
}

// static
OP_STATUS VEGAFilter::updateClampTexture(VEGA3dDevice* device, VEGA3dTexture* copyTex,
							 unsigned int& sx, unsigned int& sy,
							 unsigned int width, unsigned int height,
							 unsigned int borderWidth, VEGAFilterEdgeMode clampMode,
							 VEGA3dFramebufferObject* clampTexRT, unsigned int minBorder)
{
	VEGA3dShaderProgram::WrapMode mode;
	OP_STATUS err = OpStatus::OK;
	VEGA3dDevice::Vega2dVertex verts_s[20];
	VEGA3dDevice::Vega2dVertex* verts = verts_s;
	OpAutoArray<VEGA3dDevice::Vega2dVertex> _verts;
	unsigned int numverts = 0;
	switch (clampMode)
	{
	case VEGAFILTEREDGE_WRAP:
		mode = VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;
		if (clampTexRT->getWidth()  != width  + 2*borderWidth ||
		    clampTexRT->getHeight() != height + 2*borderWidth)
		{
			OP_ASSERT(!"Wrong size of texture, cannot update clamp texture");
			return OpStatus::ERR;
		}
		{
			// gist: create a bigger texture with original image data
			// in the middle, and add enough tiles of the original
			// image around it so that wrapping is not needed

			// number of tiles to each side of the center image
			int repeats_x_side = (borderWidth + width -1) / width;
			int repeats_y_side = (borderWidth + height-1) / height;
			// total number of tiles
			unsigned int tiles = (2*repeats_x_side+1) * (2*repeats_y_side+1);
			numverts = 4*tiles;

			if (numverts > ARRAY_SIZE(verts_s))
			{
				_verts.reset(OP_NEWA(VEGA3dDevice::Vega2dVertex, numverts));
				verts = _verts.get();
				RETURN_OOM_IF_NULL(verts);
			}

			// offset in verts to the first of the four vertices making up the center tile
			const size_t center_offs = 4 * (repeats_y_side*(2*repeats_x_side + 1) + repeats_x_side);
			// set up center tile
			VEGA_SetVertex(verts[center_offs+0],
			               borderWidth, borderWidth,
			               copyTex,
			               sx, sy);
			VEGA_SetVertex(verts[center_offs+1], borderWidth, borderWidth + height,
			               copyTex,
			               sx, sy+height);
			VEGA_SetVertex(verts[center_offs+2],
			               borderWidth + width, borderWidth + height,
			               copyTex,
			               sx+width, sy+height);
			VEGA_SetVertex(verts[center_offs+3],
			               borderWidth + width, borderWidth,
			               copyTex,
			               sx+width, sy);

			// create the surrounding tiles by cloning and ofsetting
			// the vertices of the center tile
			size_t idx = 0;
			for (int y = -repeats_y_side; y <= repeats_y_side; ++y)
			{
				int y_offs = y*height;
				for (int x = -repeats_x_side; x <= repeats_x_side; ++x)
				{
					// center tile is already set
					if (x || y)
					{
						int x_offs = x*width;
						for (unsigned int i = 0; i < 4; ++i)
						{
							OP_ASSERT(center_offs != idx);
							VEGA_CopyOffsetVertex(verts[center_offs+i], verts[idx+i], x_offs, y_offs);
						}
					}
					idx += 4;
				}
			}
			OP_ASSERT(idx == numverts);
			// this is the offset from the top-left corner of the
			// texture to the top-left corner of the center tile
			sx = sy = borderWidth;
		}
		break;
	case VEGAFILTEREDGE_DUPLICATE:
		mode = VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;
		if (clampTexRT->getWidth() != width || clampTexRT->getHeight() != height)
		{
			OP_ASSERT(!"Wrong size of texture, cannot update clamp texture");
			return OpStatus::ERR;
		}
		numverts = 4;
		VEGA_SetVertex(verts[0], 0, 0, copyTex, sx, sy);
		VEGA_SetVertex(verts[1], 0, height, copyTex, sx, sy+height);
		VEGA_SetVertex(verts[2], width, height, copyTex, sx+width, sy+height);
		VEGA_SetVertex(verts[3], width, 0, copyTex, sx+width, sy);
		copyTex->setWrapMode(VEGA3dTexture::WRAP_CLAMP_EDGE, VEGA3dTexture::WRAP_CLAMP_EDGE);

		sx = sy = 0;
		break;
	case VEGAFILTEREDGE_NONE:
	default:
		mode = VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;
		if ((clampTexRT->getWidth() != width+2 || clampTexRT->getHeight() != height+2) && !minBorder)
		{
			OP_ASSERT(!"Wrong size of texture, cannot update clamp texture");
			return OpStatus::ERR;
		}
		numverts = 20;
		VEGA_SetVertex(verts[0], 1, 1, copyTex, sx, sy);
		VEGA_SetVertex(verts[1], 1, height+1, copyTex, sx, sy+height);
		VEGA_SetVertex(verts[2], width+1, height+1, copyTex, sx+width, sy+height);
		VEGA_SetVertex(verts[3], width+1, 1, copyTex, sx+width, sy);

		VEGA_SetVertex(verts[4], 0, 0, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[5], 0, height+2+minBorder, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[6], 1, height+2+minBorder, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[7], 1, 0, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[8], 1, 0, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[9], 1, 1, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[10], width+1, 1, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[11], width+1, 0, copyTex, 0, 0, 0);

		VEGA_SetVertex(verts[12], width+1, 0, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[13], width+1, height+2, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[14], width+2+minBorder, height+2+minBorder, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[15], width+2+minBorder, 0, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[16], 1, height+1, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[17], 1, height+2+minBorder, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[18], width+1, height+2+minBorder, copyTex, 0, 0, 0);
		VEGA_SetVertex(verts[19], width+1, height+1, copyTex, 0, 0, 0);
		copyTex->setWrapMode(VEGA3dTexture::WRAP_CLAMP_EDGE, VEGA3dTexture::WRAP_CLAMP_EDGE);

		sx = sy = 1;
		break;
	};

	VEGA3dBuffer* vbuf = NULL;
	VEGA3dVertexLayout* vlayout = NULL;
	if (OpStatus::IsSuccess(err))
	{
		vbuf = device->getTempBuffer(numverts*sizeof(VEGA3dDevice::Vega2dVertex));
		if (vbuf)
			VEGARefCount::IncRef(vbuf);
		else
			err = OpStatus::ERR;
	}
	unsigned int firstVert = 0;
	if (OpStatus::IsSuccess(err))
		err = vbuf->writeAnywhere(numverts, sizeof(VEGA3dDevice::Vega2dVertex), verts, firstVert);

	if (OpStatus::IsSuccess(err))
	{
		VEGA3dShaderProgram* shd = NULL;
		err = device->createShaderProgram(&shd, VEGA3dShaderProgram::SHADER_VECTOR2D, mode);
		if (OpStatus::IsSuccess(err))
			err = device->createVega2dVertexLayout(&vlayout, VEGA3dShaderProgram::SHADER_VECTOR2D);
		if (OpStatus::IsSuccess(err))
		{
			device->setRenderTarget(clampTexRT);

			VEGA3dRenderState* state = device->getDefault2dNoBlendNoScissorRenderState();
			device->setRenderState(state);

			device->setShaderProgram(shd);
			shd->setOrthogonalProjection();
			device->setTexture(0, copyTex);
			unsigned int firstInd = (firstVert/4)*6;
			err = device->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, vlayout, device->getQuadIndexBuffer(firstInd + (numverts/4)*6), 2, firstInd, (numverts/4)*6);
		}
		VEGARefCount::DecRef(shd);
		VEGARefCount::DecRef(vlayout);
	}

	VEGARefCount::DecRef(vbuf);

	return err;
}


#define NEEDS_POW2_WORKAROUND(w,h) (!g_vegaGlobals.vega3dDevice->fullNPotSupport() && !(g_vegaGlobals.vega3dDevice->isPow2(w) && g_vegaGlobals.vega3dDevice->isPow2(h)))
OP_STATUS VEGAFilter::applyShader(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& in_region, unsigned int frame,
								  unsigned int borderWidth, VEGAFilterEdgeMode edgeMode)
{
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	OP_ASSERT(device);

	VEGAFilterRegion region = in_region; // This could end up being modified

	OP_STATUS err;
	VEGA3dTexture* tmpSource = NULL;
	VEGA3dTexture* srctex = NULL;
	VEGA3dRenderTarget* destRT = destStore->GetWriteRenderTarget(frame);
	if (sourceRT->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
		srctex = static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture();
	if (sourceRT == destRT || !srctex)
	{
		tmpSource = device->getTempTexture(region.width, region.height);
		if (!tmpSource)
			return OpStatus::ERR;
		VEGARefCount::IncRef(tmpSource);
		device->setRenderTarget(sourceRT);
		device->copyToTexture(tmpSource, VEGA3dTexture::CUBE_SIDE_NONE, 0, 0, 0, region.sx, region.sy, region.width, region.height);

		region.sx = region.sy = 0;
		srctex = tmpSource;
	}

	if (!srctex)
		return OpStatus::ERR;

	if (borderWidth)
	{
		if (isSubRegion(region, srctex) || edgeMode == VEGAFILTEREDGE_NONE || 
		    (edgeMode == VEGAFILTEREDGE_WRAP && NEEDS_POW2_WORKAROUND(srctex->getWidth(), srctex->getHeight())))
		{
			VEGA3dFramebufferObject* fbo = NULL;
			err = device->createFramebuffer(&fbo);
			if (OpStatus::IsError(err))
			{
				VEGARefCount::DecRef(tmpSource);
				return err;
			}

			VEGA3dTexture* tmp2Source = NULL;
			err = createClampTexture(device, srctex, region.sx, region.sy,
											   region.width, region.height, borderWidth, edgeMode,
											   fbo, &tmp2Source);
			VEGARefCount::DecRef(fbo);
			VEGARefCount::DecRef(tmpSource);
			RETURN_IF_ERROR(err);

			tmpSource = tmp2Source;
			srctex = tmp2Source;
		}
		else if (edgeMode == VEGAFILTEREDGE_WRAP)
		{
			srctex->setWrapMode(VEGA3dTexture::WRAP_REPEAT, VEGA3dTexture::WRAP_REPEAT);
		}
		else
			srctex->setWrapMode(VEGA3dTexture::WRAP_CLAMP_EDGE, VEGA3dTexture::WRAP_CLAMP_EDGE);
	}
	else
		srctex->setWrapMode(VEGA3dTexture::WRAP_CLAMP_EDGE, VEGA3dTexture::WRAP_CLAMP_EDGE);

	VEGA3dShaderProgram* shader = NULL;
	err = getShader(device, &shader, srctex);
	if (OpStatus::IsSuccess(err))
	{
		// FIXME: Set color?
		VEGA3dBuffer* vbuf = NULL;
		VEGA3dVertexLayout* vlayout = NULL;
		unsigned int vbufStartIndex = 0;
		err = setupVertexBuffer(device, &vbuf, &vlayout, &vbufStartIndex, srctex, shader, region);

		if (OpStatus::IsSuccess(err))
		{
			device->setRenderTarget(destRT);
			device->setRenderState(device->getDefault2dNoBlendNoScissorRenderState());

			shader->setOrthogonalProjection();
			err = device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, vlayout, vbufStartIndex, 4);

		}
		putShader(device, shader);
		VEGARefCount::DecRef(vlayout);
		VEGARefCount::DecRef(vbuf);
	}


	VEGARefCount::DecRef(tmpSource);

	return err;
}
#endif // VEGA_3DDEVICE

#endif // VEGA_SUPPORT

