/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAD3D10DEVICE_H
#define VEGAD3D10DEVICE_H

#ifdef VEGA_BACKEND_DIRECT3D10

/* The following bits are set/unset in id's returned by
   VEGAd3d10ShaderProgram::getConstantLocation() to indicate properties of the
   uniform. */

// If set, the uniform is from a vertex shader. Otherwise, it is from a pixel
// shader.
const UINT32 FROM_VSHADER_BIT = 1 << 30;

#include "modules/libvega/vega3ddevice.h"
#include "modules/libvega/src/3ddevice/vega3dcompatindexbuffer.h"
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
#include <d2d1.h>
#endif
#ifdef CANVAS3D_SUPPORT
#include <D3Dcompiler.h>
#ifndef NO_DXSDK
typedef HRESULT (WINAPI *VEGAD3D10_pD3DReflectShader)
	(CONST void *pShaderBytecode,
	 SIZE_T ByteCodeLength,
	 __out ID3D10ShaderReflection **ppReflector);
#endif //NO_DXSDK
#endif // CANVAS3D_SUPPORT

#ifndef NO_DXSDK

class VEGAD3d10Texture;

static const size_t d3d10ActiveTextureCount = D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT;

class VEGAD3d10Device : public VEGA3dDevice
{
public:
	VEGAD3d10Device(
#ifdef VEGA_3DDEVICE
		 VEGAWindow* nativeWin, D3D10_DRIVER_TYPE driverType = D3D10_DRIVER_TYPE_HARDWARE
#endif // VEGA_3DDEVICE
		);
	virtual ~VEGAD3d10Device();
	static OP_STATUS Create(VEGA3dDevice** dev VEGA_3D_NATIVE_WIN_DECL, D3D10_DRIVER_TYPE driverType = D3D10_DRIVER_TYPE_HARDWARE);
	OP_STATUS Init();
	OP_STATUS Construct(VEGA_3D_BLOCKLIST_ENTRY_DECL);

	virtual bool fullNPotSupport() { return m_featureLevel >= D3D10_FEATURE_LEVEL_10_0; }

	const uni_char* getName(){return m_driverType == D3D10_DRIVER_TYPE_HARDWARE ? UNI_L("DX") : UNI_L("WR");}
	const uni_char* getDescription() const {return m_description.CStr();}
	virtual unsigned int getDeviceType() const;

	/** See documentation for VEGA3dDevice::SamplerModel */
	virtual SamplerModel getSamplerModel() const { return SamplersAreTextureUnits; }

	/** See documentation for VEGA3dDevice::AttribModel */
	virtual AttribModel getAttribModel() const { return D3DAttribModel; }

	virtual bool supportsMultisampling(){return true;}

	virtual void flush();

	virtual OP_STATUS createRenderState(VEGA3dRenderState** state, bool copyCurrentState);
	virtual OP_STATUS setRenderState(VEGA3dRenderState* state);
	// d3d10 always has the extended blend modes
	virtual bool hasExtendedBlend(){return true;}
	virtual void setScissor(int x, int y, unsigned int w, unsigned int h);

	virtual void clear(bool color, bool depth, bool stencil, UINT32 colorVal, float depthVal, int stencilVal);

	virtual void setTexture(unsigned int unit, VEGA3dTexture* tex);
	virtual void setCubeTexture(unsigned int unit, VEGA3dTexture* tex);

	virtual unsigned int getMaxVertexAttribs() const;

	virtual void getAliasedPointSizeRange(float &low, float &high);
	virtual void getAliasedLineWidthRange(float &low, float &high);
	virtual unsigned int getMaxCubeMapTextureSize() const;
	virtual unsigned int getMaxFragmentUniformVectors() const;
	virtual unsigned int getMaxRenderBufferSize() const;
	virtual unsigned int getMaxTextureSize() const;
	virtual unsigned int getMaxMipLevel() const;
	virtual unsigned int getMaxVaryingVectors() const;
	virtual unsigned int getMaxVertexTextureUnits() const;
	virtual unsigned int getMaxFragmentTextureUnits() const;
	virtual unsigned int getMaxCombinedTextureUnits() const;
	virtual unsigned int getMaxVertexUniformVectors() const;
	virtual void getMaxViewportDims(int &w, int &h) const;
	virtual unsigned int getMaxQuality() const;
	virtual unsigned int getSubpixelBits() const;

