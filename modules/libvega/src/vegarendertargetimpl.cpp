/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT

#include "modules/libvega/src/vegarendertargetimpl.h"
#include "modules/libvega/src/vegaimage.h"
#include "modules/libvega/vegawindow.h"
#include "modules/libvega/src/vegabackend.h"
#include "modules/libvega/vegarenderer.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#else
#include "modules/pi/OpBitmap.h"
#include "modules/libvega/src/vegabackend_sw.h"
#endif // VEGA_OPPAINTER_SUPPORT

VEGARenderTarget::~VEGARenderTarget()
{
	VEGARefCount::DecRef(backingstore);
}

/* static */
void VEGARenderTarget::Destroy(VEGARenderTarget* renderTarget)
{
	if (renderTarget)
	{
		if (renderTarget->renderer)
			renderTarget->renderer->setRenderTarget(NULL);
		OP_DELETE(renderTarget);
	}
}


void VEGARenderTarget::invalidate()
{
	backingstore->Invalidate();
}

VEGAIntermediateRenderTarget::VEGAIntermediateRenderTarget(VEGABackingStore* store,
														   VEGARenderTarget::RTColorFormat fmt)
	: img(NULL)
{
	backingstore = store; // Inherits reference from caller

	format = fmt;
}

VEGAIntermediateRenderTarget::~VEGAIntermediateRenderTarget()
{
	OP_DELETE(img);
}

void VEGAIntermediateRenderTarget::flush(const OpRect* update_rects, unsigned int num_rects)
{
	backingstore->Flush(update_rects, num_rects);
}

unsigned int VEGAIntermediateRenderTarget::getWidth()
{
	return backingstore->GetWidth();
}

unsigned int VEGAIntermediateRenderTarget::getHeight()
{
	return backingstore->GetHeight();
}

OP_STATUS VEGAIntermediateRenderTarget::copyToBitmap(OpBitmap* bmp,
													 unsigned int width, unsigned int height,
													 unsigned int srcx, unsigned int srcy,
													 unsigned int dstx, unsigned int dsty)
{
	backingstore->Validate();

	if (format != RT_RGBA32)
	{
		OP_ASSERT(format == RT_RGBA32);
		return OpStatus::ERR;
	}

#ifdef VEGA_OPPAINTER_SUPPORT
	VEGAOpBitmap* vbmp = static_cast<VEGAOpBitmap*>(bmp);
# ifdef VEGA_LIMIT_BITMAP_SIZE
	if (vbmp->isTiled())
		return vbmp->copyTiled(this, width, height, srcx, srcy, dstx, dsty);
# endif // VEGA_LIMIT_BITMAP_SIZE

	VEGABackingStore* bmp_store = vbmp->GetBackingStore();
	if (width == 0)
		width = backingstore->GetWidth();
	if (height == 0)
		height = backingstore->GetHeight();

	return bmp_store->CopyRect(OpPoint(dstx, dsty), OpRect(srcx, srcy, width, height), backingstore);
#else
	// Luckily, this also implies (or, well, should) that we are
	// backed by something SWBuffer-ish, so get the buffer.
	OP_ASSERT(backingstore->IsA(VEGABackingStore::SWBUFFER));
	VEGASWBuffer* buffer = static_cast<VEGABackingStore_SWBuffer*>(backingstore)->GetBuffer();
	return buffer->CopyToBitmap(bmp, srcx, srcy, dstx, dsty, width, height);
#endif // VEGA_OPPAINTER_SUPPORT
}

OP_STATUS VEGAIntermediateRenderTarget::getImage(VEGAFill** fill)
{
	if (format != RT_RGBA32)
	{
		OP_ASSERT(format == RT_RGBA32);
		return OpStatus::ERR;
	}

	OP_STATUS status = OpStatus::OK;
	if (!img)
	{
		img = OP_NEW(VEGAImage, ());
		if (!img)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS status = img->init(backingstore);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(img);
			img = NULL;
		}
	}
	*fill = img;
	return status;
}

