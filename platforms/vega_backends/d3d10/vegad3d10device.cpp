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

#include <dxgi.h>

#include "modules/util/opautoptr.h"
#include "platforms/vega_backends/d3d10/vegad3d10device.h"
#include "platforms/vega_backends/d3d10/vegad3d10texture.h"
#include "platforms/vega_backends/d3d10/vegad3d10window.h"
#include "platforms/vega_backends/d3d10/vegad3d10buffer.h"
#include "platforms/vega_backends/d3d10/vegad3d10shader.h"
#include "platforms/vega_backends/d3d10/vegad3d10fbo.h"

#ifdef VEGA_3DDEVICE
# include "modules/libvega/vegawindow.h"
#endif //VEGA_3DDEVICE

#ifdef VEGA_BACKEND_DYNAMIC_LIBRARY_LOADING
#include "modules/pi/OpDLL.h"
#endif
typedef HRESULT (WINAPI *PFNCREATEDXGIFACTORY1PROC)(REFIID riid, void **ppFactory);
typedef HRESULT (WINAPI *PFNCREATEDXGIFACTORYPROC)(REFIID riid, void **ppFactory);
typedef HRESULT (WINAPI *PFND3D10CREATEDEVICE1PROC)(
    IDXGIAdapter *pAdapter,
    D3D10_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    D3D10_FEATURE_LEVEL1 HardwareLevel,
    UINT SDKVersion,
    ID3D10Device1 **ppDevice);
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
typedef HRESULT (WINAPI *PFND2D1CREATEFACTORYPROC)(
        __in D2D1_FACTORY_TYPE factoryType,
        __in REFIID riid,
        __in_opt CONST D2D1_FACTORY_OPTIONS *pFactoryOptions,
        __out void **ppIFactory
        );
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY

static const D3D10_BLEND_OP D3D10BlendOp[] = 
{
	D3D10_BLEND_OP_ADD,
	D3D10_BLEND_OP_SUBTRACT,
	D3D10_BLEND_OP_REV_SUBTRACT
};

static const D3D10_BLEND D3D10BlendWeightColor[] = 
{
	D3D10_BLEND_ONE,
	D3D10_BLEND_ZERO,
	D3D10_BLEND_SRC_ALPHA,
	D3D10_BLEND_INV_SRC_ALPHA,
	D3D10_BLEND_DEST_ALPHA,
	D3D10_BLEND_INV_DEST_ALPHA,
	D3D10_BLEND_SRC_COLOR,
	D3D10_BLEND_INV_SRC_COLOR,
	D3D10_BLEND_DEST_COLOR,
	D3D10_BLEND_INV_DEST_COLOR,
	D3D10_BLEND_BLEND_FACTOR,
	D3D10_BLEND_INV_BLEND_FACTOR,
	D3D10_BLEND_BLEND_FACTOR,
	D3D10_BLEND_INV_BLEND_FACTOR,
	D3D10_BLEND_SRC_ALPHA_SAT,
	D3D10_BLEND_SRC1_COLOR,
	D3D10_BLEND_SRC1_ALPHA,
	D3D10_BLEND_INV_SRC1_COLOR,
	D3D10_BLEND_INV_SRC1_ALPHA
};
static const D3D10_BLEND D3D10BlendWeightAlpha[] = 
{
	D3D10_BLEND_ONE,
	D3D10_BLEND_ZERO,
	D3D10_BLEND_SRC_ALPHA,
	D3D10_BLEND_INV_SRC_ALPHA,
	D3D10_BLEND_DEST_ALPHA,
	D3D10_BLEND_INV_DEST_ALPHA,
	D3D10_BLEND_SRC_ALPHA,
	D3D10_BLEND_INV_SRC_ALPHA,
	D3D10_BLEND_DEST_ALPHA,
	D3D10_BLEND_INV_DEST_ALPHA,
	D3D10_BLEND_BLEND_FACTOR,
	D3D10_BLEND_INV_BLEND_FACTOR,
	D3D10_BLEND_BLEND_FACTOR,
	D3D10_BLEND_INV_BLEND_FACTOR,
	D3D10_BLEND_SRC_ALPHA_SAT,
	D3D10_BLEND_SRC1_ALPHA,
	D3D10_BLEND_SRC1_ALPHA,
	D3D10_BLEND_INV_SRC1_ALPHA,
	D3D10_BLEND_INV_SRC1_ALPHA
};

static const D3D10_COMPARISON_FUNC D3D10DepthFunc[] = 
{
	D3D10_COMPARISON_NEVER,
	D3D10_COMPARISON_EQUAL,
	D3D10_COMPARISON_LESS,
	D3D10_COMPARISON_LESS_EQUAL,
	D3D10_COMPARISON_GREATER,
	D3D10_COMPARISON_GREATER_EQUAL,
	D3D10_COMPARISON_NOT_EQUAL,
	D3D10_COMPARISON_ALWAYS
};

static const D3D10_COMPARISON_FUNC D3D10StencilFunc[] = 
{
	D3D10_COMPARISON_LESS,
	D3D10_COMPARISON_LESS_EQUAL,
	D3D10_COMPARISON_GREATER,
	D3D10_COMPARISON_GREATER_EQUAL,
	D3D10_COMPARISON_EQUAL,
	D3D10_COMPARISON_NOT_EQUAL,
	D3D10_COMPARISON_ALWAYS,
	D3D10_COMPARISON_NEVER
};

static const D3D10_STENCIL_OP D3D10StencilOp[] = 
{
	D3D10_STENCIL_OP_KEEP,
	D3D10_STENCIL_OP_INVERT,
	D3D10_STENCIL_OP_INCR_SAT,
	D3D10_STENCIL_OP_DECR_SAT,
	D3D10_STENCIL_OP_ZERO,
	D3D10_STENCIL_OP_REPLACE,
	D3D10_STENCIL_OP_INCR,
	D3D10_STENCIL_OP_DECR
};

static const D3D10_CULL_MODE D3D10CullMode[] = 
{
	D3D10_CULL_BACK,
	D3D10_CULL_FRONT
};

static const D3D10_TEXTURE_ADDRESS_MODE D3D10TextureWrapMode[] = 
{
	D3D10_TEXTURE_ADDRESS_WRAP,
	D3D10_TEXTURE_ADDRESS_MIRROR,
	D3D10_TEXTURE_ADDRESS_CLAMP
};

static const D3D10_FEATURE_LEVEL1 featureLevel[] =
{
	D3D10_FEATURE_LEVEL_10_1,
	D3D10_FEATURE_LEVEL_10_0,
	D3D10_FEATURE_LEVEL_9_3,
	D3D10_FEATURE_LEVEL_9_2,
	D3D10_FEATURE_LEVEL_9_1
};

class VEGAD3d10RenderState : public VEGA3dRenderState
{
public:
	VEGAD3d10RenderState() : m_blendState(NULL), m_depthStencilState(NULL), m_rasterState(NULL)
	{}
	virtual ~VEGAD3d10RenderState()
	{
		if (m_blendState)
			m_blendState->Release();
		if (m_depthStencilState)
			m_depthStencilState->Release();
		if (m_rasterState)
			m_rasterState->Release();
	}
	virtual bool isImplementationSpecific(){return true;}

	ID3D10BlendState1* m_blendState;
	ID3D10DepthStencilState* m_depthStencilState;
	ID3D10RasterizerState* m_rasterState;
};

OP_STATUS VEGAD3d10Device::Create(VEGA3dDevice** dev VEGA_3D_NATIVE_WIN_DECL, D3D10_DRIVER_TYPE driverType)
{
#ifdef VEGA_3DDEVICE
	if (VEGA_3D_NATIVE_WIN_NAME && !VEGA_3D_NATIVE_WIN_NAME->getNativeHandle())
		return OpStatus::ERR;
#endif // VEGA_3DDEVICE
	*dev = NULL;
	VEGAD3d10Device* wdev = OP_NEW(VEGAD3d10Device, (VEGA_3D_NATIVE_WIN_NAME, driverType));
	RETURN_OOM_IF_NULL(wdev);
	OP_STATUS s = wdev->Init();
	if (OpStatus::IsError(s))
	{
		OP_DELETE(wdev);
		return s;
	}
	*dev = wdev;
	return OpStatus::OK;
}

VEGAD3d10Device::VEGAD3d10Device(
#ifdef VEGA_3DDEVICE
							 VEGAWindow* nativeWin, D3D10_DRIVER_TYPE driver_type) : m_nativeWindow(nativeWin),
#else
							 ) :
#endif // VEGA_3DDEVICE
	m_currentRenderState(NULL), m_scissorX(0), m_scissorY(0), m_scissorW(0), m_scissorH(0),
	m_renderTarget(NULL), m_shaderProgram(NULL), m_shaderProgramLinkId(0),
	m_currentVertexLayout(NULL), m_currentConstantBuffers(NULL),
	m_currentPrimitive(NUM_PRIMITIVES), m_noStencilState(NULL),
	m_blendDisabledState(NULL), m_dxgiFactory(NULL), m_d3dDevice(NULL),
	m_rasterState(NULL), m_depthState(NULL), m_blendState(NULL),
	m_emptyTexture(NULL), m_emptySRV(NULL), m_featureLevel(D3D10_FEATURE_LEVEL_10_0), m_driverType(driver_type)
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	, m_d2dFactory(NULL), m_textMode(D2D1_TEXT_ANTIALIAS_MODE_DEFAULT)
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
#ifdef CANVAS3D_SUPPORT
	, m_D3DCompileProc(NULL)
	, m_D3DReflectShaderProc(NULL)
#endif
#ifdef VEGA_BACKEND_DYNAMIC_LIBRARY_LOADING
	, m_dxgiDLL(NULL), m_d3dDLL(NULL)
#ifdef CANVAS3D_SUPPORT
	, m_d3dCompilerDLL(NULL)
#endif
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	, m_d2dDLL(NULL)
#endif
#endif
{
	op_memset(m_activeTextures, NULL, d3d10ActiveTextureCount*sizeof(*m_activeTextures));
	op_memset(m_samplers, NULL, d3d10ActiveTextureCount*sizeof(*m_samplers));
	for (int i = 0; i < ARRAY_SIZE(m_shaderProgramCache); ++i)
	{
		m_shaderProgramCache[i] = NULL;
	}
}

VEGAD3d10Device::~VEGAD3d10Device()
{
	// Unbind all textures
	for (unsigned int unit = 0; unit < d3d10ActiveTextureCount; ++unit)
	{
		setTexture(unit, NULL);
	}
	if (m_d3dDevice)
		m_d3dDevice->ClearState();
	destroyDefault2dObjects();
	if (m_currentConstantBuffers)
		VEGARefCount::DecRef(m_currentConstantBuffers);
	if (m_noStencilState)
		m_noStencilState->Release();
	if (m_blendDisabledState)
		m_blendDisabledState->Release();
	VEGARefCount::DecRef(m_currentVertexLayout);
	VEGARefCount::DecRef(m_shaderProgram);
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	if (m_d2dFactory)
		m_d2dFactory->Release();
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	for (int i = 0; i < ARRAY_SIZE(m_shaderProgramCache); ++i)
	{
		VEGARefCount::DecRef(m_shaderProgramCache[i]);
	}
	for (size_t i = 0; i < d3d10ActiveTextureCount; ++i)
	{
		if (m_samplers[i])
			m_samplers[i]->Release();
	}
	if (m_emptySRV)
		m_emptySRV->Release();
	if (m_emptyTexture)
		m_emptyTexture->Release();
	if (m_blendState)
		m_blendState->Release();
	if (m_depthState)
		m_depthState->Release();
	if (m_rasterState)
		m_rasterState->Release();
	if (m_d3dDevice)
		m_d3dDevice->Release();
	if (m_dxgiFactory)
		m_dxgiFactory->Release();
	m_compatBuffer.Destroy();
#ifdef VEGA_BACKEND_DYNAMIC_LIBRARY_LOADING
	OP_DELETE(m_dxgiDLL);
	OP_DELETE(m_d3dDLL);
#ifdef CANVAS3D_SUPPORT
	OP_DELETE(m_d3dCompilerDLL);
#endif
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	OP_DELETE(m_d2dDLL);
#endif
#endif
}

