/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_DIRECT3D10

#include <d3d10_1.h>
// If WebGL is enabled we need both the DXSDK and the D3DCompiler header. To find out if we
// have both we include the D3DCompiler header here. If the header isn't installed we will
// fall back and include our dummy D3DCompiler header which will define NO_DXSDK. That will
// in turn disable compilation of the DX backend.
#ifdef CANVAS3D_SUPPORT
#include <D3DCompiler.h>
#endif //CANVAS3D_SUPPORT
#ifndef NO_DXSDK

#include "platforms/vega_backends/d3d10/vegad3d10fbo.h"
#include "platforms/vega_backends/d3d10/vegad3d10texture.h"

VEGAD3d10RenderbufferObject::VEGAD3d10RenderbufferObject(unsigned int w, unsigned int h, ColorFormat fmt, unsigned int quality) : 
	m_width(w), m_height(h), m_format(fmt), m_samples(quality),
	m_rtView(NULL), m_dsView(NULL), m_texture(NULL)
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	, m_activeD2DFBO(NULL)
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
{}

VEGAD3d10RenderbufferObject::~VEGAD3d10RenderbufferObject()
{
	if (m_rtView)
		m_rtView->Release();
	if (m_dsView)
		m_dsView->Release();
	if (m_texture)
		m_texture->Release();
}

OP_STATUS VEGAD3d10RenderbufferObject::Construct(ID3D10Device1* d3dDevice)
{
	if (m_width == 0 || m_height == 0)
		return OpStatus::OK;
	D3D10_TEXTURE2D_DESC texDesc;
	op_memset(&texDesc, 0, sizeof(D3D10_TEXTURE2D_DESC));
	texDesc.Width = m_width;
	texDesc.Height = m_height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D10_USAGE_DEFAULT;
	texDesc.BindFlags = D3D10_BIND_RENDER_TARGET;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	switch (m_format)
	{
	case FORMAT_RGBA8:
	case FORMAT_RGBA4:
	case FORMAT_RGB5_A1:
		texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case FORMAT_RGB565:
		texDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
		break;
	case FORMAT_DEPTH16:
		texDesc.Format = DXGI_FORMAT_D16_UNORM;
		texDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
		break;
	case FORMAT_STENCIL8:
	case FORMAT_DEPTH16_STENCIL8:
		texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		texDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
		break;
	default:
		return OpStatus::ERR;
	}
	if (m_samples > 1)
	{
		UINT quality = 0;
		while (m_samples > 1 && quality == 0)
		{
			d3dDevice->CheckMultisampleQualityLevels(texDesc.Format, m_samples, &quality);
			if (quality == 0)
				--m_samples;
		}
		if (m_samples > 1)
		{
			texDesc.SampleDesc.Count = m_samples;
			texDesc.SampleDesc.Quality = quality-1;
		}
	}
	if (FAILED(d3dDevice->CreateTexture2D(&texDesc, NULL, &m_texture)))
		return OpStatus::ERR;
	if (texDesc.BindFlags == D3D10_BIND_DEPTH_STENCIL)
	{
		D3D10_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		op_memset(&dsvDesc, 0, sizeof(D3D10_DEPTH_STENCIL_VIEW_DESC));
		dsvDesc.Format = texDesc.Format;
		dsvDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
		if (m_samples > 1)
		{
			dsvDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			dsvDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;
		}
		if (FAILED(d3dDevice->CreateDepthStencilView(m_texture, &dsvDesc, &m_dsView)))
			return OpStatus::ERR;
	}
	else
	{
		D3D10_RENDER_TARGET_VIEW_DESC rtvDesc;
		op_memset(&rtvDesc, 0, sizeof(D3D10_RENDER_TARGET_VIEW_DESC));
		rtvDesc.Format = texDesc.Format;
		if (m_samples > 1)
		{
			rtvDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			rtvDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = 0;
		}
		if (FAILED(d3dDevice->CreateRenderTargetView(m_texture, &rtvDesc, &m_rtView)))
			return OpStatus::ERR;
	}
	return OpStatus::OK;
}

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
void VEGAD3d10RenderbufferObject::setActiveD2DFBO(VEGAD3d10FramebufferObject* fbo)
{
	if (fbo == m_activeD2DFBO)
		return;
	VEGAD3d10FramebufferObject* oldfbo = m_activeD2DFBO;
	m_activeD2DFBO = NULL;
	if (oldfbo)
		oldfbo->StopDrawingD2D();
	m_activeD2DFBO = fbo;
}
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY

