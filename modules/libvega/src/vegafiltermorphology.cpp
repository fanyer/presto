/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegafiltermorphology.h"
#include "modules/libvega/src/vegapixelformat.h"
#ifdef VEGA_3DDEVICE
#include "modules/libvega/src/vegabackend_hw3d.h"
#endif // VEGA_3DDEVICE

VEGAFilterMorphology::VEGAFilterMorphology() :
	morphType(VEGAMORPH_ERODE), wrap(false), rad_x(0), rad_y(0)
{}

VEGAFilterMorphology::VEGAFilterMorphology(VEGAMorphologyType mt) :
	morphType(mt), wrap(false), rad_x(0), rad_y(0)
{}

#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"

struct VEGAMultiPassParamsMorph
{
	VEGA3dDevice* device;
	VEGA3dShaderProgram* shader;
	VEGA3dBuffer* vtxbuf;
	unsigned int startIndex;
	VEGA3dVertexLayout* vtxlayout;

	VEGA3dTexture* temporary;
	VEGA3dFramebufferObject* temporaryRT;
	VEGA3dTexture* temporaryClamp;
	VEGA3dRenderTarget* destination;

	VEGA3dTexture* source;
	VEGA3dRenderTarget* target;

	VEGA3dDevice::Vega2dVertex* verts;

	VEGAFilterRegion region;
	unsigned int num_passes;
};

OP_STATUS VEGAFilterMorphology::preparePass(unsigned int passno, VEGAMultiPassParamsMorph& params)
{
	float offsets[7*4];

	op_memset(offsets, 0, sizeof (offsets));

	VEGA3dDevice::Vega2dVertex* verts = params.verts;
	if (passno == 0)
	{
		VEGA3dTexture* srcTex = static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture();
		if (params.temporaryClamp)
			srcTex = params.temporaryClamp;

		float xyoffset = (wrap||morphType == VEGAMORPH_DILATE)?0.f:1.f;
		verts[0].x = xyoffset;
		verts[0].y = xyoffset;
		verts[0].s = (float)params.region.sx / srcTex->getWidth();
		verts[0].t = (float)params.region.sy / srcTex->getHeight();
		verts[0].color = (sourceAlphaOnly?0xff000000:0xffffffff);
		verts[1].x = xyoffset;
		verts[1].y = xyoffset+(wrap?(float)params.temporary->getHeight():(float)params.region.height);
		verts[1].s = (float)params.region.sx / srcTex->getWidth();
		verts[1].t = (float)(params.region.sy+params.region.height) / srcTex->getHeight();
		verts[1].color = (sourceAlphaOnly?0xff000000:0xffffffff);
		verts[2].x = xyoffset+(wrap?(float)params.temporary->getWidth():(float)params.region.width);
		verts[2].y = xyoffset;
		verts[2].s = (float)(params.region.sx+params.region.width) / srcTex->getWidth();
		verts[2].t = (float)params.region.sy / srcTex->getHeight();
		verts[2].color = (sourceAlphaOnly?0xff000000:0xffffffff);
		verts[3].x = xyoffset+(wrap?(float)params.temporary->getWidth():(float)params.region.width);
		verts[3].y = xyoffset+(wrap?(float)params.temporary->getHeight():(float)params.region.height);
		verts[3].s = (float)(params.region.sx+params.region.width) / srcTex->getWidth();
		verts[3].t = (float)(params.region.sy+params.region.height) / srcTex->getHeight();
		verts[3].color = (sourceAlphaOnly?0xff000000:0xffffffff);

		if (!wrap)
		{
			if (morphType == VEGAMORPH_ERODE || params.region.width != params.temporary->getWidth() || params.region.height != params.temporary->getHeight())
			{
				params.device->setRenderState(params.device->getDefault2dNoBlendRenderState());
				params.device->setScissor(0, 0, params.region.width+7+2, params.region.height+7+2);
				params.device->setRenderTarget(params.temporaryRT);
				params.device->clear(true, false, false, 0, 1.f, 0);
				params.device->setRenderState(params.device->getDefault2dNoBlendNoScissorRenderState());
			}
		}

		for (int i = 0; i < rad_x; ++i)
			offsets[i*4+0] = (float)(i+1) / srcTex->getWidth();

		params.source = srcTex;
		params.target = params.temporaryRT;
	}
	else if (passno == params.num_passes - 1) // AKA pass "1"
	{
		VEGA3dTexture* tmpTex = params.temporary;

		float soffset = (wrap||morphType == VEGAMORPH_DILATE)?0.f:1.f / (float)tmpTex->getWidth();
		float toffset = (wrap||morphType == VEGAMORPH_DILATE)?0.f:1.f / (float)tmpTex->getHeight();
		verts[0].x = (float)params.region.dx;
		verts[0].y = (float)params.region.dy;
		verts[0].s = soffset;
		verts[0].t = toffset;
		verts[0].color = (sourceAlphaOnly?0xff000000:0xffffffff);
		verts[1].x = (float)params.region.dx;
		verts[1].y = (float)(params.region.dy+params.region.height);
		verts[1].s = soffset;
		verts[1].t = toffset+((float)params.region.height / tmpTex->getHeight());
		verts[1].color = (sourceAlphaOnly?0xff000000:0xffffffff);
		verts[2].x = (float)(params.region.dx+params.region.width);
		verts[2].y = (float)params.region.dy;
		verts[2].s = soffset+((float)params.region.width / tmpTex->getWidth());
		verts[2].t = toffset;
		verts[2].color = (sourceAlphaOnly?0xff000000:0xffffffff);
		verts[3].x = (float)(params.region.dx+params.region.width);
		verts[3].y = (float)(params.region.dy+params.region.height);
		verts[3].s = soffset+((float)params.region.width / tmpTex->getWidth());
		verts[3].t = toffset+((float)params.region.height / tmpTex->getHeight());
		verts[3].color = (sourceAlphaOnly?0xff000000:0xffffffff);

		for (int i = 0; i < rad_y; ++i)
			offsets[i*4+1] = (float)(i+1) / tmpTex->getHeight();

		params.source = tmpTex;
		params.target = params.destination;
	}

	params.shader->setVector4(params.shader->getConstantLocation("offsets"), offsets, 7);

	return params.vtxbuf->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, params.startIndex);
}

