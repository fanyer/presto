/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CANVAS_SUPPORT

#include "modules/libvega/src/canvas/canvas.h"
#include "modules/libvega/src/canvas/canvascontext2d.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/libvega/src/canvas/canvascontextwebgl.h"
#endif // CANVAS3D_SUPPORT

#include "modules/pi/OpBitmap.h"

#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"

// For png encoding
#include "modules/minpng/minpng_encoder.h"
#include "modules/formats/encoder.h"

// for base64 output of png/jpg images
#include "modules/img/imagedump.h"

Canvas::Canvas() : m_bitmap(NULL),
	m_renderTarget(NULL),
#ifdef CANVAS3D_SUPPORT
	m_backbufferRenderTarget(NULL),
#endif
	m_context2d(NULL),
#ifdef CANVAS3D_SUPPORT
	m_contextwebgl(NULL),
	m_useCase(VEGA3dDevice::For3D),
	m_canvasWidth(-1),
	m_canvasHeight(-1),
#endif // CANVAS3D_SUPPORT
	secure(TRUE),
	bitmap_initialized(FALSE)
{}

Canvas::~Canvas()
{
	if (m_context2d)
	{
		m_context2d->ClearCanvas();
		m_context2d->Release();
	}
#ifdef CANVAS3D_SUPPORT
	if (m_contextwebgl)
	{
		m_contextwebgl->ClearCanvas();
		VEGARefCount::DecRef(m_contextwebgl);
	}
#endif // CANVAS3D_SUPPORT
	m_renderer.setRenderTarget(NULL);
	VEGARenderTarget::Destroy(m_renderTarget);
#ifdef CANVAS3D_SUPPORT
	VEGARenderTarget::Destroy(m_backbufferRenderTarget);
#endif
	OP_DELETE(m_bitmap);
}

OP_STATUS
Canvas::SetSize(int width, int height)
{
#ifdef CANVAS3D_SUPPORT
	int requestedWidth = width;
	int requestedHeight = height;

	if (m_contextwebgl)
	{
		// Limit the size of the drawing buffer (required for WebGL: if asked
		// for a drawing buffer that is bigger than the maximum supported size,
		// we should create a smaller drawing buffer).
		// NOTE: The SW back end imposes a UINT16 limitation in
		// VEGARasterizer::initialize().
		OP_ASSERT(g_vegaGlobals.vega3dDevice);
		int maxSize = USHRT_MAX >> 1;
		VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
		if ((int)dev->getMaxTextureSize() < maxSize)
			maxSize = (int)dev->getMaxTextureSize();
		if ((int)dev->getMaxRenderBufferSize() < maxSize)
			maxSize = (int)dev->getMaxRenderBufferSize();
		width = MIN(width, maxSize);
		height = MIN(height, maxSize);
	}
#endif

	// Restore the contexts of the canvas as specified in the specification
	if (m_context2d)
	{
		m_context2d->setDefaults();
		m_context2d->beginPath();
	}
	if (m_bitmap && width == (int)m_bitmap->Width() && height == (int)m_bitmap->Height())
	{
		if (m_context2d)
		{
			m_renderer.setRenderTarget(m_renderTarget);
			m_renderer.clear(0,0,m_renderTarget->getWidth(), m_renderTarget->getHeight(), 0);
		}
		return OpStatus::OK;
	}
	OpBitmap *bmp;

	RETURN_IF_ERROR(OpBitmap::Create(&bmp, width, height, FALSE, TRUE,
                                                        0, 0, FALSE));

	VEGARenderTarget* rt = NULL;
	OP_STATUS err = m_renderer.Init(bmp->Width(), bmp->Height());
	if (OpStatus::IsSuccess(err))
	{
		if (m_context2d)
			err = m_renderer.createIntermediateRenderTarget(&rt, bmp->Width(), bmp->Height());
#ifdef CANVAS3D_SUPPORT
		else if (m_contextwebgl)
			err = m_renderer.createBitmapRenderTarget(&rt, bmp);
#endif
	}
	if (OpStatus::IsError(err))
	{
		OP_DELETE(bmp);
		return err;
	}
	VEGARenderTarget::Destroy(m_renderTarget);
	m_renderTarget = rt;
	OP_DELETE(m_bitmap);
	m_bitmap = bmp;
#ifdef CANVAS3D_SUPPORT
	VEGARenderTarget::Destroy(m_backbufferRenderTarget);
	m_backbufferRenderTarget = NULL;
#endif

#ifdef CANVAS3D_SUPPORT
	// At this point we have successfully changed the size of the bitmap, so
	// update the canvas dimensions (which may be different from the drawing
	// buffer dimensions for a WebGL canvas).
	m_canvasWidth = requestedWidth;
	m_canvasHeight = requestedHeight;
#endif

	bitmap_initialized = FALSE;

	if (m_context2d)
	{
		// Clear the render target to transparent black since it is used directly as a backbuffer by the 2d context
		m_renderer.setRenderTarget(m_renderTarget);
		m_renderer.clear(0,0,m_renderTarget->getWidth(), m_renderTarget->getHeight(), 0);
		RETURN_IF_ERROR(m_context2d->initBuffer(m_renderTarget));
	}
#ifdef CANVAS3D_SUPPORT
	else if (m_contextwebgl)
	{
		// WebGL has its own backbuffer which is cleared by the webgl code, no need to
		// clear m_renderTarget which points directly to the bitmap since the bitmap
		// is already marked as invalid.
		RETURN_IF_ERROR(m_contextwebgl->initBuffer(m_renderTarget, m_useCase));
	}
#endif
	return OpStatus::OK;
}