VEGABitmapRenderTarget::VEGABitmapRenderTarget(VEGABackingStore* store) : img(NULL)
{
	backingstore = store; // Inherits reference from caller
}

VEGABitmapRenderTarget::~VEGABitmapRenderTarget()
{
#ifndef VEGA_OPPAINTER_SUPPORT
	backingstore->Flush();
#endif // VEGA_OPPAINTER_SUPPORT

	OP_DELETE(img);
}

OP_STATUS VEGABitmapRenderTarget::getImage(VEGAFill** fill)
{
	if (!img)
	{
		img = OP_NEW(VEGAImage, ());
		if (!img)
			return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = img->init(backingstore);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(img);
		img = NULL;
	}
	*fill = img;
	return status;
}

OP_STATUS VEGABitmapRenderTarget::copyToBitmap(OpBitmap* bmp, unsigned int width, unsigned int height, unsigned int srcx, unsigned int srcy, unsigned int dstx, unsigned int dsty)
{
	backingstore->Validate();

#ifdef VEGA_OPPAINTER_SUPPORT
	VEGAOpBitmap* vbmp = static_cast<VEGAOpBitmap*>(bmp);
# ifdef VEGA_LIMIT_BITMAP_SIZE
	if (vbmp->isTiled())
		return vbmp->copyTiled(this, width, height, srcx, srcy, dstx, dsty);
# endif // VEGA_LIMIT_BITMAP_SIZE

	VEGABackingStore* bmp_store = vbmp->GetBackingStore();
	if (width == 0)
		width = backingstore->GetWidth();
	if (height == 0)
		height = backingstore->GetHeight();

	return bmp_store->CopyRect(OpPoint(dstx, dsty), OpRect(srcx, srcy, width, height), backingstore);
#else
	// Luckily, this also implies (or, well, should) that we are
	// backed by something SWBuffer-ish, so get the buffer.
	OP_ASSERT(backingstore->IsA(VEGABackingStore::SWBUFFER));
	VEGASWBuffer* buffer = static_cast<VEGABackingStore_SWBuffer*>(backingstore)->GetBuffer();
	return buffer->CopyToBitmap(bmp, srcx, srcy, dstx, dsty, width, height);
#endif // VEGA_OPPAINTER_SUPPORT
}

unsigned int VEGABitmapRenderTarget::getWidth()
{
	return backingstore->GetWidth();
}

unsigned int VEGABitmapRenderTarget::getHeight()
{
	return backingstore->GetHeight();
}

#ifdef VEGA_OPPAINTER_SUPPORT
VEGAWindowRenderTarget::VEGAWindowRenderTarget(VEGABackingStore* store, VEGAWindow* win)
  : window(win)
{
	backingstore = store; // Inherits reference from caller
}

void VEGAWindowRenderTarget::flush(const OpRect* update_rects, unsigned int num_rects)
{
	backingstore->Flush(update_rects, num_rects);

	window->present(update_rects, num_rects);
}

unsigned int VEGAWindowRenderTarget::getWidth()
{
	return window->getWidth();
}

unsigned int VEGAWindowRenderTarget::getHeight()
{
	return window->getHeight();
}

OP_STATUS VEGAWindowRenderTarget::copyToBitmap(OpBitmap* bmp, unsigned int width, unsigned int height, unsigned int srcx, unsigned int srcy, unsigned int dstx, unsigned int dsty)
{
	backingstore->Validate();

	VEGAOpBitmap* vbmp = static_cast<VEGAOpBitmap*>(bmp);
# ifdef VEGA_LIMIT_BITMAP_SIZE
	if (vbmp->isTiled())
		return vbmp->copyTiled(this, width, height, srcx, srcy, dstx, dsty);
# endif // VEGA_LIMIT_BITMAP_SIZE

	VEGABackingStore* bmp_store = vbmp->GetBackingStore();
	if (width == 0)
		width = backingstore->GetWidth();
	if (height == 0)
		height = backingstore->GetHeight();

	return bmp_store->CopyRect(OpPoint(dstx, dsty), OpRect(srcx, srcy, width, height), backingstore);
}