unsigned int VEGAD3d10RenderbufferObject::getRedBits()
{
	switch (m_format)
	{
	case FORMAT_RGBA8:
	case FORMAT_RGBA4:
		return 8;
	case FORMAT_RGB565:
	case FORMAT_RGB5_A1:
		return 5;
	default:
		return 0;
	}
}
unsigned int VEGAD3d10RenderbufferObject::getGreenBits()
{
	switch (m_format)
	{
	case FORMAT_RGBA8:
	case FORMAT_RGBA4:
		return 8;
	case FORMAT_RGB565:
		return 6;
	case FORMAT_RGB5_A1:
		return 5;
	default:
		return 0;
	}
}
unsigned int VEGAD3d10RenderbufferObject::getBlueBits()
{
	switch (m_format)
	{
	case FORMAT_RGBA8:
	case FORMAT_RGBA4:
		return 8;
	case FORMAT_RGB565:
	case FORMAT_RGB5_A1:
		return 5;
	default:
		return 0;
	}
}
unsigned int VEGAD3d10RenderbufferObject::getAlphaBits()
{
	switch (m_format)
	{
	case FORMAT_RGBA8:
	case FORMAT_RGBA4:
		return 8;
	case FORMAT_RGB5_A1:
		return 1;
	default:
		return 0;
	}
}
unsigned int VEGAD3d10RenderbufferObject::getDepthBits()
{
	switch (m_format)
	{
	case FORMAT_DEPTH16:
		return 16;
	case FORMAT_DEPTH16_STENCIL8:
	case FORMAT_STENCIL8:
		return 24;
	default:
		return 0;
	}
}
unsigned int VEGAD3d10RenderbufferObject::getStencilBits()
{
	switch (m_format)
	{
	case FORMAT_DEPTH16_STENCIL8:
	case FORMAT_STENCIL8:
		return 8;
	default:
		return 0;
	}
}

VEGAD3d10FramebufferObject::VEGAD3d10FramebufferObject(ID3D10Device1* d3dDevice
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
		, ID2D1Factory* d2dFactory, D2D1_FEATURE_LEVEL flevel, D2D1_TEXT_ANTIALIAS_MODE textMode
#endif
		, bool featureLevel9
	) : m_attachedColorTex(NULL), m_attachedColorBuf(NULL), m_attachedDepthStencilBuf(NULL), m_texRTView(NULL), m_d3dDevice(d3dDevice)
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	, m_d2dFactory(d2dFactory), m_d2dFLevel(flevel), m_d2dRenderTarget(NULL), m_isDrawingD2D(false), m_textMode(textMode), m_fontFlushListener(NULL)
#endif
	,m_featureLevel9(featureLevel9)
{}

VEGAD3d10FramebufferObject::~VEGAD3d10FramebufferObject()
{
	if (g_vegaGlobals.vega3dDevice && g_vegaGlobals.vega3dDevice->getRenderTarget() == this)
		g_vegaGlobals.vega3dDevice->setRenderTarget(NULL);
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	StopDrawingD2D();
	if (m_d2dRenderTarget)
		m_d2dRenderTarget->Release();
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY

	if (m_texRTView)
		m_texRTView->Release();
	VEGARefCount::DecRef(m_attachedColorTex);
	VEGARefCount::DecRef(m_attachedColorBuf);
	VEGARefCount::DecRef(m_attachedDepthStencilBuf);
}

unsigned int VEGAD3d10FramebufferObject::getWidth()
{
	if (m_attachedColorTex)
		return m_attachedColorTex->getWidth();
	if (m_attachedColorBuf)
		return m_attachedColorBuf->getWidth();
	if (m_attachedDepthStencilBuf)
		return m_attachedDepthStencilBuf->getWidth();
	return 0;
}

unsigned int VEGAD3d10FramebufferObject::getHeight()
{
	if (m_attachedColorTex)
		return m_attachedColorTex->getHeight();
	if (m_attachedColorBuf)
		return m_attachedColorBuf->getHeight();
	if (m_attachedDepthStencilBuf)
		return m_attachedDepthStencilBuf->getHeight();
	return 0;
}