OP_STATUS
Canvas::Paint(VisualDevice* visual_device, int width, int height, short image_rendering)
{
	// Makes sure the OpBitmap is up to date
	OpBitmap *bmp = GetOpBitmap();
	if (!bmp)
		return OpStatus::ERR;

	VDStateNoScale paint_state = visual_device->BeginNoScalePainting(OpRect(0, 0, width, height));

#ifdef VEGA_CANVAS_OPTIMIZE_SPEED
	if (image_rendering == CSS_VALUE_auto)
		image_rendering = CSS_VALUE_optimizeSpeed;
#endif // VEGA_CANVAS_OPTIMIZE_SPEED
	visual_device->SetImageInterpolation(image_rendering);

	OP_STATUS status = visual_device->BlitImage(bmp, OpRect(0, 0, bmp->Width(), bmp->Height()),
												paint_state.dst_rect);

	visual_device->ResetImageInterpolation();
	visual_device->EndNoScalePainting(paint_state);
	return status;
}

OP_STATUS Canvas::Realize(HTML_Element* element)
{
	if (m_renderTarget)
		return OpStatus::OK;

	return SetSize(GetWidth(element), GetHeight(element));
}

OpBitmap* Canvas::GetOpBitmap()
{
	if (!m_bitmap)
		return NULL;
	if (!bitmap_initialized)
	{
		if (m_context2d)
		{
			// Make sure the cached font drawing operations are flushed before copying
			m_renderer.flush();
			m_renderTarget->copyToBitmap(m_bitmap);
		}
#ifdef CANVAS3D_SUPPORT
		else if (m_contextwebgl)
		{
			// In the WebGL case the rendertarget is connected to the bitmap, so no need to copy after swapBuffers
			m_contextwebgl->swapBuffers(&m_renderer, false);
			m_renderer.flush();
		}
#endif // CANVAS3D_SUPPORT
		else
		{
			// There are a few cases where a bitmap can be created without a context being present, they include:
			// 1. Context creation fails after creating the bitmap.
			// 2. The canvas is resized without having a context (canvas.width=x).
			// 3. A canvas is used as source in a drawImage or similar operation on another canvas.
			// 4. toDataURL is called on the canvas.
			UINT8 col[] = {0, 0, 0, 0};
			if (!m_bitmap->SetColor(col, TRUE, NULL))
				return NULL;
		}
		// Otherwise the alpha values might be broken
		m_bitmap->ForceAlpha();
		bitmap_initialized = TRUE;
	}
	return m_bitmap;
}

UINT32 Canvas::GetWidth(HTML_Element* element)
{
#ifdef CANVAS3D_SUPPORT
	if (m_contextwebgl && m_canvasWidth >= 0)
		return m_canvasWidth;
#endif // CANVAS3D_SUPPORT
	if (!m_bitmap)
	{
		int width = element ? element->GetNumAttr(ATTR_WIDTH) : -1;
		if (width <= 0)
			width = 300;
		return width;
	}
	return m_bitmap->Width();
}

UINT32 Canvas::GetHeight(HTML_Element* element)
{
#ifdef CANVAS3D_SUPPORT
	if (m_contextwebgl && m_canvasHeight >= 0)
		return m_canvasHeight;
#endif // CANVAS3D_SUPPORT
	if (!m_bitmap)
	{
		int height = element ? element->GetNumAttr(ATTR_HEIGHT) : -1;
		if (height <= 0)
			height = 150;
		return height;
	}
	return m_bitmap->Height();
}