OP_STATUS VEGAD3d10Device::Init()
{
	PFNCREATEDXGIFACTORYPROC CreateDXGIFactoryProc = NULL;
	PFNCREATEDXGIFACTORY1PROC CreateDXGIFactory1Proc = NULL;
#ifdef VEGA_BACKEND_DYNAMIC_LIBRARY_LOADING
	g_vega_backends_module.SetCreationStatus(UNI_L("Could not load library"));

	RETURN_IF_ERROR(OpDLL::Create(&m_dxgiDLL));
	RETURN_IF_ERROR(m_dxgiDLL->Load(UNI_L("dxgi.dll")));
	CreateDXGIFactory1Proc = (PFNCREATEDXGIFACTORY1PROC)m_dxgiDLL->GetSymbolAddress("CreateDXGIFactory1");
	CreateDXGIFactoryProc = (PFNCREATEDXGIFACTORYPROC)m_dxgiDLL->GetSymbolAddress("CreateDXGIFactory");
	if (!CreateDXGIFactory1Proc && !CreateDXGIFactoryProc)
		return OpStatus::ERR;
	RETURN_IF_ERROR(OpDLL::Create(&m_d3dDLL));
	RETURN_IF_ERROR(m_d3dDLL->Load(UNI_L("d3d10_1.dll")));
	PFND3D10CREATEDEVICE1PROC D3D10CreateDevice1Proc = (PFND3D10CREATEDEVICE1PROC)m_d3dDLL->GetSymbolAddress("D3D10CreateDevice1");
	if (!D3D10CreateDevice1Proc)
		return OpStatus::ERR;
#  ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	RETURN_IF_ERROR(OpDLL::Create(&m_d2dDLL));
	RETURN_IF_ERROR(m_d2dDLL->Load(UNI_L("d2d1.dll")));
	PFND2D1CREATEFACTORYPROC D2D1CreateFactoryProc = (PFND2D1CREATEFACTORYPROC)m_d2dDLL->GetSymbolAddress("D2D1CreateFactory");
	if (!D2D1CreateFactoryProc)
		return OpStatus::ERR;
#  endif // VEGA_BACKEND_D2D_INTEROPERABILITY
#  ifdef CANVAS3D_SUPPORT
	RETURN_IF_ERROR(OpDLL::Create(&m_d3dCompilerDLL));
	RETURN_IF_ERROR(m_d3dCompilerDLL->Load(D3DCOMPILER_DLL));
	m_D3DCompileProc = (pD3DCompile)m_d3dCompilerDLL->GetSymbolAddress("D3DCompile");
	if (!m_D3DCompileProc)
		return OpStatus::ERR;

	m_D3DReflectShaderProc = (VEGAD3D10_pD3DReflectShader)m_d3dDLL->GetSymbolAddress("D3D10ReflectShader");
	if (!m_D3DReflectShaderProc)
		return OpStatus::ERR;
#  endif // CANVAS3D_SUPPORT

	g_vega_backends_module.SetCreationStatus(UNI_L(""));

#else // VEGA_BACKEND_DYNAMIC_LIBRARY_LOADING

	CreateDXGIFactoryProc = CreateDXGIFactory;
	CreateDXGIFactory1Proc = CreateDXGIFactory1;
	PFND3D10CREATEDEVICE1PROC D3D10CreateDevice1Proc = D3D10CreateDevice1;
#  ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	PFND2D1CREATEFACTORYPROC D2D1CreateFactoryProc = D2D1CreateFactory;
#  endif // VEGA_BACKEND_D2D_INTEROPERABILITY
#  ifdef CANVAS3D_SUPPORT
	m_D3DCompileProc = D3DCompile;
	m_D3DReflectShaderProc = D3D10ReflectShader;
#  endif // CANVAS3D_SUPPORT
#endif // VEGA_BACKEND_DYNAMIC_LIBRARY_LOADING

	IDXGIAdapter* dxgiAdapter = NULL;
	IDXGIFactory* dxgiFactory = NULL;
	IDXGIFactory1* dxgiFactory1 = NULL;

#if 0
	// This code is used to initialize a device which can be profiled and debugged using nvidia PerfHUD.
	// If you want to use PerfHUD change to this code-path.
	if (CreateDXGIFactory1Proc && SUCCEEDED(CreateDXGIFactory1Proc(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory1))))
		dxgiFactory = dxgiFactory1;
	else if (FAILED(CreateDXGIFactoryProc(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory))))
		return OpStatus::ERR;

	int adapter_num = 0;
	IDXGIAdapter* adapter = NULL;
	while (dxgiFactory->EnumAdapters(adapter_num, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		if (adapter)
		{
			DXGI_ADAPTER_DESC adaptDesc;
			if (SUCCEEDED(adapter->GetDesc(&adaptDesc)))
			{
				// Select the PerfHUD one if it exists.
				if (uni_str_eq(adaptDesc.Description, UNI_L("NVIDIA PerfHUD")))
				{
					dxgiAdapter = adapter;
					m_driverType = D3D10_DRIVER_TYPE_REFERENCE;
					break;
				}
			}
			adapter->Release();
		}
		++adapter_num;
	}
#else
	// If we want a Warp device. We have to pass NULL as the adapter. D3D10CreateDevice1Proc will create the adapter and the factory.
	if (m_driverType != D3D10_DRIVER_TYPE_WARP)
	{
		// If we want to be sure to get a DXGI 1.1 adapter, we have to enumerate the adapters with a DXGI 1.1 factory.
		if (CreateDXGIFactory1Proc && SUCCEEDED(CreateDXGIFactory1Proc(__uuidof(IDXGIFactory1), (void**)&dxgiFactory1)))
			dxgiFactory = dxgiFactory1;
		else if (FAILED(CreateDXGIFactoryProc(__uuidof(IDXGIFactory), (void**)&dxgiFactory)))
			return OpStatus::ERR;

		if (FAILED(dxgiFactory->EnumAdapters(0, &dxgiAdapter)))
		{
			dxgiFactory->Release();
			dxgiFactory = NULL;
			return OpStatus::ERR;
		}
	}
#endif

	UINT deviceflags = D3D10_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	deviceflags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	bool initialized = false;
	for (UINT i = 0; i < ARRAY_SIZE(featureLevel); i++)
	{
		if (SUCCEEDED(D3D10CreateDevice1Proc(dxgiAdapter, m_driverType, NULL, deviceflags, featureLevel[i], D3D10_1_SDK_VERSION, &m_d3dDevice)))
		{
			initialized = true;
			m_featureLevel = featureLevel[i];
			break;
		}
	}

	if (dxgiAdapter)
	{
		dxgiAdapter->Release();
		dxgiAdapter = NULL;
	}

	if (dxgiFactory)
	{
		dxgiFactory->Release();
		dxgiFactory = NULL;
	}

	if (!initialized)
		return OpStatus::ERR;

	// Make sure we get the right factory which belongs to this device. If we passed dxgiAdapter == NULL to D3D10CreateDevice1Proc to use Warp,
	// it uses it's own factory to create the device. And if we create a Level9 device, we would not have the right adapter either.
	IDXGIDevice* dxgiDevice = NULL;
	HRESULT hr = m_d3dDevice->QueryInterface(&dxgiDevice);

	if (SUCCEEDED(hr))
		hr = dxgiDevice->GetAdapter(&dxgiAdapter);

	if (SUCCEEDED(hr))
	{
		// Prefer a DXGI 1.1 factory, but if the adapter does not support that, fallback to a 1.0 factory.
		if (SUCCEEDED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory1))))
			m_dxgiFactory = dxgiFactory1;
		else
			hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&m_dxgiFactory));
	}

	if (dxgiAdapter)
		dxgiAdapter->Release();

	if (dxgiDevice)
		dxgiDevice->Release();

	if (FAILED(hr))
		return OpStatus::ERR;

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	if (FAILED(D2D1CreateFactoryProc(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), NULL, (void**)&m_d2dFactory)))
		return OpStatus::ERR;
#endif

	D3D10_FEATURE_LEVEL1 level = m_d3dDevice->GetFeatureLevel();

	switch (level)
	{
		case D3D10_FEATURE_LEVEL_10_1:
			m_description.Set(UNI_L("Direct3D 10.1 (Level 10.1)"));
			break;
		case D3D10_FEATURE_LEVEL_10_0:
			m_description.Set(UNI_L("Direct3D 10.1 (Level 10.0)"));
			break;
		case D3D10_FEATURE_LEVEL_9_3:
			m_description.Set(UNI_L("Direct3D 10.1 (Level 9.3)"));
			break;
		case D3D10_FEATURE_LEVEL_9_2:
			m_description.Set(UNI_L("Direct3D 10.1 (Level 9.2)"));
			break;
		case D3D10_FEATURE_LEVEL_9_1:
			m_description.Set(UNI_L("Direct3D 10.1 (Level 9.1)"));
			break;
		default:
			m_description.Set(UNI_L("Direct3D 10.1 (Unknown)"));
			break;
	}

	if (m_driverType == D3D10_DRIVER_TYPE_WARP)
		m_description.Append(UNI_L(" Warp"));

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Device::Construct(VEGA_3D_BLOCKLIST_ENTRY_DECL)
{
	RETURN_IF_ERROR(applyRenderState());
	D3D10_SUBRESOURCE_DATA texData;
	UINT32 whitePix = 0xffffffff;
	texData.pSysMem = &whitePix;
	texData.SysMemPitch = 4;
	texData.SysMemSlicePitch = 4;

	D3D10_TEXTURE2D_DESC texDesc;
	op_memset(&texDesc, 0, sizeof(D3D10_TEXTURE2D_DESC));
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D10_USAGE_DEFAULT;
	texDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	
	if (FAILED(m_d3dDevice->CreateTexture2D(&texDesc, &texData, &m_emptyTexture)))
		return OpStatus::ERR;
	D3D10_SHADER_RESOURCE_VIEW_DESC srvDesc;
	op_memset(&srvDesc, 0, sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC));
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	if (FAILED(m_d3dDevice->CreateShaderResourceView(m_emptyTexture, &srvDesc, &m_emptySRV)))
	{
#ifdef VEGA_BACKENDS_USE_BLOCKLIST
		g_vega_backends_module.SetCreationStatus(UNI_L("No shader support"));
#endif // VEGA_BACKENDS_USE_BLOCKLIST
		return OpStatus::ERR;
	}
	m_d3dDevice->PSSetShaderResources(0, 1, &m_emptySRV);
	m_d3dDevice->PSSetShaderResources(1, 1, &m_emptySRV);
	m_d3dDevice->PSSetShaderResources(2, 1, &m_emptySRV);
	m_d3dDevice->PSSetShaderResources(3, 1, &m_emptySRV);

	for (unsigned int tu = 0; tu < d3d10ActiveTextureCount; ++tu)
	{
		op_memset(&m_samplerDesc[tu], 0, sizeof(m_samplerDesc[tu]));
		m_samplerDesc[tu].Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
		m_samplerDesc[tu].AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
		m_samplerDesc[tu].AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
		m_samplerDesc[tu].AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
		m_samplerDesc[tu].ComparisonFunc = D3D10_COMPARISON_NEVER;
		m_samplerDesc[tu].MaxLOD = FLT_MAX;
		m_d3dDevice->CreateSamplerState(&m_samplerDesc[tu], &m_samplers[tu]);
	}
	m_d3dDevice->PSSetSamplers(0, d3d10ActiveTextureCount, m_samplers);

	D3D10_BLEND_DESC1 blendDesc;
	op_memset(&blendDesc, 0, sizeof(D3D10_BLEND_DESC1));
	blendDesc.RenderTarget->BlendEnable = FALSE;
	blendDesc.RenderTarget->BlendOp = D3D10_BLEND_OP_ADD;
	blendDesc.RenderTarget->BlendOpAlpha = D3D10_BLEND_OP_ADD;
	blendDesc.RenderTarget->SrcBlend = D3D10_BLEND_ONE;
	blendDesc.RenderTarget->DestBlend = D3D10_BLEND_ZERO;
	blendDesc.RenderTarget->SrcBlendAlpha = D3D10_BLEND_ONE;
	blendDesc.RenderTarget->DestBlendAlpha = D3D10_BLEND_ZERO;
	blendDesc.RenderTarget->RenderTargetWriteMask = D3D10_COLOR_WRITE_ENABLE_ALL;
	for (int i = 1; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		blendDesc.RenderTarget[i] = blendDesc.RenderTarget[0];
	if (FAILED(m_d3dDevice->CreateBlendState1(&blendDesc, &m_blendDisabledState)))
		return OpStatus::ERR;

	D3D10_DEPTH_STENCIL_DESC dsDesc;
	op_memset(&dsDesc, 0, sizeof(D3D10_DEPTH_STENCIL_DESC));
	if (FAILED(m_d3dDevice->CreateDepthStencilState(&dsDesc, &m_noStencilState)))
		return OpStatus::ERR;
	const OP_STATUS status = createDefault2dObjects();
#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	if (OpStatus::IsError(status))
	{
		g_vega_backends_module.SetCreationStatus(UNI_L("Failed to create objects required for hardware accelerated 2D rendering"));
	}
#endif // VEGA_BACKENDS_USE_BLOCKLIST
	return status;
}

