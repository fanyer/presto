/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGARENDERTARGETIMPL_H
#define VEGARENDERTARGETIMPL_H

#include "modules/libvega/vegarendertarget.h"
#include "modules/libvega/vegastencil.h"

class VEGAFill;
class VEGAImage;
#ifdef VEGA_OPPAINTER_SUPPORT
class VEGAWindow;
#endif // VEGA_OPPAINTER_SUPPORT

class VEGAIntermediateRenderTarget : public VEGAStencil
{
public:
	VEGAIntermediateRenderTarget(VEGABackingStore* store, VEGARenderTarget::RTColorFormat fmt);
	virtual ~VEGAIntermediateRenderTarget();

	virtual void flush(const OpRect* update_rects = NULL, unsigned int num_rects = 0);
	virtual bool getDirtyRect(int& left, int& right, int& top, int& bottom)
	{
		// Dirty rect is only valid for stencils
		if (getColorFormat() == RT_ALPHA8)
			return VEGAStencil::getDirtyRect(left, right, top, bottom);

		return false;
	}
	OP_STATUS copyToBitmap(OpBitmap* bmp, unsigned int width, unsigned int height, unsigned int srcx, unsigned int srcy, unsigned int dstx, unsigned int dsty);

	unsigned int getWidth();
	unsigned int getHeight();

	OP_STATUS getImage(VEGAFill** fill);

private:
	VEGAImage* img;
};

/** The software backend implementation of the bitmap render target. */
class VEGABitmapRenderTarget : public VEGARenderTarget
{
public:
	VEGABitmapRenderTarget(VEGABackingStore* store);
	virtual ~VEGABitmapRenderTarget();

	OP_STATUS copyToBitmap(OpBitmap* bmp, unsigned int width, unsigned int height, unsigned int srcx, unsigned int srcy, unsigned int dstx, unsigned int dsty);

	unsigned int getWidth();
	unsigned int getHeight();

	OP_STATUS getImage(VEGAFill** fill);

private:
	VEGAImage* img;
};

#ifdef VEGA_OPPAINTER_SUPPORT
/** The software backend of the window render target. */
class VEGAWindowRenderTarget : public VEGARenderTarget
{
public:
	VEGAWindowRenderTarget(VEGABackingStore* store, VEGAWindow* win);


	virtual void flush(const OpRect* update_rects = NULL, unsigned int num_rects = 0);
	OP_STATUS copyToBitmap(OpBitmap* bmp, unsigned int width, unsigned int height, unsigned int srcx, unsigned int srcy, unsigned int dstx, unsigned int dsty);

	unsigned int getWidth();
	unsigned int getHeight();

	OP_STATUS getImage(VEGAFill** fill);

	virtual VEGAWindow* getTargetWindow() { return window; }

private:
	VEGAWindow* window;
};
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"

class VEGA3dWindowRenderTarget : public VEGARenderTarget
{
public:
	VEGA3dWindowRenderTarget(VEGABackingStore* bstore, VEGAWindow* win, VEGA3dWindow* win3d);

	virtual void flush(const OpRect* update_rects, unsigned int num_rects);

	OP_STATUS copyToBitmap(OpBitmap* bmp, unsigned int width, unsigned int height, unsigned int srcx, unsigned int srcy, unsigned int dstx, unsigned int dsty);

	unsigned int getWidth();
	unsigned int getHeight();

	OP_STATUS getImage(VEGAFill** fill);

#ifdef VEGA_OPPAINTER_SUPPORT
	virtual VEGAWindow* getTargetWindow(){return vegaWindow;}
#endif // VEGA_OPPAINTER_SUPPORT

private:
	VEGAWindow* vegaWindow;
	VEGA3dWindow* window;
};
#endif // VEGA_3DDEVICE

#ifdef VEGA_2DDEVICE
#include "modules/libvega/vega2ddevice.h"

class VEGA2dWindowRenderTarget : public VEGARenderTarget
{
public:
	VEGA2dWindowRenderTarget(VEGABackingStore* bstore, VEGA2dWindow* win2d);

	OP_STATUS copyToBitmap(OpBitmap* bmp, unsigned int width, unsigned int height, unsigned int srcx, unsigned int srcy, unsigned int dstx, unsigned int dsty);

	unsigned int getWidth();
	unsigned int getHeight();

	OP_STATUS getImage(VEGAFill** fill);

private:
	VEGA2dWindow* window;
};
#endif // VEGA_2DDEVICE

#endif // !VEGARENDERTARGETIMPL_H

