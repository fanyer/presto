/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAGLDEVICE_H
#define VEGAGLDEVICE_H

#ifdef VEGA_BACKEND_OPENGL

#include "modules/libvega/vegafixpoint.h"
#include "modules/libvega/vega3ddevice.h"

class VEGAGlAPI;
class VEGAGlBuffer;

class VEGAGlRenderbufferParameters {
public:
	VEGAGlRenderbufferParameters() :
		format(0), redBits(0), greenBits(0), blueBits(0),
		alphaBits(0), depthBits(0), stencilBits(0)
	{}

	int format;
	unsigned char redBits;
	unsigned char greenBits;
	unsigned char blueBits;
	unsigned char alphaBits;
	unsigned char depthBits;
	unsigned char stencilBits;
};

class VEGAGlDevice : public VEGA3dDevice
{
public:
	VEGAGlDevice();
	virtual ~VEGAGlDevice();
	static OP_STATUS Create(VEGA3dDevice** dev VEGA_3D_NATIVE_WIN_DECL); ///< implemented by platform
	virtual OP_STATUS Init();
	virtual void Destroy();

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	virtual OP_STATUS GenerateSpecificBackendInfo(class OperaGPU* page, VEGABlocklistFile* blocklist, DataProvider* provider);
#endif // VEGA_BACKENDS_USE_BLOCKLIST

	const uni_char* getName(){return UNI_L("GL");}
	const uni_char* getDescription() const {return UNI_L("OpenGL");}

	/** See documentation for VEGA3dDevice::SamplerModel */
	virtual SamplerModel getSamplerModel() const { return SamplerValuesAreTextureUnits; }

	/** See documentation for VEGA3dDevice::AttribModel */
	virtual AttribModel getAttribModel() const { return GLAttribModel; }

	virtual bool supportsMultisampling();
	virtual bool fullNPotSupport();

	virtual OP_STATUS Construct(VEGA_3D_BLOCKLIST_ENTRY_DECL);
	virtual void flush();

	virtual OP_STATUS createRenderState(VEGA3dRenderState** state, bool copyCurrentState);
	virtual OP_STATUS setRenderState(VEGA3dRenderState* state);
	virtual void setScissor(int x, int y, unsigned int w, unsigned int h);

	virtual void clear(bool color, bool depth, bool stencil, UINT32 colorVal, float depthVal, int stencilVal);

	virtual void setTexture(unsigned int unit, VEGA3dTexture* tex);
	virtual void setCubeTexture(unsigned int unit, VEGA3dTexture* tex);

	virtual unsigned int getMaxVertexAttribs() const;
	virtual unsigned int getVertexBufferSize() const {return m_vertexBufferSize;}

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

	void getRenderbufferParameters(int format, VEGAGlRenderbufferParameters &params);

	virtual bool getHighPrecisionFragmentSupport() const { return supportHighPrecFrag; }

	virtual VEGA3dShaderProgram::ShaderLanguage getShaderLanguage(unsigned &version);
	virtual bool supportsExtension(Extension e);

	virtual OP_STATUS createShaderProgram(VEGA3dShaderProgram** shader, VEGA3dShaderProgram::ShaderType shdtype, VEGA3dShaderProgram::WrapMode shdmode);
	virtual OP_STATUS setShaderProgram(VEGA3dShaderProgram* shader);
	virtual VEGA3dShaderProgram* getCurrentShaderProgram(){return currentShaderProgram;}
#ifdef CANVAS3D_SUPPORT
	virtual OP_STATUS createShader(VEGA3dShader** shader, BOOL fragmentShader, const char* source, unsigned int attribute_count, VEGA3dDevice::AttributeData *attributes, OpString *info_log);
#endif // CANVAS3D_SUPPORT

	void applyWrapHack();

	virtual OP_STATUS drawPrimitives(Primitive type, VEGA3dVertexLayout* layout, unsigned int firstVert, unsigned int numVerts);
	virtual OP_STATUS drawIndexedPrimitives(Primitive type, VEGA3dVertexLayout* verts, VEGA3dBuffer* ibuffer, unsigned int bytesPerInd, unsigned int firstInd, unsigned int numInd);

	virtual OP_STATUS createTexture(VEGA3dTexture** texture, unsigned int width, unsigned int height, VEGA3dTexture::ColorFormat fmt, bool mipmaps = false, bool as_rendertarget = true);
	virtual OP_STATUS createCubeTexture(VEGA3dTexture** texture, unsigned int size, VEGA3dTexture::ColorFormat fmt, bool mipmaps = false, bool as_rendertarget = true);
	virtual OP_STATUS createFramebuffer(VEGA3dFramebufferObject** fbo);
	virtual OP_STATUS createRenderbuffer(VEGA3dRenderbufferObject** rbo, unsigned int width, unsigned int height, VEGA3dRenderbufferObject::ColorFormat fmt, int multisampleQuality);
	virtual OP_STATUS resolveMultisample(VEGA3dRenderTarget* rt, unsigned int x, unsigned int y, unsigned int w, unsigned int h);
	virtual OP_STATUS createBuffer(VEGA3dBuffer** buffer, unsigned int size, VEGA3dBuffer::Usage usage, bool indexBuffer, bool deferredUpdate);
	virtual OP_STATUS createVertexLayout(VEGA3dVertexLayout** layout, VEGA3dShaderProgram* sprog);