	virtual VEGA3dShaderProgram::ShaderLanguage getShaderLanguage(unsigned &version) { return VEGA3dShaderProgram::SHADER_HLSL_10; }

	virtual OP_STATUS createShaderProgram(VEGA3dShaderProgram** shader, VEGA3dShaderProgram::ShaderType shdtype, VEGA3dShaderProgram::WrapMode shdmode);
	virtual OP_STATUS setShaderProgram(VEGA3dShaderProgram* shader);
	virtual VEGA3dShaderProgram* getCurrentShaderProgram();

#ifdef CANVAS3D_SUPPORT
	virtual OP_STATUS createShader(VEGA3dShader** shader, BOOL fragmentShader, const char* source, unsigned int attribute_count, VEGA3dDevice::AttributeData *attributes, OpString* info_log);
#endif // CANVAS3D_SUPPORT

	virtual OP_STATUS drawPrimitives(Primitive type, VEGA3dVertexLayout* layout, unsigned int firstVert, unsigned int numVerts);
	virtual OP_STATUS drawIndexedPrimitives(Primitive type, VEGA3dVertexLayout* verts, VEGA3dBuffer* ibuffer, unsigned int bytesPerInd, unsigned int firstInd, unsigned int numInd);

	virtual OP_STATUS createTexture(VEGA3dTexture** texture, unsigned int width, unsigned int height, VEGA3dTexture::ColorFormat fmt, bool mipmaps = false, bool as_rendertarget = true);
	virtual OP_STATUS createCubeTexture(VEGA3dTexture** texture, unsigned int size, VEGA3dTexture::ColorFormat fmt, bool mipmaps = false, bool as_rendertarget = true);
	virtual OP_STATUS createFramebuffer(VEGA3dFramebufferObject** fbo);
	virtual OP_STATUS createRenderbuffer(VEGA3dRenderbufferObject** rbo, unsigned int width, unsigned int height, VEGA3dRenderbufferObject::ColorFormat fmt, int multisampleQuality);
	virtual OP_STATUS resolveMultisample(VEGA3dRenderTarget* rt, unsigned int x, unsigned int y, unsigned int w, unsigned int h);
	
#ifdef VEGA_3DDEVICE
	virtual OP_STATUS createWindow(VEGA3dWindow** win, VEGAWindow* nativeWin);
#endif // VEGA_3DDEVICE

	virtual OP_STATUS createBuffer(VEGA3dBuffer** buffer, unsigned int size, VEGA3dBuffer::Usage usage, bool indexBuffer, bool deferredUpdate);
	virtual OP_STATUS createVertexLayout(VEGA3dVertexLayout** layout, VEGA3dShaderProgram* sprog);

	virtual OP_STATUS setRenderTarget(VEGA3dRenderTarget* rt, bool changeViewport = true);
	virtual VEGA3dRenderTarget* getRenderTarget();
	virtual void setViewport(int viewportX, int viewportY, int viewportWidth, int viewportHeight, float viewportNear, float viewportFar);
	virtual void loadOrthogonalProjection(VEGATransform3d& transform);
	virtual void loadOrthogonalProjection(VEGATransform3d& transform, unsigned int w, unsigned int h);

	virtual OP_STATUS lockRect(FramebufferData* data, bool readPixels);
	virtual void unlockRect(FramebufferData* data, bool modified);
	virtual OP_STATUS copyToTexture(VEGA3dTexture* tex, VEGA3dTexture::CubeSide side, int level, int texx, int texy, int x, int y, unsigned int w, unsigned int h);