OP_STATUS VEGAFilterMorphology::applyRecursive(VEGAMultiPassParamsMorph& params)
{
	OP_STATUS err = OpStatus::OK;
	VEGA3dDevice* device = params.device;

	device->setShaderProgram(params.shader);

	for (unsigned i = 0; i < params.num_passes; ++i)
	{
		err = preparePass(i, params);
		if (OpStatus::IsError(err))
			break;

		device->setTexture(0, params.source);
		device->setRenderTarget(params.target);

		params.shader->setOrthogonalProjection();
		err = device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, params.vtxlayout, params.startIndex, 4);
	}

	return err;
}

OP_STATUS VEGAFilterMorphology::apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame)
{
	// FIXME: Needs refining
	if (rad_x > 7 || rad_y > 7)
		return applyFallback(destStore, region);

	if (sourceRT->getType() != VEGA3dRenderTarget::VEGA3D_RT_TEXTURE ||
		!static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture())
		return OpStatus::ERR;

	VEGAMultiPassParamsMorph params;
	params.region = region;
	params.device = g_vegaGlobals.vega3dDevice;
	params.num_passes = 2;

	params.shader = NULL;
	params.vtxbuf = NULL;
	params.vtxlayout = NULL;
	params.temporary = NULL;
	params.temporaryRT = NULL;
	params.temporaryClamp = NULL;
	params.destination = destStore->GetWriteRenderTarget(frame);

	OP_STATUS err = OpStatus::OK;
	if (wrap)
	{
		err = params.device->createTexture(&params.temporary, params.device->pow2_ceil(region.width), params.device->pow2_ceil(region.height),
										   VEGA3dTexture::FORMAT_RGBA8888);
	}
	else
	{
		params.temporary = params.device->getTempTexture(region.width+2, region.height+2);
		params.temporaryRT = params.device->getTempTextureRenderTarget();
		if (!params.temporary || !params.temporaryRT)
			return OpStatus::ERR;
		VEGARefCount::IncRef(params.temporary);
		VEGARefCount::IncRef(params.temporaryRT);
	}
	if (OpStatus::IsSuccess(err) && !params.temporaryRT)
	{
		err = params.device->createFramebuffer(&params.temporaryRT);
		if (OpStatus::IsSuccess(err))
			err = params.temporaryRT->attachColor(params.temporary);
	}
	bool needClampTexture = OpStatus::IsSuccess(err) &&
		(isSubRegion(params.region, static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture()) ||
		(wrap && (!params.device->isPow2(sourceRT->getWidth()) || !params.device->isPow2(sourceRT->getHeight()))) ||
		(!wrap && morphType == VEGAMORPH_ERODE));
	if (needClampTexture)
	{
		VEGA3dFramebufferObject* tempFBO;
		err = params.device->createFramebuffer(&tempFBO);
		if (OpStatus::IsSuccess(err))
		{
			VEGAFilterEdgeMode edgeMode;
			if (wrap)
				edgeMode = VEGAFILTEREDGE_DUPLICATE;
			else
				edgeMode = (morphType == VEGAMORPH_ERODE) ? VEGAFILTEREDGE_NONE : VEGAFILTEREDGE_DUPLICATE;

			createClampTexture(params.device, static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture(),
				params.region.sx, params.region.sy, region.width, region.height, 0, edgeMode,
				tempFBO, &params.temporaryClamp);

			VEGARefCount::DecRef(tempFBO);
		}
	}
	else
	{
		VEGA3dTexture::WrapMode wrapMode = wrap ? VEGA3dTexture::WRAP_REPEAT : VEGA3dTexture::WRAP_CLAMP_EDGE;
		static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture()->setWrapMode(wrapMode, wrapMode);
	}

	if (OpStatus::IsSuccess(err))
	{
		VEGA3dShaderProgram::WrapMode smode = wrap ? VEGA3dShaderProgram::WRAP_REPEAT_REPEAT : VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;
		VEGA3dShaderProgram::ShaderType stype;
		if (morphType == VEGAMORPH_DILATE)
			stype = VEGA3dShaderProgram::SHADER_MORPHOLOGY_DILATE_15;
		else
			stype = VEGA3dShaderProgram::SHADER_MORPHOLOGY_ERODE_15;

		err = params.device->createShaderProgram(&params.shader, stype, smode);
		if (OpStatus::IsSuccess(err))
		{
			params.vtxbuf = params.device->getTempBuffer(4*sizeof(VEGA3dDevice::Vega2dVertex));
			if (params.vtxbuf)
				VEGARefCount::IncRef(params.vtxbuf);
			else
				err = OpStatus::ERR;
			if (OpStatus::IsSuccess(err))
				err = params.device->createVega2dVertexLayout(&params.vtxlayout, params.shader->getShaderType());
			if (OpStatus::IsSuccess(err))
			{
				params.device->setRenderState(params.device->getDefault2dNoBlendNoScissorRenderState());
				VEGA3dDevice::Vega2dVertex verts[4];
				params.verts = verts;

				err = applyRecursive(params);
			}
		}
	}

	// Free allocated resources
	VEGARefCount::DecRef(params.vtxlayout);
	VEGARefCount::DecRef(params.vtxbuf);

	VEGARefCount::DecRef(params.temporaryRT);
	VEGARefCount::DecRef(params.temporary);
	VEGARefCount::DecRef(params.temporaryClamp);

	VEGARefCount::DecRef(params.shader);

	return err;
}
#endif // VEGA_3DDEVICE

