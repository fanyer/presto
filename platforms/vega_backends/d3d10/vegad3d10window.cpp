/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
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

#include <dxgi.h>

#include "platforms/vega_backends/d3d10/vegad3d10window.h"
#include "platforms/vega_backends/d3d10/vegad3d10texture.h"
#include "platforms/vega_backends/d3d10/vegad3d10fbo.h"
#include "modules/libvega/vegawindow.h"

VEGAD3d10Window::VEGAD3d10Window(VEGAWindow* w, ID3D10Device1* dev, IDXGIFactory* dxgi
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
		, ID2D1Factory* d2dFactory, D2D1_FEATURE_LEVEL flevel, D2D1_TEXT_ANTIALIAS_MODE textMode
#endif
	) : m_nativeWin(w), m_width(0), m_height(0), m_d3dDevice(dev), m_dxgiFactory(dxgi), m_swapChain(NULL), m_dxgiSurface(NULL), m_rtResource(NULL), m_bufferResource(NULL), m_rtView(NULL)
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
		, m_d2dFactory(d2dFactory), m_d2dFLevel(flevel), m_d2dRenderTarget(NULL), m_d2dLayer(NULL), m_isDrawingD2D(false), m_textMode(textMode), m_fontFlushListener(NULL)
#endif
{}

VEGAD3d10Window::~VEGAD3d10Window()
{
	destroyResources();
	if (m_d2dLayer)
		m_d2dLayer->Release();
	if (m_swapChain)
		m_swapChain->Release();
}

OP_STATUS VEGAD3d10Window::Construct()
{
	return resizeBackbuffer(m_nativeWin->getWidth(), m_nativeWin->getHeight());
}

void VEGAD3d10Window::destroyResources()
{
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	if (m_isDrawingD2D)
	{
		m_fontFlushListener->discardBatch();
		m_d2dRenderTarget->EndDraw();
		m_isDrawingD2D = false;
	}
#endif

	if (g_vegaGlobals.vega3dDevice && g_vegaGlobals.vega3dDevice->getRenderTarget() == this)
		g_vegaGlobals.vega3dDevice->setRenderTarget(NULL);

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	if (m_d2dRenderTarget)
		m_d2dRenderTarget->Release();
	m_d2dRenderTarget = NULL;
#endif

	if (m_rtView)
		m_rtView->Release();
	m_rtView = NULL;

	if (m_rtResource)
		m_rtResource->Release();
	m_rtResource = NULL;

	if (m_bufferResource)
		m_bufferResource->Release();
	m_bufferResource = NULL;

	if (m_dxgiSurface)
		m_dxgiSurface->Release();
	m_dxgiSurface = NULL;
}

void VEGAD3d10Window::present(const OpRect*, unsigned int)
{
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	if (m_isDrawingD2D)
	{
		m_fontFlushListener->flushFontBatch();
		m_d2dRenderTarget->EndDraw();
		m_isDrawingD2D = false;
	}
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	if (m_swapChain)
		m_swapChain->Present(0, 0);
}

