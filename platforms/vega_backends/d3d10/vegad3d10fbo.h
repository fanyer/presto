/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAD3D10FBO_H
#define VEGAD3D10FBO_H

#ifdef VEGA_BACKEND_DIRECT3D10

#include "modules/libvega/vega3ddevice.h"
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
#include <d2d1.h>
class VEGAD3d10FramebufferObject;
#endif

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	class FontFlushListener
	{
	public:
		/*
			The flushFontBatch listener is called when EndDraw()/Flush() is called.
			This gives the platform the possibility to limit the number of
			DrawText/DrawGlypRun calls on the render target, which speeds
			up font rendering.
		*/
		virtual void flushFontBatch() = 0;
		/*
			discardBatch is usually only called if the render target is resized before
			the EndDraw() calls happens. The platform should then reset the font batch.
		*/
		virtual void discardBatch() = 0;
	};
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY

class VEGAD3d10RenderbufferObject : public VEGA3dRenderbufferObject
{
public:
	VEGAD3d10RenderbufferObject(unsigned int w, unsigned int h, ColorFormat fmt, unsigned int quality);
	~VEGAD3d10RenderbufferObject();
	OP_STATUS Construct(ID3D10Device1* d3dDevice);

	ID3D10RenderTargetView* getRenderTargetView(){return m_rtView;}
	ID3D10DepthStencilView* getDepthStencilView(){return m_dsView;}
	ID3D10Texture2D* getTextureResource(){return m_texture;}

	virtual unsigned int getWidth(){return m_width;}
	virtual unsigned int getHeight(){return m_height;}
	virtual ColorFormat getFormat(){return m_format;}
	virtual unsigned int getNumSamples(){return m_samples;}
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	void setActiveD2DFBO(VEGAD3d10FramebufferObject* fbo);
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	virtual unsigned int getRedBits();
	virtual unsigned int getGreenBits();
	virtual unsigned int getBlueBits();
	virtual unsigned int getAlphaBits();
	virtual unsigned int getDepthBits();
	virtual unsigned int getStencilBits();
private:
	unsigned int m_width;
	unsigned int m_height;
	ColorFormat m_format;
	unsigned int m_samples;

	ID3D10RenderTargetView* m_rtView;
	ID3D10DepthStencilView* m_dsView;
	ID3D10Texture2D* m_texture;
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	VEGAD3d10FramebufferObject* m_activeD2DFBO;
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
};

class VEGAD3d10FramebufferObject : public VEGA3dFramebufferObject
{
public:
	VEGAD3d10FramebufferObject(ID3D10Device1* d3dDevice
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
		, ID2D1Factory* d2dFactory, D2D1_FEATURE_LEVEL flevel, D2D1_TEXT_ANTIALIAS_MODE textMode
#endif
		, bool featureLevel9);
	virtual ~VEGAD3d10FramebufferObject();

	virtual unsigned int getWidth();
	virtual unsigned int getHeight();

	virtual OP_STATUS attachColor(class VEGA3dTexture* color, VEGA3dTexture::CubeSide side);
	virtual OP_STATUS attachColor(VEGA3dRenderbufferObject* color);
	virtual OP_STATUS attachDepth(VEGA3dRenderbufferObject* depth);
	virtual OP_STATUS attachStencil(VEGA3dRenderbufferObject* stencil);
	virtual OP_STATUS attachDepthStencil(VEGA3dRenderbufferObject* depthStencil);
	virtual class VEGA3dTexture* getAttachedColorTexture();
	virtual class VEGA3dRenderbufferObject* getAttachedColorBuffer();
	virtual class VEGA3dRenderbufferObject* getAttachedDepthStencilBuffer();

	virtual unsigned int getRedBits();
	virtual unsigned int getGreenBits();
	virtual unsigned int getBlueBits();
	virtual unsigned int getAlphaBits();
	virtual unsigned int getDepthBits();
	virtual unsigned int getStencilBits();
	virtual unsigned int getSubpixelBits();

	virtual unsigned int getSampleBuffers();
	virtual unsigned int getSamples();

	ID3D10RenderTargetView* getRenderTargetView();
	ID3D10DepthStencilView* getDepthStencilView();
	ID3D10Texture2D* getColorResource();

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	ID2D1RenderTarget* getD2DRenderTarget(bool requireDestAlpha, FontFlushListener* fontFlushListener);
	void StopDrawingD2D();
# ifdef VEGA_NATIVE_FONT_SUPPORT
	virtual void flushFonts();
# endif
#endif
private:
	VEGA3dTexture* m_attachedColorTex;
	VEGA3dRenderbufferObject* m_attachedColorBuf;
	VEGA3dRenderbufferObject* m_attachedDepthStencilBuf;
	ID3D10RenderTargetView* m_texRTView;
	ID3D10Device1* m_d3dDevice;

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	ID2D1Factory* m_d2dFactory;
	D2D1_FEATURE_LEVEL m_d2dFLevel;
	ID2D1RenderTarget* m_d2dRenderTarget;
	bool m_isDrawingD2D;
	D2D1_TEXT_ANTIALIAS_MODE m_textMode;
	FontFlushListener* m_fontFlushListener;
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	bool m_featureLevel9;
};

#endif // VEGA_BACKEND_DIRECT3D10
#endif // !VEGAD3D10FBO_H