#ifndef USE_PREMULTIPLIED_ALPHA
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

static inline VEGA_PIXEL find_max(VEGA_PIXEL* buf, unsigned buf_ofs, unsigned buf_mask, unsigned len)
{
	unsigned int max_a = 0;
	unsigned int max_r = 0;
	unsigned int max_g = 0;
	unsigned int max_b = 0;

	while (len-- > 0)
	{
		VEGA_PIXEL v = buf[buf_ofs++];
		unsigned sa = VEGA_UNPACK_A(v);
		unsigned sr = VEGA_UNPACK_R(v);
		unsigned sg = VEGA_UNPACK_G(v);
		unsigned sb = VEGA_UNPACK_B(v);

		VEGA_COND_PREMULT(sa, sr, sg, sb);

		max_a = MAX(max_a, sa);
		max_r = MAX(max_r, sr);
		max_g = MAX(max_g, sg);
		max_b = MAX(max_b, sb);

		buf_ofs &= buf_mask;
	}

	VEGA_COND_UNPREMULT(max_a, max_r, max_g, max_b);

	return VEGA_PACK_ARGB(max_a, max_r, max_g, max_b);
}

static inline VEGA_PIXEL find_min(VEGA_PIXEL* buf, unsigned buf_ofs, unsigned buf_mask, unsigned len)
{
	unsigned min_a = 255;
	unsigned min_r = 255;
	unsigned min_g = 255;
	unsigned min_b = 255;

	while (len-- > 0)
	{
		VEGA_PIXEL v = buf[buf_ofs++];
		unsigned sa = VEGA_UNPACK_A(v);
		unsigned sr = VEGA_UNPACK_R(v);
		unsigned sg = VEGA_UNPACK_G(v);
		unsigned sb = VEGA_UNPACK_B(v);

		VEGA_COND_PREMULT(sa, sr, sg, sb);

		min_a = MIN(min_a, sa);
		min_r = MIN(min_r, sr);
		min_g = MIN(min_g, sg);
		min_b = MIN(min_b, sb);

		buf_ofs &= buf_mask;
	}

	VEGA_COND_UNPREMULT(min_a, min_r, min_g, min_b);

	return VEGA_PACK_ARGB(min_a, min_r, min_g, min_b);
}