	virtual OP_STATUS setRenderTarget(VEGA3dRenderTarget* rtex, bool changeViewport = true);
	virtual VEGA3dRenderTarget* getRenderTarget(){return renderTarget;}
	virtual void setViewport(int x, int y, int w, int h, float depthNear, float depthFar);
	virtual void loadOrthogonalProjection(VEGATransform3d& transform);
	virtual void loadOrthogonalProjection(VEGATransform3d& transform, unsigned int width, unsigned int height);

	virtual OP_STATUS lockRect(FramebufferData* data, bool readPixels);
	virtual void unlockRect(FramebufferData* data, bool modified);
	virtual OP_STATUS copyToTexture(VEGA3dTexture* tex, VEGA3dTexture::CubeSide side, int level, int texx, int texy, int x, int y, unsigned int w, unsigned int h);
	/** Activate the apropriate rendering context. */
	void clearActiveTexture(VEGA3dTexture* tex);

#ifdef VEGA_ENABLE_PERF_EVENTS
	/** Insert a marker into the render command stream for debugging and performance usage. */
	virtual void SetPerfMarker(const uni_char *marker);

	/** Insert markers to bracket an event in the render command stream for debugging and performance usage. */
	virtual void BeginPerfEvent(const uni_char *marker);
	virtual void EndPerfEvent();
#endif //VEGA_ENABLE_PERF_EVENTS

	virtual bool hasExtendedBlend();
	virtual void* getGLFunction(const char* funcName) = 0;

#ifndef VEGA_OPENGLES
	static const VEGAGlAPI * GetGlAPI() { return static_cast<VEGAGlDevice*>(g_opera->libvega_module.vega3dDevice)->m_glapi.get(); }
#endif

	/* Warning: Be careful if you change buffer bindings yourself using
	   glBindBuffer() rather than the methods below, or you might cause
	   internal state that keeps track of buffer bindings to avoid redundant
	   bind calls to become unsynchronized. */

	void bindBuffer(bool indexBuffer, UINT32 buffer);
	void deleteBuffers(bool indexBuffers, unsigned int n, const UINT32* buffers);

	void initGlStateVariables();

	bool fboFormatsTested;
	bool supportRGBA4_fbo;
	bool supportRGB5_A1_fbo;
	bool supportHighPrecFrag;
#ifdef VEGA_OPENGLES
	bool supportStencilAttachment;
	bool supportPackedDepthStencil() {return m_extensions.OES_packed_depth_stencil;}
#endif //VEGA_OPENGLES

#ifndef VEGA_OPENGLES
	OpAutoPtr<VEGAGlAPI> m_glapi;
	OP_STATUS InitAPI();
#endif

	virtual UINT32 convertToNativeColor(UINT32 color){return (color&0xff00ff00)|((color<<16)&0xff0000)|((color>>16)&0xff);}
	OP_STATUS testFBOFormats();
	void applyRenderState();

	VEGA3dRenderTarget* renderTarget;

	VEGA3dShaderProgram* cachedShaderPrograms[VEGA3dShaderProgram::NUM_ACTUAL_SHADERS];
	bool cachedShaderProgramsPresent[VEGA3dShaderProgram::NUM_ACTUAL_SHADERS];
	VEGA3dShaderProgram* currentShaderProgram;

	VEGA3dVertexLayout* currentLayout;

	unsigned int activeTextureNum;
	VEGA3dTexture** activeTextures;
	VEGA3dTexture** activeCubeTextures;
	Head textures;
	Head buffers;
	Head shaderPrograms;
	Head framebuffers;
	Head renderbuffers;

	VEGA3dRenderState renderState;
	VEGA3dRenderState* clientRenderState;
	int m_scissorX;
	int m_scissorY;
	unsigned int m_scissorW;
	unsigned int m_scissorH;
	int m_viewportX;
	int m_viewportY;
	int m_viewportW;
	int m_viewportH;
	float m_depthNear;
	float m_depthFar;
	UINT32 m_clearColor;
	float m_clearDepth;
	int m_clearStencil;

	int m_maxMSAA;
	unsigned int m_vertexBufferSize;

	struct GlStateVariables {
		GlStateVariables():
			maxVertexAttribs(0), maxCubeMapTextureSize(0),
			maxFragmentUniformVectors(0), maxRenderBufferSize(0),
			maxTextureSize(0), maxVaryingVectors(0),
			maxVertexTextureUnits(0), maxFragmentTextureUnits(0),
			maxCombinedTextureUnits(0), maxVertexUniformVectors(0),
			subpixelBits(0)
		{
			aliasedPointSizeRange[0] = 0.f;
			aliasedPointSizeRange[1] = 0.f;
			aliasedLineWidthRange[0] = 0.f;
			aliasedLineWidthRange[1] = 0.f;
			maxViewportDims[0] = 0;
			maxViewportDims[1] = 0;
			boundBuffer[0] = 0;
			boundBuffer[1] = 0;
		}