VEGAFill* Canvas::GetBackBuffer()
{
	VEGAFill* f;
#ifdef CANVAS3D_SUPPORT
	if (m_contextwebgl)
	{
		if (!m_backbufferRenderTarget)
			RETURN_VALUE_IF_ERROR(m_renderer.createIntermediateRenderTarget(&m_backbufferRenderTarget, m_bitmap->Width(), m_bitmap->Height()), NULL);
		m_contextwebgl->swapBuffers(&m_renderer, false, m_backbufferRenderTarget);
		m_renderer.flush();
		RETURN_VALUE_IF_ERROR(m_backbufferRenderTarget->getImage(&f), NULL);
	}
	else
#endif // CANVAS3D_SUPPORT
	if (!m_renderTarget || OpStatus::IsError(m_renderTarget->getImage(&f)))
		return NULL;
	return f;
}

BOOL Canvas::IsPremultiplied()
{
#ifdef CANVAS3D_SUPPORT
# ifdef USE_PREMULTIPLIED_ALPHA
	return (!m_contextwebgl || m_contextwebgl->HasPremultipliedAlpha());
# else
	return (m_contextwebgl && m_contextwebgl->HasPremultipliedAlpha());
# endif
#else
# ifdef USE_PREMULTIPLIED_ALPHA
	return TRUE;
# else
	return FALSE;
# endif
#endif
}

OP_STATUS Canvas::GetNonPremultipliedBackBuffer(VEGAPixelStore* store)
{
#ifdef CANVAS3D_SUPPORT
	OP_ASSERT(m_contextwebgl);
	RETURN_IF_ERROR(m_contextwebgl->lockBufferContent(&m_lockedNonPremultipliedData));
	*store = m_lockedNonPremultipliedData.pixels;
	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif
}

void Canvas::ReleaseNonPremultipliedBackBuffer()
{
#ifdef CANVAS3D_SUPPORT
	OP_ASSERT(m_contextwebgl);
	m_contextwebgl->unlockBufferContent(&m_lockedNonPremultipliedData);
#endif
}

CanvasContext2D *Canvas::GetContext2D()
{
	if (!m_context2d
#ifdef CANVAS3D_SUPPORT
		&& !m_contextwebgl
#endif
		)
	{
		// the context will get a refcount of one when creating it
		m_context2d = OP_NEW(CanvasContext2D, (this, &m_renderer));
		if (m_context2d && m_bitmap)
		{
			if (!m_renderTarget)
			{
				OP_STATUS err = m_renderer.createIntermediateRenderTarget(&m_renderTarget, m_bitmap->Width(), m_bitmap->Height());
				if (OpStatus::IsError(err))
				{
					OP_DELETE(m_context2d);
					m_context2d = NULL;
					return NULL;
				}
				m_renderer.setRenderTarget(m_renderTarget);
				m_renderer.clear(0,0,m_renderTarget->getWidth(), m_renderTarget->getHeight(), 0);
			}
			if (OpStatus::IsError(m_context2d->initBuffer(m_renderTarget)))
			{
				OP_DELETE(m_context2d);
				m_context2d = NULL;
			}
		}
	}
	return m_context2d;
}

#ifdef CANVAS3D_SUPPORT
CanvasContextWebGL* Canvas::GetContextWebGL(BOOL alpha, BOOL depth, BOOL stencil, BOOL antialias, BOOL premultipliedAlpha, BOOL preserveDrawingBuffer)
{
	if (!m_contextwebgl && !m_context2d)
	{
		m_contextwebgl = OP_NEW(CanvasContextWebGL, (this, alpha, depth, stencil, antialias, premultipliedAlpha, preserveDrawingBuffer));
		if (m_contextwebgl && m_bitmap)
		{
			// Make sure that we have a 3d device.
			if (OpStatus::IsError(m_contextwebgl->ensureDevice(m_useCase)))
			{
				VEGARefCount::DecRef(m_contextwebgl);
				m_contextwebgl = NULL;
				return NULL;
			}

			// We might need to limit the size of the bitmap, so re-size now
			// that we know that this is a WebGL context.
			SetSize(m_bitmap->Width(), m_bitmap->Height());

			if (!m_renderTarget)
			{
				OP_STATUS err = m_renderer.createBitmapRenderTarget(&m_renderTarget, m_bitmap);
				if (OpStatus::IsError(err))
				{
					VEGARefCount::DecRef(m_contextwebgl);
					m_contextwebgl = NULL;
					return NULL;
				}
			}
			if (OpStatus::IsError(m_contextwebgl->initBuffer(m_renderTarget, m_useCase)))
			{
				VEGARefCount::DecRef(m_contextwebgl);
				m_contextwebgl = NULL;
			}
		}
	}
	return m_contextwebgl;
}
#endif // CANVAS3D_SUPPORT