static inline VEGA_PIXEL find_max_A(UINT8* buf, unsigned buf_ofs, unsigned buf_mask, unsigned len)
{
	unsigned int max_a = 0;
	while (len-- > 0)
	{
		unsigned v = buf[buf_ofs++];
		max_a = MAX(max_a, v);
		buf_ofs &= buf_mask;
	}
	return VEGA_PACK_ARGB(max_a, 0, 0, 0);
}

static inline VEGA_PIXEL find_min_A(UINT8* buf, unsigned buf_ofs, unsigned buf_mask, unsigned len)
{
	unsigned min_a = 255;
	while (len-- > 0)
	{
		unsigned v = buf[buf_ofs++];
		min_a = MIN(min_a, v);
		buf_ofs &= buf_mask;
	}
	return VEGA_PACK_ARGB(min_a, 0, 0, 0);
}

// Process a row of data using zeropadding.
// BUF - Intermediate buffer
// SRCV - Source value extraction
// FUNC - Evaluation function
#define PROCESS_ROW_ZEROPAD(BUF, SRCV, FUNC)							\
	do {																\
		unsigned rbuf_ofs = 0;											\
		const unsigned rbuf_slack = (rbuf_mask + 1) - 2 * radius;		\
																		\
		VEGAConstPixelAccessor src(srcp);								\
		VEGAPixelPtr src_end = srcp + length * srcstride;				\
																		\
		unsigned __cnt = radius;										\
		while (__cnt-- > 0)												\
			BUF[rbuf_ofs++] = 0;										\
																		\
		__cnt = MIN(length, radius);									\
		while (__cnt-- > 0)												\
		{																\
			BUF[rbuf_ofs++] = SRCV;										\
			src += srcstride;											\
		}																\
																		\
		if (length < radius)											\
		{																\
			__cnt = radius - length;									\
			while (__cnt-- > 0)											\
				BUF[rbuf_ofs++] = 0;									\
		}																\
																		\
		rbuf_ofs &= rbuf_mask;											\
																		\
		unsigned __dst_remaining = length;								\
		VEGAPixelAccessor dst(dstp);									\
		while (src.Ptr() < src_end)										\
		{																\
			dst.Store(FUNC(BUF, (rbuf_ofs + rbuf_slack) & rbuf_mask, rbuf_mask, 2 * radius)); \
			dst += dststride;											\
																		\
			__dst_remaining--;											\
																		\
			BUF[rbuf_ofs++] = SRCV;										\
			rbuf_ofs &= rbuf_mask;										\
																		\
			src += srcstride;											\
		}																\
																		\
		while (__dst_remaining-- > 0)									\
		{																\
			dst.Store(FUNC(BUF, (rbuf_ofs + rbuf_slack) & rbuf_mask, rbuf_mask, 2 * radius)); \
			dst += dststride;											\
																		\
			BUF[rbuf_ofs++] = 0;										\
			rbuf_ofs &= rbuf_mask;										\
		}																\
	} while(0)