		unsigned int maxVertexAttribs;
		unsigned int maxCubeMapTextureSize;
		unsigned int maxFragmentUniformVectors;
		unsigned int maxRenderBufferSize;
		unsigned int maxTextureSize;
		unsigned int maxMipLevel;
		unsigned int maxVaryingVectors;
		unsigned int maxVertexTextureUnits;
		unsigned int maxFragmentTextureUnits;
		unsigned int maxCombinedTextureUnits;
		unsigned int maxVertexUniformVectors;
		unsigned int subpixelBits;
		float aliasedPointSizeRange[2];
		float aliasedLineWidthRange[2];
		int maxViewportDims[2];
		// 0: GL_ARRAY_BUFFER
		// 1: GL_ELEMENT_ARRAY_BUFFER
		unsigned int boundBuffer[2];
	} stateVariables;

	VEGAGlRenderbufferParameters renderbufferParameters;

	// bit-mask for extension support. lower MAX_ESTENSIONS bits is
	// used as booleans and store whether an extension is
	// supported. the next MAX_ESTENSIONS bits store whether support
	// has been checked.
	unsigned int m_cachedExtensions;

#ifndef VEGA_OPENGLES
	/* A small dummy VBO. Could be empty if GFX drivers were decently written,
	   but as some drivers barf on empty buffers being bound to generic vertex
	   attributes, it contains a small amount of data (N_EMPTY_BUF_DUMMY_BYTES
	   bytes, each set to EMPTY_BUF_DUMMY_VALUE). */
	VEGAGlBuffer *dummyVBO;
#endif // VEGA_OPENGLES

	// compile-time assertion to check that MAX_ESTENSIONS is not too big
	unsigned __cachedExtensionsCompileTimeAssert()
	{
		const unsigned __dummy[8*sizeof(m_cachedExtensions) >= 2*MAX_EXTENSIONS] = { 4711 };
		return __dummy[0];
	}
#ifdef VEGA_BACKENDS_OPENGL_FLIPY
	bool m_flipY;
	int m_backupScissorY;
#endif // VEGA_BACKENDS_OPENGL_FLIPY


#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	// See UpdateQuirks() for mapping between enums and blocklist strings.
	enum Quirk
	{
		QUIRK_DONT_DETACH_SHADERS,

	    NUM_QUIRKS
	};

	bool isQuirkEnabled(Quirk quirk)
	{
		return (m_quirks[quirk / 32] & (1 << (quirk % 32))) != 0;
	}
#endif // VEGA_BACKENDS_USE_BLOCKLIST

private:
#ifndef VEGA_OPENGLES
	enum
	{
		/* This size is somewhat arbitrary. We could probably get away
		   with 16 bytes (4*sizeof(float)), but put in some extra headroom just
		   to be safe. */
		N_EMPTY_BUF_DUMMY_BYTES = 64,
		/* Put something a bit more recognizable than 0 in the buffer just in
		   case */
		EMPTY_BUF_DUMMY_VALUE = 0xAB
	};
#endif // VEGA_OPENGLES

#ifdef VEGA_OPENGLES
	// Check if there is support for high precision in the fragment shader by
	// compiling and linking a small test.
	virtual bool checkHighPrecisionFragmentSupport() const;
#endif //VEGA_OPENGLES

	struct GlExtensions {
		GlExtensions():
			IMG_read_format(false),
			EXT_texture_format_BGRA8888(false),
			OES_packed_depth_stencil(false)
		{}

		bool IMG_read_format;
		bool EXT_texture_format_BGRA8888;
		bool OES_packed_depth_stencil;
	} m_extensions;

	// Initialize OpenGL specific extensions.
	void initGlExtensions();

	// Get the optimal read back format for the currently bound render target.
	void getOptimalReadFormat(VEGAPixelStoreFormat* targetFormat, int* format, int* type,  int* bytesPerPixel);

	// Convert the given VEGA pixel store format to an OpenGL texture format.
	// If the format is not supported by the GL implementation, the function
	// returns OpStatus::Err.
	OP_STATUS pixelStoreFormatToGLFormat(VEGAPixelStoreFormat targetFormat, int* format, int* type);

#ifdef VEGA_OPENGLES
	// Called from testFBOFormats to check if stencil attachment
	// (without depth) is supported.
	void testStencilAttachment();
#endif // VEGA_OPENGLES

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	void updateQuirks(const OpStringHashTable<uni_char> &entries);

	UINT32 m_quirks[(NUM_QUIRKS + 31) / 32];
#endif // VEGA_BACKENDS_USE_BLOCKLIST
};

#endif // VEGA_BACKEND_OPENGL
#endif // VEGAGLDEVICE_H

