/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_3DDEVICE)

#include "modules/libvega/src/vegabackend_hw3d.h"

#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegastencil.h"
#include "modules/libvega/vega3ddevice.h"

#include "modules/libvega/src/vegaimage.h"
#include "modules/libvega/src/vegagradient.h"
#include "modules/libvega/src/vegarendertargetimpl.h"

#include "modules/libvega/src/vegafilterdisplace.h"

#include "modules/mdefont/processedstring.h"

#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#include "modules/libvega/vegawindow.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"

// disable multisampling if not needed - updateQuality cannot do this directly
#define NEEDS_MULTISAMPLING(q) ((q) > 1)
static inline
void CheckMultisampling(unsigned int quality, VEGABackingStore* bstore)
{
	unsigned int samples = VEGABackend_HW3D::qualityToSamples(quality);

	if (bstore && !NEEDS_MULTISAMPLING(samples))
		static_cast<VEGABackingStore_FBO*>(bstore)->DisableMultisampling();
}

VEGABackend_HW3D::VEGABackend_HW3D() :
	m_vbuffer(NULL),
		m_vlayout2d(NULL), m_vlayout2dTexGen(NULL),
		m_vert(NULL), m_defaultRenderState(NULL), m_defaultNoBlendRenderState(NULL), m_additiveRenderState(NULL),
		m_stencilModulateRenderState(NULL), m_updateStencilRenderState(NULL), m_updateStencilXorRenderState(NULL),
		m_invertRenderState(NULL), m_premultiplyRenderState(NULL),
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
		m_subpixelRenderState(NULL),
#endif
		m_triangleIndexBuffer(NULL), m_triangleIndices(NULL),
		m_numRemainingBatchVerts(0), m_firstBatchVert(0),
		m_triangleIndexCount(0),
		m_batchList(NULL), m_numBatchOperations(0),
		m_currentFrame(1)
{
	op_memset(shader2dV, 0, sizeof(shader2dV));
	op_memset(shader2dTexGenV, 0, sizeof(shader2dTexGenV));
	op_memset(shader2dText, 0, sizeof(shader2dText));
	op_memset(textShaderLoaded, 0, sizeof(textShaderLoaded));
	op_memset(m_vlayout2dText, 0, sizeof(m_vlayout2dText));
}

VEGABackend_HW3D::~VEGABackend_HW3D()
{
	OP_ASSERT(!m_firstBatchVert); // Deleting while there are pending batches should never happen
	OP_DELETEA(m_batchList);

	releaseShaderPrograms();

	for (size_t i=0; i<ARRAY_SIZE(m_vlayout2dText); i++)
		VEGARefCount::DecRef(m_vlayout2dText[i]);

	VEGARefCount::DecRef(m_vlayout2d);
	VEGARefCount::DecRef(m_vlayout2dTexGen);
	VEGARefCount::DecRef(m_vbuffer);
	VEGARefCount::DecRef(m_triangleIndexBuffer);
	OP_DELETEA(m_vert);
	OP_DELETE(m_additiveRenderState);
	OP_DELETE(m_stencilModulateRenderState);
	OP_DELETE(m_updateStencilRenderState);
	OP_DELETE(m_updateStencilXorRenderState);
	OP_DELETE(m_invertRenderState);
	OP_DELETE(m_premultiplyRenderState);
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
	OP_DELETE(m_subpixelRenderState);
#endif

	if (g_vegaGlobals.m_current_batcher == this)
		g_vegaGlobals.m_current_batcher = NULL;
}

OP_STATUS VEGABackend_HW3D::init(unsigned int w, unsigned int h, unsigned int q)
{
	quality = q;
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	if (!device)
	{
		VEGA3dDevice::UseCase useCase;

		OP_ASSERT(g_pccore && "VEGA backend initialized before prefs in core");
		switch (g_pccore->GetIntegerPref(PrefsCollectionCore::EnableHardwareAcceleration))
		{
		case PrefsCollectionCore::Disable:
			return OpStatus::ERR;
		case PrefsCollectionCore::Force:
			useCase = VEGA3dDevice::Force;
			break;
		default:
			OP_ASSERT(!"Pref value isn't handled! Falling back on For2D."); /* fall through */
		case PrefsCollectionCore::Auto:
			useCase = VEGA3dDevice::For2D;
			break;
		}
		RETURN_IF_ERROR(VEGA3dDeviceFactory::Create(&g_vegaGlobals.vega3dDevice, useCase,
						g_vegaGlobals.mainNativeWindow));
		device = g_vegaGlobals.vega3dDevice;
	}

	unsigned int maxVerts = device->getVertexBufferSize();

	BOOL all_shaders_present = shader2dText[0] != NULL && shader2dText[Stencil] != NULL;
	if (all_shaders_present)
		for (size_t i = 0; i < VEGA3dShaderProgram::WRAP_MODE_COUNT; ++i)
		{
			if (!shader2dV[i] || !shader2dTexGenV[i])
			{
				all_shaders_present = FALSE;
				break;
			}
		}

	if (!all_shaders_present)
	{
		releaseShaderPrograms();

		for (size_t i = 0; i < VEGA3dShaderProgram::WRAP_MODE_COUNT; ++i)
		{
			RETURN_IF_ERROR(device->createShaderProgram(&shader2dV[i], VEGA3dShaderProgram::SHADER_VECTOR2D, (VEGA3dShaderProgram::WrapMode)i));
			RETURN_IF_ERROR(device->createShaderProgram(&shader2dTexGenV[i], VEGA3dShaderProgram::SHADER_VECTOR2DTEXGEN, (VEGA3dShaderProgram::WrapMode)i));
		}

		g_vegaGlobals.getShaderLocations(worldProjMatrix2dLocationV,
		                                 texTransSLocationV,
		                                 texTransTLocationV,
		                                 stencilCompBasedLocation,
		                                 straightAlphaLocation);

		// 2 basic text shaders must exist, others are created when needed
		RETURN_IF_ERROR(createTextShader(0));
		RETURN_IF_ERROR(createTextShader(Stencil));
	}
	if (!m_vbuffer)
	{
		m_vbuffer = device->getTempBuffer(maxVerts*sizeof(VEGA3dDevice::Vega2dVertex));
		if (!m_vbuffer)
			return OpStatus::ERR;
		VEGARefCount::IncRef(m_vbuffer);
	}
	if (!m_triangleIndexBuffer)
	{
		device->getTriangleIndexBuffers(&m_triangleIndexBuffer, &m_triangleIndices);
		if (!m_triangleIndexBuffer)
			return OpStatus::ERR;
		VEGARefCount::IncRef(m_triangleIndexBuffer);
	}

	if (!m_vlayout2d)
		RETURN_IF_ERROR(device->createVega2dVertexLayout(&m_vlayout2d, VEGA3dShaderProgram::SHADER_VECTOR2D));

	if (!m_vlayout2dTexGen)
		RETURN_IF_ERROR(device->createVega2dVertexLayout(&m_vlayout2dTexGen, VEGA3dShaderProgram::SHADER_VECTOR2DTEXGEN));

	if (!m_vert)
	{
		m_vert = OP_NEWA(VEGA3dDevice::Vega2dVertex, maxVerts);
		RETURN_OOM_IF_NULL(m_vert);
	}

	m_defaultRenderState = device->getDefault2dRenderState();
	m_defaultNoBlendRenderState = device->getDefault2dNoBlendRenderState();
	if (!m_batchList)
	{
		m_batchList = OP_NEWA(BatchOperation, maxVerts/4);
		if (!m_batchList)
			return OpStatus::ERR_NO_MEMORY;
		m_numRemainingBatchVerts = maxVerts;
	}

	RETURN_IF_ERROR(rasterizer.initialize(w, h));

	rasterizer.setConsumer(this);
	rasterizer.setQuality(q);

	return OpStatus::OK;
}

bool VEGABackend_HW3D::bind(VEGARenderTarget* rt)
{
	VEGABackingStore* bstore = rt->GetBackingStore();
	if (!bstore || !bstore->IsA(VEGABackingStore::FRAMEBUFFEROBJECT))
		// Would be very surprising if we got a NULL backing store
		return false;

	CheckMultisampling(quality, bstore);
	bstore->Validate();

	return true;
}

void VEGABackend_HW3D::unbind()
{
	flushBatches();
}

void VEGABackend_HW3D::flush(const OpRect* update_rects, unsigned int num_rects)
{
	flushBatches();
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	device->setRenderTarget(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetReadRenderTarget());
	device->flush();
	VEGARendererBackend::flush(update_rects, num_rects);
	++m_currentFrame;
}

void VEGABackend_HW3D::flushIfUsingRenderState(VEGA3dRenderState* rs)
{
	OP_ASSERT(rs);
	for (unsigned int batch = 0; batch < m_numBatchOperations; ++batch)
	{
		if (m_batchList[batch].renderState == rs)
		{
			flushBatches();
			return;
		}
	}
}

void VEGABackend_HW3D::flushIfContainsTexture(VEGA3dTexture* tex)
{
	OP_ASSERT(tex);
	for (unsigned int batch = 0; batch < m_numBatchOperations; ++batch)
	{
		if (m_batchList[batch].texture == tex)
		{
			flushBatches();
			return;
		}
	}
}

OP_STATUS VEGABackingStore_FBO::Construct(unsigned width, unsigned height)
{
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	VEGA3dTexture* fbo_texture;
	VEGA3dFramebufferObject* fbo;
	RETURN_IF_ERROR(device->createFramebuffer(&fbo));

	OP_STATUS status = device->createTexture(&fbo_texture, width, height,
											 VEGA3dTexture::FORMAT_RGBA8888, false);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(fbo);
		return status;
	}

	status = fbo->attachColor(fbo_texture);
	VEGARefCount::DecRef(fbo_texture);
	m_fbo = fbo;
	return status;
}

OP_STATUS VEGABackingStore_FBO::Construct(VEGA3dWindow* win3d)
{
	m_fbo = win3d;
	return OpStatus::OK;
}

OP_STATUS VEGABackingStore_FBO::Construct(VEGABackingStore_Texture* tex)
{
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	VEGA3dFramebufferObject* fbo;
	RETURN_IF_ERROR(device->createFramebuffer(&fbo));
	OP_STATUS status = fbo->attachColor(tex->GetTexture());
	m_fbo = fbo;
	m_textureStore = tex;
	VEGARefCount::IncRef(tex);
	return status;
}

VEGABackingStore_FBO::~VEGABackingStore_FBO()
{
	// Avoid flushing when the device has disappeared
	if (g_vegaGlobals.vega3dDevice)
		FlushTransaction();

	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	if (device && device->getRenderTarget() == m_fbo)
		device->setRenderTarget(0, false);

	if (m_fbo && m_fbo->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
		VEGARefCount::DecRef(static_cast<VEGA3dFramebufferObject*>(m_fbo));
	else
		OP_DELETE(m_fbo);
	VEGARefCount::DecRef(m_msfbo);
	VEGARefCount::DecRef(m_textureStore);
}

VEGA3dRenderTarget* VEGABackingStore_FBO::GetReadRenderTarget()
{
	if (m_batcher)
		m_batcher->flushBatches();
	if (m_msfbo && m_writeRTDirty)
		ResolveMultisampling();

	return m_fbo;
}

VEGA3dRenderTarget* VEGABackingStore_FBO::GetWriteRenderTarget(unsigned int frame)
{
	// If the current frame is not the same as last frame this must be the start of a new frame
	// If the last frame using ms is different from the last frame when starting a new one that means
	// the entire last frame was rendered without ms and we can try to disable ms again
	if (m_msActive && frame != m_lastFrame && m_lastMSFrame != m_lastFrame)
		DisableMultisampling();
	m_lastFrame = frame;
	if (m_msfbo && (m_msfbo->getWidth() != m_fbo->getWidth() || m_msfbo->getHeight() != m_fbo->getHeight()))
	{
		VEGARefCount::DecRef(m_msfbo);
		m_msfbo = NULL;
		m_msActive = false;
	}
	if (m_textureStore)
	{
		m_textureStore->MarkBufferDirty();
	}
	if (m_msActive)
	{
		m_writeRTDirty = true;
		return m_msfbo;
	}
	return m_fbo;
}

void VEGABackingStore_FBO::DisableMultisampling()
{
	m_msActive = false;
}

bool VEGABackingStore_FBO::EnableMultisampling(unsigned int quality, unsigned int frame)
{
	unsigned int samples = VEGABackend_HW3D::qualityToSamples(quality);

	m_lastFrame = frame;
	m_lastMSFrame = frame;
	if (m_msfbo && (m_msfbo->getWidth() != m_fbo->getWidth() || m_msfbo->getHeight() != m_fbo->getHeight() || samples != m_msSamples))
	{
		if (m_msActive && m_writeRTDirty)
			ResolveMultisampling();

		VEGARefCount::DecRef(m_msfbo);
		m_msfbo = NULL;
		m_msActive = false;
	}
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	// FIXME: When bound to a texture store the invalid flags are a bit messed up, so multisampling currently does not work
	if (m_msActive || !device->supportsMultisampling() || m_textureStore || !NEEDS_MULTISAMPLING(samples))
		return false;
	if (m_msfbo)
	{
		m_msActive = true;
	}
	else
	{
		VEGA3dRenderbufferObject* rb;
		RETURN_VALUE_IF_ERROR(device->createRenderbuffer(&rb, m_fbo->getWidth(), m_fbo->getHeight(), VEGA3dRenderbufferObject::FORMAT_RGBA8, samples), false);
		if (OpStatus::IsError(device->createFramebuffer(&m_msfbo)))
		{
			VEGARefCount::DecRef(rb);
			return false;
		}
		if (OpStatus::IsError(m_msfbo->attachColor(rb)))
		{
			VEGARefCount::DecRef(rb);
			return false;
		}
		VEGARefCount::DecRef(rb);
		m_msSamples = samples;
	}
	// Copy the non multisampled data to the multisampled buffer
#ifdef VEGA_NATIVE_FONT_SUPPORT
	m_fbo->flushFonts();
#endif
	VEGA3dTexture* tex = NULL;
	if (m_fbo->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE &&
		static_cast<VEGA3dFramebufferObject*>(m_fbo)->getAttachedColorTexture())
	{
		tex = static_cast<VEGA3dFramebufferObject*>(m_fbo)->getAttachedColorTexture();
	}
	else
	{
		tex = device->getTempTexture(m_fbo->getWidth(), m_fbo->getHeight());
		if (!tex)
			return false;
		device->setRenderTarget(m_fbo, false);
		RETURN_VALUE_IF_ERROR(device->copyToTexture(tex, VEGA3dTexture::CUBE_SIDE_NONE, 0, 0, 0, 0, 0, m_fbo->getWidth(), m_fbo->getHeight()), true);
	}
	VEGA3dDevice::Vega2dVertex verts[] =
	{
		{0, 0,0,0,0xffffffff},
		{(float)m_fbo->getWidth(), 0,((float)m_fbo->getWidth())/((float)tex->getWidth()),0,0xffffffff},
		{0, (float)m_fbo->getHeight(),0,((float)m_fbo->getHeight())/((float)tex->getHeight()),0xffffffff},
		{(float)m_fbo->getWidth(), (float)m_fbo->getHeight(),((float)m_fbo->getWidth())/((float)tex->getWidth()),((float)m_fbo->getHeight())/((float)tex->getHeight()),0xffffffff}
	};
	VEGA3dBuffer* vbuffer = device->getTempBuffer(4*sizeof(VEGA3dDevice::Vega2dVertex));
	unsigned int firstVert;
	RETURN_VALUE_IF_ERROR(vbuffer->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, firstVert), true);

	device->setRenderState(device->getDefault2dNoBlendNoScissorRenderState());

	device->setRenderTarget(m_msfbo);
	tex->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);
	device->setTexture(0, tex);
	VEGA3dShaderProgram* shd;
	RETURN_VALUE_IF_ERROR(device->createShaderProgram(&shd, VEGA3dShaderProgram::SHADER_VECTOR2D, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP), true);
	device->setShaderProgram(shd);
	shd->setOrthogonalProjection();
	// setShaderProgram will add a reference to the shader program
	VEGARefCount::DecRef(shd);
	VEGA3dVertexLayout* layout;
	RETURN_VALUE_IF_ERROR(device->createVega2dVertexLayout(&layout, VEGA3dShaderProgram::SHADER_VECTOR2D), true);
	device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, layout, firstVert, 4);
	VEGARefCount::DecRef(layout);
	m_writeRTDirty = false;
	m_msActive = true;
	return true;
}