// Process a row of data using wrapping at boundaries.
// BUF - Intermediate buffer
// SRCV - Source value extraction
// FUNC - Evaluation function
#define PROCESS_ROW_WRAP(BUF, SRCV, FUNC)								\
	do {																\
		unsigned rbuf_ofs = 0;											\
		const unsigned rbuf_slack = (rbuf_mask + 1) - 2 * radius;		\
																		\
		VEGAConstPixelAccessor src(srcp);								\
		VEGAPixelPtr src_end = srcp + length * srcstride;				\
																		\
		unsigned ofs = (length - 1) - ((radius - 1) % length);			\
		src += ofs * srcstride;											\
																		\
		unsigned __cnt = 2 * radius;									\
		while (__cnt-- > 0)												\
		{																\
			BUF[rbuf_ofs++] = SRCV;										\
																		\
			src += srcstride;											\
			if (src.Ptr() >= src_end)									\
				src.Reset(srcp);										\
		}																\
																		\
		rbuf_ofs &= rbuf_mask;											\
																		\
		__cnt = length;													\
		VEGAPixelAccessor dst(dstp);									\
		while (__cnt-- > 0)												\
		{																\
			dst.Store(FUNC(BUF, (rbuf_ofs + rbuf_slack) & rbuf_mask, rbuf_mask, 2 * radius)); \
			dst += dststride;											\
																		\
			BUF[rbuf_ofs++] = SRCV;										\
			rbuf_ofs &= rbuf_mask;										\
																		\
			src += srcstride;											\
			if (src.Ptr() >= src_end)									\
				src.Reset(srcp);										\
		}																\
	} while(0)

void VEGAFilterMorphology::dilateRow(VEGAPixelPtr dstp, unsigned int dststride,
									 VEGAPixelPtr srcp, unsigned int srcstride,
									 unsigned length, unsigned radius, unsigned rbuf_mask)
{
	PROCESS_ROW_ZEROPAD(rbuf, src.Load(), find_max);
}

void VEGAFilterMorphology::dilateRow_A(VEGAPixelPtr dstp, unsigned int dststride,
									   VEGAPixelPtr srcp, unsigned int srcstride,
									   unsigned length, unsigned radius, unsigned rbuf_mask)
{
	PROCESS_ROW_ZEROPAD(rbuf_a, VEGA_UNPACK_A(src.Load()), find_max_A);
}

void VEGAFilterMorphology::dilateRow_W(VEGAPixelPtr dstp, unsigned int dststride,
									   VEGAPixelPtr srcp, unsigned int srcstride,
									   unsigned length, unsigned radius, unsigned rbuf_mask)
{
	PROCESS_ROW_WRAP(rbuf, src.Load(), find_max);
}

void VEGAFilterMorphology::dilateRow_AW(VEGAPixelPtr dstp, unsigned int dststride,
										VEGAPixelPtr srcp, unsigned int srcstride,
										unsigned length, unsigned radius, unsigned rbuf_mask)
{
	PROCESS_ROW_WRAP(rbuf_a, VEGA_UNPACK_A(src.Load()), find_max_A);
}