OP_STATUS VEGAD3d10FramebufferObject::attachColor(VEGA3dTexture* color, VEGA3dTexture::CubeSide side)
{
	VEGARefCount::IncRef(color);
	VEGARefCount::DecRef(m_attachedColorTex);
	m_attachedColorTex = color;
	VEGARefCount::DecRef(m_attachedColorBuf);
	m_attachedColorBuf = NULL;
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	StopDrawingD2D();
	if (m_d2dRenderTarget)
		m_d2dRenderTarget->Release();
	m_d2dRenderTarget = NULL;
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	if (m_texRTView)
		m_texRTView->Release();
	m_texRTView = NULL;
	if (color)
	{
		D3D10_RENDER_TARGET_VIEW_DESC rtvDesc;
		op_memset(&rtvDesc, 0, sizeof(D3D10_RENDER_TARGET_VIEW_DESC));
		switch (color->getFormat())
		{
		case VEGA3dTexture::FORMAT_RGBA8888:
		case VEGA3dTexture::FORMAT_RGBA4444:
			rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			break;
		case VEGA3dTexture::FORMAT_RGB888:
			rtvDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
			break;
		case VEGA3dTexture::FORMAT_RGBA5551:
			rtvDesc.Format = DXGI_FORMAT_B5G5R5A1_UNORM;
			break;
		case VEGA3dTexture::FORMAT_RGB565:
			rtvDesc.Format = DXGI_FORMAT_B5G6R5_UNORM;
			break;
		case VEGA3dTexture::FORMAT_ALPHA8:
			rtvDesc.Format = m_featureLevel9?DXGI_FORMAT_B8G8R8A8_UNORM:DXGI_FORMAT_A8_UNORM;
			break;
		case VEGA3dTexture::FORMAT_LUMINANCE8:
			// Wastes memory, but there is no luminance texture in d3d 10 and we cannot rely on shaders since the shaders can come from webgl
			rtvDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
			break;
		case VEGA3dTexture::FORMAT_LUMINANCE8_ALPHA8:
			// Wastes memory, but there is no luminance alpha texture in d3d 10 and we cannot rely on shaders since the shaders can come from webgl
			rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			break;
		default:
			OP_ASSERT(false);
			break;
		}

		switch (side)
		{
		case VEGA3dTexture::CUBE_SIDE_NEG_X:
			rtvDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.FirstArraySlice = D3D10_TEXTURECUBE_FACE_NEGATIVE_X;
			rtvDesc.Texture2DArray.ArraySize = 1;
			break;
		case VEGA3dTexture::CUBE_SIDE_POS_X:
			rtvDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.FirstArraySlice = D3D10_TEXTURECUBE_FACE_POSITIVE_X;
			rtvDesc.Texture2DArray.ArraySize = 1;
			break;
		case VEGA3dTexture::CUBE_SIDE_NEG_Y:
			rtvDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.FirstArraySlice = D3D10_TEXTURECUBE_FACE_NEGATIVE_Y;
			rtvDesc.Texture2DArray.ArraySize = 1;
			break;
		case VEGA3dTexture::CUBE_SIDE_POS_Y:
			rtvDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.FirstArraySlice = D3D10_TEXTURECUBE_FACE_POSITIVE_Y;
			rtvDesc.Texture2DArray.ArraySize = 1;
			break;
		case VEGA3dTexture::CUBE_SIDE_NEG_Z:
			rtvDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.FirstArraySlice = D3D10_TEXTURECUBE_FACE_NEGATIVE_Z;
			rtvDesc.Texture2DArray.ArraySize = 1;
			break;
		case VEGA3dTexture::CUBE_SIDE_POS_Z:
			rtvDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.FirstArraySlice = D3D10_TEXTURECUBE_FACE_POSITIVE_Z;
			rtvDesc.Texture2DArray.ArraySize = 1;
			break;
		default:
			rtvDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = 0;
			break;
		}
		HRESULT res = m_d3dDevice->CreateRenderTargetView(static_cast<VEGAD3d10Texture*>(color)->getTextureResource(), &rtvDesc, &m_texRTView);
		if (FAILED(res))
		{
			m_texRTView = NULL;
			VEGARefCount::DecRef(m_attachedColorTex);
			m_attachedColorTex = NULL;
			return (res == E_OUTOFMEMORY)?OpStatus::ERR_NO_MEMORY:OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10FramebufferObject::attachColor(VEGA3dRenderbufferObject* color)
{
	VEGARefCount::IncRef(color);
	VEGARefCount::DecRef(m_attachedColorTex);
	m_attachedColorTex = NULL;
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	StopDrawingD2D();
	if (m_d2dRenderTarget)
		m_d2dRenderTarget->Release();
	m_d2dRenderTarget = NULL;
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	if (m_texRTView)
		m_texRTView->Release();
	m_texRTView = NULL;
	VEGARefCount::DecRef(m_attachedColorBuf);
	m_attachedColorBuf = color;
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10FramebufferObject::attachDepth(VEGA3dRenderbufferObject* depth)
{
	VEGARefCount::IncRef(depth);
	VEGARefCount::DecRef(m_attachedDepthStencilBuf);
	m_attachedDepthStencilBuf = depth;
	return OpStatus::OK;
}
OP_STATUS VEGAD3d10FramebufferObject::attachStencil(VEGA3dRenderbufferObject* stencil)
{
	VEGARefCount::IncRef(stencil);
	VEGARefCount::DecRef(m_attachedDepthStencilBuf);
	m_attachedDepthStencilBuf = stencil;
	return OpStatus::OK;
}
OP_STATUS VEGAD3d10FramebufferObject::attachDepthStencil(VEGA3dRenderbufferObject* depthStencil)
{
	VEGARefCount::IncRef(depthStencil);
	VEGARefCount::DecRef(m_attachedDepthStencilBuf);
	m_attachedDepthStencilBuf = depthStencil;
	return OpStatus::OK;
}

VEGA3dTexture* VEGAD3d10FramebufferObject::getAttachedColorTexture()
{
	return m_attachedColorTex;
}

VEGA3dRenderbufferObject* VEGAD3d10FramebufferObject::getAttachedColorBuffer()
{
	return m_attachedColorBuf;
}

VEGA3dRenderbufferObject* VEGAD3d10FramebufferObject::getAttachedDepthStencilBuffer()
{
	return m_attachedDepthStencilBuf;
}

unsigned int VEGAD3d10FramebufferObject::getRedBits()
{
	if (m_attachedColorBuf)
		return m_attachedColorBuf->getRedBits();
	if (m_attachedColorTex)
	{
		switch (m_attachedColorTex->getFormat())
		{
		case VEGA3dTexture::FORMAT_RGBA8888:
		case VEGA3dTexture::FORMAT_RGB888:
		case VEGA3dTexture::FORMAT_RGBA4444:
			return 8;
		case VEGA3dTexture::FORMAT_RGBA5551:
		case VEGA3dTexture::FORMAT_RGB565:
			return 5;
		default:
			break;
		}
	}
	return 0;
}

unsigned int VEGAD3d10FramebufferObject::getGreenBits()
{
	if (m_attachedColorBuf)
		return m_attachedColorBuf->getGreenBits();
	if (m_attachedColorTex)
	{
		switch (m_attachedColorTex->getFormat())
		{
		case VEGA3dTexture::FORMAT_RGBA8888:
		case VEGA3dTexture::FORMAT_RGB888:
		case VEGA3dTexture::FORMAT_RGBA4444:
			return 8;
		case VEGA3dTexture::FORMAT_RGBA5551:
			return 5;
		case VEGA3dTexture::FORMAT_RGB565:
			return 6;
		default:
			break;
		}
	}
	return 0;
}

unsigned int VEGAD3d10FramebufferObject::getBlueBits()
{
	if (m_attachedColorBuf)
		return m_attachedColorBuf->getBlueBits();
	if (m_attachedColorTex)
	{
		switch (m_attachedColorTex->getFormat())
		{
		case VEGA3dTexture::FORMAT_RGBA8888:
		case VEGA3dTexture::FORMAT_RGB888:
		case VEGA3dTexture::FORMAT_RGBA4444:
			return 8;
		case VEGA3dTexture::FORMAT_RGBA5551:
		case VEGA3dTexture::FORMAT_RGB565:
			return 5;
		default:
			break;
		}
	}
	return 0;
}

unsigned int VEGAD3d10FramebufferObject::getAlphaBits()
{
	if (m_attachedColorBuf)
		return m_attachedColorBuf->getAlphaBits();
	if (m_attachedColorTex)
	{
		switch (m_attachedColorTex->getFormat())
		{
		case VEGA3dTexture::FORMAT_RGBA8888:
		case VEGA3dTexture::FORMAT_RGBA4444:
			return 8;
		case VEGA3dTexture::FORMAT_RGBA5551:
			return 1;
		default:
			break;
		}
	}
	return 0;
}

unsigned int VEGAD3d10FramebufferObject::getDepthBits()
{
	if (m_attachedDepthStencilBuf)
		m_attachedDepthStencilBuf->getDepthBits();
	return 0;
}

unsigned int VEGAD3d10FramebufferObject::getStencilBits()
{
	if (m_attachedDepthStencilBuf)
		m_attachedDepthStencilBuf->getStencilBits();
	return 0;
}

unsigned int VEGAD3d10FramebufferObject::getSubpixelBits()
{
	return D3D10_SUBPIXEL_FRACTIONAL_BIT_COUNT;
}

unsigned int VEGAD3d10FramebufferObject::getSampleBuffers()
{
	if (m_attachedColorBuf)
		return (m_attachedColorBuf->getNumSamples()>1)?1:0;
	return 0;
}

unsigned int VEGAD3d10FramebufferObject::getSamples()
{
	if (m_attachedColorBuf)
		return m_attachedColorBuf->getNumSamples();
	return 0;
}

ID3D10RenderTargetView* VEGAD3d10FramebufferObject::getRenderTargetView()
{
	if (m_attachedColorBuf)
		return static_cast<VEGAD3d10RenderbufferObject*>(m_attachedColorBuf)->getRenderTargetView();
	return m_texRTView;
}

ID3D10DepthStencilView* VEGAD3d10FramebufferObject::getDepthStencilView()
{
	if (m_attachedDepthStencilBuf)
		return static_cast<VEGAD3d10RenderbufferObject*>(m_attachedDepthStencilBuf)->getDepthStencilView();
	return NULL;
}

ID3D10Texture2D* VEGAD3d10FramebufferObject::getColorResource()
{
	if (m_attachedColorTex)
		return static_cast<VEGAD3d10Texture*>(m_attachedColorTex)->getTextureResource();
	if (m_attachedColorBuf)
		return static_cast<VEGAD3d10RenderbufferObject*>(m_attachedColorBuf)->getTextureResource();
	return NULL;
}

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
ID2D1RenderTarget* VEGAD3d10FramebufferObject::getD2DRenderTarget(bool requireDestAlpha, FontFlushListener* fontFlushListener)
{
	if (!m_d2dRenderTarget)
	{
		HRESULT hr = S_OK;
		IDXGISurface *pDxgiSurface = NULL;
		if (m_attachedColorTex)
			hr = static_cast<VEGAD3d10Texture*>(m_attachedColorTex)->getTextureResource()->QueryInterface(&pDxgiSurface);
		else if (m_attachedColorBuf)
			hr = static_cast<VEGAD3d10RenderbufferObject*>(m_attachedColorBuf)->getTextureResource()->QueryInterface(&pDxgiSurface);
		else
			return NULL;

		if (FAILED(hr))
			return NULL;

		D2D1_RENDER_TARGET_PROPERTIES rtProps;
		rtProps.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
		rtProps.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
		rtProps.pixelFormat.alphaMode = requireDestAlpha?D2D1_ALPHA_MODE_PREMULTIPLIED:D2D1_ALPHA_MODE_IGNORE;
		rtProps.dpiX = 0;
		rtProps.dpiY = 0;
		rtProps.usage = D2D1_RENDER_TARGET_USAGE_NONE;
		rtProps.minLevel = m_d2dFLevel;
		hr = m_d2dFactory->CreateDxgiSurfaceRenderTarget(pDxgiSurface, &rtProps, &m_d2dRenderTarget);
		pDxgiSurface->Release();
		if (FAILED(hr))
			m_d2dRenderTarget = NULL;
		else
			m_d2dRenderTarget->SetTextAntialiasMode(m_textMode);
	}
	if (!m_isDrawingD2D && m_d2dRenderTarget)
	{
		m_fontFlushListener = fontFlushListener;
		m_d2dRenderTarget->BeginDraw();
		m_isDrawingD2D = true;
		if (m_attachedColorTex)
			static_cast<VEGAD3d10Texture*>(m_attachedColorTex)->setActiveD2DFBO(this);
		else
			static_cast<VEGAD3d10RenderbufferObject*>(m_attachedColorBuf)->setActiveD2DFBO(this);
	}
	return m_d2dRenderTarget;
}

void VEGAD3d10FramebufferObject::StopDrawingD2D()
{
	if (m_isDrawingD2D)
	{
		m_isDrawingD2D = false;
		m_fontFlushListener->flushFontBatch();
		m_d2dRenderTarget->EndDraw();
		if (m_attachedColorTex)
			static_cast<VEGAD3d10Texture*>(m_attachedColorTex)->setActiveD2DFBO(NULL);
		else
			static_cast<VEGAD3d10RenderbufferObject*>(m_attachedColorBuf)->setActiveD2DFBO(NULL);
	}
}

# ifdef VEGA_NATIVE_FONT_SUPPORT
void VEGAD3d10FramebufferObject::flushFonts()
{
	if (m_isDrawingD2D)
	{
		m_fontFlushListener->flushFontBatch();
		m_d2dRenderTarget->Flush();
	}
}
# endif // VEGA_NATIVE_FONT_SUPPORT
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY

#endif //NO_DXSDK
#endif // VEGA_BACKEND_DIRECT3D10