OP_STATUS VEGAWindowRenderTarget::getImage(VEGAFill** fill)
{
	OP_ASSERT(!"Drawing with windows not supported");
	return OpStatus::ERR;
}

#ifdef VEGA_3DDEVICE
VEGA3dWindowRenderTarget::VEGA3dWindowRenderTarget(VEGABackingStore* bstore, VEGAWindow* win, VEGA3dWindow* win3d)
	: vegaWindow(win), window(win3d)
{
	backingstore = bstore; // Inherits reference from caller
}

void VEGA3dWindowRenderTarget::flush(const OpRect* update_rects, unsigned int num_rects)
{
	backingstore->Flush();

	vegaWindow->present(update_rects, num_rects);
}

OP_STATUS VEGA3dWindowRenderTarget::copyToBitmap(OpBitmap* bmp, unsigned int width, unsigned int height, unsigned int srcx, unsigned int srcy, unsigned int dstx, unsigned int dsty)
{
	backingstore->Validate();

	VEGAOpBitmap* vbmp = static_cast<VEGAOpBitmap*>(bmp);
# ifdef VEGA_LIMIT_BITMAP_SIZE
	if (vbmp->isTiled())
		return vbmp->copyTiled(this, width, height, srcx, srcy, dstx, dsty);
# endif // VEGA_LIMIT_BITMAP_SIZE

	VEGABackingStore* bmp_store = vbmp->GetBackingStore();
	if (width == 0)
		width = backingstore->GetWidth();
	if (height == 0)
		height = backingstore->GetHeight();

	return bmp_store->CopyRect(OpPoint(dstx, dsty), OpRect(srcx, srcy, width, height), backingstore);
}

unsigned int VEGA3dWindowRenderTarget::getWidth()
{
	return window->getWidth();
}

unsigned int VEGA3dWindowRenderTarget::getHeight()
{
	return window->getHeight();
}

OP_STATUS VEGA3dWindowRenderTarget::getImage(VEGAFill** fill)
{
	OP_ASSERT(!"Drawing with windows not supported");
	return OpStatus::ERR;
}
#endif // VEGA_3DDEVICE

#ifdef VEGA_2DDEVICE
VEGA2dWindowRenderTarget::VEGA2dWindowRenderTarget(VEGABackingStore* bstore, VEGA2dWindow* win2d)
  : window(win2d)
{
	backingstore = bstore; // Inherits reference from caller
}

OP_STATUS VEGA2dWindowRenderTarget::copyToBitmap(OpBitmap* bmp, unsigned int width, unsigned int height, unsigned int srcx, unsigned int srcy, unsigned int dstx, unsigned int dsty)
{
	backingstore->Validate();
	VEGAOpBitmap* vbmp = static_cast<VEGAOpBitmap*>(bmp);
# ifdef VEGA_LIMIT_BITMAP_SIZE
	if (vbmp->isTiled())
		return vbmp->copyTiled(this, width, height, srcx, srcy, dstx, dsty);
# endif // VEGA_LIMIT_BITMAP_SIZE
	VEGABackingStore* bmp_store = vbmp->GetBackingStore();
	if (width == 0)
		width = backingstore->GetWidth();
	if (height == 0)
		height = backingstore->GetHeight();

	return bmp_store->CopyRect(OpPoint(dstx, dsty), OpRect(srcx, srcy, width, height), backingstore);
}

unsigned int VEGA2dWindowRenderTarget::getWidth()
{
	return window->getWidth();
}

unsigned int VEGA2dWindowRenderTarget::getHeight()
{
	return window->getHeight();
}

OP_STATUS VEGA2dWindowRenderTarget::getImage(VEGAFill** fill)
{
	OP_ASSERT(!"Drawing with windows not supported");
	return OpStatus::ERR;
}
#endif // VEGA_2DDEVICE
#endif // VEGA_OPPAINTER_SUPPORT

#endif // VEGA_SUPPORT