void VEGAFilterMorphology::erodeRow(VEGAPixelPtr dstp, unsigned int dststride,
									VEGAPixelPtr srcp, unsigned int srcstride,
									unsigned length, unsigned radius, unsigned rbuf_mask)
{
	PROCESS_ROW_ZEROPAD(rbuf, src.Load(), find_min);
}

void VEGAFilterMorphology::erodeRow_A(VEGAPixelPtr dstp, unsigned int dststride,
									  VEGAPixelPtr srcp, unsigned int srcstride,
									  unsigned length, unsigned radius, unsigned rbuf_mask)
{
	PROCESS_ROW_ZEROPAD(rbuf_a, VEGA_UNPACK_A(src.Load()), find_min_A);
}

void VEGAFilterMorphology::erodeRow_W(VEGAPixelPtr dstp, unsigned int dststride,
									  VEGAPixelPtr srcp, unsigned int srcstride,
									  unsigned length, unsigned radius, unsigned rbuf_mask)
{
	PROCESS_ROW_WRAP(rbuf, src.Load(), find_min);
}

void VEGAFilterMorphology::erodeRow_AW(VEGAPixelPtr dstp, unsigned int dststride,
									   VEGAPixelPtr srcp, unsigned int srcstride,
									   unsigned length, unsigned radius, unsigned rbuf_mask)
{
	PROCESS_ROW_WRAP(rbuf_a, VEGA_UNPACK_A(src.Load()), find_min_A);
}