void VEGABackingStore_FBO::ResolveMultisampling()
{
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	device->setRenderState(device->getDefault2dNoBlendNoScissorRenderState());
	VEGA3dRenderTarget* const oldrt = device->getRenderTarget();
	device->setRenderTarget(m_msfbo, false);
	device->resolveMultisample(m_fbo, 0, 0, m_msfbo->getWidth(), m_msfbo->getHeight());
	m_writeRTDirty = false;
	device->setRenderTarget(oldrt, false);
}

VEGA3dTexture* VEGABackingStore_FBO::SwapTexture(VEGA3dTexture* tex)
{
	VEGA3dTexture* curtex = NULL;
	if (m_fbo->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
		curtex = static_cast<VEGA3dFramebufferObject*>(m_fbo)->getAttachedColorTexture();
	if (!curtex)
		return NULL;
	if (tex->getWidth() != curtex->getWidth() || tex->getHeight() != curtex->getHeight() || tex->getFormat() != curtex->getFormat())
		return NULL;
	DisableMultisampling();
	VEGARefCount::IncRef(curtex);
	static_cast<VEGA3dFramebufferObject*>(m_fbo)->attachColor(tex);
	VEGARefCount::DecRef(tex);
	return curtex;
}


void VEGABackingStore_FBO::setCurrentBatcher(VEGABackend_HW3D* batcher)
{
	if (batcher == m_batcher)
		return;
	if (batcher && m_batcher)
		m_batcher->flushBatches();
	m_batcher = batcher;
}


void VEGABackingStore_FBO::SetColor(const OpRect& rect, UINT32 color)
{
	FlushTransaction();

	// Flush if the texture is currently scheduled in a batch. This can happen
	// when reusing fbo's in the backbuffer code.
	if (g_vegaGlobals.m_current_batcher)
	{
		if (m_fbo->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
		{
			VEGA3dTexture* tex = static_cast<VEGA3dFramebufferObject*>(m_fbo)->getAttachedColorTexture();
			g_vegaGlobals.m_current_batcher->flushIfContainsTexture(tex);
		}
	}

	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;

	device->setRenderTarget(GetReadRenderTarget());
	m_msActive = false;

	device->setScissor(rect.x, rect.y, rect.width, rect.height);
	device->setRenderState(device->getDefault2dNoBlendRenderState());
	device->clear(true, false, false, VEGAPixelPremultiply_BGRA8888(color), 1.f, 0);
}

OP_STATUS VEGABackingStore_FBO::CopyRect(const OpPoint& dstp, const OpRect& srcr,
										 VEGABackingStore* store)
{
	FlushTransaction();

	if (m_fbo->getType() != VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
		return OpStatus::ERR;

	// Make sure rendering operations are flushed
	GetReadRenderTarget();
	m_msActive = false;

	if (!store->IsA(VEGABackingStore::FRAMEBUFFEROBJECT))
		return FallbackCopyRect(dstp, srcr, store);

	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;

	VEGA3dRenderTarget* oldrt = device->getRenderTarget();

	VEGABackingStore_FBO* fbo_store = static_cast<VEGABackingStore_FBO*>(store);
	fbo_store->GetReadRenderTarget(); // must flush non-multisampled FBO
	device->setRenderTarget(fbo_store->m_fbo);

	device->copyToTexture(static_cast<VEGA3dFramebufferObject*>(m_fbo)->getAttachedColorTexture(), VEGA3dTexture::CUBE_SIDE_NONE, 0, dstp.x, dstp.y,
						  srcr.x, srcr.y, srcr.width, srcr.height);

	device->setRenderTarget(oldrt);
	return OpStatus::OK;
}

OP_STATUS VEGABackingStore_FBO::LockRect(const OpRect& rect, AccessType acc)
{
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;

	device->setRenderTarget(GetReadRenderTarget());

	OP_ASSERT(OpRect(0, 0, m_fbo->getWidth(), m_fbo->getHeight()).Contains(rect));

	m_lock_data.x = rect.x;
	m_lock_data.y = rect.y;
	m_lock_data.w = rect.width;
	m_lock_data.h = rect.height;

	RETURN_IF_ERROR(device->lockRect(&m_lock_data, acc != ACC_WRITE_ONLY));

	OP_STATUS status = m_lock_buffer.Bind(&m_lock_data.pixels);
	if (OpStatus::IsError(status))
		device->unlockRect(&m_lock_data, false);

	return status;
}

OP_STATUS VEGABackingStore_FBO::UnlockRect(BOOL commit)
{
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;

	if (commit)
		m_lock_buffer.Sync(&m_lock_data.pixels);

	m_lock_buffer.Unbind(&m_lock_data.pixels);
	device->setRenderTarget(GetReadRenderTarget());
	device->unlockRect(&m_lock_data, commit!=FALSE);
	if (commit && m_msActive)
	{
		m_msActive = false;
	}
	return OpStatus::OK;
}

OP_STATUS VEGABackingStore_Texture::Construct(unsigned width, unsigned height, bool indexed)
{
	VEGASWBufferType swtype = indexed ? VSWBUF_INDEXED_COLOR : VSWBUF_COLOR;
	RETURN_IF_ERROR(VEGABackingStore_SWBuffer::Construct(width, height, swtype, false));

	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	RETURN_IF_ERROR(device->createTexture(&m_texture, width, height,
										  VEGA3dTexture::FORMAT_RGBA8888));
	device->RegisterContextLostListener(this);
	return OpStatus::OK;
}

VEGABackingStore_Texture::~VEGABackingStore_Texture()
{
	if (m_texture)
	{
		g_vegaGlobals.vega3dDevice->UnregisterContextLostListener(this);
		VEGARefCount::DecRef(m_texture);
	}
}

void VEGABackingStore_Texture::OnContextLost()
{
}

void VEGABackingStore_Texture::OnContextRestored()
{
	m_texture_dirty = true;
	// not necessarily true, but we don't know the context was lost
	// until after it happened so there's no chance to sync to buffer
	// beforehand. as texture is completely lost buffer is least
	// dirty.
	m_buffer_dirty = false;
}

OP_STATUS VEGABackingStore_Texture::SyncToBuffer(const OpRect& rect)
{
	OP_ASSERT(!m_texture_dirty);
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	VEGA3dFramebufferObject* fbo;
	RETURN_IF_ERROR(device->createFramebuffer(&fbo));
	OP_STATUS err = fbo->attachColor(m_texture);
	if (OpStatus::IsSuccess(err))
		err = device->setRenderTarget(fbo);
	if (OpStatus::IsSuccess(err))
	{
		VEGA3dDevice::FramebufferData data;
		data.x = rect.x;
		data.y = rect.y;
		data.w = rect.width;
		data.h = rect.height;
		err = device->lockRect(&data, true);
		if (OpStatus::IsSuccess(err))
		{
			m_buffer.CopyFromPixelStore(&data.pixels);
			device->unlockRect(&data, false);
		}
	}
	VEGARefCount::DecRef(fbo);
	return err;
}

void VEGABackingStore_Texture::SyncToTexture(const OpRect& rect)
{
	OP_ASSERT(!m_buffer_dirty);
	VEGASWBuffer sub = m_buffer.CreateSubset(rect.x, rect.y, rect.width, rect.height);
	m_texture->update(rect.x, rect.y, &sub);
}

void VEGABackingStore_Texture::SetColor(const OpRect& rect, UINT32 color)
{
	if (m_buffer_dirty)
	{
		if (WholeArea(rect))
			m_buffer_dirty = false;
		else
		{
			if (OpStatus::IsError(SyncToBuffer()))
				return;
		}
	}
	VEGABackingStore_SWBuffer::SetColor(rect, color);
	m_texture_dirty = true;
}

OP_STATUS VEGABackingStore_Texture::CopyRect(const OpPoint& dstp, const OpRect& srcr,
											 VEGABackingStore* store)
{
	if (store->IsA(FRAMEBUFFEROBJECT))
	{
		if (m_texture_dirty)
		{
			if (WholeArea(srcr))
				m_texture_dirty = false;
			else
				SyncToTexture();
		}
		VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
		RETURN_IF_ERROR(device->setRenderTarget(static_cast<VEGABackingStore_FBO*>(store)->GetReadRenderTarget()));
		RETURN_IF_ERROR(device->copyToTexture(m_texture, VEGA3dTexture::CUBE_SIDE_NONE, 0, dstp.x, dstp.y, srcr.x, srcr.y, srcr.width, srcr.height));
		m_buffer_dirty = true;
	}
	else
	{
		if (m_buffer_dirty)
		{
			if (WholeArea(srcr))
				m_buffer_dirty = false;
			else
				RETURN_IF_ERROR(SyncToBuffer());
		}
		RETURN_IF_ERROR(VEGABackingStore_SWBuffer::CopyRect(dstp, srcr, store));
		m_texture_dirty = true;
	}

	return OpStatus::OK;
}

VEGASWBuffer* VEGABackingStore_Texture::BeginTransaction(const OpRect& rect, AccessType acc)
{
	if (m_buffer_dirty)
	{
		OP_ASSERT(!m_texture_dirty);
		if (acc != ACC_WRITE_ONLY || !rect.Equals(OpRect(0,0,m_texture->getWidth(), m_texture->getHeight())))
			RETURN_VALUE_IF_ERROR(SyncToBuffer(), NULL);
		m_buffer_dirty = false;
	}
	return VEGABackingStore_SWBuffer::BeginTransaction(rect, acc);
}

void VEGABackingStore_Texture::EndTransaction(BOOL commit)
{
	m_texture_dirty = m_texture_dirty || commit;
}

void VEGABackingStore_Texture::Validate()
{
	if (!m_texture_dirty)
		return;
	SyncToTexture();
}

OP_STATUS VEGABackend_HW3D::createBitmapRenderTarget(VEGARenderTarget** rt, OpBitmap* bmp)
{
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (static_cast<VEGAOpBitmap*>(bmp)->isTiled())
	{
		OP_ASSERT(!"this operation is not allowed on tiled bitmaps, check calling code");
		return OpStatus::ERR; // better than crashing
	}
#endif // VEGA_LIMIT_BITMAP_SIZE
	VEGABackingStore* bstore = static_cast<VEGAOpBitmap*>(bmp)->GetBackingStore();

	if (bstore->IsA(VEGABackingStore::TEXTURE))
	{
		VEGABackingStore_FBO* nbstore = OP_NEW(VEGABackingStore_FBO, ());
		if (!nbstore)
			return OpStatus::ERR_NO_MEMORY;
		OP_STATUS err = nbstore->Construct(static_cast<VEGABackingStore_Texture*>(bstore));
		if (OpStatus::IsError(err))
		{
			VEGARefCount::DecRef(nbstore);
			return err;
		}
		bstore = nbstore;
	}
	else
		VEGARefCount::IncRef(bstore);

	OP_ASSERT(bstore->IsA(VEGABackingStore::FRAMEBUFFEROBJECT));

	VEGABitmapRenderTarget* rend = OP_NEW(VEGABitmapRenderTarget, (bstore));
	if (!rend)
	{
		VEGARefCount::DecRef(bstore);
		return OpStatus::ERR_NO_MEMORY;
	}

	*rt = rend;
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW3D::createWindowRenderTarget(VEGARenderTarget** rt, VEGAWindow* window)
{
	VEGA3dWindow* win3d;
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	RETURN_IF_ERROR(device->createWindow(&win3d, window));

	OP_STATUS status = window->initHardwareBackend(win3d);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(win3d);
		return status;
	}

	VEGABackingStore_FBO* wstore = OP_NEW(VEGABackingStore_FBO, ());
	if (!wstore)
	{
		OP_DELETE(win3d);
		return OpStatus::ERR_NO_MEMORY;
	}

	OpStatus::Ignore(wstore->Construct(win3d));

	VEGA3dWindowRenderTarget* winrt = OP_NEW(VEGA3dWindowRenderTarget, (wstore, window, win3d));
	if (!winrt)
	{
		VEGARefCount::DecRef(wstore);
		return OpStatus::ERR_NO_MEMORY;
	}

	*rt = winrt;
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW3D::createIntermediateRenderTarget(VEGARenderTarget** rt, unsigned int w, unsigned int h)
{
	VEGABackingStore_FBO* bstore = OP_NEW(VEGABackingStore_FBO, ());
	if (!bstore)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = bstore->Construct(w, h);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(bstore);
		return status;
	}

	VEGAIntermediateRenderTarget* rend = OP_NEW(VEGAIntermediateRenderTarget, (bstore, VEGARenderTarget::RT_RGBA32));
	if (!rend)
	{
		VEGARefCount::DecRef(bstore);
		return OpStatus::ERR_NO_MEMORY;
	}

	*rt = rend;
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW3D::createStencil(VEGAStencil** sten, bool component, unsigned int w, unsigned int h)
{
	VEGABackingStore_FBO* bstore = OP_NEW(VEGABackingStore_FBO, ());
	if (!bstore)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = bstore->Construct(w, h);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(bstore);
		return status;
	}

	VEGARenderTarget::RTColorFormat fmt = component ?
		VEGARenderTarget::RT_RGBA32 : VEGARenderTarget::RT_ALPHA8;

	VEGAIntermediateRenderTarget* rend = OP_NEW(VEGAIntermediateRenderTarget, (bstore, fmt));
	if (!rend)
	{
		VEGARefCount::DecRef(bstore);
		return OpStatus::ERR_NO_MEMORY;
	}

	bstore->SetColor(OpRect(0, 0, w, h), 0);

	*sten = rend;
	return OpStatus::OK;
}

/* static */
OP_STATUS VEGABackend_HW3D::createBitmapStore(VEGABackingStore** store, unsigned w, unsigned h, bool is_indexed)
{
	VEGABackingStore_Texture* bstore = OP_NEW(VEGABackingStore_Texture, ());
	if (!bstore)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = bstore->Construct(w, h, is_indexed);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(bstore);
		return status;
	}
	*store = bstore;
	return OpStatus::OK;
}

bool VEGABackend_HW3D::supportsStore(VEGABackingStore* store)
{
	return store->IsA(VEGABackingStore::TEXTURE) || store->IsA(VEGABackingStore::FRAMEBUFFEROBJECT);
}

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGABackend_HW3D::createImage(VEGAImage* img, VEGA3dTexture* tex, bool isPremultiplied)
{
	return img->init(tex, !!isPremultiplied);
}
#endif // CANVAS3D_SUPPORT

void VEGABackend_HW3D::clear(int x, int y, unsigned int w, unsigned int h, unsigned int color, VEGATransform* transform)
{
	VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;
	UINT32 col;
	if (renderTarget->getColorFormat() == VEGARenderTarget::RT_ALPHA8)
		col = (color&0xff000000) | ((color>>8)&0xff0000) | ((color>>16)&0xff00) | (color>>24);
	else
	{
		col = VEGAPixelPremultiply_BGRA8888(color);
		col = dev3d->convertToNativeColor(col);
	}
	VEGA3dRenderState* renderState = m_defaultRenderState;
	if ((col>>24) != 255)
	{
		renderState = m_defaultNoBlendRenderState;
	}

	if (!transform)
	{
		int sx = cliprect_sx;
		int sy = cliprect_sy;
		int ex = cliprect_ex;
		int ey = cliprect_ey;

		if (x > sx)
			sx = x;
		if (y > sy)
			sy = y;
		if (x+(int)w < ex)
			ex = x+w;
		if (y+(int)h < ey)
			ey = y+h;

		if (ex <= sx || ey <= sy)
			return;

		if ((color>>24) == 0)
		{
			int d_left, d_right, d_top, d_bottom;
			if (renderTarget->getDirtyRect(d_left, d_right, d_top, d_bottom))
			{
				sx = MAX(sx, d_left);
				sy = MAX(sy, d_top);
				ex = MIN(ex, d_right + 1);
				ey = MIN(ey, d_bottom + 1);

				/* If the dirty rect extends outside the render
				 * target, the call to unmarkDirty below will not work
				 * very well.  So trim the excess first. */
				if (d_left < 0)
					renderTarget->unmarkDirty(d_left, -1, d_top, d_bottom);
				if (d_top < 0)
					renderTarget->unmarkDirty(d_left, d_right, d_top, -1);
				if (d_right >= (int)renderTarget->getWidth())
					renderTarget->unmarkDirty(renderTarget->getWidth(), d_right, d_top, d_bottom);
				if (d_bottom >= (int)renderTarget->getHeight())
					renderTarget->unmarkDirty(d_left, d_right, renderTarget->getHeight(), d_bottom);
			}
			if (ex <= sx || ey <= sy)
				return;
			renderTarget->unmarkDirty(sx, ex - 1, sy, ey - 1);
		}
		else
			renderTarget->markDirty(sx, ex - 1, sy, ey - 1);

		if (m_numRemainingBatchVerts < 4)
			flushBatches();
		m_vert[m_firstBatchVert].x = VEGA_INTTOFIX(sx);
		m_vert[m_firstBatchVert].y = VEGA_INTTOFIX(sy);
		m_vert[m_firstBatchVert].color = col;
		m_vert[m_firstBatchVert+1].x = VEGA_INTTOFIX(ex);
		m_vert[m_firstBatchVert+1].y = VEGA_INTTOFIX(sy);
		m_vert[m_firstBatchVert+1].color = col;
		m_vert[m_firstBatchVert+2].x = VEGA_INTTOFIX(ex);
		m_vert[m_firstBatchVert+2].y = VEGA_INTTOFIX(ey);
		m_vert[m_firstBatchVert+2].color = col;
		m_vert[m_firstBatchVert+3].x = VEGA_INTTOFIX(sx);
		m_vert[m_firstBatchVert+3].y = VEGA_INTTOFIX(ey);
		m_vert[m_firstBatchVert+3].color = col;
		OpRect affected(sx, sy, ex-sx, ey-sy);
		scheduleBatch(NULL, 4, affected, NULL, renderState == m_defaultRenderState ? NULL : renderState);
		return;
	}
	flushBatches();

	dev3d->setRenderTarget(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame));

	if ((color>>24) != 0)
	{
		/* We could account for the transform rather than marking the
		 * whole render target as dirty.  But it may not be worth the
		 * hassle.
		 *
		 * (On the other hand, properly accounting for the transform
		 * would allow us to call unmarkDirty() when filling with
		 * fully transparent.  Again, in practice that may not
		 * actually be very useful.) */
		renderTarget->markDirty(cliprect_sx, cliprect_ex - 1, cliprect_sy, cliprect_ey - 1);
	}

	dev3d->setScissor(cliprect_sx, cliprect_sy, cliprect_ex-cliprect_sx, cliprect_ey-cliprect_sy);
	dev3d->setRenderState(renderState);
	VEGA3dDevice::Vega2dVertex verts[] =
		{
			{VEGA_INTTOFIX(x), VEGA_INTTOFIX(y),0,0,col},
			{VEGA_INTTOFIX(x+(int)w), VEGA_INTTOFIX(y),1,0,col},
			{VEGA_INTTOFIX(x), VEGA_INTTOFIX(y+(int)h),0,1,col},
			{VEGA_INTTOFIX(x+(int)w), VEGA_INTTOFIX(y+(int)h),1,1,col}
		};
	unsigned int firstVert;
	m_vbuffer->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, firstVert);
	const VEGA3dShaderProgram::WrapMode mode = VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;
	dev3d->setShaderProgram(shader2dV[mode]);
	if (transform)
	{
		VEGATransform3d projection;
		dev3d->loadOrthogonalProjection(projection);
		VEGATransform3d trans;
		trans.copy(*transform);
		projection.multiply(trans);
		shader2dV[mode]->setMatrix4(worldProjMatrix2dLocationV[mode], &projection[0], 1);
		shader2dV[mode]->invalidateOrthogonalProjection();
	}
	else
		shader2dV[mode]->setOrthogonalProjection();
	dev3d->setTexture(0, NULL);
	dev3d->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, m_vlayout2d, firstVert, 4);

	// FIXME: this invalidates too much
	dev3d->invalidate(cliprect_sx, cliprect_sy, cliprect_ex, cliprect_ey);
}

