/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAD3D10WINDOW_H
#define VEGAD3D10WINDOW_H

#ifdef VEGA_BACKEND_DIRECT3D10

#include "modules/libvega/vega3ddevice.h"

class VEGAD3d10FramebufferObject;
class VEGAD3d10Texture;
class VEGA3dBuffer;
class VEGA3dVertexLayout;
class FontFlushListener;

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
#include <d2d1.h>
#endif

class VEGAD3d10Window : public VEGA3dWindow
{
public:
	VEGAD3d10Window(VEGAWindow* w, ID3D10Device1* dev, IDXGIFactory* dxgi
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
		, ID2D1Factory* d2dFactory, D2D1_FEATURE_LEVEL flevel, D2D1_TEXT_ANTIALIAS_MODE textMode
#endif
		);
	~VEGAD3d10Window();
	OP_STATUS Construct();

	virtual void present(const OpRect* updateRects, unsigned int numRects);

	virtual unsigned int getWidth(){return m_width;}
	virtual unsigned int getHeight(){return m_height;}

	virtual OP_STATUS resizeBackbuffer(unsigned int width, unsigned int height);

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	ID2D1RenderTarget* getD2DRenderTarget(FontFlushListener* fontFlushListener);
#ifdef VEGA_NATIVE_FONT_SUPPORT
	virtual void flushFonts();
#endif
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	ID3D10Texture2D* getRTTexture(){return m_rtResource;}
	ID3D10RenderTargetView* getRTView(){return m_rtView;}

	virtual void* getBackbufferHandle();
	virtual void releaseBackbufferHandle(void* handle);

	virtual OP_STATUS readBackbuffer(VEGAPixelStore*);
protected:

	void destroyResources();

	VEGAWindow* m_nativeWin;
	unsigned int m_width;
	unsigned int m_height;

	ID3D10Device1* m_d3dDevice;
	IDXGIFactory* m_dxgiFactory;
	IDXGISwapChain* m_swapChain;

	IDXGISurface1* m_dxgiSurface;
	ID3D10Texture2D* m_rtResource;
	ID3D10Texture2D* m_bufferResource;
	ID3D10RenderTargetView* m_rtView;
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	ID2D1Factory* m_d2dFactory;
	D2D1_FEATURE_LEVEL m_d2dFLevel;
	ID2D1RenderTarget* m_d2dRenderTarget;
	ID2D1Layer* m_d2dLayer; ///< dummy object, used to avoid recreating the d3d device on resize
	bool m_isDrawingD2D;
	D2D1_TEXT_ANTIALIAS_MODE m_textMode;
	FontFlushListener* m_fontFlushListener;
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
};

#endif // VEGA_BACKEND_DIRECT3D10
#endif // !VEGAD3D10WINDOW_H