OP_STATUS VEGAFilterMorphology::apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region)
{
	unsigned int xp, yp;
	VEGAConstPixelAccessor src = source.GetConstAccessor(region.sx, region.sy);
	VEGAPixelAccessor dst = dest.GetAccessor(region.dx, region.dy);

	unsigned int srcPixelStride = source.GetPixelStride();
	unsigned int dstPixelStride = dest.GetPixelStride();

	unsigned rbuf_size = VEGAround_pot(2*MAX(rad_x, rad_y));
	unsigned rbuf_mask = rbuf_size - 1;

	if (!sourceAlphaOnly)
	{
		rbuf = OP_NEWA(VEGA_PIXEL, rbuf_size);
		if (!rbuf)
			return OpStatus::ERR_NO_MEMORY;

		switch (morphType)
		{
		case VEGAMORPH_DILATE:
			if (wrap)
			{
				// Apply rows
				for (yp = 0; yp < region.height; ++yp)
				{
					dilateRow_W(dst.Ptr(), 1, src.Ptr(), 1, region.width, rad_x, rbuf_mask);

					dst += dstPixelStride;
					src += srcPixelStride;
				}

				dst = dest.GetAccessor(region.dx, region.dy);

				// Apply columns
				for (xp = 0; xp < region.width; ++xp)
				{
					dilateRow_W(dst.Ptr(), dstPixelStride, dst.Ptr(), dstPixelStride, region.height, rad_y, rbuf_mask);

					dst++;
				}
			}
			else /* !wrap */
			{
				// Apply rows
				for (yp = 0; yp < region.height; ++yp)
				{
					dilateRow(dst.Ptr(), 1, src.Ptr(), 1, region.width, rad_x, rbuf_mask);

					dst += dstPixelStride;
					src += srcPixelStride;
				}

				dst = dest.GetAccessor(region.dx, region.dy);

				// Apply columns
				for (xp = 0; xp < region.width; ++xp)
				{
					dilateRow(dst.Ptr(), dstPixelStride, dst.Ptr(), dstPixelStride, region.height, rad_y, rbuf_mask);

					dst++;
				}
			}
			break;

		case VEGAMORPH_ERODE:
			if (wrap)
			{
				// Apply rows
				for (yp = 0; yp < region.height; ++yp)
				{
					erodeRow_W(dst.Ptr(), 1, src.Ptr(), 1, region.width, rad_x, rbuf_mask);

					dst += dstPixelStride;
					src += srcPixelStride;
				}

				dst = dest.GetAccessor(region.dx, region.dy);

				// Apply columns
				for (xp = 0; xp < region.width; ++xp)
				{
					erodeRow_W(dst.Ptr(), dstPixelStride, dst.Ptr(), dstPixelStride, region.height, rad_y, rbuf_mask);

					dst++;
				}
			}
			else /* !wrap */
			{
				// Apply rows
				for (yp = 0; yp < region.height; ++yp)
				{
					erodeRow(dst.Ptr(), 1, src.Ptr(), 1, region.width, rad_x, rbuf_mask);

					dst += dstPixelStride;
					src += srcPixelStride;
				}

				dst = dest.GetAccessor(region.dx, region.dy);

				// Apply columns
				for (xp = 0; xp < region.width; ++xp)
				{
					erodeRow(dst.Ptr(), dstPixelStride, dst.Ptr(), dstPixelStride, region.height, rad_y, rbuf_mask);

					dst++;
				}
			}
			break;
		}

		OP_DELETEA(rbuf);
	}
	else
	{
		/* Alpha Only case */
		rbuf_a = OP_NEWA(UINT8, rbuf_size);
		if (!rbuf_a)
			return OpStatus::ERR_NO_MEMORY;

		switch (morphType)
		{
		case VEGAMORPH_DILATE:
			if (wrap)
			{
				// Apply rows
				for (yp = 0; yp < region.height; ++yp)
				{
					dilateRow_AW(dst.Ptr(), 1, src.Ptr(), 1, region.width, rad_x, rbuf_mask);

					dst += dstPixelStride;
					src += srcPixelStride;
				}

				dst = dest.GetAccessor(region.dx, region.dy);

				// Apply columns
				for (xp = 0; xp < region.width; ++xp)
				{
					dilateRow_AW(dst.Ptr(), dstPixelStride, dst.Ptr(), dstPixelStride, region.height, rad_y, rbuf_mask);

					dst++;
				}
			}
			else /* !wrap */
			{
				// Apply rows
				for (yp = 0; yp < region.height; ++yp)
				{
					dilateRow_A(dst.Ptr(), 1, src.Ptr(), 1, region.width, rad_x, rbuf_mask);

					dst += dstPixelStride;
					src += srcPixelStride;
				}

				dst = dest.GetAccessor(region.dx, region.dy);

				// Apply columns
				for (xp = 0; xp < region.width; ++xp)
				{
					dilateRow_A(dst.Ptr(), dstPixelStride, dst.Ptr(), dstPixelStride, region.height, rad_y, rbuf_mask);

					dst++;
				}
			}
			break;

		case VEGAMORPH_ERODE:
			if (wrap)
			{
				// Apply rows
				for (yp = 0; yp < region.height; ++yp)
				{
					erodeRow_AW(dst.Ptr(), 1, src.Ptr(), 1, region.width, rad_x, rbuf_mask);

					dst += dstPixelStride;
					src += srcPixelStride;
				}

				dst = dest.GetAccessor(region.dx, region.dy);

				// Apply columns
				for (xp = 0; xp < region.width; ++xp)
				{
					erodeRow_AW(dst.Ptr(), dstPixelStride, dst.Ptr(), dstPixelStride, region.height, rad_y, rbuf_mask);

					dst++;
				}
			}
			else /* !wrap */
			{
				// Apply rows
				for (yp = 0; yp < region.height; ++yp)
				{
					erodeRow_A(dst.Ptr(), 1, src.Ptr(), 1, region.width, rad_x, rbuf_mask);

					dst += dstPixelStride;
					src += srcPixelStride;
				}

				dst = dest.GetAccessor(region.dx, region.dy);

				// Apply columns
				for (xp = 0; xp < region.width; ++xp)
				{
					erodeRow_A(dst.Ptr(), dstPixelStride, dst.Ptr(), dstPixelStride, region.height, rad_y, rbuf_mask);

					dst++;
				}
			}
			break;
		}

		OP_DELETEA(rbuf_a);
	}

	return OpStatus::OK;
}

#undef VEGA_COND_PREMULT
#undef VEGA_COND_UNPREMULT

#endif // VEGA_SUPPORT