void VEGABackend_HW3D::updateAlphaTextureStencil(VEGAPath* path)
{
	VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;
	unsigned int maxVerts = dev3d->getVertexBufferSize();

	if (xorFill)
	{
		//dev3d->setClearValues(0,0,0);
		//dev3d->clear(false, false, true);

		dev3d->setRenderState(m_updateStencilXorRenderState);

		unsigned int numVerts = 0;
		unsigned int numLines = path->getNumLines();
		for (unsigned int i = 0; i < numLines; ++i)
		{
			VEGA_FIX* lineData = path->getNonWarpLine(i);
			if (lineData && lineData[VEGALINE_STARTY] != lineData[VEGALINE_ENDY])
			{
				if (numVerts+4 > maxVerts)
				{
					unsigned int firstVert;
					m_vbuffer->writeAnywhere(numVerts, sizeof(VEGA3dDevice::Vega2dVertex), m_vert, firstVert);
					unsigned int firstInd = (firstVert/4)*6;
					dev3d->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, m_vlayout2d, dev3d->getQuadIndexBuffer(firstInd + (numVerts/4)*6), 2, firstInd, (numVerts/4)*6);
					numVerts = 0;
				}
				m_vert[numVerts].x = lineData[VEGALINE_STARTX];
				m_vert[numVerts].y = lineData[VEGALINE_STARTY];
				++numVerts;
				m_vert[numVerts].x = lineData[VEGALINE_ENDX];
				m_vert[numVerts].y = lineData[VEGALINE_ENDY];
				++numVerts;
				m_vert[numVerts].x = VEGA_INTTOFIX(width+1);
				m_vert[numVerts].y = lineData[VEGALINE_ENDY];
				++numVerts;
				m_vert[numVerts].x = VEGA_INTTOFIX(width+1);
				m_vert[numVerts].y = lineData[VEGALINE_STARTY];
				++numVerts;
			}
		}
		if (numVerts)
		{
			unsigned int firstVert;
			m_vbuffer->writeAnywhere(numVerts, sizeof(VEGA3dDevice::Vega2dVertex), m_vert, firstVert);
			unsigned int firstInd = (firstVert/4)*6;
			dev3d->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, m_vlayout2d, dev3d->getQuadIndexBuffer(firstInd + (numVerts/4)*6), 2, firstInd, (numVerts/4)*6);
		}
	}
	else
	{
		unsigned int i;

		//dev3d->setClearValues(0,0,1);
		//dev3d->clear(false, false, true);

		dev3d->setRenderState(m_updateStencilRenderState);

		unsigned int numVerts = 0;
		unsigned int numLines = path->getNumLines();
		for (i = 0; i < numLines; ++i)
		{
			VEGA_FIX* lineData = path->getNonWarpLine(i);
			if (lineData && lineData[VEGALINE_STARTY] != lineData[VEGALINE_ENDY])
			{
				if (numVerts+4 > maxVerts)
				{
					unsigned int firstVert;
					m_vbuffer->writeAnywhere(numVerts, sizeof(VEGA3dDevice::Vega2dVertex), m_vert, firstVert);
					unsigned int firstInd = (firstVert/4)*6;
					dev3d->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, m_vlayout2d, dev3d->getQuadIndexBuffer(firstInd + (numVerts/4)*6), 2, firstInd, (numVerts/4)*6);
					numVerts = 0;
				}
				m_vert[numVerts].x = lineData[VEGALINE_STARTX];
				m_vert[numVerts].y = lineData[VEGALINE_STARTY];
				++numVerts;
				m_vert[numVerts].x = lineData[VEGALINE_ENDX];
				m_vert[numVerts].y = lineData[VEGALINE_ENDY];
				++numVerts;
				m_vert[numVerts].x = VEGA_INTTOFIX(width+1);
				m_vert[numVerts].y = lineData[VEGALINE_ENDY];
				++numVerts;
				m_vert[numVerts].x = VEGA_INTTOFIX(width+1);
				m_vert[numVerts].y = lineData[VEGALINE_STARTY];
				++numVerts;
			}
		}
		if (numVerts)
		{
			unsigned int firstVert;
			m_vbuffer->writeAnywhere(numVerts, sizeof(VEGA3dDevice::Vega2dVertex), m_vert, firstVert);
			unsigned int firstInd = (firstVert/4)*6;
			dev3d->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, m_vlayout2d, dev3d->getQuadIndexBuffer(firstInd + (numVerts/4)*6), 2, firstInd, (numVerts/4)*6);
		}
	}
}

void VEGABackend_HW3D::releaseShaderPrograms()
{
	size_t i;
	for (i = 0; i < VEGA3dShaderProgram::WRAP_MODE_COUNT; ++i)
	{
		VEGARefCount::DecRef(shader2dV[i]);
		shader2dV[i] = NULL;
		VEGARefCount::DecRef(shader2dTexGenV[i]);
		shader2dTexGenV[i] = NULL;
	}

	for (i = 0; i < TEXT_SHADER_COUNT; ++i)
	{
		VEGARefCount::DecRef(shader2dText[i]);
		shader2dText[i] = NULL;
	}
}

unsigned VEGABackend_HW3D::calculateArea(VEGA_FIX minx, VEGA_FIX miny, VEGA_FIX maxx, VEGA_FIX maxy)
{
	// It isn't currently expected that this method be called for this
	// backend. If any code needs to call this method, we need to
	// decide how the area should be calculated.
	OP_ASSERT(!"Not implemented");
	return 0;
}

void VEGABackend_HW3D::updateStencil(VEGAStencil* stencil, VEGA_FIX* strans, VEGA_FIX* ttrans, VEGA3dTexture*& stencilTex)
{
	if (stencil)
	{
		strans[0] = VEGA_INTTOFIX(1) / stencil->getWidth();
		strans[3] = -VEGA_INTTOFIX(stencil->getOffsetX()) / stencil->getWidth();
		ttrans[1] = VEGA_INTTOFIX(1) / stencil->getHeight();
		ttrans[3] = -VEGA_INTTOFIX(stencil->getOffsetY()) / stencil->getHeight();

		VEGABackingStore* stencil_store = stencil->GetBackingStore();
		OP_ASSERT(stencil_store->IsA(VEGABackingStore::FRAMEBUFFEROBJECT));
		VEGA3dRenderTarget* stencil_rt = static_cast<VEGABackingStore_FBO*>(stencil_store)->GetReadRenderTarget();
		OP_ASSERT(stencil_rt->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE);
		stencilTex = static_cast<VEGA3dFramebufferObject*>(stencil_rt)->getAttachedColorTexture();
	}
	else
		stencilTex = NULL;
}

void VEGABackend_HW3D::DrawTriangleArgs::setVert(VEGA3dDevice::Vega2dVertex* vert, unsigned idx)
{
	const VEGA_FIX* line = path->getLine(idx);
	vert->color = col;
	vert->x = line[VEGALINE_STARTX];
	vert->y = line[VEGALINE_STARTY];
	vert->s = vert->x;
	vert->t = vert->y;
	textrans->apply(vert->s, vert->t);
}

void VEGABackend_HW3D::drawTriangles(DrawTriangleArgs& args)
{
	// FIXME: maybe make second loop for s,t, so they're only set when tex != 0

#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(args.path->getCategory() != VEGAPath::COMPLEX);
#endif // DEBUG_ENABLE_OPASSERT
	const unsigned int numLines = args.path->getNumLines();
	OP_ASSERT(numLines >= 3);

	VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;
	unsigned int maxTris = dev3d->getVertexBufferSize() / 3;

	flushBatches();
	dev3d->setRenderTarget(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame));
	dev3d->setScissor(args.clampRect->x, args.clampRect->y, args.clampRect->width, args.clampRect->height);
	if (args.prog)
	{
		dev3d->setShaderProgram(args.prog);
		args.prog->setOrthogonalProjection();
	}
	dev3d->setTexture(0, args.tex);
	dev3d->setTexture(1, args.stencilTex);
	dev3d->setTexture(2, NULL);

	dev3d->setRenderState(args.renderState ? args.renderState : m_defaultRenderState);

	switch (args.mode)
	{
	case BM_INDEXED_TRIANGLES:
	{
		OP_ASSERT(args.indexedTriangles);
		OP_ASSERT(args.indexedTriangleCount > 0);

		int firstTri = 0;
		unsigned o = 0;
		do
		{
			unsigned int iTris = args.indexedTriangleCount - firstTri;
			if (iTris > maxTris)
				iTris = maxTris;
			VEGA3dDevice::Vega2dVertex* curVert = m_vert;
			for (unsigned int i = 0; i < 3*iTris; ++i)
			{
				args.setVert(curVert, args.indexedTriangles[o]);
				++curVert;
				++o;
			}
			m_vbuffer->update(0, iTris*3*sizeof(VEGA3dDevice::Vega2dVertex), m_vert);
			dev3d->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, args.layout, 0, iTris*3);
			firstTri += iTris;
		} while (firstTri < args.indexedTriangleCount);
		break;
	}
	case BM_TRIANGLE_FAN:
	{
		VEGA_FIX x0 = args.path->getLine(0)[VEGALINE_STARTX];
		VEGA_FIX y0 = args.path->getLine(0)[VEGALINE_STARTY];

		VEGA_FIX s0 = x0, t0 = y0;
		args.textrans->apply(s0, t0);
		VEGA_FIX prevs = args.path->getLine(0)[VEGALINE_ENDX];
		VEGA_FIX prevt = args.path->getLine(0)[VEGALINE_ENDY];
		args.textrans->apply(prevs, prevt);

		unsigned int firstLine = 1;
		do
		{
			unsigned int numTris = numLines-1-firstLine;
			if (numTris > maxTris)
				numTris = maxTris;
			VEGA3dDevice::Vega2dVertex* curVert = m_vert;
			for (unsigned int i = 0; i < numTris; ++i)
			{
				curVert->color = args.col;
				curVert->x = x0;
				curVert->y = y0;
				curVert->s = s0;
				curVert->t = t0;
				++curVert;
				const VEGA_FIX* lineData = args.path->getLine(i+firstLine);
				curVert->color = args.col;
				curVert->x = lineData[VEGALINE_STARTX];
				curVert->y = lineData[VEGALINE_STARTY];
				curVert->s = prevs;
				curVert->t = prevt;
				++curVert;
				prevs = lineData[VEGALINE_ENDX];
				prevt = lineData[VEGALINE_ENDY];
				curVert->color = args.col;
				curVert->x = prevs;
				curVert->y = prevt;
				args.textrans->apply(prevs, prevt);
				curVert->s = prevs;
				curVert->t = prevt;
				++curVert;
			}
			m_vbuffer->update(0, numTris*3*sizeof(VEGA3dDevice::Vega2dVertex), m_vert);
			dev3d->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, args.layout, 0, numTris*3);

			firstLine += numTris;
		} while (firstLine < numLines-1);
		break;
	}
	default:
		OP_ASSERT(!"unsupported batch mode");
		return;
	}
}