	virtual UINT32 convertToNativeColor(UINT32 color){return (color&0xff00ff00)|((color<<16)&0xff0000)|((color>>16)&0xff);}

	void unbindTexture(VEGA3dTexture* tex);


#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	DataProvider* CreateDataProvider();
	virtual BlocklistType GetBlocklistType() const { return D3d10; }
	virtual void GetHWInfo(DXGI_ADAPTER_DESC& desc, UINT32* driver_version) const;
	OP_STATUS GenerateSpecificBackendInfo(OperaGPU* page, VEGABlocklistFile* blocklist, DataProvider* provider);
#endif // VEGA_BACKENDS_USE_BLOCKLIST

#ifdef VEGA_ENABLE_PERF_EVENTS
	/** Insert a marker into the render command stream for debugging and performance usage. */
	virtual void SetPerfMarker(const uni_char *marker);

	/** Insert markers to bracket an event in the render command stream for debugging and performance usage. */
	virtual void BeginPerfEvent(const uni_char *marker);
	virtual void EndPerfEvent();
#endif //VEGA_ENABLE_PERF_EVENTS

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	/** Set the text rendering mode to Aliased, Grayscale or ClearType **/
	void SetTextMode(D2D1_TEXT_ANTIALIAS_MODE textMode) { m_textMode = textMode; }
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY

protected:
	OP_STATUS applyRenderState();

#ifdef VEGA_3DDEVICE
	VEGAWindow* m_nativeWindow;
#endif // VEGA_3DDEVICE

	VEGA3dRenderState m_renderState;
	VEGA3dRenderState* m_currentRenderState;
	int m_scissorX;
	int m_scissorY;
	unsigned int m_scissorW;
	unsigned int m_scissorH;
	VEGA3dRenderTarget* m_renderTarget;
	VEGA3dShaderProgram* m_shaderProgram;
	unsigned int m_shaderProgramLinkId;

	VEGA3dVertexLayout* m_currentVertexLayout;
	VEGA3dShaderProgram* m_currentConstantBuffers;
	Primitive m_currentPrimitive;

	ID3D10DepthStencilState* m_noStencilState;
	ID3D10BlendState1* m_blendDisabledState;

	IDXGIFactory* m_dxgiFactory;
	ID3D10Device1* m_d3dDevice;

	ID3D10RasterizerState* m_rasterState;
	ID3D10DepthStencilState* m_depthState;
	ID3D10BlendState1* m_blendState;

	ID3D10Texture2D* m_emptyTexture;
	ID3D10ShaderResourceView* m_emptySRV;

	VEGA3dTexture* m_activeTextures[d3d10ActiveTextureCount];
	ID3D10SamplerState* m_samplers[d3d10ActiveTextureCount];
	D3D10_SAMPLER_DESC m_samplerDesc[d3d10ActiveTextureCount];

	VEGA3dShaderProgram* m_shaderProgramCache[VEGA3dShaderProgram::NUM_ACTUAL_SHADERS];

	VEGA3dCompatIndexBuffer m_compatBuffer;

	D3D10_FEATURE_LEVEL1 m_featureLevel;
	D3D10_DRIVER_TYPE m_driverType;
	OpString m_description;
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	ID2D1Factory* m_d2dFactory;
	D2D1_TEXT_ANTIALIAS_MODE m_textMode;
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY
#ifdef CANVAS3D_SUPPORT
	pD3DCompile m_D3DCompileProc;
	VEGAD3D10_pD3DReflectShader m_D3DReflectShaderProc;
#endif
#ifdef VEGA_BACKEND_DYNAMIC_LIBRARY_LOADING
	OpDLL* m_dxgiDLL;
	OpDLL* m_d3dDLL;
#ifdef CANVAS3D_SUPPORT
	OpDLL* m_d3dCompilerDLL;
#endif
#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	OpDLL* m_d2dDLL;
#endif
#endif
};

#endif // NO_DXSDK
#endif // VEGA_BACKEND_DIRECT3D10
#endif // !VEGAD3D10DEVICE_H