void Canvas::invalidate(FramesDocument *frm_doc, HTML_Element* element)
{
	RECT r = {0, 0, 0, 0};

	bool visible = element && frm_doc && frm_doc->GetLogicalDocument() &&
		frm_doc->GetLogicalDocument()->GetRoot() &&
		frm_doc->GetLogicalDocument()->GetRoot()->IsAncestorOf(element) &&
		frm_doc->GetLogicalDocument()->GetBoxRect(element, BOUNDING_BOX, r);

	if (m_bitmap)
	{
		if (m_context2d)
		{
			// Make sure the cached font drawing operations are flushed before copying
			m_renderer.flush();
			m_renderTarget->copyToBitmap(m_bitmap);
		}
#ifdef CANVAS3D_SUPPORT
		else if (m_contextwebgl)
		{
			// According to the spec the canvas should only be cleared when it is composited, which means it should not happen if the canvas is not visible
			m_contextwebgl->swapBuffers(&m_renderer, visible);
			m_renderer.flush();

			// If we're currently hooked up to a debugger, fire off the end of frame event.
			DOM_Environment *domEnv = frm_doc->GetDOMEnvironment();
			if (g_ecmaManager->IsDebugging(domEnv->GetRuntime()))
			{
				DOM_Environment::EventData eventData;
				eventData.type = ONFRAMEEND;
				eventData.target = element;
				domEnv->HandleEvent(eventData);
			}
		}
#endif // CANVAS3D_SUPPORT
		else
			return;
		// Otherwise the alpha values might be broken
		m_bitmap->ForceAlpha();
		bitmap_initialized = TRUE;
	}

	if (visible)
	{
		FramesDocument* invalidate_doc = frm_doc;
#ifdef DOM_FULLSCREEN_MODE

		bool paint_fullscreen = frm_doc->GetFullscreenElement() == element && !frm_doc->IsFullscreenElementObscured();

		/* If the Canvas element is the fullscreen element for the document, and
		   not obscured, we paint it using the FramesDocument::DisplayFullscreenElement
		   method. If the Canvas element is inside an iframe and that iframe is also
		   truly fullscreen (not obscured), that method will use the top-level
		   document's VisualDevice for painting. The code below will set
		   invalidate_doc to the top-level document if none of the elements in the
		   fullscreen stack is obscured. */
		if (paint_fullscreen)
		{
			FramesDocument* parent_doc = frm_doc;
			while (parent_doc = parent_doc->GetParentDoc())
			{
				invalidate_doc = parent_doc;
				if (parent_doc->IsFullscreenElementObscured())
				{
					paint_fullscreen = false;
					invalidate_doc = frm_doc;
					break;
				}
			}
		}

# ifdef CANVAS3D_SUPPORT
		if (m_contextwebgl)
			m_contextwebgl->SetFullscreen(paint_fullscreen);
# endif // CANVAS3D_SUPPORT

#endif // DOM_FULLSCREEN_MODE

		invalidate_doc->GetVisualDevice()->Update(r.left, r.top, r.right - r.left, r.bottom - r.top);
	}
}

/**
 * Used to clone HTML Elements. Returning OpStatus::ERR_NOT_SUPPORTED here
 * will prevent the attribute from being cloned with the html element.
 */
/*virtual*/ OP_STATUS
Canvas::CreateCopy(ComplexAttr **copy_to)
{
	*copy_to = OP_NEW(Canvas, ());
	if (!*copy_to)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS err = OpStatus::OK;
	if (m_bitmap)
		((Canvas*)*copy_to)->SetSize(m_bitmap->Width(), m_bitmap->Height());
	if (OpStatus::IsError(err))
	{
		OP_DELETE(*copy_to);
		*copy_to = NULL;
	}
	return err;
}