void VEGABackend_HW3D::scheduleTriangles(DrawTriangleArgs& args)
{
	// FIXME: maybe make second loop for s,t, so they're only set when tex != 0

#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(args.path->getCategory() != VEGAPath::COMPLEX);
#endif // DEBUG_ENABLE_OPASSERT
	const unsigned int numLines = args.path->getNumLines();
	OP_ASSERT(numLines >= 3);

	switch (args.mode)
	{
	case BM_INDEXED_TRIANGLES:
	{
		OP_ASSERT(args.indexedTriangles);
		OP_ASSERT(args.indexedTriangleCount > 0);

		if (numLines > m_numRemainingBatchVerts)
		{
			VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;
			if (numLines <= dev3d->getVertexBufferSize())
				flushBatches();
			else
			{
				// cannot draw indexed triangles unless all vertices can be accessed at once
				drawTriangles(args);
				return;
			}
		}

		// copy vertex data
		OP_ASSERT(numLines <= m_numRemainingBatchVerts);
		VEGA3dDevice::Vega2dVertex* curVert = m_vert+m_firstBatchVert;
		for (unsigned int line = 0; line < numLines; ++line)
		{
			args.setVert(curVert, line);
			++curVert;
		}

		// copy index data
		OP_ASSERT(args.indexedTriangleCount <= (int)(numLines-2));
		const int indexCount = 3*args.indexedTriangleCount;
		OP_ASSERT(m_triangleIndexCount + indexCount <= g_vegaGlobals.vega3dDevice->getTriangleIndexBufferSize());
		op_memcpy(m_triangleIndices + m_triangleIndexCount, args.indexedTriangles, indexCount*sizeof(*m_triangleIndices));
		// update offsets
		for (int i = 0; i < indexCount; ++i)
			m_triangleIndices[m_triangleIndexCount+i] += m_firstBatchVert;

		scheduleBatch(args.tex, numLines, *args.affected, args.needsClamp ? args.clampRect : 0, args.renderState, args.mode, indexCount);
		break;
	}
	case BM_TRIANGLE_FAN:
	{
		unsigned int line = 1;
		while (line < numLines)
		{
			unsigned int numVerts = 1;
			if (m_numRemainingBatchVerts < 3)
				flushBatches();
			VEGA3dDevice::Vega2dVertex* curVert = m_vert+m_firstBatchVert;
			args.setVert(curVert, 0);
			++curVert;
			while (line < numLines && m_numRemainingBatchVerts > numVerts)
			{
				args.setVert(curVert, line);
				++curVert;
				++line;
				++numVerts;
			}
			if (line < numLines)
			{
				// We need to chop the path in two, we need to render the final point again
				// for the next path batch.
				// If we don't do this, there will be a gap, see DSK-351013
				--line;
			}
			scheduleBatch(args.tex, numVerts, *args.affected, args.needsClamp ? args.clampRect : 0, args.renderState, args.mode);
		}
		break;
	}
	default:
		OP_ASSERT(!"unsupported batch mode");
		return;
	}
}

OP_STATUS VEGABackend_HW3D::fillPath(VEGAPath *path, VEGAStencil *stencil, VEGA3dRenderState* renderState)
{
	if (!path->isClosed())
	{
		RETURN_IF_ERROR(path->close(false));
	}

	VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;

	int sx, sy, ex, ey;
	sx = cliprect_sx;
	sy = cliprect_sy;
	ex = cliprect_ex;
	ey = cliprect_ey;

	int minx = width-1;
	int miny = height-1;
	int maxx = 0;
	int maxy = 0;
	unsigned int numLines = path->getNumLines();
	// Drawing a shape with less than 3 lines is not possible and could mess up the fast-path
	if (numLines < 3)
		return OpStatus::OK;
	VEGAPath::Category category = path->getCategory();

	// If the path has multiple sub paths we need to rebuild it to be able to triangulate it.
	// This is the rebuilt version (the original path is untouched).
	VEGAPath mergedPath;
	if (category == VEGAPath::COMPLEX && !path->cannotBeConvertedToSimple())
	{
		// Merges the sub paths into a single path and checks
		// if the resulting path is self intersecting.
		const OP_STATUS status = path->tryToMakeMultipleSubPathsSimple(mergedPath, xorFill);
		// fallback to slow path for merging error
		if (OpStatus::IsSuccess(status))
		{
			if (mergedPath.getCategory() == VEGAPath::SIMPLE)
			{
				path = &mergedPath; // mergedPath is triangulateable, switch to that one so we don't change the original path
				category = path->getCategory(); // will be updated by now
			}
		}
	}

	OP_ASSERT(category != VEGAPath::UNDETERMINED);

	// If the path is now simple it can be triangulated.
	if (category == VEGAPath::SIMPLE)
	{
		unsigned short* tris;
		int numTris;
		const OP_STATUS s = path->getTriangles(&tris, &numTris);
		RETURN_IF_MEMORY_ERROR(s);
		// fallback to slow path for triangulation errors
		if (OpStatus::IsError(s))
			category = VEGAPath::COMPLEX;
		else if (!numTris)
			return OpStatus::OK;
	}

	numLines = path->getNumLines(); // might have changed.
	VEGA_FIX* lineData = path->getLine(0);
	VEGA_FIX fixminx = lineData[VEGALINE_STARTX], fixmaxx = lineData[VEGALINE_STARTX], fixminy = lineData[VEGALINE_STARTY], fixmaxy = lineData[VEGALINE_STARTY];
	for (unsigned int i = 1; i < numLines; ++i)
	{
		lineData = path->getLine(i);
		VEGA_FIX lineStartX = lineData[VEGALINE_STARTX];
		VEGA_FIX lineStartY = lineData[VEGALINE_STARTY];
		if (lineStartX < fixminx)
		{
			fixminx = lineStartX;
		}
		if (lineStartX > fixmaxx)
		{
			fixmaxx = lineStartX;
		}
		if (lineStartY < fixminy)
		{
			fixminy = lineStartY;
		}
		if (lineStartY > fixmaxy)
		{
			fixmaxy = lineStartY;
		}
	}
	minx = VEGA_FIXTOINT(VEGA_FLOOR(fixminx));
	maxx = VEGA_FIXTOINT(VEGA_CEIL(fixmaxx));
	miny = VEGA_FIXTOINT(VEGA_FLOOR(fixminy));
	maxy = VEGA_FIXTOINT(VEGA_CEIL(fixmaxy));
	bool needsClamp = false;
	if (minx < sx)
	{
		minx = sx;
		needsClamp = true;
	}
	if (miny < sy)
	{
		miny = sy;
		needsClamp = true;
	}
	if (maxx > ex)
	{
		maxx = ex;
		needsClamp = true;
	}
	if (maxy > ey)
	{
		maxy = ey;
		needsClamp = true;
	}
	r_minx = minx;
	r_miny = miny;
	r_maxx = maxx;
	r_maxy = maxy;
	if (maxy <= miny || maxx <= minx)
	{
		r_minx = width-1;
		r_miny = height-1;
		r_maxx = 0;
		r_maxy = 0;
		return OpStatus::OK;
	}

	renderTarget->markDirty(minx, maxx, miny, maxy);

	// initialize stuff that's common for fast (convex) and slow (complex) paths
	VEGATransform textrans;
	textrans.loadIdentity();
	VEGA3dVertexLayout* layout = m_vlayout2dTexGen;
	VEGA3dShaderProgram* prog = NULL;
	VEGAFill* const fill = fillstate.fill;
	VEGA3dTexture* tex = NULL;
	BOOL useStraightAlpha = FALSE;
	BOOL useCustomShader = FALSE;
	if (fill)
	{
		tex = fill->getTexture();
		if (tex)
		{
			VEGATransform temptrans = fill->getTexTransform();
			textrans.loadScale(VEGA_INTTOFIX(1)/tex->getWidth(), VEGA_INTTOFIX(1)/tex->getHeight());
			textrans.multiply(temptrans);

			// OpenGL ES doesn't support clamp-to-border - use slow-path
			if (category == VEGAPath::SIMPLE)
			{
				const bool clamp_x = fill->getXSpread() == VEGAFill::SPREAD_CLAMP_BORDER;
				const bool clamp_y = fill->getYSpread() == VEGAFill::SPREAD_CLAMP_BORDER;
				if (clamp_x || clamp_y)
					category = VEGAPath::COMPLEX;
			}
		}
		// Render target may have changed in the above VEGAFill::getTexture call
		dev3d->setRenderTarget(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame));

		prog = fill->getCustomShader();
		useCustomShader = !!prog;

		if (fill->usePremulRenderState())
		{
			RETURN_IF_ERROR(createPremulRenderState());
			OP_ASSERT(!renderState);
			renderState = m_premultiplyRenderState;
			useStraightAlpha = TRUE;
		}

		VEGA3dVertexLayout* l = fill->getCustomVertexLayout();
		if (l)
		{
			OP_ASSERT(prog); // can only get custom layout when using custom shader
			layout = l;
		}
	}
	VEGA_FIX strans[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	VEGA_FIX ttrans[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	VEGA3dTexture* stencilTex;
	updateStencil(stencil, strans, ttrans, stencilTex);
	const VEGA3dShaderProgram::WrapMode stencilMode = VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;
	BOOL stencilColorToLuminance = stencil ? stencil->isComponentBased() : FALSE;

	if (category != VEGAPath::COMPLEX)
	{
		VEGABackingStore* bstore = renderTarget->GetBackingStore();
#ifdef VEGA_NATIVE_FONT_SUPPORT
		if (static_cast<VEGABackingStore_FBO*>(bstore)->EnableMultisampling(quality, m_currentFrame))
			m_nativeFontRect.Empty();
#else  // VEGA_NATIVE_FONT_SUPPORT
		static_cast<VEGABackingStore_FBO*>(bstore)->EnableMultisampling(quality, m_currentFrame);
#endif // VEGA_NATIVE_FONT_SUPPORT

		OpRect clampRect(sx, sy, ex-sx, ey-sy);

		if (stencil)
		{
			if (!prog)
				prog = shader2dTexGenV[stencilMode];
		}
		if (prog)
		{
			flushBatches();
			dev3d->setRenderTarget(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame));
			dev3d->setScissor(clampRect.x, clampRect.y, clampRect.width, clampRect.height);
			dev3d->setShaderProgram(prog);
			prog->setOrthogonalProjection();

			if (stencil)
			{
				int transSLocation = useCustomShader ? prog->getConstantLocation("texTransS") : texTransSLocationV[stencilMode];
				int transTLocation = useCustomShader ? prog->getConstantLocation("texTransT") : texTransTLocationV[stencilMode];

				prog->setVector4(transSLocation, strans);
				prog->setVector4(transTLocation, ttrans);
			}

			dev3d->setTexture(0, tex);
			dev3d->setTexture(1, stencilTex);
			dev3d->setTexture(2, NULL);
		}
		VEGAPath constrained;
		if (tex)
		{
			VEGATransform temptrans = fill->getTexTransform();
			textrans.loadScale(VEGA_INTTOFIX(1)/tex->getWidth(), VEGA_INTTOFIX(1)/tex->getHeight());
			textrans.multiply(temptrans);

			// OpenGL ES doesn't support clamp-to-border, so we have
			// to constrain the path when textures shouldn't wrap or
			// the edge pixels will be repeated - see CT-1287
			const bool clamp_x = fill->getXSpread() == VEGAFill::SPREAD_CLAMP_BORDER;
			const bool clamp_y = fill->getYSpread() == VEGAFill::SPREAD_CLAMP_BORDER;
			if (clamp_x || clamp_y)
			{
				OP_ASSERT(category == VEGAPath::CONVEX);
				RETURN_IF_ERROR(path->buildConstrainedPath(constrained, textrans, clamp_x, clamp_y));
				path = &constrained;
				numLines = path->getNumLines();
				if (!numLines)
					return OpStatus::OK;
			}
		}

		OpRect affected(r_minx, r_miny, r_maxx-r_minx, r_maxy-r_miny);
		DrawTriangleArgs args;
		args.path = path;
		args.mode = (category == VEGAPath::SIMPLE) ? BM_INDEXED_TRIANGLES : BM_TRIANGLE_FAN;
		args.affected = &affected;
		args.clampRect = &clampRect;
		args.needsClamp = needsClamp;
		// args.col = 0; // set below
		args.textrans = &textrans;
		args.tex = tex;
		args.stencilTex = stencilTex;
		args.renderState = renderState;
		args.layout = m_vlayout2dTexGen;
		args.prog = prog;

		if (category == VEGAPath::SIMPLE) // should be cached by previous call
			RETURN_IF_ERROR(path->getTriangles(&args.indexedTriangles, &args.indexedTriangleCount));

		bool direct = false;
		int compBasedLocation = -1;

		if (fill && renderTarget->getColorFormat() != VEGARenderTarget::RT_ALPHA8)
		{
			unsigned char opacity = fill->getFillOpacity();
			args.col = ((opacity<<24)|(opacity<<16)|(opacity<<8)|opacity);

			if (prog)
			{
				if (stencilColorToLuminance)
					compBasedLocation = useCustomShader ? prog->getConstantLocation("stencilComponentBased") : stencilCompBasedLocation[stencilMode];
				direct = true;
			}
		}
		else
		{
			UINT32 col =
				(renderTarget->getColorFormat() == VEGARenderTarget::RT_ALPHA8) ?
				0xffffffff :
				fillstate.color;
			unsigned char opacity = (col >> 24);
			col = (opacity << 24) | (col & 0x00ffffff);
			col = dev3d->convertToNativeColor(VEGAPixelPremultiply_BGRA8888(col));
			args.col = col;

			if (stencil)
			{
				dev3d->setTexture(0, NULL);
				direct = true;
			}
		}

		if (direct)
		{
			if (compBasedLocation >= 0)
				prog->setScalar(compBasedLocation, TRUE);
			drawTriangles(args);
			if (compBasedLocation >= 0)
				prog->setScalar(compBasedLocation, FALSE);
		}
		else
		{
			scheduleTriangles(args);
		}

		// FIXME: this invalidates too much
		dev3d->invalidate(cliprect_sx, cliprect_sy, cliprect_ex, cliprect_ey);
		return OpStatus::OK;
	}

	flushBatches();

	// OpenGL ES cannot clamp to border, so an extra stencil buffer is
	// needed for this.
	VEGAAutoRenderTarget _additionalStencil;
	if (tex)
	{
		const bool clamp_x = fill->getXSpread() == VEGAFill::SPREAD_CLAMP_BORDER;
		const bool clamp_y = fill->getYSpread() == VEGAFill::SPREAD_CLAMP_BORDER;

		if (clamp_x || clamp_y)
		{
			// Gist: create a new stencil buffer, and render the 'clipped
			// texture' rect to it.
			VEGAStencil* additionalStencil = 0;

			RETURN_IF_ERROR(createStencil(&additionalStencil, false, renderTarget->getWidth(), renderTarget->getHeight()));
			_additionalStencil.reset(additionalStencil);

			VEGA_FIX px, py;
			VEGA_FIX mins = 0, mint = 0, maxs = 1, maxt = 1;

			// if one direction is not clamped, get min and max s and
			// t from bounding box
			if (!(clamp_x && clamp_y))
			{
				// upper left
				px = fixminx;
				py = fixminy;
				textrans.apply(px, py);
				mins = maxs = px;
				mint = maxt = py;

				// upper right
				px = fixmaxx;
				py = fixminy;
				textrans.apply(px, py);
				if (px < mins)
					mins = px;
				if (px > maxs)
					maxs = px;
				if (py < mint)
					mint = py;
				if (py > maxt)
					maxt = py;

				// lower left
				px = fixminx;
				py = fixmaxy;
				textrans.apply(px, py);
				if (px < mins)
					mins = px;
				if (px > maxs)
					maxs = px;
				if (py < mint)
					mint = py;
				if (py > maxt)
					maxt = py;

				// lower right
				px = fixmaxx;
				py = fixmaxy;
				textrans.apply(px, py);
				if (px < mins)
					mins = px;
				if (px > maxs)
					maxs = px;
				if (py < mint)
					mint = py;
				if (py > maxt)
					maxt = py;
			}

			if (clamp_x)
			{
				mins = 0;
				maxs = 1;
			}
			if (clamp_y)
			{
				mint = 0;
				maxt = 1;
			}

			// 'clipped texture' rect is obtained by applying the
			// inverse texcoord transformation matrix to the min and
			// max s and t coords
			VEGATransform texinv = textrans;
#ifdef DEBUG_ENABLE_OPASSERT
			const bool r =
#endif // DEBUG_ENABLE_OPASSERT
			texinv.invert();
			OP_ASSERT(r);

			VEGA3dDevice::Vega2dVertex* curVert = m_vert;
			unsigned col = 0xffffffff;

			px = mins; py = mint; texinv.apply(px, py);
			curVert->x = px;
			curVert->y = py;
			curVert->s = 0;
			curVert->t = 0;
			curVert->color = col;
			++curVert;

			px = maxs; py = mint; texinv.apply(px, py);
			curVert->x = px;
			curVert->y = py;
			curVert->s = 1;
			curVert->t = 0;
			curVert->color = col;
			++curVert;

			px = maxs; py = maxt; texinv.apply(px, py);
			curVert->x = px;
			curVert->y = py;
			curVert->s = 1;
			curVert->t = 1;
			curVert->color = col;
			++curVert;

			px = mins; py = maxt; texinv.apply(px, py);
			curVert->x = px;
			curVert->y = py;
			curVert->s = 0;
			curVert->t = 1;
			curVert->color = col;
			++curVert;

			VEGA3dVertexLayout* stencil_layout = m_vlayout2dTexGen;
			VEGA3dShaderProgram* stencil_prog = shader2dV[VEGA3dShaderProgram::WRAP_CLAMP_CLAMP];
			VEGABackingStore* bs = additionalStencil->GetBackingStore();
			OP_ASSERT(bs->IsA(VEGABackingStore::FRAMEBUFFEROBJECT));
			VEGA3dRenderState* rs = dev3d->getDefault2dNoBlendNoScissorRenderState();

			dev3d->setShaderProgram(stencil_prog);
			stencil_prog->setOrthogonalProjection();
			dev3d->setRenderTarget(static_cast<VEGABackingStore_FBO*>(bs)->GetWriteRenderTarget(m_currentFrame));
			dev3d->setRenderState(rs);

			unsigned int firstVert;
			m_vbuffer->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), m_vert, firstVert);
			dev3d->setTexture(0, NULL);
			dev3d->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_FAN, stencil_layout, firstVert, 4);

			stencil = additionalStencil;
			updateStencil(stencil, strans, ttrans, stencilTex);
		}
	}

	if (!m_stencilModulateRenderState)
	{
		RETURN_IF_ERROR(dev3d->createRenderState(&m_stencilModulateRenderState, false));
		m_stencilModulateRenderState->enableScissor(true);
		m_stencilModulateRenderState->setBlendFunc(VEGA3dRenderState::BLEND_DST_COLOR, VEGA3dRenderState::BLEND_ZERO);
		m_stencilModulateRenderState->enableBlend(true);
	}

	// The stencil approach
	VEGA3dTexture* alphaTexture = dev3d->getAlphaTexture();
	if (!alphaTexture)
		return OpStatus::ERR;

	// FIXME: use FSAA if available
	int xscale = 1;
	int yscale = 1;

	unsigned int xTileSize = alphaTexture->getWidth()/xscale;
	unsigned int yTileSize = alphaTexture->getHeight()/yscale;
	unsigned int numXTiles = ((r_maxx-r_minx)+xTileSize-1)/xTileSize;
	unsigned int numYTiles = ((r_maxy-r_miny)+yTileSize-1)/yTileSize;
	unsigned int numTiles = numXTiles*numYTiles;
	m_curRasterBuffer = dev3d->getAlphaTextureBuffer();
	for (unsigned int tile = 0; tile < numTiles; ++tile)
	{
		unsigned int ytile = tile/numXTiles;
		unsigned int xtile = tile-ytile*numXTiles;
		unsigned int tileX = r_minx+xTileSize*xtile;
		unsigned int tileY = r_miny+yTileSize*ytile;
		unsigned int tileWidth = xTileSize;
		unsigned int tileHeight = yTileSize;
		if (xtile == numXTiles-1)
			tileWidth = (r_maxx-r_minx)-(numXTiles-1)*xTileSize;
		if (ytile == numYTiles-1)
			tileHeight = (r_maxy-r_miny)-(numYTiles-1)*yTileSize;

		// Rasterize to an alpha buffer
		rasterizer.setXORFill(xorFill);
		rasterizer.setRegion(tileX, tileY, tileX+tileWidth, tileY+tileHeight);
		m_lastRasterPos = 0;

		m_curRasterArea.x = tileX;
		m_curRasterArea.y = tileY;
		m_curRasterArea.width = tileWidth;
		m_curRasterArea.height = tileHeight;
		RETURN_IF_ERROR(rasterizer.rasterize(path));
		if (tileWidth*tileHeight > m_lastRasterPos)
			op_memset(m_curRasterBuffer+m_lastRasterPos, 0, (tileWidth*tileHeight-m_lastRasterPos));

		// Upload the alpha buffer to a texture
		alphaTexture->update(0, 0, tileWidth, tileHeight, m_curRasterBuffer);

		// Draw using the alpha texture as a mask

		VEGA3dDevice::Vega2dVertex verts[] =
		{
			{VEGA_INTTOFIX(tileX), VEGA_INTTOFIX(tileY),0,0,0xffffffff},
			{VEGA_INTTOFIX(tileX), VEGA_INTTOFIX(tileY+tileHeight),0,0,0xffffffff},
			{VEGA_INTTOFIX(tileX+tileWidth), VEGA_INTTOFIX(tileY),0,0,0xffffffff},
			{VEGA_INTTOFIX(tileX+tileWidth), VEGA_INTTOFIX(tileY+tileHeight),0,0,0xffffffff}
		};

		strans[4] = VEGA_INTTOFIX(xscale)/alphaTexture->getWidth();
		strans[7] = -VEGA_INTTOFIX(tileX)*VEGA_INTTOFIX(xscale)/alphaTexture->getWidth();
		ttrans[5] = VEGA_INTTOFIX(yscale)/alphaTexture->getHeight();
		ttrans[7] = -VEGA_INTTOFIX(tileY)*VEGA_INTTOFIX(yscale)/alphaTexture->getHeight();

		dev3d->setRenderTarget(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame));

		dev3d->setScissor(tileX, tileY, tileWidth, tileHeight);
		dev3d->setRenderState(renderState ? renderState : m_defaultRenderState);

		dev3d->setTexture(0, tex);
		dev3d->setTexture(1, stencilTex);
		dev3d->setTexture(2, alphaTexture);

		if (tex && renderTarget->getColorFormat() != VEGARenderTarget::RT_ALPHA8)
		{
			const VEGA3dShaderProgram::WrapMode mode = VEGA3dShaderProgram::GetWrapMode(tex);

			if (!prog)
				prog = shader2dTexGenV[mode];

			int transSLocation = useCustomShader ? prog->getConstantLocation("texTransS") : texTransSLocationV[mode];
			int transTLocation = useCustomShader ? prog->getConstantLocation("texTransT") : texTransTLocationV[mode];
			int alphaLocation = useCustomShader ? prog->getConstantLocation("straightAlpha") : straightAlphaLocation[mode];

			dev3d->setShaderProgram(prog);
			prog->setOrthogonalProjection();
			prog->setVector4(transSLocation, strans, 2);
			prog->setVector4(transTLocation, ttrans, 2);

			UINT32 col = ((fill->getFillOpacity()<<24)|(fill->getFillOpacity()<<16)|(fill->getFillOpacity()<<8)|fill->getFillOpacity());

			verts[0].color = verts[1].color = verts[2].color = verts[3].color = col;
			verts[0].s = verts[0].x;
			verts[0].t = verts[0].y;
			textrans.apply(verts[0].s, verts[0].t);
			verts[1].s = verts[1].x;
			verts[1].t = verts[1].y;
			textrans.apply(verts[1].s, verts[1].t);
			verts[2].s = verts[2].x;
			verts[2].t = verts[2].y;
			textrans.apply(verts[2].s, verts[2].t);
			verts[3].s = verts[3].x;
			verts[3].t = verts[3].y;
			textrans.apply(verts[3].s, verts[3].t);

			unsigned int firstVert;
			m_vbuffer->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, firstVert);

			if (useStraightAlpha)
				prog->setScalar(alphaLocation, TRUE);

			dev3d->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, layout, firstVert, 4);

			if (useStraightAlpha)
				prog->setScalar(alphaLocation, FALSE);
		}
		else
		{
			const VEGA3dShaderProgram::WrapMode mode = VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;

			// FIXME: what if prog is already set? looks wrong to just overwrite...
			prog = shader2dTexGenV[mode];
			dev3d->setShaderProgram(prog);
			prog->setOrthogonalProjection();
			prog->setVector4(texTransSLocationV[mode], strans, 2);
			prog->setVector4(texTransTLocationV[mode], ttrans, 2);

			UINT32 col;
			if (renderTarget->getColorFormat() == VEGARenderTarget::RT_ALPHA8)
				col = 0xffffffff;
			else
				col = dev3d->convertToNativeColor(VEGAPixelPremultiply_BGRA8888(fillstate.color));

			verts[0].color = verts[1].color = verts[2].color = verts[3].color = col;
			unsigned int firstVert;
			m_vbuffer->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, firstVert);

			dev3d->setTexture(0, NULL);
			dev3d->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, layout, firstVert, 4);
		}
	}


	dev3d->invalidate(r_minx, r_miny, r_maxx, r_maxy);
	return OpStatus::OK;
}