OP_STATUS VEGAD3d10Window::resizeBackbuffer(unsigned int width, unsigned int height)
{
	if (m_width == width && m_height == height)
		return OpStatus::OK;

	destroyResources();
	OP_STATUS status = OpStatus::OK;

	static const DXGI_FORMAT defaultFmt = DXGI_FORMAT_B8G8R8A8_UNORM;
	if (width && height)
	{
		if (m_swapChain)
		{
			UINT flags = 0;
			if (m_nativeWin->requiresBackbufferReading())
				flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;
			HRESULT err = m_swapChain->ResizeBuffers(1, width, height, defaultFmt, flags);
			if (FAILED(err))
				status = OpStatus::ERR;
		}
		else
		{
			DXGI_SWAP_CHAIN_DESC desc;
			op_memset(&desc, 0, sizeof(DXGI_SWAP_CHAIN_DESC));
			desc.BufferDesc.Width = width;
			desc.BufferDesc.Height = height;
			desc.BufferDesc.Format = defaultFmt;
			desc.SampleDesc.Count = 1;
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.BufferCount = 1;
			desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;//DXGI_SWAP_EFFECT_DISCARD;
			desc.Windowed = true;
			desc.OutputWindow = (HWND)m_nativeWin->getNativeHandle();
			if (m_nativeWin->requiresBackbufferReading())
				desc.Flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;

			HRESULT err = m_dxgiFactory->CreateSwapChain(m_d3dDevice, &desc, &m_swapChain);
			if (FAILED(err))
			{
				return OpStatus::ERR;
			}

			// Make sure Alt+Enter doesn't trigger fullscreen, since this shortcut is used to bring up
			// the security information dialog in desktop opera.
			m_dxgiFactory->MakeWindowAssociation((HWND)m_nativeWin->getNativeHandle(), DXGI_MWA_NO_WINDOW_CHANGES);
		}
		if (FAILED(m_swapChain->GetBuffer(0, __uuidof(m_rtResource), (void**)&m_rtResource)))
		{
			status = OpStatus::ERR;
		}
		D3D10_RENDER_TARGET_VIEW_DESC viewDesc;
		op_memset(&viewDesc, 0, sizeof(D3D10_RENDER_TARGET_VIEW_DESC));
		viewDesc.Format = defaultFmt;
		viewDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = 0;
		if (FAILED(m_d3dDevice->CreateRenderTargetView(m_rtResource, &viewDesc, &m_rtView)))
		{
			status = OpStatus::ERR;
		}
	}
	if (OpStatus::IsSuccess(status))
	{
		m_width = width;
		m_height = height;
	}
	return status;
}

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
ID2D1RenderTarget* VEGAD3d10Window::getD2DRenderTarget(FontFlushListener* fontFlushListener)
{
	if (!m_d2dRenderTarget)
	{
		IDXGISurface1 *pDxgiSurface = NULL;
		if (!m_rtResource || FAILED(m_rtResource->QueryInterface(&pDxgiSurface)))
			return NULL;

		D2D1_RENDER_TARGET_PROPERTIES rtProps;
		rtProps.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
		rtProps.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
		rtProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;//D2D1_ALPHA_MODE_PREMULTIPLIED;
		rtProps.dpiX = 0;
		rtProps.dpiY = 0;
		rtProps.usage = D2D1_RENDER_TARGET_USAGE_NONE;
		rtProps.minLevel = m_d2dFLevel;
		HRESULT hr = m_d2dFactory->CreateDxgiSurfaceRenderTarget(pDxgiSurface, &rtProps, &m_d2dRenderTarget);
		pDxgiSurface->Release();
		if (FAILED(hr))
			m_d2dRenderTarget = NULL;
		else
			m_d2dRenderTarget->SetTextAntialiasMode(m_textMode);
		// Create a dummy d2d object which keeps a reference to the d3d device to ensure the d3d device is not recreated on resize
		if (!m_d2dLayer && m_d2dRenderTarget)
			m_d2dRenderTarget->CreateLayer(NULL, &m_d2dLayer);

	}
	if (!m_isDrawingD2D && m_d2dRenderTarget)
	{
		m_fontFlushListener = fontFlushListener;
		m_d2dRenderTarget->BeginDraw();
		m_isDrawingD2D = true;
	}
	return m_d2dRenderTarget;
}

# ifdef VEGA_NATIVE_FONT_SUPPORT
void VEGAD3d10Window::flushFonts()
{
	if (m_isDrawingD2D)
	{
		m_fontFlushListener->flushFontBatch();
		m_d2dRenderTarget->Flush();
	}
}
# endif // VEGA_NATIVE_FONT_SUPPORT
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY

void* VEGAD3d10Window::getBackbufferHandle()
{
	HDC dc;
	// GetDC will unbind the rendertarget in dx, so make sure vega does not think it is still bound
	if (g_vegaGlobals.vega3dDevice && g_vegaGlobals.vega3dDevice->getRenderTarget() == this)
		g_vegaGlobals.vega3dDevice->setRenderTarget(NULL);
	if (!m_dxgiSurface)
	{
		if (!m_rtResource || FAILED(m_rtResource->QueryInterface(&m_dxgiSurface)))
			return NULL;
	}

	if (FAILED(m_dxgiSurface->GetDC(TRUE, &dc)))
		return NULL;
	return (void*)dc;
}

void VEGAD3d10Window::releaseBackbufferHandle(void* handle)
{
	if (!handle || !m_dxgiSurface)
		return;
	m_dxgiSurface->ReleaseDC(NULL);
}

OP_STATUS VEGAD3d10Window::readBackbuffer(VEGAPixelStore* store)
{
	if (!store || !store->buffer)
		return OpStatus::ERR;

	if (!m_bufferResource)
	{
		D3D10_TEXTURE2D_DESC desc;
		ZeroMemory( &desc, sizeof(desc) );
		desc.Width = m_width;
		desc.Height = m_height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D10_USAGE_STAGING;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;

		HRESULT ret = m_d3dDevice->CreateTexture2D(&desc, 0, &m_bufferResource);
		if(FAILED(ret))
			return OpStatus::ERR;
	}

	m_d3dDevice->CopyResource(m_bufferResource, m_rtResource);

	D3D10_MAPPED_TEXTURE2D mapped;
	if (FAILED(m_bufferResource->Map(0, D3D10_MAP_READ, 0, &mapped)))
		return OpStatus::ERR;

	if (store->stride == mapped.RowPitch)
	{
		op_memcpy((char*)store->buffer, (char*)mapped.pData, m_height*mapped.RowPitch);
	}
	else
	{
		for(unsigned int row = 0; row<m_height; row++)
		{
			op_memcpy((char*)store->buffer + row*store->stride, (char*)mapped.pData + row*mapped.RowPitch, m_width*4);
		}
	}

	m_bufferResource->Unmap(0);

	return OpStatus::OK;
}

#endif //NO_DXSDK
#endif // VEGA_BACKEND_DIRECT3D10