OP_STATUS Canvas::GetPNGDataURL(TempBuffer* buffer)
{
	RETURN_IF_ERROR(buffer->Append("data:image/png;base64,"));
#if defined(CANVAS3D_SUPPORT) && defined(USE_PREMULTIPLIED_ALPHA)
	if (m_contextwebgl && !m_contextwebgl->HasPremultipliedAlpha())
	{
		OP_STATUS err;
		// In this case we must read the pixels without storing them in a OpBitmap to avoid loosing precision
		VEGAPixelStore store;
		RETURN_IF_ERROR(GetNonPremultipliedBackBuffer(&store));
		if (store.format == VPSF_BGRA8888 && store.stride == store.width*4)
		{
			err = GetNonPremultipliedBufferAsBase64PNG(static_cast<UINT32*>(store.buffer), store.width, store.height, buffer);
		}
		else
		{
			VEGASWBuffer buf;
			err = buf.Bind(&store);
			if (OpStatus::IsError(err))
			{
				ReleaseNonPremultipliedBackBuffer();
				return err;
			}
			VEGAPixelStoreWrapper bgra_store;
			err = bgra_store.Allocate(VPSF_BGRA8888, store.width, store.height, 4);
			if (OpStatus::IsSuccess(err))
			{
				buf.CopyToPixelStore(&bgra_store.ps, false, false, VPSF_BGRA8888);
				OP_ASSERT(bgra_store.ps.stride == bgra_store.ps.width*4);
				err = GetNonPremultipliedBufferAsBase64PNG(static_cast<UINT32*>(bgra_store.ps.buffer), store.width, store.height, buffer);
				bgra_store.Free();
			}
			buf.Unbind(&store);
		}
		ReleaseNonPremultipliedBackBuffer();
		return err;
	}
#endif
	return GetOpBitmapAsBase64PNG(m_bitmap, buffer);
}

#ifdef VEGA_CANVAS_JPG_DATA_URL
OP_STATUS Canvas::GetJPGDataURL(TempBuffer* buffer, int quality)
{
#ifdef JAYPEG_ENCODE_SUPPORT
	RETURN_IF_ERROR(buffer->Append("data:image/jpeg;base64,"));
#if defined(CANVAS3D_SUPPORT) && defined(USE_PREMULTIPLIED_ALPHA)
	if (m_contextwebgl && !m_contextwebgl->HasPremultipliedAlpha())
	{
		OP_STATUS err;
		// In this case we must read the pixels without storing them in a OpBitmap to avoid loosing precision
		VEGAPixelStore store;
		RETURN_IF_ERROR(GetNonPremultipliedBackBuffer(&store));
		if (store.format == VPSF_BGRA8888 && store.stride == store.width*4)
		{
			err = GetNonPremultipliedBufferAsBase64JPG(static_cast<UINT32*>(store.buffer), store.width, store.height, buffer, quality);
		}
		else
		{
			VEGASWBuffer buf;
			err = buf.Bind(&store);
			if (OpStatus::IsError(err))
			{
				ReleaseNonPremultipliedBackBuffer();
				return err;
			}
			VEGAPixelStoreWrapper bgra_store;
			err = bgra_store.Allocate(VPSF_BGRA8888, store.width, store.height, 4);
			if (OpStatus::IsSuccess(err))
			{
				buf.CopyToPixelStore(&bgra_store.ps, false, false, VPSF_BGRA8888);
				OP_ASSERT(bgra_store.ps.stride == bgra_store.ps.width*4);
				err = GetNonPremultipliedBufferAsBase64JPG(static_cast<UINT32*>(bgra_store.ps.buffer), store.width, store.height, buffer, quality);
				bgra_store.Free();
			}
			buf.Unbind(&store);
		}
		ReleaseNonPremultipliedBackBuffer();
		return err;
	}
#endif
	return GetOpBitmapAsBase64JPG(m_bitmap, buffer, quality);
#else // !JAYPEG_ENCODE_SUPPORT
	return GetPNGDataURL(buffer);
#endif // !JAYPEG_ENCODE_SUPPORT
}
#endif // VEGA_CANVAS_JPG_DATA_URL

/* static */ unsigned int
Canvas::allocationCost(Canvas *canvas)
{
	unsigned int cost = sizeof(Canvas);

	if (OpBitmap *bitmap = canvas->m_bitmap)
		cost += g_op_screen_info->GetBitmapAllocationSize(bitmap->Width(), bitmap->Height(), bitmap->IsTransparent(), bitmap->HasAlpha(), 0);

	if (VEGABackingStore *backingstore = canvas->m_renderTarget ? canvas->m_renderTarget->GetBackingStore() : NULL)
		cost += backingstore->GetWidth() * backingstore->GetHeight() * backingstore->GetBytesPerPixel();

	return cost;
}
#endif // CANVAS_SUPPORT