void VEGABackend_HW3D::drawSpans(VEGASpanInfo* raster_spans, unsigned int span_count)
{
	for (unsigned int span_num = 0; span_num < span_count; ++span_num)
	{
		VEGASpanInfo& span = raster_spans[span_num];

		if (span.length == 0)
			continue;

		unsigned int rasterPos = (span.scanline-m_curRasterArea.y)*m_curRasterArea.width + span.pos-m_curRasterArea.x;
		if (rasterPos > m_lastRasterPos)
			op_memset(m_curRasterBuffer+m_lastRasterPos, 0, rasterPos-m_lastRasterPos);
		m_lastRasterPos = rasterPos+span.length;
		UINT8* stenbuf = m_curRasterBuffer + rasterPos;
		if (!span.mask)
		{
			op_memset(stenbuf, 0xff, span.length);
		}
		else /* span.mask && !alphaBlend */
		{
			op_memcpy(stenbuf, span.mask, span.length);
		}
	}
}

OP_STATUS VEGABackend_HW3D::drawString(VEGAFont* font, int x, int y, const ProcessedString* processed_string, VEGAStencil* stencil)
{
	OP_ASSERT(processed_string);
	OP_STATUS status = OpStatus::OK;

	VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;

	VEGA3dTexture* tex = font->getTexture(dev3d, false);
	if (!tex)
		return OpStatus::ERR;

	OpRect clipRect(cliprect_sx, cliprect_sy, cliprect_ex-cliprect_sx, cliprect_ey-cliprect_sy);
	UINT32 col = dev3d->convertToNativeColor(VEGAPixelPremultiply_BGRA8888(fillstate.color));
	int xpos = x, ypos = y + font->Ascent();

	// Make sure nothing is bound to texture unit 1 (stencil texture in shader).
	dev3d->setTexture(1, NULL);

	if (stencil)
		flushBatches();

	for (unsigned int c = 0; c < processed_string->m_length; ++c)
	{
		ypos = y + processed_string->m_processed_glyphs[c].m_pos.y;
		xpos = x + processed_string->m_processed_glyphs[c].m_pos.x;

		if (xpos >= cliprect_ex)
			break;

		VEGAFont::VEGAGlyph glyph;
		float s1, t1, s2, t2;
		unsigned int texx, texy;
		glyph.advance = 0;

		OP_STATUS s = font->getGlyph(processed_string->m_processed_glyphs[c].m_id, glyph, texx, texy, processed_string->m_is_glyph_indices);
		if (OpStatus::IsSuccess(s) &&
			glyph.width>0 && glyph.height>0)
		{
			int glyph_sx = xpos + glyph.left;
			int glyph_sy = ypos - glyph.top;

			OpRect glyphRect(glyph_sx, glyph_sy, glyph.width, glyph.height);
			glyphRect.SafeIntersectWith(clipRect);
			if (glyphRect.IsEmpty())
				continue;

			s1 = (float)(texx + glyphRect.x - glyph_sx)/(float)tex->getWidth();
			s2 = (float)(texx + glyphRect.Right() - glyph_sx)/(float)tex->getWidth();
			t1 = (float)(texy + glyphRect.y - glyph_sy)/(float)tex->getHeight();
			t2 = (float)(texy + glyphRect.Bottom() - glyph_sy)/(float)tex->getHeight();

			if (m_numRemainingBatchVerts < 4)
			{
				flushBatches();
			}
			m_vert[m_firstBatchVert].x = VEGA_INTTOFIX(glyphRect.x);
			m_vert[m_firstBatchVert].y = VEGA_INTTOFIX(glyphRect.y);
			m_vert[m_firstBatchVert].s = s1;
			m_vert[m_firstBatchVert].t = t1;
			m_vert[m_firstBatchVert].color = col;
			m_vert[m_firstBatchVert+1].x = VEGA_INTTOFIX(glyphRect.Right());
			m_vert[m_firstBatchVert+1].y = VEGA_INTTOFIX(glyphRect.y);
			m_vert[m_firstBatchVert+1].s = s2;
			m_vert[m_firstBatchVert+1].t = t1;
			m_vert[m_firstBatchVert+1].color = col;
			m_vert[m_firstBatchVert+2].x = VEGA_INTTOFIX(glyphRect.Right());
			m_vert[m_firstBatchVert+2].y = VEGA_INTTOFIX(glyphRect.Bottom());
			m_vert[m_firstBatchVert+2].s = s2;
			m_vert[m_firstBatchVert+2].t = t2;
			m_vert[m_firstBatchVert+2].color = col;
			m_vert[m_firstBatchVert+3].x = VEGA_INTTOFIX(glyphRect.x);
			m_vert[m_firstBatchVert+3].y = VEGA_INTTOFIX(glyphRect.Bottom());
			m_vert[m_firstBatchVert+3].s = s1;
			m_vert[m_firstBatchVert+3].t = t2;
			m_vert[m_firstBatchVert+3].color = col;

			scheduleFontBatch(font, 4, glyphRect);
		}
		else if (OpStatus::IsError(s) && !OpStatus::IsMemoryError(status))
			status = s;
	}
	finishCurrentFontBatch();

	if (stencil)
		flushBatches(stencil);

	ypos = y+font->Height();
	if (x < cliprect_sx)
		x = cliprect_sx;
	if (y < cliprect_sy)
		y = cliprect_sy;
	if (xpos > cliprect_ex)
		xpos = cliprect_ex;
	if (ypos > cliprect_ey)
		ypos = cliprect_ey;
	if (x<xpos && y<ypos)
		dev3d->invalidate(x, y, xpos, ypos);
	return status;
}
# if defined(VEGA_NATIVE_FONT_SUPPORT) || defined(CSS_TRANSFORMS)
OP_STATUS VEGABackend_HW3D::drawString(VEGAFont* font, int x, int y, const uni_char* _text, UINT32 len, INT32 extra_char_space, short adjust, VEGAStencil* stencil)
{
	OP_ASSERT(stencil == NULL);

#  ifdef VEGA_NATIVE_FONT_SUPPORT
	if (font->nativeRendering())
	{
		OpRect clipRect(cliprect_sx,cliprect_sy,cliprect_ex-cliprect_sx,cliprect_ey-cliprect_sy);
		OpRect worstCaseCoverage(x, y, font->MaxAdvance()*len, font->Height());
		worstCaseCoverage.IntersectWith(clipRect);
		scheduleNativeFontBatch(worstCaseCoverage);
		if (!font->DrawString(OpPoint(x, y), _text, len, extra_char_space, adjust, clipRect, fillstate.color, static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame), renderTarget->getTargetWindow()!=NULL))
			return OpStatus::ERR;

		return OpStatus::OK;
	}
