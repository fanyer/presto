/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CANVAS_H
#define CANVAS_H

#ifdef CANVAS_SUPPORT
#include "modules/logdoc/complex_attr.h"
#include "modules/libvega/vegarenderer.h"
class VEGARenderTarget;
class CanvasContext2D;
#ifdef CANVAS3D_SUPPORT
#include "modules/libvega/vega3ddevice.h"
class CanvasContextWebGL;
#endif
class VisualDevice;
class OpBitmap;
class HTML_Element;
class FramesDocument;

/** The Canvas class keeps the bitmap which is painted on by the various
 * contexts. DOM creates an instance of this class and stores it as a
 * complex attribute in the html element. */
class Canvas : public ComplexAttr
{
public:
	Canvas();
	~Canvas();

	OP_STATUS SetSize(int width, int height);
	OP_STATUS Paint(VisualDevice* vis_dev, int width, int height, short image_rendering);

	/** Ensure that the canvas element has backing store (rendertarget et.c) allocated */
	OP_STATUS Realize(HTML_Element* element);

	/** Get the 2d context class. Creates it if it does not exist.
	 * @returns a 2d context bound to this canvas. */
	CanvasContext2D *GetContext2D();
#ifdef CANVAS3D_SUPPORT
	/** Get the WebGL context class. Creates it if it does not exist.
	 * @returns a WebGL context bound to this canvas. */
	CanvasContextWebGL* GetContextWebGL(BOOL alpha, BOOL depth, BOOL stencil, BOOL antialias, BOOL premultipliedAlpha, BOOL preserveDrawingBuffer);
#endif // CANVAS3D_SUPPORT

	/** Invalidate this canvas in it's parent frames document to trigger
	 * a repaint.
	 * @param frm_doc the parent frames document. */
	void invalidate(FramesDocument *frm_doc, HTML_Element* element);

	/** Get a string representing the content of the canvas as a base-64
	 * encoded string with the png representation of the content.
	 *
	 * Before calling, make sure that a bitmap is allocated and present.
	 *
	 * @param buffer a pointer to a TempBuffer where the output is stored.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS GetPNGDataURL(TempBuffer* buffer);
#ifdef VEGA_CANVAS_JPG_DATA_URL
	/** Get a string representing the content of the canvas as a base-64
	 * encoded string with the jpg representation of the content. This will
	 * lose all alpha values. If a jpeg encoder is not available this
	 * function will automatically call GetPNGDataURL instead.
	 *
	 * Before calling, make sure that a bitmap is allocated and present.
	 *
	 * @param buffer a pointer to a TempBuffer where the output is stored.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS GetJPGDataURL(TempBuffer* buffer, int quality = 85);
#endif // VEGA_CANVAS_JPG_DATA_URL

	/** Get the OpBitmap of the canvas. Should be used to paint the canvas
	 * in the document.
	 * @returns the Opbitmap assosiated with the canvas. */
	OpBitmap* GetOpBitmap();
	/** @returns the width of the canvas. */
	UINT32 GetWidth(HTML_Element* element);
	/** @returns the height of the canvas. */
	UINT32 GetHeight(HTML_Element* element);

	/** Get the back buffer of the canvas. */
	VEGAFill* GetBackBuffer();

	BOOL IsPremultiplied();
	OP_STATUS GetNonPremultipliedBackBuffer(VEGAPixelStore* store);
	void ReleaseNonPremultipliedBackBuffer();

	/**
	 * Used to clone HTML Elements. Returning OpStatus::ERR_NOT_SUPPORTED here
	 * will prevent the attribute from being cloned with the html element.
	 */
	virtual OP_STATUS CreateCopy(ComplexAttr **copy_to);

	/** Mark the canvas as insecure. Should be called when something from
	 * a different domain is painted on the canvas. */
	void MakeInsecure(){secure=FALSE;}
	/** Check if the canvas is marked as insecure.
	 * @returns TRUE if the canvas is marked as insecure, FALSE otherwise. */
	BOOL IsSecure(){return secure;}

	/** Estimate the storage (in bytes) allocated by the canvas argument. */
	static unsigned int allocationCost(Canvas *canvas);

#if defined(VEGA_3DDEVICE) && defined(SELFTEST)
	unsigned int selftestGetNumberOfSamples() { return m_renderer.getNumberOfSamples(); }
#endif // VEGA_3DDEVICE && SELFTEST

#ifdef CANVAS3D_SUPPORT
	void SetUseCase(VEGA3dDevice::UseCase use_case) { m_useCase = use_case; }
#endif

private:
	OpBitmap *m_bitmap;

	VEGARenderer m_renderer;
	VEGARenderTarget* m_renderTarget;
#ifdef CANVAS3D_SUPPORT
	VEGARenderTarget* m_backbufferRenderTarget;
#endif

	CanvasContext2D *m_context2d;
#ifdef CANVAS3D_SUPPORT
	CanvasContextWebGL* m_contextwebgl;
	VEGA3dDevice::UseCase m_useCase;
	VEGA3dDevice::FramebufferData m_lockedNonPremultipliedData;
	int m_canvasWidth;	/**< The width of the canvas element (may differ from the width of m_bitmap) */
	int m_canvasHeight;	/**< The height of the canvas element (may differ from the height of m_bitmap) */
#endif

	BOOL secure;
	BOOL bitmap_initialized;
};

#endif // CANVAS_SUPPORT
#endif // CANVAS_H