void VEGAD3d10Device::flush()
{
}

OP_STATUS VEGAD3d10Device::createRenderState(VEGA3dRenderState** state, bool copyCurrentState)
{
	if (copyCurrentState)
		*state = OP_NEW(VEGA3dRenderState, (m_renderState));
	else
		*state = OP_NEW(VEGAD3d10RenderState, ());
	if (!(*state))
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Device::setRenderState(VEGA3dRenderState* state)
{
	// Only check for changes in state if we are setting a new
	// state or the state has changed
	if (state != m_currentRenderState || state->isChanged())
	{
		if (!state->isChanged() && state->isImplementationSpecific() && 
			static_cast<VEGAD3d10RenderState*>(state)->m_blendState &&
			static_cast<VEGAD3d10RenderState*>(state)->m_depthStencilState &&
			static_cast<VEGAD3d10RenderState*>(state)->m_rasterState)
		{
			if (static_cast<VEGAD3d10RenderState*>(state)->m_blendState != m_blendState)
			{
				ID3D10BlendState1* tempBlendstate = m_blendState;
				m_blendState = static_cast<VEGAD3d10RenderState*>(state)->m_blendState;
				m_blendState->AddRef();

				float blendFactors[4];
				state->getBlendColor(blendFactors[0], blendFactors[1], blendFactors[2], blendFactors[3]);
				m_d3dDevice->OMSetBlendState(m_blendState, blendFactors, ~0u);

				if (tempBlendstate)
					tempBlendstate->Release();
			}
			if (static_cast<VEGAD3d10RenderState*>(state)->m_depthStencilState != m_depthState)
			{
				ID3D10DepthStencilState* tempDepthState = m_depthState;
				m_depthState = static_cast<VEGAD3d10RenderState*>(state)->m_depthStencilState;
				m_depthState->AddRef();

				VEGA3dRenderState::StencilFunc test, testBack;
				unsigned int reference, mask;
				state->getStencilFunc(test, testBack, reference, mask);
				m_d3dDevice->OMSetDepthStencilState(m_depthState, reference);

				if (tempDepthState)
					tempDepthState->Release();
			}
			if (static_cast<VEGAD3d10RenderState*>(state)->m_rasterState != m_rasterState)
			{
				ID3D10RasterizerState* tempRasterState = m_rasterState;
				m_rasterState = static_cast<VEGAD3d10RenderState*>(state)->m_rasterState;
				m_rasterState->AddRef();

				m_d3dDevice->RSSetState(m_rasterState);

				if (tempRasterState)
					tempRasterState->Release();
			}
	
			m_currentRenderState = state;
			m_renderState = *state;
			return OpStatus::OK;
		}

		D3D10_RASTERIZER_DESC rasterizerDesc;
		D3D10_DEPTH_STENCIL_DESC depthStencilDesc;
		D3D10_BLEND_DESC1 blendDesc;
		float blendFactors[4];
		state->getBlendColor(blendFactors[0], blendFactors[1], blendFactors[2], blendFactors[3]);
		m_rasterState->GetDesc(&rasterizerDesc);
		m_depthState->GetDesc(&depthStencilDesc);
		m_blendState->GetDesc1(&blendDesc);
		bool rasterChanged = false;
		bool depthChanged = false;
		bool blendChanged = false;

		if (state->isBlendEnabled() != m_renderState.isBlendEnabled() || state->blendChanged(m_renderState))
		{
			m_renderState.enableBlend(state->isBlendEnabled());
			blendDesc.RenderTarget->BlendEnable = state->isBlendEnabled()?TRUE:FALSE;

			VEGA3dRenderState::BlendOp op, opA;
			state->getBlendEquation(op, opA);
			OP_ASSERT(op < VEGA3dRenderState::NUM_BLEND_OPS && opA < VEGA3dRenderState::NUM_BLEND_OPS);
			m_renderState.setBlendEquation(op, opA);
			VEGA3dRenderState::BlendWeight src, dst, srcA, dstA;
			state->getBlendFunc(src, dst, srcA, dstA);
			OP_ASSERT(src < VEGA3dRenderState::NUM_BLEND_WEIGHTS && dst < VEGA3dRenderState::NUM_BLEND_WEIGHTS &&
					srcA < VEGA3dRenderState::NUM_BLEND_WEIGHTS && dstA < VEGA3dRenderState::NUM_BLEND_WEIGHTS);
			m_renderState.setBlendFunc(src, dst, srcA, dstA);
			m_renderState.setBlendColor(blendFactors[0], blendFactors[1], blendFactors[2], blendFactors[3]);

			blendDesc.RenderTarget->BlendOp = D3D10BlendOp[op];
			blendDesc.RenderTarget->BlendOpAlpha = D3D10BlendOp[opA];
			blendDesc.RenderTarget->SrcBlend = D3D10BlendWeightColor[src];
			blendDesc.RenderTarget->DestBlend = D3D10BlendWeightColor[dst];
			blendDesc.RenderTarget->SrcBlendAlpha = D3D10BlendWeightAlpha[srcA];
			blendDesc.RenderTarget->DestBlendAlpha = D3D10BlendWeightAlpha[dstA];
			blendChanged = true;
		}

		if (state->colorMaskChanged(m_renderState))
		{
			bool r, g, b, a;
			state->getColorMask(r, g, b, a);
			m_renderState.setColorMask(r, g, b, a);
			blendDesc.RenderTarget->RenderTargetWriteMask = (r?D3D10_COLOR_WRITE_ENABLE_RED:0)|(g?D3D10_COLOR_WRITE_ENABLE_GREEN:0)|(b?D3D10_COLOR_WRITE_ENABLE_BLUE:0)|(a?D3D10_COLOR_WRITE_ENABLE_ALPHA:0);
			blendChanged = true;
		}

		if (state->isCullFaceEnabled() != m_renderState.isCullFaceEnabled() || state->cullFaceChanged(m_renderState))
		{
			VEGA3dRenderState::Face cf;
			state->getCullFace(cf);
			OP_ASSERT(cf < VEGA3dRenderState::NUM_FACES);
			m_renderState.enableCullFace(state->isCullFaceEnabled());
			m_renderState.setCullFace(cf);
			rasterizerDesc.CullMode = state->isCullFaceEnabled()?D3D10CullMode[cf]:D3D10_CULL_NONE;
			rasterChanged = true;
		}

		if (state->isDepthTestEnabled() != m_renderState.isDepthTestEnabled())
		{
			depthStencilDesc.DepthEnable = state->isDepthTestEnabled()?TRUE:FALSE;
			depthStencilDesc.DepthFunc = D3D10_COMPARISON_LESS;
			m_renderState.enableDepthTest(state->isDepthTestEnabled());
			depthChanged = true;
		}
		if (state->depthFuncChanged(m_renderState))
		{
			VEGA3dRenderState::DepthFunc depth;
			state->getDepthFunc(depth);
			OP_ASSERT(depth < VEGA3dRenderState::NUM_DEPTH_FUNCS);
			m_renderState.setDepthFunc(depth);
			depthStencilDesc.DepthFunc = D3D10DepthFunc[depth];
			depthChanged = true;
		}
		if (state->isDepthWriteEnabled() != m_renderState.isDepthWriteEnabled())
		{
			depthStencilDesc.DepthWriteMask = state->isDepthWriteEnabled()?D3D10_DEPTH_WRITE_MASK_ALL:D3D10_DEPTH_WRITE_MASK_ZERO;
			m_renderState.enableDepthWrite(state->isDepthWriteEnabled());
			depthChanged = true;
		}
		
		if (state->isFrontFaceCCW() != m_renderState.isFrontFaceCCW())
		{
			// We're negating the winding here to compensate for the fact that we reflect about the x-axis
			// in the cross compiled shaders for WebGL. Nothing else than WebGL currently uses culling.
			rasterizerDesc.FrontCounterClockwise = !state->isFrontFaceCCW();
			m_renderState.setFrontFaceCCW(state->isFrontFaceCCW());
			rasterChanged = true;
		}

		if (state->isStencilEnabled() != m_renderState.isStencilEnabled() || state->stencilChanged(m_renderState))
		{
			m_renderState.enableStencil(state->isStencilEnabled());
			depthStencilDesc.StencilEnable = state->isStencilEnabled()?TRUE:FALSE;

			VEGA3dRenderState::StencilFunc test, testBack;
			unsigned int reference, mask, writeMask;
			VEGA3dRenderState::StencilOp fail, zFail, pass;
			VEGA3dRenderState::StencilOp failBack, zFailBack, passBack;
			state->getStencilFunc(test, testBack, reference, mask);
			state->getStencilWriteMask(writeMask);
			state->getStencilOp(fail, zFail, pass, failBack, zFailBack, passBack);
			OP_ASSERT(test < VEGA3dRenderState::NUM_STENCIL_FUNCS && 
				fail < VEGA3dRenderState::NUM_STENCIL_OPS && 
				zFail < VEGA3dRenderState::NUM_STENCIL_OPS && 
				pass < VEGA3dRenderState::NUM_STENCIL_OPS &&
				testBack < VEGA3dRenderState::NUM_STENCIL_FUNCS && 
				failBack < VEGA3dRenderState::NUM_STENCIL_OPS && 
				zFailBack < VEGA3dRenderState::NUM_STENCIL_OPS && 
				passBack < VEGA3dRenderState::NUM_STENCIL_OPS);
			m_renderState.setStencilFunc(test, testBack, reference, mask);
			m_renderState.setStencilWriteMask(writeMask);
			m_renderState.setStencilOp(fail, zFail, pass, failBack, zFailBack, passBack);

			depthStencilDesc.StencilReadMask = mask;
			depthStencilDesc.StencilWriteMask = writeMask;
			depthStencilDesc.FrontFace.StencilFunc = D3D10StencilFunc[test];
			depthStencilDesc.FrontFace.StencilFailOp = D3D10StencilOp[fail];
			depthStencilDesc.FrontFace.StencilDepthFailOp = D3D10StencilOp[zFail];
			depthStencilDesc.FrontFace.StencilPassOp = D3D10StencilOp[pass];
			depthStencilDesc.BackFace.StencilFunc = D3D10StencilFunc[testBack];
			depthStencilDesc.BackFace.StencilFailOp = D3D10StencilOp[failBack];
			depthStencilDesc.BackFace.StencilDepthFailOp = D3D10StencilOp[zFailBack];
			depthStencilDesc.BackFace.StencilPassOp = D3D10StencilOp[passBack];
			depthChanged = true;
		}

		if (state->isScissorEnabled() != m_renderState.isScissorEnabled())
		{
			m_renderState.enableScissor(state->isScissorEnabled());
			rasterizerDesc.ScissorEnable = state->isScissorEnabled()?TRUE:FALSE;
			rasterChanged = true;
		}

		if (state->isPolygonOffsetFillEnabled() != m_renderState.isPolygonOffsetFillEnabled() ||
		    state->polygonOffsetChanged(m_renderState))
		{
			float offsetFactor = state->getPolygonOffsetFactor();
			float offsetUnits = state->getPolygonOffsetUnits();

			m_renderState.enablePolygonOffsetFill(state->isPolygonOffsetFillEnabled());
			m_renderState.polygonOffset(offsetFactor, offsetUnits);

			int newDepthBias;
			float newSlopeScaledDepthBias;

			if (!state->isPolygonOffsetFillEnabled())
			{
				newDepthBias = D3D10_DEFAULT_DEPTH_BIAS;
				newSlopeScaledDepthBias = D3D10_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
			}
			else
			{
				/* See http://msdn.microsoft.com/en-us/library/windows/desktop/cc308048(v=vs.85).aspx .
				   Both PolygonOffset()'s 'units' argument and D3D10's
				   DepthBias are scaled by a value representing the smallest
				   resolveable difference in depth value (the calculation for
				   floating-point depth buffers in the above link yields a
				   value corresponding to the LSB of the mantissa of the depth
				   of the fragment with the greatest z in the primitive), so a
				   straight assignment should be OK. 'factor' and
				   SlopeScaledDepthBias appear to have the same interpretation
				   as well. */
				newDepthBias = static_cast<int>(offsetUnits);
				newSlopeScaledDepthBias = offsetFactor;
			}

			if (newDepthBias != rasterizerDesc.DepthBias ||
			    newSlopeScaledDepthBias != rasterizerDesc.SlopeScaledDepthBias)
			{
				rasterizerDesc.DepthBias = newDepthBias;
				rasterizerDesc.SlopeScaledDepthBias = newSlopeScaledDepthBias;
				rasterChanged = true;
			}
		}

		if (rasterChanged)
		{
			ID3D10RasterizerState* ns;
			if (FAILED(m_d3dDevice->CreateRasterizerState(&rasterizerDesc, &ns)))
				return OpStatus::ERR;
			m_d3dDevice->RSSetState(ns);
			m_rasterState->Release();
			m_rasterState = ns;
		}
		if (depthChanged)
		{
			ID3D10DepthStencilState* ns;
			VEGA3dRenderState::StencilFunc test, testBack;
			unsigned int reference, mask;
			state->getStencilFunc(test, testBack, reference, mask);
			if (FAILED(m_d3dDevice->CreateDepthStencilState(&depthStencilDesc, &ns)))
				return OpStatus::ERR;
			m_d3dDevice->OMSetDepthStencilState(ns, reference);
			m_depthState->Release();
			m_depthState = ns;
		}
		if (blendChanged)
		{
			ID3D10BlendState1* ns;
			for (int i = 1; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
				blendDesc.RenderTarget[i] = blendDesc.RenderTarget[0];
			if (FAILED(m_d3dDevice->CreateBlendState1(&blendDesc, &ns)))
				return OpStatus::ERR;
			m_d3dDevice->OMSetBlendState(ns, blendFactors, ~0u);
			m_blendState->Release();
			m_blendState = ns;
		}
		if (state->isImplementationSpecific())
		{
			if (static_cast<VEGAD3d10RenderState*>(state)->m_blendState)
				static_cast<VEGAD3d10RenderState*>(state)->m_blendState->Release();
			static_cast<VEGAD3d10RenderState*>(state)->m_blendState = m_blendState;
			m_blendState->AddRef();
			if (static_cast<VEGAD3d10RenderState*>(state)->m_depthStencilState)
				static_cast<VEGAD3d10RenderState*>(state)->m_depthStencilState->Release();
			static_cast<VEGAD3d10RenderState*>(state)->m_depthStencilState = m_depthState;
			m_depthState->AddRef();
			if (static_cast<VEGAD3d10RenderState*>(state)->m_rasterState)
				static_cast<VEGAD3d10RenderState*>(state)->m_rasterState->Release();
			static_cast<VEGAD3d10RenderState*>(state)->m_rasterState = m_rasterState;
			m_rasterState->AddRef();
		}

		state->clearChanged();
		m_currentRenderState = state;
	}

	return OpStatus::OK;
}

void VEGAD3d10Device::setScissor(int x, int y, unsigned int w, unsigned int h)
{
	if (x != m_scissorX || y != m_scissorY || w != m_scissorW || h != m_scissorH)
	{
		D3D10_RECT r;
		r.left = x;
		r.top = y;
		r.right = x+w;
		r.bottom = y+h;
		m_d3dDevice->RSSetScissorRects(1, &r);
		m_scissorX = x;
		m_scissorY = y;
		m_scissorW = w;
		m_scissorH = h;
	}
}

OP_STATUS VEGAD3d10Device::applyRenderState()
{
	D3D10_RASTERIZER_DESC rasterizerDesc;
	D3D10_DEPTH_STENCIL_DESC depthStencilDesc;
	D3D10_BLEND_DESC1 blendDesc;
	float blendFactors[4];
	m_renderState.getBlendColor(blendFactors[0], blendFactors[1], blendFactors[2], blendFactors[3]);

	op_memset(&blendDesc, 0, sizeof(D3D10_BLEND_DESC));
	VEGA3dRenderState::BlendOp op, opA;
	m_renderState.getBlendEquation(op, opA);
	VEGA3dRenderState::BlendWeight src, dst, srcA, dstA;
	m_renderState.getBlendFunc(src, dst, srcA, dstA);
	bool r, g, b, a;
	m_renderState.getColorMask(r, g, b, a);
	const D3D10_RENDER_TARGET_BLEND_DESC1 rtbdesc =
	{
		m_renderState.isBlendEnabled()?TRUE:FALSE,
		D3D10BlendWeightColor[src], D3D10BlendWeightColor[dst], D3D10BlendOp[op],
		D3D10BlendWeightAlpha[srcA], D3D10BlendWeightAlpha[dstA], D3D10BlendOp[opA],
		(r?D3D10_COLOR_WRITE_ENABLE_RED:0)|(g?D3D10_COLOR_WRITE_ENABLE_GREEN:0)|(b?D3D10_COLOR_WRITE_ENABLE_BLUE:0)|(a?D3D10_COLOR_WRITE_ENABLE_ALPHA:0)
	};
	for (int i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		blendDesc.RenderTarget[i] = rtbdesc;


	op_memset(&depthStencilDesc, 0, sizeof(D3D10_DEPTH_STENCIL_DESC));
	depthStencilDesc.DepthEnable = m_renderState.isDepthTestEnabled();
	depthStencilDesc.DepthWriteMask = m_renderState.isDepthWriteEnabled()?D3D10_DEPTH_WRITE_MASK_ALL:D3D10_DEPTH_WRITE_MASK_ZERO;
	VEGA3dRenderState::DepthFunc depth;
	m_renderState.getDepthFunc(depth);
	OP_ASSERT(depth < VEGA3dRenderState::NUM_DEPTH_FUNCS);
	depthStencilDesc.DepthFunc = D3D10DepthFunc[depth];
	depthStencilDesc.StencilEnable = m_renderState.isStencilEnabled();

	VEGA3dRenderState::StencilFunc test, testBack;
	unsigned int reference, mask, writeMask;
	VEGA3dRenderState::StencilOp fail, zFail, pass;
	VEGA3dRenderState::StencilOp failBack, zFailBack, passBack;
	m_renderState.getStencilFunc(test, testBack, reference, mask);
	m_renderState.getStencilWriteMask(writeMask);
	m_renderState.getStencilOp(fail, zFail, pass, failBack, zFailBack, passBack);
	OP_ASSERT(test < VEGA3dRenderState::NUM_STENCIL_FUNCS && 
		fail < VEGA3dRenderState::NUM_STENCIL_OPS && 
		zFail < VEGA3dRenderState::NUM_STENCIL_OPS && 
		pass < VEGA3dRenderState::NUM_STENCIL_OPS &&
		testBack < VEGA3dRenderState::NUM_STENCIL_FUNCS && 
		failBack < VEGA3dRenderState::NUM_STENCIL_OPS && 
		zFailBack < VEGA3dRenderState::NUM_STENCIL_OPS && 
		passBack < VEGA3dRenderState::NUM_STENCIL_OPS);

	depthStencilDesc.StencilReadMask = mask;
	depthStencilDesc.StencilWriteMask = writeMask;
	depthStencilDesc.FrontFace.StencilFunc = D3D10StencilFunc[test];
	depthStencilDesc.FrontFace.StencilFailOp = D3D10StencilOp[fail];
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D10StencilOp[zFail];
	depthStencilDesc.FrontFace.StencilPassOp = D3D10StencilOp[pass];
	depthStencilDesc.BackFace.StencilFunc = D3D10StencilFunc[testBack];
	depthStencilDesc.BackFace.StencilFailOp = D3D10StencilOp[failBack];
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D10StencilOp[zFailBack];
	depthStencilDesc.BackFace.StencilPassOp = D3D10StencilOp[passBack];

	op_memset(&rasterizerDesc, 0, sizeof(D3D10_RASTERIZER_DESC));
	rasterizerDesc.FillMode = D3D10_FILL_SOLID;
	VEGA3dRenderState::Face cf;
	m_renderState.getCullFace(cf);
	OP_ASSERT(cf < VEGA3dRenderState::NUM_FACES);
	rasterizerDesc.CullMode = m_renderState.isCullFaceEnabled()?D3D10CullMode[cf]:D3D10_CULL_NONE;
	// We're negating the winding here to compensate for the fact that we reflect about the x-axis
	// in the cross compiled shaders for WebGL. Nothing else than WebGL currently uses culling.
	rasterizerDesc.FrontCounterClockwise = !m_renderState.isFrontFaceCCW();
	if (m_renderState.isPolygonOffsetFillEnabled())
	{
		rasterizerDesc.DepthBias = static_cast<int>(m_renderState.getPolygonOffsetUnits());
		rasterizerDesc.SlopeScaledDepthBias = m_renderState.getPolygonOffsetFactor();
	}
	else
	{
		rasterizerDesc.DepthBias = D3D10_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.SlopeScaledDepthBias = D3D10_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	}
	rasterizerDesc.DepthBiasClamp = D3D10_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.ScissorEnable = m_renderState.isScissorEnabled();
	rasterizerDesc.MultisampleEnable = TRUE;
	rasterizerDesc.AntialiasedLineEnable = FALSE;

	D3D10_RECT sr;
	sr.left = m_scissorX;
	sr.top = m_scissorY;
	sr.right = m_scissorX+m_scissorW;
	sr.bottom = m_scissorY+m_scissorH;
	m_d3dDevice->RSSetScissorRects(1, &sr);

	ID3D10RasterizerState* nrs;
	if (FAILED(m_d3dDevice->CreateRasterizerState(&rasterizerDesc, &nrs)))
		return OpStatus::ERR;
	m_d3dDevice->RSSetState(nrs);
	if (m_rasterState)
		m_rasterState->Release();
	m_rasterState = nrs;

	ID3D10DepthStencilState* nds;
	if (FAILED(m_d3dDevice->CreateDepthStencilState(&depthStencilDesc, &nds)))
		return OpStatus::ERR;
	m_d3dDevice->OMSetDepthStencilState(nds, reference);
	if (m_depthState)
		m_depthState->Release();
	m_depthState = nds;

	ID3D10BlendState1* nbs;
	if (FAILED(m_d3dDevice->CreateBlendState1(&blendDesc, &nbs)))
		return OpStatus::ERR;
	m_d3dDevice->OMSetBlendState(nbs, blendFactors, ~0u);
	if (m_blendState)
		m_blendState->Release();
	m_blendState = nbs;
	return OpStatus::OK;
}

void VEGAD3d10Device::clear(bool color, bool depth, bool stencil, UINT32 colorVal, float depthVal, int stencilVal)
{
	ID3D10RenderTargetView* rtView = NULL;
	ID3D10DepthStencilView* dsView = NULL;
	if (m_renderTarget && m_renderTarget->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW)
	{
		rtView = static_cast<VEGAD3d10Window*>(m_renderTarget)->getRTView();
	}
	else if (m_renderTarget)
	{
		VEGAD3d10FramebufferObject* rtfbo = static_cast<VEGAD3d10FramebufferObject*>(m_renderTarget);
		if (rtfbo)
		{
			rtView = rtfbo->getRenderTargetView();
			dsView = rtfbo->getDepthStencilView();
		}
	}
	if (rtView && color)
	{
		// Check if some color should be masked out.
		bool r,g,b,a;
		m_renderState.getColorMask(r,g,b,a);
		bool needsMask = !(r && g && b && a);

		// If a scissor rect is set or if we need to mask out some component then we need a special case
		// where we draw a quad instead of clearing.
		if (m_renderState.isScissorEnabled() && (m_scissorX != 0 || m_scissorY != 0 || m_scissorW < m_renderTarget->getWidth() || m_scissorH < m_renderTarget->getHeight()) || needsMask)
		{
			// Record some states that we should restore.
			float blendFactors[4];
			m_renderState.getBlendColor(blendFactors[0], blendFactors[1], blendFactors[2], blendFactors[3]);
			VEGA3dRenderState::StencilFunc test, testBack;
			unsigned int reference, mask;
			m_renderState.getStencilFunc(test, testBack, reference, mask);
			VEGA3dShaderProgram* oldshader = m_shaderProgram;

			// If we need to mask out some component we need to create a new blend state.
			ID3D10BlendState1* blendColMask = NULL;
			if (needsMask)
			{
				D3D10_BLEND_DESC1 blendDesc;
				op_memset(&blendDesc, 0, sizeof(D3D10_BLEND_DESC1));
				// Need to be enabled for the RenderTargetWriteMask to have effect.
				blendDesc.RenderTarget->BlendEnable = TRUE;
				blendDesc.RenderTarget->BlendOp = D3D10_BLEND_OP_ADD;
				blendDesc.RenderTarget->BlendOpAlpha = D3D10_BLEND_OP_ADD;
				blendDesc.RenderTarget->SrcBlend = D3D10_BLEND_SRC_COLOR;
				blendDesc.RenderTarget->DestBlend = D3D10_BLEND_ZERO;
				blendDesc.RenderTarget->SrcBlendAlpha = D3D10_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget->DestBlendAlpha = D3D10_BLEND_ZERO;
				blendDesc.RenderTarget->RenderTargetWriteMask = (r?D3D10_COLOR_WRITE_ENABLE_RED:0)|(g?D3D10_COLOR_WRITE_ENABLE_GREEN:0)|(b?D3D10_COLOR_WRITE_ENABLE_BLUE:0)|(a?D3D10_COLOR_WRITE_ENABLE_ALPHA:0);
				for (int i = 1; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
					blendDesc.RenderTarget[i] = blendDesc.RenderTarget[0];
				if (FAILED(m_d3dDevice->CreateBlendState1(&blendDesc, &blendColMask)))
					return;
				m_d3dDevice->OMSetBlendState(blendColMask, blendFactors, ~0u);
			}
			else
				m_d3dDevice->OMSetBlendState(m_blendDisabledState, blendFactors, ~0u);
			m_d3dDevice->OMSetDepthStencilState(m_noStencilState, reference);

			// Set up the vertices for the quad.
			UINT32 ccol = convertToNativeColor(colorVal);
			float x0 = 0,
			      y0 = 0,
			      x1 = static_cast<float>(m_renderTarget->getWidth()),
			      y1 = static_cast<float>(m_renderTarget->getHeight());
			if (m_renderState.isScissorEnabled())
			{
				x0 = static_cast<float>(m_scissorX);
				y0 = static_cast<float>(m_scissorY);
				x1 = static_cast<float>(m_scissorX+m_scissorW);
				y1 = static_cast<float>(m_scissorY+m_scissorH);
			}
			Vega2dVertex verts[4] = {
				{ x0, y0, 0, 0, ccol},
				{ x1, y0, 0, 0, ccol},
				{ x0, y1, 0, 0, ccol},
				{ x1, y1, 0, 0, ccol}
			};

			unsigned int firstVert;
			m_tempBuffer->writeAnywhere(4, sizeof(Vega2dVertex), verts, firstVert);

			// Get a reference to the shader.
			VEGA3dShaderProgram *shader;
			if (OpStatus::IsError(createShaderProgram(&shader, VEGA3dShaderProgram::SHADER_VECTOR2D, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP)))
			{
				if (blendColMask)
					blendColMask->Release();
				return;
			}

			// Draw the quad using the shader (note that it actually uses texture unit 0).
			setShaderProgram(shader);
			shader->setOrthogonalProjection();
			setTexture(0, NULL);
			drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, m_2dVertexLayout, firstVert, 4);

			// Reset the state, and release the blendstate.
			setShaderProgram(oldshader);
			VEGARefCount::DecRef(shader);

			m_d3dDevice->OMSetBlendState(m_blendState, blendFactors, ~0u);
			m_d3dDevice->OMSetDepthStencilState(m_depthState, reference);
			if (blendColMask)
				blendColMask->Release();
		}
		else
		{
			float c[4];
			c[0] = (float)((colorVal>>16)&0xff)/255.f;
			c[1] = (float)((colorVal>>8)&0xff)/255.f;
			c[2] = (float)(colorVal&0xff)/255.f;
			c[3] = (float)(colorVal>>24)/255.f;
			m_d3dDevice->ClearRenderTargetView(rtView, c);
		}
	}
	if (dsView && (depth||stencil))
	{
		float d = depthVal;
		UINT8 s = (UINT8)stencilVal;
		m_d3dDevice->ClearDepthStencilView(dsView, (depth?D3D10_CLEAR_DEPTH:0)|(stencil?D3D10_CLEAR_STENCIL:0), d, s);
	}
}

void VEGAD3d10Device::setTexture(unsigned int unit, VEGA3dTexture* tex)
{
	if (unit >= d3d10ActiveTextureCount)
		return;
	if (tex)
	{
		bool changed = false;
		if (m_samplerDesc[unit].AddressU != D3D10TextureWrapMode[tex->getWrapModeS()])
		{
			m_samplerDesc[unit].AddressU = D3D10TextureWrapMode[tex->getWrapModeS()];
			changed = true;
		}
		if (m_samplerDesc[unit].AddressV != D3D10TextureWrapMode[tex->getWrapModeT()])
		{
			m_samplerDesc[unit].AddressV = D3D10TextureWrapMode[tex->getWrapModeT()];
			changed = true;
		}
		D3D10_FILTER filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
		switch (tex->getMinFilter())
		{
		case VEGA3dTexture::FILTER_NEAREST:
			filter = (tex->getMagFilter() == VEGA3dTexture::FILTER_NEAREST)?D3D10_FILTER_MIN_MAG_MIP_POINT:D3D10_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			break;
		case VEGA3dTexture::FILTER_LINEAR:
			filter = (tex->getMagFilter() == VEGA3dTexture::FILTER_NEAREST)?D3D10_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			break;
		case VEGA3dTexture::FILTER_NEAREST_MIPMAP_NEAREST:
			filter = (tex->getMagFilter() == VEGA3dTexture::FILTER_NEAREST)?D3D10_FILTER_MIN_MAG_MIP_POINT:D3D10_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			break;
		case VEGA3dTexture::FILTER_LINEAR_MIPMAP_NEAREST:
			filter = (tex->getMagFilter() == VEGA3dTexture::FILTER_NEAREST)?D3D10_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			break;
		case VEGA3dTexture::FILTER_NEAREST_MIPMAP_LINEAR:
			filter = (tex->getMagFilter() == VEGA3dTexture::FILTER_NEAREST)?D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR:D3D10_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			break;
		case VEGA3dTexture::FILTER_LINEAR_MIPMAP_LINEAR:
			filter = (tex->getMagFilter() == VEGA3dTexture::FILTER_NEAREST)?D3D10_FILTER_MIN_MAG_MIP_LINEAR:D3D10_FILTER_MIN_POINT_MAG_MIP_LINEAR;
			break;
		}
		if (m_samplerDesc[unit].Filter != filter)
		{
			m_samplerDesc[unit].Filter = filter;
			changed = true;
		}
		if (changed)
		{
			ID3D10SamplerState* newsamp = NULL;
			m_d3dDevice->CreateSamplerState(&m_samplerDesc[unit], &newsamp);
			m_d3dDevice->PSSetSamplers(unit, 1, &newsamp);
			if (m_samplers[unit])
				m_samplers[unit]->Release();
			m_samplers[unit] = newsamp;
		}
	}
	if (tex == m_activeTextures[unit])
		return;
	ID3D10ShaderResourceView* rsv = tex?((VEGAD3d10Texture*)tex)->getSRView():m_emptySRV;
	if (tex)
	{
		// Don't set the actual shader resource if the texture is bound as a render target.
		// It will be set when the render target is unbound
		VEGAD3d10FramebufferObject* rtfbo = NULL;
		if (m_renderTarget && m_renderTarget->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
		{
			rtfbo = static_cast<VEGAD3d10FramebufferObject*>(m_renderTarget);
		}
		if (rtfbo && rtfbo->getAttachedColorTexture() == tex)
			rsv = m_emptySRV;
	}
	m_d3dDevice->PSSetShaderResources(unit, 1, &rsv);
	if (m_activeTextures[unit])
		static_cast<VEGAD3d10Texture*>(m_activeTextures[unit])->decBindCount();
	if (tex)
		static_cast<VEGAD3d10Texture*>(tex)->incBindCount();
	m_activeTextures[unit] = tex;
}

void VEGAD3d10Device::unbindTexture(VEGA3dTexture* tex)
{
	for (unsigned int unit = 0; unit < d3d10ActiveTextureCount; ++unit)
	{
		if (m_activeTextures[unit] == tex)
		{
			setTexture(unit, NULL);
		}
	}
}

void VEGAD3d10Device::setCubeTexture(unsigned int unit, VEGA3dTexture* tex)
{
	// No special handling required, just call the 2d version.
	setTexture(unit, tex);
}

unsigned int VEGAD3d10Device::getMaxVertexAttribs() const
{
	return D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
}

void VEGAD3d10Device::getAliasedPointSizeRange(float &low, float &high)
{
	low = 1.0f;
	high = 1.0f;
}

void VEGAD3d10Device::getAliasedLineWidthRange(float &low, float &high)
{
	low = 1.0f;
	high = 1.0f;
}

unsigned int VEGAD3d10Device::getMaxCubeMapTextureSize() const
{
	return D3D10_REQ_TEXTURECUBE_DIMENSION;
}

unsigned int VEGAD3d10Device::getMaxFragmentUniformVectors() const
{
	return 4096; // We limit ourselves to a single constant buffer in our implementation.
}

unsigned int VEGAD3d10Device::getMaxRenderBufferSize() const
{
	return D3D10_REQ_RENDER_TO_BUFFER_WINDOW_WIDTH;
}

unsigned int VEGAD3d10Device::getMaxTextureSize() const
{
	return D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
}

unsigned int VEGAD3d10Device::getMaxMipLevel() const
{
	// LOG2(D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION)
	#define D3D10_MAX_MIP_LEVEL 13
	OP_STATIC_ASSERT(1 << D3D10_MAX_MIP_LEVEL == D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION);
	return D3D10_MAX_MIP_LEVEL;
}

unsigned int VEGAD3d10Device::getMaxVaryingVectors() const
{
	return D3D10_PS_INPUT_REGISTER_COUNT;
}

unsigned int VEGAD3d10Device::getMaxVertexTextureUnits() const
{
	return D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT;
}

unsigned int VEGAD3d10Device::getMaxFragmentTextureUnits() const
{
	return D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT;
}

unsigned int VEGAD3d10Device::getMaxCombinedTextureUnits() const
{
	return D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT;
}

unsigned int VEGAD3d10Device::getMaxVertexUniformVectors() const
{
	return 4096; // We limit ourselves to a single constant buffer in our implementation.
}

void VEGAD3d10Device::getMaxViewportDims(int &w, int &h) const
{
	w = D3D10_VIEWPORT_BOUNDS_MAX;
	h = D3D10_VIEWPORT_BOUNDS_MAX;
}

unsigned int VEGAD3d10Device::getMaxQuality() const
{
	DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
	unsigned int samples = D3D10_MAX_MULTISAMPLE_SAMPLE_COUNT;
	if (samples > 1)
	{
		UINT quality = 0;
		while (samples > 1 && quality == 0)
		{
			m_d3dDevice->CheckMultisampleQualityLevels(format, samples, &quality);
			if (quality == 0)
				--samples;
		}
	}
	return samples;
}

unsigned int VEGAD3d10Device::getSubpixelBits() const
{
	return D3D10_SUBPIXEL_FRACTIONAL_BIT_COUNT;
}

OP_STATUS VEGAD3d10Device::createShaderProgram(VEGA3dShaderProgram** shader, VEGA3dShaderProgram::ShaderType shdtype, VEGA3dShaderProgram::WrapMode shdmode)
{
	// shdmode is ignored, since d3d10 always has full npot support
	if (m_featureLevel >= D3D10_FEATURE_LEVEL_10_0)
	{
		shdmode = VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;
	}
	else if (m_featureLevel < D3D10_FEATURE_LEVEL_9_3)
	{
		// This shader is too complicated for feature level 9_1 and 9_2
		if (shdtype == VEGA3dShaderProgram::SHADER_CONVOLVE_GEN_25_BIAS && shdmode == VEGA3dShaderProgram::WRAP_REPEAT_REPEAT)
			return OpStatus::ERR;
	}
	size_t shader_index = VEGA3dShaderProgram::GetShaderIndex(shdtype, shdmode);

	if (m_shaderProgramCache[shader_index])
	{
		VEGARefCount::IncRef(m_shaderProgramCache[shader_index]);
		*shader = m_shaderProgramCache[shader_index];
		return OpStatus::OK;
	}
	VEGAD3d10ShaderProgram* shd = OP_NEW(VEGAD3d10ShaderProgram, (m_d3dDevice, m_featureLevel < D3D10_FEATURE_LEVEL_10_0));
	if (!shd)
		return OpStatus::ERR_NO_MEMORY;
#ifdef CANVAS3D_SUPPORT
	OP_STATUS err = shd->loadShader(shdtype, shdmode, m_D3DCompileProc, m_D3DReflectShaderProc);
#else
	OP_STATUS err = shd->loadShader(shdtype, shdmode);
#endif
	if (OpStatus::IsError(err))
	{
		VEGARefCount::DecRef(shd);
		shd = NULL;
	}
	else if (shdtype != VEGA3dShaderProgram::SHADER_CUSTOM)
	{
		m_shaderProgramCache[shader_index] = shd;
		VEGARefCount::IncRef(shd);
	}
	*shader = shd;
	return err;
}

OP_STATUS VEGAD3d10Device::setShaderProgram(VEGA3dShaderProgram* shader)
{
	// In webgl shader programs can be relinked so we need an extra check here.
	if (shader == m_shaderProgram && static_cast<VEGAD3d10ShaderProgram*>(shader)->getLinkId() == m_shaderProgramLinkId)
		return OpStatus::OK;
	VEGARefCount::DecRef(m_shaderProgram);
	VEGARefCount::IncRef(shader);
	m_shaderProgram = shader;
	if (shader)
	{
		m_shaderProgramLinkId = static_cast<VEGAD3d10ShaderProgram*>(shader)->getLinkId();
#ifdef CANVAS3D_SUPPORT
		const OP_STATUS s = shader->setAsRenderTarget(getRenderTarget());
		if (OpStatus::IsError(s))
		{
			VEGARefCount::DecRef(shader);
			m_shaderProgram = 0;
			return s;
		}
#endif // CANVAS3D_SUPPORT
	}

	m_d3dDevice->PSSetShader(shader?((VEGAD3d10ShaderProgram*)shader)->getPixelShader():NULL);
	m_d3dDevice->VSSetShader(shader?((VEGAD3d10ShaderProgram*)shader)->getVertexShader():NULL);
	return OpStatus::OK;
}

VEGA3dShaderProgram* VEGAD3d10Device::getCurrentShaderProgram()
{
	return m_shaderProgram;
}

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGAD3d10Device::createShader(VEGA3dShader** shader, BOOL fragmentShader, const char* source, unsigned int attribute_count, VEGA3dDevice::AttributeData *attributes, OpString *info_log)
{
	VEGAD3d10Shader* sh = OP_NEW(VEGAD3d10Shader, ());
	if (!sh)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS res = sh->Construct(source, fragmentShader, attribute_count, attributes);
	if (OpStatus::IsError(res))
	{
		OP_DELETE(sh);
		return res;
	}
	*shader = sh;
	return OpStatus::OK;
}
#endif // CANVAS3D_SUPPORT

OP_STATUS VEGAD3d10Device::drawPrimitives(Primitive type, VEGA3dVertexLayout* layout, unsigned int firstVert, unsigned int numVerts)
{
	if (!m_shaderProgram)
		return OpStatus::ERR;
	if (layout != m_currentVertexLayout)
	{
		RETURN_IF_ERROR(((VEGAD3d10VertexLayout*)layout)->bind(m_d3dDevice));
		VEGARefCount::DecRef(m_currentVertexLayout);
		m_currentVertexLayout = layout;
		VEGARefCount::IncRef(m_currentVertexLayout);
	}

	if (m_shaderProgram != m_currentConstantBuffers || ((VEGAD3d10ShaderProgram*)m_shaderProgram)->isVertConstantDataModified() || ((VEGAD3d10ShaderProgram*)m_shaderProgram)->isPixConstantDataModified())
	{
		VEGARefCount::IncRef(m_shaderProgram);
		VEGARefCount::DecRef(m_currentConstantBuffers);
		m_currentConstantBuffers = m_shaderProgram;
		ID3D10Buffer* b = ((VEGAD3d10ShaderProgram*)m_shaderProgram)->getVertConstBuffer();
		m_d3dDevice->VSSetConstantBuffers(0, 1, &b);
		b = ((VEGAD3d10ShaderProgram*)m_shaderProgram)->getPixConstBuffer();
		m_d3dDevice->PSSetConstantBuffers(0, 1, &b);
	}

	if (m_currentPrimitive != type)
	{
		switch (type)
		{
		case PRIMITIVE_TRIANGLE_STRIP:
			m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			break;
		case PRIMITIVE_TRIANGLE:
			m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			break;
		case PRIMITIVE_POINT:
			m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
			break;
		case PRIMITIVE_LINE:
			m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
			break;
		case PRIMITIVE_LINE_LOOP:
			{
				m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
				const VEGA3dBuffer *tmpBuffer = NULL;
				unsigned int numIndices = 0;
				RETURN_IF_ERROR(m_compatBuffer.GetIndexBuffer(this, tmpBuffer, VEGA3dCompatIndexBuffer::LINE_LOOP, numVerts, numIndices));
				m_d3dDevice->IASetIndexBuffer(((VEGAD3d10Buffer*)tmpBuffer)->getBuffer(), DXGI_FORMAT_R16_UINT, 0);
				m_d3dDevice->DrawIndexed(numIndices, 0, firstVert);
				m_currentPrimitive = NUM_PRIMITIVES;
				return OpStatus::OK;
			}
			break;
		case PRIMITIVE_LINE_STRIP:
			m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
			break;
		case PRIMITIVE_TRIANGLE_FAN:
			{
				m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				const VEGA3dBuffer *tmpBuffer = NULL;
				unsigned int numIndices = 0;
				RETURN_IF_ERROR(m_compatBuffer.GetIndexBuffer(this, tmpBuffer, VEGA3dCompatIndexBuffer::TRI_FAN, numVerts, numIndices));
				m_d3dDevice->IASetIndexBuffer(((VEGAD3d10Buffer*)tmpBuffer)->getBuffer(), DXGI_FORMAT_R16_UINT, 0);
				m_d3dDevice->DrawIndexed(numIndices, 0, firstVert);
				m_currentPrimitive = NUM_PRIMITIVES;
				return OpStatus::OK;
			}
			break;
		};

		m_currentPrimitive = type;
	}

	m_d3dDevice->Draw(numVerts, firstVert);

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Device::drawIndexedPrimitives(Primitive type, VEGA3dVertexLayout* verts, VEGA3dBuffer* ibuffer, unsigned int bytesPerInd, unsigned int firstInd, unsigned int numInd)
{
	if (!m_shaderProgram)
		return OpStatus::ERR;
	if (verts != m_currentVertexLayout)
	{
		RETURN_IF_ERROR(((VEGAD3d10VertexLayout*)verts)->bind(m_d3dDevice));
		VEGARefCount::DecRef(m_currentVertexLayout);
		m_currentVertexLayout = verts;
		VEGARefCount::IncRef(m_currentVertexLayout);
	}

	if (m_shaderProgram != m_currentConstantBuffers || ((VEGAD3d10ShaderProgram*)m_shaderProgram)->isVertConstantDataModified() || ((VEGAD3d10ShaderProgram*)m_shaderProgram)->isPixConstantDataModified())
	{
		VEGARefCount::IncRef(m_shaderProgram);
		VEGARefCount::DecRef(m_currentConstantBuffers);
		m_currentConstantBuffers = m_shaderProgram;
		ID3D10Buffer* b = ((VEGAD3d10ShaderProgram*)m_shaderProgram)->getVertConstBuffer();
		m_d3dDevice->VSSetConstantBuffers(0, 1, &b);
		b = ((VEGAD3d10ShaderProgram*)m_shaderProgram)->getPixConstBuffer();
		m_d3dDevice->PSSetConstantBuffers(0, 1, &b);
	}

	if (m_currentPrimitive != type)
	{
		switch (type)
		{
		case PRIMITIVE_TRIANGLE_STRIP:
			m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			break;
		case PRIMITIVE_TRIANGLE:
			m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			break;
		case PRIMITIVE_POINT:
			m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
			break;
		case PRIMITIVE_LINE:
			m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
			break;
		case PRIMITIVE_LINE_LOOP:
			{
				m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
				const VEGA3dBuffer *tmpBuffer = NULL;
				VEGAD3d10Buffer *ibuf = static_cast<VEGAD3d10Buffer *>(ibuffer);
				RETURN_IF_ERROR(m_compatBuffer.GetIndexBuffer(this, tmpBuffer, VEGA3dCompatIndexBuffer::LINE_LOOP, numInd, numInd, (const unsigned char*)ibuf->GetUnalignedBuffer(), bytesPerInd, firstInd));
				m_d3dDevice->IASetIndexBuffer(((VEGAD3d10Buffer*)tmpBuffer)->getBuffer(), DXGI_FORMAT_R16_UINT, 0);
				m_d3dDevice->DrawIndexed(numInd, 0, 0);
				m_currentPrimitive = NUM_PRIMITIVES;
				return OpStatus::OK;
			}
			break;
		case PRIMITIVE_LINE_STRIP:
			m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
			break;
		case PRIMITIVE_TRIANGLE_FAN:
			{
				m_d3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				const VEGA3dBuffer *tmpBuffer = NULL;
				VEGAD3d10Buffer *ibuf = static_cast<VEGAD3d10Buffer *>(ibuffer);
				RETURN_IF_ERROR(m_compatBuffer.GetIndexBuffer(this, tmpBuffer, VEGA3dCompatIndexBuffer::TRI_FAN, (numInd - 2) * 3, numInd, (const unsigned char *)ibuf->GetUnalignedBuffer(), bytesPerInd, firstInd));
				m_d3dDevice->IASetIndexBuffer(((VEGAD3d10Buffer*)tmpBuffer)->getBuffer(), DXGI_FORMAT_R16_UINT, 0);
				m_d3dDevice->DrawIndexed(numInd, 0, 0);
				m_currentPrimitive = NUM_PRIMITIVES;
				return OpStatus::OK;
			}
			break;
		};

		m_currentPrimitive = type;
	}

	bool byteToShort = false;
	DXGI_FORMAT fmt;
	switch (bytesPerInd)
	{
	case 1:
		byteToShort = true; // Fall-through, use 16-bit format.
	case 2:
		fmt = DXGI_FORMAT_R16_UINT;
		break;
	case 4:
		fmt = DXGI_FORMAT_R32_UINT;
		break;
	default:
		return OpStatus::ERR;
	}
	VEGAD3d10Buffer *d3d10ibuffer = static_cast<VEGAD3d10Buffer*>(ibuffer);
	d3d10ibuffer->Sync(byteToShort);
	m_d3dDevice->IASetIndexBuffer(d3d10ibuffer->getBuffer(), fmt, 0);
	m_d3dDevice->DrawIndexed(numInd, firstInd, 0);

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Device::createTexture(VEGA3dTexture** texture, unsigned int width, unsigned int height, VEGA3dTexture::ColorFormat fmt, bool mipmaps, bool as_rendertarget)
{
	// FIXME: assumes non power of two textures
	if (width > this->getMaxTextureSize() || height > this->getMaxTextureSize())
		return OpStatus::ERR;
	VEGAD3d10Texture* tex = OP_NEW(VEGAD3d10Texture, (width, height, fmt, m_featureLevel < D3D10_FEATURE_LEVEL_10_0));
	if (!tex)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS err = tex->Construct(m_d3dDevice, mipmaps);
	if (OpStatus::IsError(err))
	{
		VEGARefCount::DecRef(tex);
		return err;
	}
	*texture = tex;
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Device::createCubeTexture(VEGA3dTexture** texture, unsigned int size, VEGA3dTexture::ColorFormat fmt, bool mipmaps, bool as_rendertarget)
{
	// FIXME: assumes non power of two textures
	if (size > this->getMaxCubeMapTextureSize())
		return OpStatus::ERR;
	VEGAD3d10Texture* tex = OP_NEW(VEGAD3d10Texture, (size, fmt, m_featureLevel < D3D10_FEATURE_LEVEL_10_0));
	if (!tex)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS err = tex->Construct(m_d3dDevice, mipmaps);
	if (OpStatus::IsError(err))
	{
		VEGARefCount::DecRef(tex);
		return err;
	}
	*texture = tex;
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Device::createRenderbuffer(VEGA3dRenderbufferObject** rbo, unsigned int width, unsigned int height, VEGA3dRenderbufferObject::ColorFormat fmt, int multisampleQuality)
{
	if (multisampleQuality > D3D10_MAX_MULTISAMPLE_SAMPLE_COUNT)
		multisampleQuality = D3D10_MAX_MULTISAMPLE_SAMPLE_COUNT;
	VEGAD3d10RenderbufferObject* rb = OP_NEW(VEGAD3d10RenderbufferObject, (width, height, fmt, multisampleQuality));
	if (!rb)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS status = rb->Construct(m_d3dDevice);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(rb);
		return status;
	}
	*rbo = rb;
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Device::resolveMultisample(VEGA3dRenderTarget* rt, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	if (!m_renderTarget || !rt)
		return OpStatus::ERR;
	ID3D10Resource* srcTex = NULL;
	ID3D10Resource* dstTex = NULL;
	if (rt->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW)
		dstTex = static_cast<VEGAD3d10Window*>(rt)->getRTTexture();
	else
		dstTex = static_cast<VEGAD3d10FramebufferObject*>(rt)->getColorResource();
	if (m_renderTarget->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW)
		srcTex = static_cast<VEGAD3d10Window*>(m_renderTarget)->getRTTexture();
	else
		srcTex = static_cast<VEGAD3d10FramebufferObject*>(m_renderTarget)->getColorResource();
	m_d3dDevice->ResolveSubresource(dstTex, D3D10CalcSubresource(0, 0, 1), 
		srcTex, D3D10CalcSubresource(0, 0, 1), DXGI_FORMAT_B8G8R8A8_UNORM);
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Device::createFramebuffer(VEGA3dFramebufferObject** fbo)
{
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	VEGAD3d10FramebufferObject* fb = OP_NEW(VEGAD3d10FramebufferObject, (m_d3dDevice, m_d2dFactory, m_featureLevel < D3D10_FEATURE_LEVEL_10_0 ? D2D1_FEATURE_LEVEL_9 : D2D1_FEATURE_LEVEL_10, m_textMode, m_featureLevel < D3D10_FEATURE_LEVEL_10_0));
#else
	VEGAD3d10FramebufferObject* fb = OP_NEW(VEGAD3d10FramebufferObject, (m_d3dDevice, m_featureLevel < D3D10_FEATURE_LEVEL_10_0));
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
	if (!fb)
		return OpStatus::ERR_NO_MEMORY;
	*fbo = fb;
	return OpStatus::OK;
}

#ifdef VEGA_3DDEVICE
OP_STATUS VEGAD3d10Device::createWindow(VEGA3dWindow** win, VEGAWindow* nativeWin)
{
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	VEGAD3d10Window* w = OP_NEW(VEGAD3d10Window, (nativeWin, m_d3dDevice, m_dxgiFactory, m_d2dFactory, m_featureLevel < D3D10_FEATURE_LEVEL_10_0 ? D2D1_FEATURE_LEVEL_9 : D2D1_FEATURE_LEVEL_10, m_textMode));
#else
	VEGAD3d10Window* w = OP_NEW(VEGAD3d10Window, (nativeWin, m_d3dDevice, m_dxgiFactory));
#endif
	if (!w)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS err = w->Construct();
	if (OpStatus::IsError(err))
	{
		OP_DELETE(w);
		return err;
	}
	*win = w;

	return OpStatus::OK;
}
#endif // VEGA_3DDEVICE

OP_STATUS VEGAD3d10Device::createBuffer(VEGA3dBuffer** buffer, unsigned int size, const VEGA3dBuffer::Usage usage, bool indexBuffer, bool deferredUpdate)
{
	VEGAD3d10Buffer* buf = OP_NEW(VEGAD3d10Buffer, ());
	if (!buf)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err = buf->Construct(m_d3dDevice, size, usage, indexBuffer, deferredUpdate);
	if (OpStatus::IsError(err))
	{
		VEGARefCount::DecRef(buf);
		return err;
	}

	*buffer = buf;
	return OpStatus::OK;
}
OP_STATUS VEGAD3d10Device::createVertexLayout(VEGA3dVertexLayout** layout, VEGA3dShaderProgram* sprog)
{
	VEGAD3d10VertexLayout* l = OP_NEW(VEGAD3d10VertexLayout, (sprog));
	if (!l)
		return OpStatus::ERR_NO_MEMORY;
	*layout = l;
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10Device::setRenderTarget(VEGA3dRenderTarget* rt, bool changeViewport)
{
	if (m_renderTarget == rt)
		return OpStatus::OK;

	ID3D10RenderTargetView* renderView = NULL;
	ID3D10DepthStencilView* depthStencilView = NULL;
	VEGAD3d10FramebufferObject* rtfbo = NULL;
	if (rt && rt->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW)
	{
		renderView = static_cast<VEGAD3d10Window*>(rt)->getRTView();
	}
	else if (rt)
	{
		rtfbo = static_cast<VEGAD3d10FramebufferObject*>(rt);
		// Unbind since a resource can't be render target and texture at the same time
		VEGA3dTexture* rttex = rtfbo->getAttachedColorTexture();
		for (unsigned int tu = 0; tu < d3d10ActiveTextureCount; ++tu)
		{
			if (rttex && rttex == m_activeTextures[tu])
			{
				ID3D10ShaderResourceView* rsv = m_emptySRV;
				m_d3dDevice->PSSetShaderResources(tu, 1, &rsv);
			}
		}
		renderView = rtfbo->getRenderTargetView();
		depthStencilView = rtfbo->getDepthStencilView();
	}
	m_d3dDevice->OMSetRenderTargets(1, &renderView, depthStencilView);

	if (rt && changeViewport)
	{
		D3D10_VIEWPORT vp;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		vp.Width = rt->getWidth();
		vp.Height = rt->getHeight();
		vp.MinDepth = 0.f;
		vp.MaxDepth = 1.f;
		m_d3dDevice->RSSetViewports(1, &vp);
	}

	if (m_renderTarget && m_renderTarget->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
	{
		VEGAD3d10FramebufferObject* oldrtfbo = static_cast<VEGAD3d10FramebufferObject*>(m_renderTarget);
		// Re-bind the texture since it is no longer bound as render target
		VEGA3dTexture* rttex = oldrtfbo->getAttachedColorTexture();
		if (rtfbo && rtfbo->getAttachedColorTexture() == rttex)
			rttex = NULL;
		for (unsigned int tu = 0; tu < d3d10ActiveTextureCount; ++tu)
		{
			if (rttex && rttex == m_activeTextures[tu])
			{
				ID3D10ShaderResourceView* rsv = ((VEGAD3d10Texture*)rttex)->getSRView();
				m_d3dDevice->PSSetShaderResources(tu, 1, &rsv);
			}
		}
	}

	m_renderTarget = rt;
	return OpStatus::OK;
}
VEGA3dRenderTarget* VEGAD3d10Device::getRenderTarget()
{
	return m_renderTarget;
}

void VEGAD3d10Device::setViewport(int viewportX, int viewportY, int viewportWidth, int viewportHeight, float viewportNear, float viewportFar)
{
	D3D10_VIEWPORT vp;
	vp.TopLeftX = viewportX;
	vp.TopLeftY = viewportY;
	vp.Width = viewportWidth;
	vp.Height = viewportHeight;
	vp.MinDepth = viewportNear;
	vp.MaxDepth = viewportFar;
	m_d3dDevice->RSSetViewports(1, &vp);
}

void VEGAD3d10Device::loadOrthogonalProjection(VEGATransform3d& transform)
{
	OP_ASSERT(m_renderTarget);
	loadOrthogonalProjection(transform, m_renderTarget->getWidth(), m_renderTarget->getHeight());
}

void VEGAD3d10Device::loadOrthogonalProjection(VEGATransform3d& transform, unsigned int w, unsigned int h)
{
		VEGA_FIX left = 0;
		VEGA_FIX right = VEGA_INTTOFIX(w);
		VEGA_FIX top = 0;
		VEGA_FIX bottom = VEGA_INTTOFIX(h);
		VEGA_FIX znear = VEGA_INTTOFIX(-1);
		VEGA_FIX zfar = VEGA_INTTOFIX(1);

		transform.loadIdentity();

		transform[0] = 2.f/(right-left); // 2/(r-l)
		transform[5] = 2.f/(top-bottom); // 2/(t-b)
		transform[10] = 1.f/(zfar-znear); // 1/(zf-zn) or 1/(zn-zf) for right handed
		transform[3] = (left+right)/(left-right); // (l+r)/(l-r)
		transform[7] = (top+bottom)/(bottom-top); // (t+b)/(b-t)
		transform[11] = znear/(znear-zfar); // zn/(zn-zf)
}

OP_STATUS VEGAD3d10Device::lockRect(FramebufferData* data, bool readPixels)
{
	UINT32* buffer = OP_NEWA(UINT32, data->w*data->h);
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoArray<UINT32> _buffer(buffer);
	if (readPixels)
	{
		D3D10_MAPPED_TEXTURE2D resource;
		ID3D10Texture2D* temptex;
		D3D10_TEXTURE2D_DESC texDesc;
		op_memset(&texDesc, 0, sizeof(D3D10_TEXTURE2D_DESC));
		texDesc.Width = data->w;
		texDesc.Height = data->h;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D10_USAGE_STAGING;
		texDesc.BindFlags = 0;
		texDesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
		texDesc.MiscFlags = 0;
		texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

		if (m_renderTarget->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
		{
			D3D10_TEXTURE2D_DESC desc;
			static_cast<VEGAD3d10FramebufferObject*>(m_renderTarget)->getColorResource()->GetDesc(&desc);
			texDesc.Format = desc.Format;
		}

		if (FAILED(m_d3dDevice->CreateTexture2D(&texDesc, NULL, &temptex)))
			return OpStatus::ERR;
		D3D10_BOX box;
		box.left = data->x;
		box.right = data->x+data->w;
		box.top = data->y;
		box.bottom = data->y+data->h;
		box.front = 0;
		box.back = 1;
		if (m_renderTarget->getWidth() != 0 && m_renderTarget->getHeight() != 0 && data->w != 0 && data->h != 0)
		{
			if (m_renderTarget->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW)
				m_d3dDevice->CopySubresourceRegion(temptex, D3D10CalcSubresource(0,0,1), 0, 0, 0, static_cast<VEGAD3d10Window*>(m_renderTarget)->getRTTexture(), D3D10CalcSubresource(0,0,1), &box);
			else
				m_d3dDevice->CopySubresourceRegion(temptex, D3D10CalcSubresource(0,0,1), 0, 0, 0, static_cast<VEGAD3d10FramebufferObject*>(m_renderTarget)->getColorResource(), D3D10CalcSubresource(0,0,1), &box);
			if (FAILED(temptex->Map(D3D10CalcSubresource(0, 0, 1), D3D10_MAP_READ, 0, &resource)))
			{
				temptex->Release();
				return OpStatus::ERR;
			}

			// Check that we don't have to do any format conversion.
			OP_ASSERT(texDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || texDesc.Format == DXGI_FORMAT_B8G8R8X8_UNORM);

			// Copy the content.
			for (unsigned int yp = 0; yp < data->h; ++yp)
				op_memcpy(buffer+yp*data->w, ((UINT8*)resource.pData)+yp*resource.RowPitch, data->w*4);
		}

		temptex->Unmap(D3D10CalcSubresource(0, 0, 1));
		temptex->Release();
	}
	_buffer.release();
	data->privateData = buffer;
	data->pixels.stride = data->w*4;
	data->pixels.buffer = buffer;
	data->pixels.format = VPSF_BGRA8888;
	data->pixels.width = data->w;
	data->pixels.height = data->h;
	return OpStatus::OK;
}
void VEGAD3d10Device::unlockRect(FramebufferData* data, bool modified)
{
	if (m_renderTarget->getWidth() == 0 || m_renderTarget->getHeight() == 0 || data->w == 0 || data->h == 0)
		return;

	if (modified)
	{
		ID3D10Texture2D* temptex;
		D3D10_TEXTURE2D_DESC texDesc;
		op_memset(&texDesc, 0, sizeof(D3D10_TEXTURE2D_DESC));
		texDesc.Width = data->w;
		texDesc.Height = data->h;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D10_USAGE_IMMUTABLE;
		texDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		D3D10_SUBRESOURCE_DATA srdata;
		srdata.pSysMem = data->pixels.buffer;
		srdata.SysMemPitch = data->pixels.stride;
		srdata.SysMemSlicePitch = data->pixels.stride;
		if (SUCCEEDED(m_d3dDevice->CreateTexture2D(&texDesc, &srdata, &temptex)))
		{
			D3D10_BOX box;
			box.left = 0;
			box.right = data->w;
			box.top = 0;
			box.bottom = data->h;
			box.front = 0;
			box.back = 1;
			if (m_renderTarget->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW)
				m_d3dDevice->CopySubresourceRegion(static_cast<VEGAD3d10Window*>(m_renderTarget)->getRTTexture(), D3D10CalcSubresource(0,0,1), data->x, data->y, 0, temptex, D3D10CalcSubresource(0,0,1), &box);
			else
				m_d3dDevice->CopySubresourceRegion(static_cast<VEGAD3d10FramebufferObject*>(m_renderTarget)->getColorResource(), D3D10CalcSubresource(0,0,1), data->x, data->y, 0, temptex, D3D10CalcSubresource(0,0,1), &box);
			temptex->Release();
		}
	}
	OP_DELETEA((UINT32*)data->privateData);
}
OP_STATUS VEGAD3d10Device::copyToTexture(VEGA3dTexture* tex, VEGA3dTexture::CubeSide side, int level, int texx, int texy, int x, int y, unsigned int w, unsigned int h)
{
	if (m_renderTarget->getWidth() == 0 || m_renderTarget->getHeight() == 0 || tex->getWidth() == 0 || tex->getHeight() == 0)
		return OpStatus::OK;

	D3D10_BOX box;
	box.left = x;
	box.right = x+w;
	box.top = y;
	box.bottom = y+h;
	box.front = 0;
	box.back = 1;
	if (m_renderTarget->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW)
		m_d3dDevice->CopySubresourceRegion(((VEGAD3d10Texture*)tex)->getTextureResource(), D3D10CalcSubresource(level,VEGACubeSideToArraySlice(side),1), texx, texy, 0, static_cast<VEGAD3d10Window*>(m_renderTarget)->getRTTexture(), D3D10CalcSubresource(0,0,1), &box);
	else
		m_d3dDevice->CopySubresourceRegion(((VEGAD3d10Texture*)tex)->getTextureResource(), D3D10CalcSubresource(level,VEGACubeSideToArraySlice(side),1), texx, texy, 0, static_cast<VEGAD3d10FramebufferObject*>(m_renderTarget)->getColorResource(), D3D10CalcSubresource(0,0,1), &box);
	return OpStatus::OK;
}

# ifdef VEGA_BACKENDS_USE_BLOCKLIST
// virtual
void VEGAD3d10Device::GetHWInfo(DXGI_ADAPTER_DESC& desc, UINT32* driver_version) const
{
	OP_ASSERT(driver_version);
	driver_version[0] = driver_version[1] = driver_version[2] = driver_version[3] = 0;

	IDXGIAdapter* dxgiAdapter = NULL;

	// Get the adapter.
	if (m_driverType == D3D10_DRIVER_TYPE_WARP)
	{
		// Warp adapter.
		IDXGIDevice* dxgiDevice = NULL;
		if (FAILED(m_d3dDevice->QueryInterface(&dxgiDevice)))
			return;

		HRESULT hr = dxgiDevice->GetAdapter(&dxgiAdapter);
		dxgiDevice->Release();

		if (FAILED(hr))
			return;
	}
	else
	{
		// Hardware adapter.
		if (FAILED(m_dxgiFactory->EnumAdapters(0, &dxgiAdapter)))
			return;
	}

	// Get device and vendor id.
	if (SUCCEEDED(dxgiAdapter->GetDesc(&desc)))
	{
		// Get driver version.
		LARGE_INTEGER version;
		if (SUCCEEDED(dxgiAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version)))
		{
			driver_version[0] = HIWORD(version.HighPart);
			driver_version[1] = LOWORD(version.HighPart);
			driver_version[2] = HIWORD(version.LowPart);
			driver_version[3] = LOWORD(version.LowPart);
		}
	}

	dxgiAdapter->Release();

	return;
}

class VEGAD3d10DataProvider : public VEGABlocklistDevice::DataProvider
{
public:
	OP_STATUS GetValueForKey(const uni_char* key, OpString& val);

	VEGAD3d10DataProvider(const VEGAD3d10Device* const dev) : dev(dev), fetched_hwinfo(FALSE)
	{
		op_memset(&driver_version, 0, sizeof(UINT32) * 4);
		op_memset(&desc, 0, sizeof(DXGI_ADAPTER_DESC));
	}

	void GetHWInfo() { if (fetched_hwinfo) return; dev->GetHWInfo(desc, driver_version); fetched_hwinfo = TRUE; }
private:
	const VEGAD3d10Device* const dev;
	BOOL fetched_hwinfo;
	UINT32 driver_version[4];
	DXGI_ADAPTER_DESC desc;
};

VEGABlocklistDevice::DataProvider* VEGAD3d10Device::CreateDataProvider()
{
	return OP_NEW(VEGAD3d10DataProvider, (this));
}

OP_STATUS VEGAD3d10DataProvider::GetValueForKey(const uni_char* key, OpString& val)
{
	GetHWInfo();
	val.Empty();

	if (!uni_stricmp(key, UNI_L("vendor id")))
		return val.AppendFormat(UNI_L("0x%.4x"), desc.VendorId);

	if (!uni_stricmp(key, UNI_L("device id")))
		return val.AppendFormat(UNI_L("0x%.4x"), desc.DeviceId);

	if (!uni_stricmp(key, UNI_L("driver version")))
		return val.AppendFormat(UNI_L("%d.%d.%d.%d"), driver_version[0], driver_version[1], driver_version[2], driver_version[3]);

	if (!uni_stricmp(key, UNI_L("Dedicated Video Memory")))
		return val.AppendFormat(UNI_L("%d"), desc.DedicatedVideoMemory / (1024 * 1024));

	if (!uni_stricmp(key, UNI_L("Total Video Memory")))
		return val.AppendFormat(UNI_L("%d"), (desc.DedicatedVideoMemory + desc.DedicatedSystemMemory + desc.SharedSystemMemory) / (1024 * 1024));

	return OpStatus::ERR;
}

#include "modules/about/operagpu.h"
# include "platforms/vega_backends/vega_blocklist_file.h"
OP_STATUS VEGAD3d10Device::GenerateSpecificBackendInfo(OperaGPU* page, VEGABlocklistFile* blocklist, DataProvider* provider)
{
	RETURN_IF_ERROR(page->OpenDefinitionList());

	// vendor id
	OpString vendor_id_str;
	OpStatus::Ignore(provider->GetValueForKey(UNI_L("vendor id"), vendor_id_str));
	if (!vendor_id_str.IsEmpty())
		RETURN_IF_ERROR(page->ListEntry(UNI_L("Vendor id"), vendor_id_str));

	// device id
	OpString device_id_str;
	OpStatus::Ignore(provider->GetValueForKey(UNI_L("device id"), device_id_str));
	if (!device_id_str.IsEmpty())
		RETURN_IF_ERROR(page->ListEntry(UNI_L("Device id"), device_id_str));

	// driver version
	OpString driver_str;
	OpStatus::Ignore(provider->GetValueForKey(UNI_L("driver version"), driver_str));
	if (!driver_str.IsEmpty())
		RETURN_IF_ERROR(page->ListEntry(UNI_L("Driver version"), driver_str));

	RETURN_IF_ERROR(page->CloseDefinitionList());

	return OpStatus::OK;

}

# endif // VEGA_BACKENDS_USE_BLOCKLIST

#ifdef VEGA_ENABLE_PERF_EVENTS
// D3DPERF_ functions are defined in d3d9.h
#include <d3d9.h>
void VEGAD3d10Device::SetPerfMarker(const uni_char *marker)
{
	D3DPERF_SetMarker(D3DCOLOR_XRGB(255,0,0), marker);
}

void VEGAD3d10Device::BeginPerfEvent(const uni_char *marker)
{
	D3DPERF_BeginEvent(D3DCOLOR_XRGB(255,0,0), marker);
}

void VEGAD3d10Device::EndPerfEvent()
{
	D3DPERF_EndEvent();
}
#endif //VEGA_ENABLE_PERF_EVENTS

#endif //NO_DXSDK
#endif // VEGA_BACKEND_DIRECT3D10