#  endif // VEGA_NATIVE_FONT_SUPPORT
	return OpStatus::ERR;
}
# endif // VEGA_NATIVE_FONT_SUPPORT || CSS_TRANSFORMS

OP_STATUS VEGABackend_HW3D::moveRect(int x, int y, unsigned int width, unsigned int height, int dx, int dy)
{
	flushBatches();
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	VEGA3dTexture* alphaTexture = device->getTempTexture(renderTarget->getWidth(), renderTarget->getHeight());
	if (!alphaTexture)
		return OpStatus::ERR;
	/* This texture will only be used for plain pixel copies.
	 * FILTER_NEAREST will protect against small rounding errors.
	 */
	alphaTexture->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);
	device->setRenderTarget(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetReadRenderTarget());
	device->copyToTexture(alphaTexture, VEGA3dTexture::CUBE_SIDE_NONE, 0, 0, 0, x, y, width, height);

	// Render a textured quad
	float s1 = 0;
	float s2 = (float)width/(float)alphaTexture->getWidth();
	float t1 = 0;
	float t2 = (float)height/(float)alphaTexture->getHeight();

	VEGA3dDevice::Vega2dVertex verts[] =
	{
		{VEGA_INTTOFIX(x+dx), VEGA_INTTOFIX(y+dy),s1,t1,0xffffffff},
		{VEGA_INTTOFIX(x+dx+width), VEGA_INTTOFIX(y+dy),s2,t1,0xffffffff},
		{VEGA_INTTOFIX(x+dx), VEGA_INTTOFIX(y+dy+height),s1,t2,0xffffffff},
		{VEGA_INTTOFIX(x+dx+width), VEGA_INTTOFIX(y+dy+height),s2,t2,0xffffffff}
	};

	unsigned int firstVert;
	m_vbuffer->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, firstVert);

	device->setRenderTarget(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame));
	// Setting the scissor rect is cheaper than disabling scissor on d3d10 since it does not require re-allocation of the state variables
	device->setScissor(cliprect_sx, cliprect_sy, cliprect_ex-cliprect_sx, cliprect_ey-cliprect_sy);
	device->setRenderState(m_defaultNoBlendRenderState);

	device->setTexture(0, alphaTexture);
	const VEGA3dShaderProgram::WrapMode mode = VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;
	device->setShaderProgram(shader2dV[mode]);
	shader2dV[mode]->setOrthogonalProjection();
	device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, m_vlayout2d, firstVert, 4);
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW3D::fillRect(int x, int y, unsigned int width, unsigned int height, VEGAStencil* stencil, bool blend)
{
	int sx = cliprect_sx;
	int sy = cliprect_sy;
	int ex = cliprect_ex;
	int ey = cliprect_ey;

	if (x > sx)
		sx = x;
	if (y > sy)
		sy = y;
	if (x+(int)width < ex)
		ex = x+width;
	if (y+(int)height < ey)
		ey = y+height;
	if (ex <= sx || ey <= sy)
		return OpStatus::OK;

	renderTarget->markDirty(sx, ex-1, sy, ey-1);

	VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;

	if (stencil ||
	    // OpenGL ES don't support clamp to border, so special tricks
	    // are needed in fillPath
	    (fillstate.fill && fillstate.fill->isImage() &&
	     (fillstate.fill->getXSpread() == VEGAFill::SPREAD_CLAMP_BORDER ||
	      fillstate.fill->getYSpread() == VEGAFill::SPREAD_CLAMP_BORDER)))
	{
		OP_ASSERT(blend);
		// FIXME: far from optimal
		VEGAPath path;
		RETURN_IF_ERROR(path.moveTo(VEGA_INTTOFIX(x), VEGA_INTTOFIX(y)));
		RETURN_IF_ERROR(path.lineTo(VEGA_INTTOFIX(x+width), VEGA_INTTOFIX(y)));
		RETURN_IF_ERROR(path.lineTo(VEGA_INTTOFIX(x+width), VEGA_INTTOFIX(y+height)));
		RETURN_IF_ERROR(path.lineTo(VEGA_INTTOFIX(x), VEGA_INTTOFIX(y+height)));
		RETURN_IF_ERROR(path.close(true));
#ifdef VEGA_3DDEVICE
		path.forceConvex();
#endif
		return fillPath(&path, stencil);
	}
	else if (fillstate.fill && renderTarget->getColorFormat() != VEGARenderTarget::RT_ALPHA8)
	{
		VEGA3dTexture* tex = fillstate.fill->getTexture();

		if (tex)
		{
			UINT32 col = ((fillstate.fill->getFillOpacity()<<24)|(fillstate.fill->getFillOpacity()<<16)|(fillstate.fill->getFillOpacity()<<8)|fillstate.fill->getFillOpacity());
			VEGA3dShaderProgram* prog = fillstate.fill->getCustomShader();
			VEGA3dRenderState* renderState = blend ? NULL : m_defaultNoBlendRenderState;
			if (fillstate.fill->usePremulRenderState() ||
			    (fillstate.fill->isImage() && !fillstate.fill->hasPremultipliedAlpha()))
			{
				RETURN_IF_ERROR(createPremulRenderState());
				if (!blend)
				{
					// Emulate the 'source' composite operation by first clearing to 0 and then doing 'over' as per normal.
					// "dst = 0; dst = src + (1 - src.a) * dst" simplifies to "dst = src".
					clear(x, y, width, height, 0, NULL);
				}
				renderState = m_premultiplyRenderState;
				col = (fillstate.fill->getFillOpacity()<<24) | 0xffffff;
			}
			VEGA3dVertexLayout* vlayout = NULL;
			if (prog)
			{
				vlayout = fillstate.fill->getCustomVertexLayout();
				flushBatches();
				dev3d->setRenderTarget(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame));
				dev3d->setScissor(0, 0, renderTarget->getWidth(), renderTarget->getHeight());
				dev3d->setRenderState(m_defaultRenderState);
				dev3d->setShaderProgram(prog);
				prog->setOrthogonalProjection();
				dev3d->setTexture(1, NULL);
				dev3d->setTexture(2, NULL);
			}
			else if (m_numRemainingBatchVerts < 4)
				flushBatches();
			VEGATransform temptrans = fillstate.fill->getTexTransform();
			VEGATransform textrans;
			textrans.loadScale(VEGA_INTTOFIX(1)/tex->getWidth(), VEGA_INTTOFIX(1)/tex->getHeight());
			textrans.multiply(temptrans);
			m_vert[m_firstBatchVert].x = VEGA_INTTOFIX(sx);
			m_vert[m_firstBatchVert].y = VEGA_INTTOFIX(sy);
			m_vert[m_firstBatchVert].color = col;
			m_vert[m_firstBatchVert].s = m_vert[m_firstBatchVert].x;
			m_vert[m_firstBatchVert].t = m_vert[m_firstBatchVert].y;
			textrans.apply(m_vert[m_firstBatchVert].s, m_vert[m_firstBatchVert].t);
			m_vert[m_firstBatchVert+1].x = VEGA_INTTOFIX(ex);
			m_vert[m_firstBatchVert+1].y = VEGA_INTTOFIX(sy);
			m_vert[m_firstBatchVert+1].color = col;
			m_vert[m_firstBatchVert+1].s = m_vert[m_firstBatchVert+1].x;
			m_vert[m_firstBatchVert+1].t = m_vert[m_firstBatchVert+1].y;
			textrans.apply(m_vert[m_firstBatchVert+1].s, m_vert[m_firstBatchVert+1].t);
			m_vert[m_firstBatchVert+2].x = VEGA_INTTOFIX(ex);
			m_vert[m_firstBatchVert+2].y = VEGA_INTTOFIX(ey);
			m_vert[m_firstBatchVert+2].color = col;
			m_vert[m_firstBatchVert+2].s = m_vert[m_firstBatchVert+2].x;
			m_vert[m_firstBatchVert+2].t = m_vert[m_firstBatchVert+2].y;
			textrans.apply(m_vert[m_firstBatchVert+2].s, m_vert[m_firstBatchVert+2].t);
			m_vert[m_firstBatchVert+3].x = VEGA_INTTOFIX(sx);
			m_vert[m_firstBatchVert+3].y = VEGA_INTTOFIX(ey);
			m_vert[m_firstBatchVert+3].color = col;
			m_vert[m_firstBatchVert+3].s = m_vert[m_firstBatchVert+3].x;
			m_vert[m_firstBatchVert+3].t = m_vert[m_firstBatchVert+3].y;
			textrans.apply(m_vert[m_firstBatchVert+3].s, m_vert[m_firstBatchVert+3].t);
			if (prog)
			{
				if (renderState)
					dev3d->setRenderState(renderState);

				dev3d->setTexture(0, tex);
				unsigned int firstVert;
				m_vbuffer->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), &m_vert[m_firstBatchVert], firstVert);
				unsigned int firstInd = (firstVert/4)*6;
				dev3d->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, vlayout, dev3d->getQuadIndexBuffer(firstInd+6), 2, firstInd, 6);
			}
			else
			{
				OpRect affected(sx, sy, ex-sx, ey-sy);
				scheduleBatch(tex, 4, affected, NULL, renderState);
			}
		}
	}
	else
	{
		if ((fillstate.color>>24) != 0)
		{
			if (m_numRemainingBatchVerts < 4)
				flushBatches();
			// premultiply alpha
			UINT32 col = dev3d->convertToNativeColor(VEGAPixelPremultiply_BGRA8888(fillstate.color));
			if (renderTarget->getColorFormat() == VEGARenderTarget::RT_ALPHA8)
				col = 0xffffffff;
			m_vert[m_firstBatchVert].x = VEGA_INTTOFIX(sx);
			m_vert[m_firstBatchVert].y = VEGA_INTTOFIX(sy);
			m_vert[m_firstBatchVert].color = col;
			m_vert[m_firstBatchVert+1].x = VEGA_INTTOFIX(ex);
			m_vert[m_firstBatchVert+1].y = VEGA_INTTOFIX(sy);
			m_vert[m_firstBatchVert+1].color = col;
			m_vert[m_firstBatchVert+2].x = VEGA_INTTOFIX(ex);
			m_vert[m_firstBatchVert+2].y = VEGA_INTTOFIX(ey);
			m_vert[m_firstBatchVert+2].color = col;
			m_vert[m_firstBatchVert+3].x = VEGA_INTTOFIX(sx);
			m_vert[m_firstBatchVert+3].y = VEGA_INTTOFIX(ey);
			m_vert[m_firstBatchVert+3].color = col;
			OpRect affected(sx, sy, ex-sx, ey-sy);
			scheduleBatch(NULL, 4, affected, NULL, blend ? NULL : m_defaultNoBlendRenderState);
		}
	}

	dev3d->invalidate(sx, sy, ex, ey);
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW3D::invertRect(int x, int y, unsigned int width, unsigned int height)
{
	int sx = cliprect_sx;
	int sy = cliprect_sy;
	int ex = cliprect_ex;
	int ey = cliprect_ey;

	if (x > sx)
		sx = x;
	if (y > sy)
		sy = y;
	if (x+(int)width < ex)
		ex = x+width;
	if (y+(int)height < ey)
		ey = y+height;
	if (ex <= sx || ey <= sy)
		return OpStatus::OK;

	VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;

	if (m_numRemainingBatchVerts < 4)
		flushBatches();
	UINT32 col = 0xffffffff;
	m_vert[m_firstBatchVert].x = VEGA_INTTOFIX(sx);
	m_vert[m_firstBatchVert].y = VEGA_INTTOFIX(sy);
	m_vert[m_firstBatchVert].color = col;
	m_vert[m_firstBatchVert+1].x = VEGA_INTTOFIX(ex);
	m_vert[m_firstBatchVert+1].y = VEGA_INTTOFIX(sy);
	m_vert[m_firstBatchVert+1].color = col;
	m_vert[m_firstBatchVert+2].x = VEGA_INTTOFIX(ex);
	m_vert[m_firstBatchVert+2].y = VEGA_INTTOFIX(ey);
	m_vert[m_firstBatchVert+2].color = col;
	m_vert[m_firstBatchVert+3].x = VEGA_INTTOFIX(sx);
	m_vert[m_firstBatchVert+3].y = VEGA_INTTOFIX(ey);
	m_vert[m_firstBatchVert+3].color = col;
	OpRect affected(sx, sy, ex-sx, ey-sy);
	// FIXME: schedule batch with different blend mode
	if (!m_invertRenderState)
	{
		RETURN_IF_ERROR(dev3d->createRenderState(&m_invertRenderState, false));
		m_invertRenderState->enableScissor(true);
		m_invertRenderState->setBlendFunc(VEGA3dRenderState::BLEND_DST_ALPHA, VEGA3dRenderState::BLEND_ONE, VEGA3dRenderState::BLEND_ZERO, VEGA3dRenderState::BLEND_ONE);
		m_invertRenderState->setBlendEquation(VEGA3dRenderState::BLEND_OP_SUB, VEGA3dRenderState::BLEND_OP_ADD);
		m_invertRenderState->enableBlend(true);
	}
	scheduleBatch(NULL, 4, affected, NULL, m_invertRenderState);

	dev3d->invalidate(sx, sy, ex, ey);
	return OpStatus::OK;
}

OP_STATUS VEGABackend_HW3D::invertPath(VEGAPath* path)
{
	if (!m_invertRenderState)
	{
		VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;
		RETURN_IF_ERROR(dev3d->createRenderState(&m_invertRenderState, false));
		m_invertRenderState->enableScissor(true);
		m_invertRenderState->setBlendFunc(VEGA3dRenderState::BLEND_DST_ALPHA, VEGA3dRenderState::BLEND_ONE, VEGA3dRenderState::BLEND_ZERO, VEGA3dRenderState::BLEND_ONE);
		m_invertRenderState->setBlendEquation(VEGA3dRenderState::BLEND_OP_SUB, VEGA3dRenderState::BLEND_OP_ADD);
		m_invertRenderState->enableBlend(true);
	}

	// temporarily set color to opaque white
	FillState backup = fillstate;
	fillstate.fill = 0;
	fillstate.color = 0xffffffff;
	fillstate.ppixel = 0;
	fillstate.alphaBlend = true;
	OP_STATUS status = fillPath(path, NULL, m_invertRenderState);
	// restore old color
	fillstate = backup;
	return status;
}

OP_STATUS VEGABackend_HW3D::updateQuality()
{
	if (renderTarget)
		CheckMultisampling(quality, renderTarget->GetBackingStore());

	rasterizer.setQuality(quality);
	return OpStatus::OK;
}


OP_STATUS VEGABackend_HW3D::applyFilter(VEGAFilter* filter, const VEGAFilterRegion& region)
{
	flushBatches();

	if (!filter->hasSource())
		return OpStatus::ERR;

	return filter->apply(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore()), region, m_currentFrame);
}

void VEGABackend_HW3D::scheduleBatch(VEGA3dTexture* tex, unsigned int numVerts, const OpRect& affected, OpRect* clip, VEGA3dRenderState* renderState, BatchMode mode/* = BM_AUTO*/, int indexCount/* = -1*/)
{
	if (mode == BM_AUTO)
		mode = (numVerts == 4) ? BM_QUAD : BM_TRIANGLE_FAN;

#ifdef DEBUG_ENABLE_OPASSERT
	if (mode == BM_INDEXED_TRIANGLES)
	{
		OP_ASSERT(indexCount > 0);
		OP_ASSERT(indexCount + m_triangleIndexCount <= g_vegaGlobals.vega3dDevice->getTriangleIndexBufferSize());
	}
#endif // DEBUG_ENABLE_OPASSERT

	// if there's a deferred transaction it has to be flushed now
	static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->FlushTransaction();

	if (g_vegaGlobals.m_current_batcher && g_vegaGlobals.m_current_batcher != this)
	{
		g_vegaGlobals.m_current_batcher->flushBatches();
	}
	g_vegaGlobals.m_current_batcher = this;
	static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->setCurrentBatcher(this);
#ifdef VEGA_NATIVE_FONT_SUPPORT
	if (!m_nativeFontRect.IsEmpty() && m_nativeFontRect.Intersecting(affected))
	{
		static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame)->flushFonts();
		m_nativeFontRect.Empty();
	}
#endif
	// Search for potential batch
	unsigned int potential = m_numBatchOperations;
	// FIXME: can probably coalesce indexed triangle batches as well,
	// though i'm not sure it's worth it. (if so, remember to update
	// indexCount.)
	if (mode == BM_QUAD)
	{
		for (unsigned int batch = m_numBatchOperations; batch > 0 && potential == m_numBatchOperations; --batch)
		{
			if (m_batchList[batch-1].texture == tex &&
			    (!tex ||
			     (m_batchList[batch-1].texWrapS == tex->getWrapModeS() &&
			      m_batchList[batch-1].texWrapT == tex->getWrapModeT() &&
			      m_batchList[batch-1].texMinFilter == tex->getMinFilter() &&
			      m_batchList[batch-1].texMagFilter == tex->getMagFilter())) &&
			    !m_batchList[batch-1].font &&
			    m_batchList[batch-1].mode == mode &&
			    m_batchList[batch-1].renderState == renderState &&
			    m_batchList[batch-1].requiresClipping == (clip!=NULL) && 
			    (clip==NULL || clip->Equals(m_batchList[batch-1].clipRect)))
				potential = batch-1;
			else if (affected.Intersecting(m_batchList[batch-1].affectedRect))
				break;
		}
	}
	if (potential != m_numBatchOperations)
	{
		if (potential != m_numBatchOperations-1)
		{
			VEGA3dDevice::Vega2dVertex temp[4];
			m_batchList[m_numBatchOperations].firstVertex = m_firstBatchVert;
			m_batchList[m_numBatchOperations].numVertices = 4;
			appendVertexDataToBatch(m_numBatchOperations, potential, temp, affected);
		}
		else
		{
			m_batchList[potential].numVertices += numVerts;
			m_batchList[potential].affectedRect.UnionWith(affected);
		}
	}
	else
	{
		m_batchList[m_numBatchOperations].firstVertex = m_firstBatchVert;
		m_batchList[m_numBatchOperations].numVertices = numVerts;
		m_batchList[m_numBatchOperations].texture = tex;
		if (tex)
		{
			m_batchList[m_numBatchOperations].texWrapS = tex->getWrapModeS();
			m_batchList[m_numBatchOperations].texWrapT = tex->getWrapModeT();
			m_batchList[m_numBatchOperations].texMinFilter = tex->getMinFilter();
			m_batchList[m_numBatchOperations].texMagFilter = tex->getMagFilter();
		}
		m_batchList[m_numBatchOperations].font = NULL;
		m_batchList[m_numBatchOperations].renderState = renderState;
		m_batchList[m_numBatchOperations].affectedRect = affected;
		m_batchList[m_numBatchOperations].mode = mode;
		m_batchList[m_numBatchOperations].indexCount = indexCount;
		m_batchList[m_numBatchOperations].requiresClipping = clip!=NULL;
		if (m_batchList[m_numBatchOperations].requiresClipping)
			m_batchList[m_numBatchOperations].clipRect = *clip;
		m_batchList[m_numBatchOperations].inProgress = false;
		++m_numBatchOperations;
		VEGARefCount::IncRef(tex);
	}
	if (numVerts&3)
	{
		numVerts = (numVerts&~3)+4;
		if (numVerts > m_numRemainingBatchVerts)
			m_numRemainingBatchVerts = numVerts;
	}
	m_firstBatchVert += numVerts;
	if (mode == BM_INDEXED_TRIANGLES)
		m_triangleIndexCount += indexCount;
	m_numRemainingBatchVerts -= numVerts;
}

void VEGABackend_HW3D::scheduleFontBatch(VEGAFont* fnt, unsigned int numVerts, const OpRect& affected)
{
	// if there's a deferred transaction it has to be flushed now
	static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->FlushTransaction();

	static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->setCurrentBatcher(this);
	OP_ASSERT(numVerts == 4);
#ifdef VEGA_NATIVE_FONT_SUPPORT
	if (!m_nativeFontRect.IsEmpty() && m_nativeFontRect.Intersecting(affected))
	{
		static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame)->flushFonts();
		m_nativeFontRect.Empty();
	}
#endif
	if (m_numBatchOperations > 0 && m_batchList[m_numBatchOperations-1].font == fnt && m_batchList[m_numBatchOperations-1].inProgress)
	{
		m_batchList[m_numBatchOperations-1].numVertices += numVerts;
		m_batchList[m_numBatchOperations-1].affectedRect.UnionWith(affected);
	}
	else
	{
		m_batchList[m_numBatchOperations].firstVertex = m_firstBatchVert;
		m_batchList[m_numBatchOperations].numVertices = numVerts;
		m_batchList[m_numBatchOperations].texture = NULL;
		m_batchList[m_numBatchOperations].font = fnt;
		m_batchList[m_numBatchOperations].renderState = NULL;
		m_batchList[m_numBatchOperations].affectedRect = affected;
		m_batchList[m_numBatchOperations].mode = BM_QUAD;
		m_batchList[m_numBatchOperations].indexCount = 0;
		m_batchList[m_numBatchOperations].requiresClipping = false;
		m_batchList[m_numBatchOperations].inProgress = true;
		++m_numBatchOperations;
		fnt->setBatchingBackend(this);
	}
	m_firstBatchVert += numVerts;
	m_numRemainingBatchVerts -= numVerts;
}

void VEGABackend_HW3D::appendVertexDataToBatch(const unsigned int source_batch_idx,
                                               const unsigned int target_batch_idx,
                                               VEGA3dDevice::Vega2dVertex* temp_storage,
                                               const OpRect& affected)
{
	OP_ASSERT(temp_storage);
	OP_ASSERT(m_numBatchOperations);
	// allow using first free batch, assuming its first vertex matches
	// first free vertex (done from scheduleBatch)
	OP_ASSERT(source_batch_idx < m_numBatchOperations ||
	          (source_batch_idx == m_numBatchOperations && m_batchList[source_batch_idx].firstVertex == m_firstBatchVert));
	OP_ASSERT(source_batch_idx > target_batch_idx);

	const BatchOperation& source = m_batchList[source_batch_idx];
	BatchOperation& target = m_batchList[target_batch_idx];

	OP_ASSERT(source.firstVertex > target.firstVertex);

	const unsigned int move_dist = source.firstVertex - m_batchList[target_batch_idx+1].firstVertex;

	// copy vertex data from vertex_data_offset to temp_storage, it
	// will be appended after target's vertex data later
	op_memcpy(temp_storage, m_vert + source.firstVertex, source.numVertices * sizeof(VEGA3dDevice::Vega2dVertex));

	if (m_triangleIndexCount)
	{
		unsigned int indexOffs = 0;

		// batch entries before target should not be touched
		for (unsigned int b = 0; b < target_batch_idx; ++b)
		{
			if (m_batchList[b].mode == BM_INDEXED_TRIANGLES)
			{
				indexOffs += m_batchList[b].indexCount;
			}
		}

		// update triangle indices for all moved batches
		for (unsigned int b = target_batch_idx+1; b < source_batch_idx; ++b)
		{
			if (m_batchList[b].mode == BM_INDEXED_TRIANGLES)
			{
				OP_ASSERT(m_batchList[b].indexCount > 0);
				const unsigned int ic = m_batchList[b].indexCount;
				for (unsigned int i = 0; i < ic; ++i)
				{
					m_triangleIndices[indexOffs + i] += source.numVertices;
				}
				indexOffs += ic;
			}
		}
	}

	for (unsigned int b = target_batch_idx+1; b < source_batch_idx; ++b)
	{
		m_batchList[b].firstVertex += source.numVertices;
	}

	// move data after target to the right
	VEGA3dDevice::Vega2dVertex* target_batch_end = m_vert + source.firstVertex - move_dist;
	op_memmove(target_batch_end + source.numVertices,
	           target_batch_end,
	           move_dist * sizeof(VEGA3dDevice::Vega2dVertex));

	// append data to target
	op_memcpy(target_batch_end, temp_storage, source.numVertices * sizeof(VEGA3dDevice::Vega2dVertex));

	target.numVertices += source.numVertices;
	target.affectedRect.UnionWith(affected);
}

void VEGABackend_HW3D::finishCurrentFontBatch()
{
	if (g_vegaGlobals.m_current_batcher && g_vegaGlobals.m_current_batcher != this)
	{
		g_vegaGlobals.m_current_batcher->flushBatches();
	}
	g_vegaGlobals.m_current_batcher = this;
	if (m_numBatchOperations < 1 || !m_batchList[m_numBatchOperations-1].inProgress)
		return;

	BatchOperation* current = m_batchList+m_numBatchOperations-1;
	current->inProgress = false;

	if (current->requiresClipping && current->clipRect.Contains(current->affectedRect))
		current->requiresClipping = false;
	else if (current->requiresClipping)
		current->affectedRect.IntersectWith(current->clipRect);
	unsigned int potential = m_numBatchOperations;
	for (unsigned int batch = m_numBatchOperations-1; batch > 0 && potential == m_numBatchOperations; --batch)
	{
		if (m_batchList[batch-1].font == current->font)
			potential = batch-1;
		else if (current->affectedRect.Intersecting(m_batchList[batch-1].affectedRect))
			break;
	}
	if (potential == m_numBatchOperations)
		return;

	if (potential != m_numBatchOperations-2)
	{
		VEGA3dDevice::Vega2dVertex* temp = OP_NEWA(VEGA3dDevice::Vega2dVertex, current->numVertices);
		if (!temp)
			return;
		appendVertexDataToBatch(m_numBatchOperations-1, potential, temp, current->affectedRect);
		OP_DELETEA(temp);
	}
	else
	{
		m_batchList[potential].numVertices += current->numVertices;
		m_batchList[potential].affectedRect.UnionWith(current->affectedRect);
	}
	--m_numBatchOperations;
}

#ifdef VEGA_NATIVE_FONT_SUPPORT
void VEGABackend_HW3D::scheduleNativeFontBatch(const OpRect& affected)
{
	//setting current batcher to 'this' while m_nativeFontRect is empty would crash later
	//since the batcher is not going to be reset.
	if (affected.IsEmpty())
		return;

	if (g_vegaGlobals.m_current_batcher && g_vegaGlobals.m_current_batcher != this)
	{
		g_vegaGlobals.m_current_batcher->flushBatches();
	}

	// If this font batch covers a pending drawing operation the batch queue must be flushed
	for (unsigned int batch = 0; batch < m_numBatchOperations; ++batch)
	{
		if (m_batchList[batch].affectedRect.Intersecting(affected))
		{
			// Clear the native font rect to ensure font data is not flushed
			OpRect old = m_nativeFontRect;
			m_nativeFontRect.Empty();
			flushBatches();
			m_nativeFontRect = old;
		}
	}
	m_nativeFontRect.UnionWith(affected);

	g_vegaGlobals.m_current_batcher = this;
}
#endif

void VEGABackend_HW3D::flushBatches(VEGAStencil* textStencil)
{
	// Some operations modify the rendertarget directly without scheduling a batch. If this happens
	// we must also make sure other potential batchers are flushed
	if (g_vegaGlobals.m_current_batcher && g_vegaGlobals.m_current_batcher != this)
	{
		g_vegaGlobals.m_current_batcher->flushBatches();
	}
#ifdef VEGA_NATIVE_FONT_SUPPORT
	if (!m_nativeFontRect.IsEmpty())
	{
		OP_ASSERT(g_vegaGlobals.m_current_batcher == this);
		g_vegaGlobals.m_current_batcher = NULL;
		static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame)->flushFonts();
		m_nativeFontRect.Empty();
	}
#endif
	if (m_firstBatchVert)
	{
		finishCurrentFontBatch();
		static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->setCurrentBatcher(NULL);
#ifdef VEGA_NATIVE_FONT_SUPPORT
		OP_ASSERT(g_vegaGlobals.m_current_batcher == this || g_vegaGlobals.m_current_batcher == NULL);
#else
		OP_ASSERT(g_vegaGlobals.m_current_batcher == this);
#endif
		g_vegaGlobals.m_current_batcher = NULL;
		VEGA3dDevice* dev3d = g_vegaGlobals.vega3dDevice;

		dev3d->setRenderTarget(static_cast<VEGABackingStore_FBO*>(renderTarget->GetBackingStore())->GetWriteRenderTarget(m_currentFrame));

		// trigger setShaderProgram and setOrthogonalProjection only when needed
		VEGA3dShaderProgram::WrapMode prev_wrap_mode = VEGA3dShaderProgram::WRAP_MODE_COUNT;
		bool shader2dTextUsed[TEXT_SHADER_COUNT];
		op_memset(shader2dTextUsed, 0, sizeof(shader2dTextUsed));

		unsigned int firstVert;
		RETURN_VOID_IF_ERROR(m_vbuffer->writeAnywhere(m_firstBatchVert, sizeof(VEGA3dDevice::Vega2dVertex), m_vert, firstVert));

		// offset from start of triangle index buffer, accumulates for
		// each indexed triangle batch
		size_t triangleIndexOffset = 0;

		bool clippingSet = true;
		bool customRenderState = true;
		for (unsigned int batch = 0; batch < m_numBatchOperations; ++batch)
		{
			m_batchList[batch].firstVertex += firstVert;

			VEGA3dVertexLayout* layout;
			if (m_batchList[batch].requiresClipping)
			{
				dev3d->setScissor(m_batchList[batch].clipRect.x, m_batchList[batch].clipRect.y, m_batchList[batch].clipRect.width, m_batchList[batch].clipRect.height);
				clippingSet = true;
			}
			else if (clippingSet)
			{
				dev3d->setScissor(0, 0, renderTarget->getWidth(), renderTarget->getHeight());
				clippingSet = false;
			}
			if (m_batchList[batch].renderState)
			{
				dev3d->setRenderState(m_batchList[batch].renderState);
				customRenderState = true;
			}
			else if (customRenderState && !m_batchList[batch].font)
			{
				dev3d->setRenderState(m_defaultRenderState);
				customRenderState = false;
			}
			if (m_batchList[batch].font)
			{
				dev3d->setTexture(0, m_batchList[batch].font->getTexture(dev3d, true));
				m_batchList[batch].font->setBatchingBackend(NULL);

				// Requested shader features
				unsigned char shaderIndex = textStencil ? Stencil : 0;

				// Index of stencil texture in shader, 1 by default, will be incremented if interpolation texture is present
				unsigned int stencilIndex = 1;

				bool renderStateSet = false;
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
				bool subpixelRendering = renderTarget->getTargetWindow() && m_batchList[batch].font->UseSubpixelRendering();

				if (subpixelRendering)
				{
					// If sub pixel rendering is required we first try ExtBlend for better performance
					shaderIndex |= ExtBlend;

#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
					// Enable interpolation if the font requests it
					if (m_batchList[batch].font->UseGlyphInterpolation() && m_batchList[batch].font->getTexture(dev3d, true, false))
						shaderIndex |= Interpolation;
#endif // VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND

					if (OpStatus::IsError(createTextShader(shaderIndex)))
					{
						// Creating shader failed, drop ExtBlend and try again
						shaderIndex &= (~ExtBlend);
						if (OpStatus::IsError(createTextShader(shaderIndex)))
						{
#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
							// Normally this shouldn't fail, but in case of OOM or driver bug we should
							// be safe to fall back to the normal text shader without interpolation
							OP_ASSERT(shaderIndex & Interpolation);
							shaderIndex &= (~Interpolation);
							// This can't fail
							OpStatus::Ignore(createTextShader(shaderIndex));
#else
							OP_ASSERT(!"shouldn't be here");
#endif
						}
					}

#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
					// Set the interpolation texture
					if (shaderIndex & Interpolation)
					{
						stencilIndex ++;
						dev3d->setTexture(1, m_batchList[batch].font->getTexture(dev3d, true, false));
					}
#endif

					if (OpStatus::IsError(createSubpixelRenderState()))
					{
						// Disable subpixel rendering
						subpixelRendering = false;
						shaderIndex &= (~ExtBlend);
					}
					else if (shaderIndex & ExtBlend)
					{
						m_subpixelRenderState->setBlendFunc(VEGA3dRenderState::BLEND_ONE, VEGA3dRenderState::BLEND_EXTENDED_ONE_MINUS_SRC1_COLOR
#ifndef VEGA_SUBPIXEL_FONT_OVERWRITE_ALPHA
															,VEGA3dRenderState::BLEND_ZERO, VEGA3dRenderState::BLEND_ONE
#endif

															);
						dev3d->setRenderState(m_subpixelRenderState);
						renderStateSet = true;
						customRenderState = true;
					}
				}
#endif // VEGA_SUBPIXEL_FONT_BLENDING
				if (!renderStateSet && customRenderState)
				{
					dev3d->setRenderState(m_defaultRenderState);
					customRenderState = false;
					renderStateSet = true;
				}

				// Now we should have a text shader ready for use, shaderIndex
				// is not only a index value but also suggest the features this
				// shader supports
				VEGA3dShaderProgram* textShader = shader2dText[shaderIndex];
				layout = m_vlayout2dText[shaderIndex];

				dev3d->setShaderProgram(textShader);

				// Initialization if shader is used for the first time in this batch
				if (!shader2dTextUsed[shaderIndex])
				{
					textShader->setOrthogonalProjection();
					shader2dTextUsed[shaderIndex] = true;
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
					if (!subpixelRendering)
#endif // VEGA_SUBPIXEL_FONT_BLENDING
					{
						float acomp[4] =  { 0, 0, 0, 1 };
						textShader->setVector4(textAlphaCompLocation[shaderIndex], acomp);
					}
				}

				// Initialize if there is a stencil
				if (shaderIndex & Stencil)
				{
					VEGA_FIX strans[4] = { 0, 0, 0, 0 };
					VEGA_FIX ttrans[4] = { 0, 0, 0, 0 };
					VEGA3dTexture* stencilTex;
					updateStencil(textStencil, strans, ttrans, stencilTex);

					shader2dText[shaderIndex]->setVector4(textTexGenTransSLocation[shaderIndex/2], strans);
					shader2dText[shaderIndex]->setVector4(textTexGenTransTLocation[shaderIndex/2], ttrans);
					dev3d->setTexture(stencilIndex, stencilTex);
				}

#ifdef VEGA_SUBPIXEL_FONT_BLENDING
				if (subpixelRendering && !(shaderIndex & ExtBlend))
				{
					int alphaCompLocation = textAlphaCompLocation[shaderIndex];
					m_subpixelRenderState->setBlendFunc(VEGA3dRenderState::BLEND_ONE, VEGA3dRenderState::BLEND_ONE_MINUS_SRC_ALPHA);

					OP_ASSERT(m_batchList[batch].mode == VEGABackend_HW3D::BM_QUAD);
					OP_ASSERT((m_batchList[batch].firstVertex&3) == 0);

					float acomp[4] = { 1, 0, 0, 0 };
					OP_ASSERT(acomp[0] == 1 &&
								acomp[1] == 0 &&
								acomp[2] == 0 &&
								acomp[3] == 0);
					m_subpixelRenderState->setColorMask(true, false, false, false);
					dev3d->setRenderState(m_subpixelRenderState);
					textShader->setVector4(alphaCompLocation, acomp);
					dev3d->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, layout, dev3d->getQuadIndexBuffer((m_batchList[batch].numVertices*6)/4), 2, (m_batchList[batch].firstVertex/4)*6, (m_batchList[batch].numVertices/4)*6);

					acomp[0] = 0;
					acomp[1] = 1;
					OP_ASSERT(acomp[0] == 0 &&
								acomp[1] == 1 &&
								acomp[2] == 0 &&
								acomp[3] == 0);
					m_subpixelRenderState->setColorMask(false, true, false, false);
					dev3d->setRenderState(m_subpixelRenderState);
					textShader->setVector4(alphaCompLocation, acomp);
					dev3d->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, layout, dev3d->getQuadIndexBuffer((m_batchList[batch].numVertices*6)/4), 2, (m_batchList[batch].firstVertex/4)*6, (m_batchList[batch].numVertices/4)*6);

					acomp[1] = 0;
					acomp[2] = 1;
					OP_ASSERT(acomp[0] == 0 &&
								acomp[1] == 0 &&
								acomp[2] == 1 &&
								acomp[3] == 0);
					m_subpixelRenderState->setColorMask(false, false, true, false);
					dev3d->setRenderState(m_subpixelRenderState);
					textShader->setVector4(alphaCompLocation, acomp);
#ifdef VEGA_SUBPIXEL_FONT_OVERWRITE_ALPHA
					dev3d->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, layout, dev3d->getQuadIndexBuffer((m_batchList[batch].numVertices*6)/4), 2, (m_batchList[batch].firstVertex/4)*6, (m_batchList[batch].numVertices/4)*6);

					acomp[2] = 0;
					acomp[3] = 1;
					OP_ASSERT(acomp[0] == 0 &&
								acomp[1] == 0 &&
								acomp[2] == 0 &&
								acomp[3] == 1);
					m_subpixelRenderState->setColorMask(false, false, false, true);
					dev3d->setRenderState(m_subpixelRenderState);
					textShader->setVector4(alphaCompLocation, acomp);
#endif
					// The draw call is intentionally missing here, it will be issued after the "if" block in the regular rendering path

					customRenderState = true;
				}
#endif // VEGA_SUBPIXEL_FONT_BLENDING
			}
			else
			{
				VEGA3dTexture* tex = m_batchList[batch].texture;
				if (tex)
				{
					tex->setWrapMode(m_batchList[batch].texWrapS, m_batchList[batch].texWrapT);
					tex->setFilterMode(m_batchList[batch].texMinFilter, m_batchList[batch].texMagFilter);
				}
				const VEGA3dShaderProgram::WrapMode wrap_mode = VEGA3dShaderProgram::GetWrapMode(tex);
				dev3d->setTexture(0, tex);

				dev3d->setShaderProgram(shader2dV[wrap_mode]);
				if (prev_wrap_mode != wrap_mode)
				{
					shader2dV[wrap_mode]->setOrthogonalProjection();
					prev_wrap_mode = wrap_mode;
				}
				layout = m_vlayout2d;
			}

			switch (m_batchList[batch].mode)
			{
			case BM_QUAD:
			{
				OP_ASSERT((m_batchList[batch].firstVertex&3) == 0);
				unsigned int firstInd = (m_batchList[batch].firstVertex/4)*6;
				VEGA3dBuffer* buffer = dev3d->getQuadIndexBuffer(firstInd + (m_batchList[batch].numVertices/4)*6);
				dev3d->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, layout, buffer, 2, firstInd, (m_batchList[batch].numVertices/4)*6);
				break;
			}

			case BM_TRIANGLE_FAN:
				dev3d->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_FAN, layout, m_batchList[batch].firstVertex, m_batchList[batch].numVertices);
				break;

			case BM_INDEXED_TRIANGLES:
			{
				// sync indices to GPU
				if (!triangleIndexOffset)
				{
					if (firstVert)
					{
						for (unsigned i = 0; i < m_triangleIndexCount; ++i)
							m_triangleIndices[i] += firstVert;
					}
					m_triangleIndexBuffer->update(0, m_triangleIndexCount*sizeof(*m_triangleIndices), m_triangleIndices);
				}

				const size_t numInd = m_batchList[batch].indexCount;
				OP_ASSERT(numInd > 0);
				dev3d->drawIndexedPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE, layout, m_triangleIndexBuffer, 2, triangleIndexOffset, numInd);
				triangleIndexOffset += numInd;
				break;
			}

			default:
				OP_ASSERT(!"unsupported batch type");
			}

			VEGARefCount::DecRef(m_batchList[batch].texture);
		}

		m_numRemainingBatchVerts = dev3d->getVertexBufferSize();
		m_firstBatchVert = 0;
		m_numBatchOperations = 0;

		OP_ASSERT(triangleIndexOffset == m_triangleIndexCount);
		m_triangleIndexCount = 0;
	}
}

void VEGABackend_HW3D::onFontDeleted(VEGAFont* font)
{
	OP_ASSERT(font);
	if (m_firstBatchVert)
	{
		for (unsigned int batch = 0; batch < m_numBatchOperations; ++batch)
		{
			if (m_batchList[batch].font == font)
			{
				flushBatches();
				return;
			}
		}
	}
}

#ifdef VEGA_SUBPIXEL_FONT_BLENDING
OP_STATUS VEGABackend_HW3D::createSubpixelRenderState()
{
	if (!m_subpixelRenderState)
	{
		RETURN_IF_ERROR(g_vegaGlobals.vega3dDevice->createRenderState(&m_subpixelRenderState, false));
		m_subpixelRenderState->enableScissor(true);
		m_subpixelRenderState->enableBlend(true);
	}
	return OpStatus::OK;
}
#endif

OP_STATUS VEGABackend_HW3D::createTextShader(unsigned char traits)
{
	if (textShaderLoaded[traits])
		return shader2dText[traits] ? OpStatus::OK : OpStatus::ERR;

	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	// Set to true no matter shader will be created successfully or not
	// to avoid always trying to load the failed shader
	textShaderLoaded[traits] = true;

	VEGA3dShaderProgram::ShaderType shdType = VEGA3dShaderProgram::SHADER_TEXT2D;
	switch (traits)
	{
	case 0:
		shdType = VEGA3dShaderProgram::SHADER_TEXT2D;
		break;
	case Stencil:
		shdType = VEGA3dShaderProgram::SHADER_TEXT2DTEXGEN;
		break;
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
	case ExtBlend:
		shdType = VEGA3dShaderProgram::SHADER_TEXT2DEXTBLEND;
		break;
	case Stencil + ExtBlend:
		shdType = VEGA3dShaderProgram::SHADER_TEXT2DEXTBLENDTEXGEN;
		break;
#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
	case Interpolation:
		shdType = VEGA3dShaderProgram::SHADER_TEXT2D_INTERPOLATE;
		break;
	case Stencil + Interpolation:
		shdType = VEGA3dShaderProgram::SHADER_TEXT2DTEXGEN_INTERPOLATE;
		break;
	case ExtBlend + Interpolation:
		shdType = VEGA3dShaderProgram::SHADER_TEXT2DEXTBLEND_INTERPOLATE;
		break;
	case Stencil + ExtBlend + Interpolation:
		shdType = VEGA3dShaderProgram::SHADER_TEXT2DEXTBLENDTEXGEN_INTERPOLATE;
		break;
#endif // VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
#endif // VEGA_SUBPIXEL_FONT_BLENDING
	default:
		OP_ASSERT(!"shouldn't be here, make sure to update this function if TextShaderTrait is changed");
	}

#ifdef VEGA_SUBPIXEL_FONT_BLENDING
	// Don't even try to load ExtBlend shader if device claims no support
	if (!(traits & ExtBlend) || device->hasExtendedBlend())
#endif
	{
		if (OpStatus::IsSuccess(device->createShaderProgram(shader2dText + traits, shdType, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP)))
			if (OpStatus::IsError(device->createVega2dVertexLayout(m_vlayout2dText + traits, shdType)))
			{
				VEGARefCount::DecRef(shader2dText[traits]);
				shader2dText[traits] = NULL;
			}
	}

	if (!shader2dText[traits])
		return OpStatus::ERR;

	// Initialize texture location variables if this shader has a stencil
	if (traits & Stencil)
	{
		textTexGenTransSLocation[traits/2] = shader2dText[traits]->getConstantLocation("texTransS");
		textTexGenTransTLocation[traits/2] = shader2dText[traits]->getConstantLocation("texTransT");
	}

#ifdef VEGA_SUBPIXEL_FONT_BLENDING
	// ExtBlend shaders don't have alphaComponent
	if (!(traits & ExtBlend))
#endif
	{
		textAlphaCompLocation[traits] = shader2dText[traits]->getConstantLocation("alphaComponent");
	}

	return OpStatus::OK;
}

#endif // VEGA_SUPPORT && VEGA_3DDEVICE
