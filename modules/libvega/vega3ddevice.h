/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGA3DDEVICE_H
#define VEGA3DDEVICE_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafixpoint.h"
#include "modules/libvega/vegatransform.h"
#include "modules/libvega/vegapixelstore.h"
#include "modules/libvega/vegarefcount.h"

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
# include "platforms/vega_backends/vega_blocklist_device.h"
#endif // VEGA_BACKENDS_USE_BLOCKLIST

class VEGASWBuffer;
class VEGAWindow;
class OpRect;

#if defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT)

class VEGA3dContextLostListener : public Link
{
public:
	virtual ~VEGA3dContextLostListener()
	{}
	virtual void OnContextLost() = 0;
	virtual void OnContextRestored() = 0;
};

/** A 3d render target. This cannot be used directly, you should create either
 * a texture with the render target flag set or a window. */
class VEGA3dRenderTarget
{
public:
	VEGA3dRenderTarget(){}
	virtual ~VEGA3dRenderTarget(){}

	/** Render target type. Supported types are window and texture. */
	enum TargetType
	{
		VEGA3D_RT_WINDOW,
		VEGA3D_RT_TEXTURE
	};

	/** @returns the width of the render target. */
	virtual unsigned int getWidth() = 0;
	/** @returns the width of the render target. */
	virtual unsigned int getHeight() = 0;

	/** @returns the type of render target. */
	virtual TargetType getType() = 0;
#ifdef VEGA_NATIVE_FONT_SUPPORT
	virtual void flushFonts(){}
#endif
};

/** Abstract interface to a window with 3d rendering capabilities.
 * A 3dWindow is a render target which has a present function. */
class VEGA3dWindow : public VEGA3dRenderTarget
{
public:
	virtual ~VEGA3dWindow(){}

	/** Make the rendered content visible. Swap for double buffering. */
	virtual void present(const OpRect* updateRects, unsigned int numRects) = 0;

	/** Resize the backbuffer. */
	virtual OP_STATUS resizeBackbuffer(unsigned int width, unsigned int height) = 0;

	virtual TargetType getType(){return VEGA3D_RT_WINDOW;}

	/** Optional functionality to access backbuffer of this window
	 * This is used if the platform code needs to access the backbuffer data for some reason,
	 * such as updating a transparent window on windows. */

	/** Copy the backbuffer to specificed pixel store, slow but always work, should be
	 * used as fallback */
	virtual OP_STATUS readBackbuffer(VEGAPixelStore*){return OpStatus::ERR;}

	/** Get dc of backbuffer, can be very effecient since readback might be avoided, can also
	 * fail even when a valid dc handle is returned. Check the validity of pixels before use.*/
	virtual void* getBackbufferHandle(){return NULL;}
	virtual void releaseBackbufferHandle(void* handle){}
};

/** Abstract interface to a texture in the 3d hardware. */
class VEGA3dTexture : public VEGARefCount
{
protected:
	VEGA3dTexture()
	{}
	virtual ~VEGA3dTexture()
	{}
public:
	/** Color format of the render target. INTENSITY8 has only one component
	 * which is considered the r, g, b and alpha component.
	 * LUMINANCE8 also has one component which is considered r g and b
	 * component. Alpha is 1. */
	enum ColorFormat
	{
		FORMAT_RGBA8888,
		FORMAT_RGB888,
		FORMAT_RGBA4444,
		FORMAT_RGBA5551,
		FORMAT_RGB565,
		FORMAT_ALPHA8,
		FORMAT_LUMINANCE8,
		FORMAT_LUMINANCE8_ALPHA8
	};
	/** Texture wrapping mode. Used when the texture coordinates is not
	 * in the 0..1 range. */
	enum WrapMode
	{
		WRAP_REPEAT = 0,
		WRAP_REPEAT_MIRROR,
		WRAP_CLAMP_EDGE
	};

	enum FilterMode
	{
		FILTER_NEAREST = 0,
		FILTER_LINEAR,
		FILTER_NEAREST_MIPMAP_NEAREST,
		FILTER_LINEAR_MIPMAP_NEAREST,
		FILTER_NEAREST_MIPMAP_LINEAR,
		FILTER_LINEAR_MIPMAP_LINEAR
	};

	enum CubeSide
	{
		CUBE_SIDE_NONE,
		CUBE_SIDE_POS_X,
		CUBE_SIDE_NEG_X,
		CUBE_SIDE_POS_Y,
		CUBE_SIDE_NEG_Y,
		CUBE_SIDE_POS_Z,
		CUBE_SIDE_NEG_Z,
		NUM_CUBE_SIDES
	};

	enum MipMapHint
	{
		HINT_DONT_CARE,
		HINT_FASTEST,
		HINT_NICEST
	};

	/** Updates the image data in the texture with rgba data.
	 * @param x x offset of the image data
	 * @param y y offset of the image data
	 * @param pixels the actual pixel data
	 * @param rowAlignment used to make sure the size (in bytes) of each scanline is a multiple of this value
	 * @param side Side in the cube map to update or CUBE_SIDE_NONE if a regular 2d texture.
	 * @param level Mipmap level to update.
	 * @param yFlip if true, flip y-axis of source data when copying
	 * @param premultiplyAlpha if true, premultiply source data with its alpha value
	 * @returns OpStatus::OK if the image was updated, an error otherwise. */
	virtual OP_STATUS update(unsigned int x, unsigned int y, const VEGASWBuffer* pixels, int rowAlignment = 4, CubeSide side = CUBE_SIDE_NONE, int level = 0, bool yFlip = false, bool premultiplyAlpha = false) = 0;
	/** Updates the image data in the texture with intensity/luminance data.
	 * @param x x offset of the image data
	 * @param y y offset of the image data
	 * @param w width of the image data
	 * @param h height of the image data
	 * @param pixels the actual pixel data
	 * @param rowAlignment specifies how rows are aligned, 1, 2, 4 or 8 bytes.
	 * @param side Side in the cube map to update or CUBE_SIDE_NONE if a regular 2d texture.
	 * @param level Mipmap level to update.
	 * @param yFlip if true, flip y-axis of source data when copying
	 * @param premultiplyAlpha if true, premultiply source data with its alpha value
	 * @returns OpStatus::OK if the image was updated, an error otherwise. */
	virtual OP_STATUS update(unsigned int x, unsigned int y, unsigned int w, unsigned int h, UINT8* pixels, unsigned int rowAlignment = 1, CubeSide side = CUBE_SIDE_NONE, int level = 0, bool yFlip = false, bool premultiplyAlpha = false) = 0;
	/** Updates the image data in the texture with intensity/luminance data.
	 * @param x x offset of the image data
	 * @param y y offset of the image data
	 * @param w width of the image data
	 * @param h height of the image data
	 * @param pixels the actual pixel data
	 * @param rowAlignment specifies how rows are aligned, 1, 2, 4 or 8 bytes.
	 * @param side Side in the cube map to update or CUBE_SIDE_NONE if a regular 2d texture.
	 * @param level Mipmap level to update.
	 * @param yFlip if true, flip y-axis of source data when copying
	 * @param premultiplyAlpha if true, premultiply source data with its alpha value
	 * @returns OpStatus::OK if the image was updated, an error otherwise. */
	virtual OP_STATUS update(unsigned int x, unsigned int y, unsigned int w, unsigned int h, UINT16* pixels, unsigned int rowAlignment = 1, CubeSide side = CUBE_SIDE_NONE, int level = 0, bool yFlip = false, bool premultiplyAlpha = false) = 0;
	/** Generates mipmap levels for the texture.
	 * @returns OpStatus::OK if the image was updated, an error otherwise. */
	virtual OP_STATUS generateMipMaps(MipMapHint hint = HINT_DONT_CARE) = 0;

	/** @return the width of the texture. */
	virtual unsigned int getWidth() = 0;
	/** @return the height of the texture. */
	virtual unsigned int getHeight() = 0;
	/** @returns the format of the texture. */
	virtual ColorFormat getFormat() = 0;

	/** Set the wrapping mode. */
	virtual void setWrapMode(WrapMode swrap, WrapMode twrap) = 0;
	virtual WrapMode getWrapModeS() = 0;
	virtual WrapMode getWrapModeT() = 0;

	virtual OP_STATUS setFilterMode(FilterMode minFilter, FilterMode magFilter) = 0;
	virtual FilterMode getMinFilter() = 0;
	virtual FilterMode getMagFilter() = 0;

	OP_STATUS slowPathUpdate(unsigned int w, unsigned int h, void *&pixels, unsigned int rowAlignment, bool yFlip, bool premultiplyAlpha, VEGAPixelStoreFormat storeFormat, VEGAPixelStoreFormat requestedFormat);
};

class VEGA3dRenderbufferObject : public VEGARefCount
{
protected:
	VEGA3dRenderbufferObject()
	{}
	virtual ~VEGA3dRenderbufferObject()
	{}
public:
	enum ColorFormat
	{
		FORMAT_RGBA8, // RGBA8 is not supported on OpenGL ES, so be very careful about using it
		FORMAT_RGBA4,
		FORMAT_RGB565,
		FORMAT_RGB5_A1,
		FORMAT_STENCIL8,
		FORMAT_DEPTH16,
		FORMAT_DEPTH16_STENCIL8
	};

	virtual unsigned int getWidth() = 0;
	virtual unsigned int getHeight() = 0;

	virtual ColorFormat getFormat() = 0;
	virtual unsigned int getNumSamples() = 0;

	virtual unsigned int getRedBits() = 0;
	virtual unsigned int getGreenBits() = 0;
	virtual unsigned int getBlueBits() = 0;
	virtual unsigned int getAlphaBits() = 0;
	virtual unsigned int getDepthBits() = 0;
	virtual unsigned int getStencilBits() = 0;
};

class VEGA3dFramebufferObject : public VEGA3dRenderTarget, public VEGARefCount
{
protected:
	VEGA3dFramebufferObject()
	{}
	virtual ~VEGA3dFramebufferObject()
	{}
public:
	virtual TargetType getType(){return VEGA3D_RT_TEXTURE;}

	/** Attach a resource to the fbo. If attaching fails the fbo acts as if attach*(NULL) was called. */
	virtual OP_STATUS attachColor(VEGA3dTexture* color, VEGA3dTexture::CubeSide side = VEGA3dTexture::CUBE_SIDE_NONE) = 0;
	virtual OP_STATUS attachColor(VEGA3dRenderbufferObject* color) = 0;
	virtual OP_STATUS attachDepth(VEGA3dRenderbufferObject* depth) = 0;
	virtual OP_STATUS attachStencil(VEGA3dRenderbufferObject* stencil) = 0;
	virtual OP_STATUS attachDepthStencil(VEGA3dRenderbufferObject* depthStencil) = 0;
	virtual VEGA3dTexture* getAttachedColorTexture() = 0;
	virtual VEGA3dRenderbufferObject* getAttachedColorBuffer() = 0;
	virtual VEGA3dRenderbufferObject* getAttachedDepthStencilBuffer() = 0;

	virtual unsigned int getRedBits() = 0;
	virtual unsigned int getGreenBits() = 0;
	virtual unsigned int getBlueBits() = 0;
	virtual unsigned int getAlphaBits() = 0;
	virtual unsigned int getDepthBits() = 0;
	virtual unsigned int getStencilBits() = 0;
	virtual unsigned int getSubpixelBits() = 0;

	virtual unsigned int getSampleBuffers() = 0;
	virtual unsigned int getSamples() = 0;
};

/** Abstract interface for a vertex or index buffer in the 3d hardware. */
class VEGA3dBuffer : public VEGARefCount
{
protected:
	VEGA3dBuffer()
	{}
	virtual ~VEGA3dBuffer(){}

public:
	/** Describes the usage pattern of a buffer.
	 *
	 * Mostly, which usage specifier is used makes no difference to
	 * the observed behaviour of the buffer.  It only affects the
	 * performance.  However, some usage specifiers will also change
	 * the behaviour, so make sure to read the documentation of the
	 * usage specifier you intend to use carefully.
	 *
	 * - STREAM buffers are typically only used once (or a few times)
	 *   before being rewritten with new data.
	 *
	 * - STATIC buffers are typically initialized once (or
	 *   infrequently) and then used many times.
	 *
	 * - DYNAMIC buffers are "plain old memory".  The data is
	 *   typically modified and used freely.
	 *
	 * - STREAM_DISCARD buffers have the same basic usage pattern as
	 *   STREAM, but adds a behavioural change: every time any
	 *   modification is done to any part of the buffer, the data in
	 *   the rest of the buffer becomes undefined.
	 */
	enum Usage
	{
		STREAM_DISCARD,
		STREAM,
		STATIC,
		DYNAMIC
	};

	/** Modify parts of the buffer data.
	  *
	  * Any use of the buffer before calling this method will see the
	  * old data, and any use of the buffer after calling this method
	  * will see the new data.
	  *
	  * @param offset offset (in bytes) from start of buffer to first
	  * byte to be modified
	  * @param size the number of bytes of data to copy
	  * @param data pointer to the new data that will be copied into
	  * the data store
	  * @return OpStatus::OK on success, an error otherwise.
	  */
	virtual OP_STATUS writeAtOffset(unsigned int offset, unsigned int size, void* data) = 0;

	/** DEPRECATED alias for writeAtOffset(), for backwards
	 * compatibility.  Will hopefully be removed one day.
	 *
	 * TODO: Add the COREAPI_DEPRECATED() macro.  But that can wait a
	 * bit.  No need to introduce a host of new warnings for everyone
	 * immediately.
	 */
	OP_STATUS update(unsigned int offset, unsigned int size, void* data)
		{ return writeAtOffset(offset, size, data); }

	/** This method does the exact same thing as writeAtOffset(index *
	  * elm_size, elm_count * elm_size, data).  However, 'index' is
	  * chosen by this method rather than being passed in as a
	  * parameter.
	  *
	  * This difference allows this method to potentially choose more
	  * efficient mechanisms to modify the data.  For example, by
	  * choosing parts of memory that are less likely to cause
	  * synchronization stalls.
	  *
	  * Note that the offset will be returned as an "index" rather
	  * than an "offset".  That is, to find out how many bytes into
	  * the buffer the data was written, you have to multiply 'index'
	  * by 'elm_size'.
	  *
	  * The offset chosen will be aligned to the nearest multiple of
	  * 'elm_size' (in bytes).  Also, the index returned will be a
	  * multiple of 4, as this allows the data to be used with
	  * getQuadIndexBuffer().
	  *
	  * This method potentially has higher performance than
	  * writeAtOffset().  So if it is easy to use this method instead
	  * of writeAtOffset(), it is recommended to use this method.
	  *
	  * But keep in mind that this method may overwrite random parts
	  * of the buffer.  If this is something you need to write code to
	  * deal with, it is probably better to use writeAtOffset().
	  *
	  * As a corollary to that, if you use this method, chances are
	  * that the buffer you use should probably be STREAM_DISCARD.
	  *
	  * Using STREAM_DISCARD instead of STREAM is likely to give
	  * higher performance in most cases (and will never be worse),
	  * regardless of which of these methods you use.  So if your
	  * buffer is a STREAM buffer, please check if the buffer can be
	  * changed to a STREAM_DISCARD buffer instead.  (For other usage
	  * hints, the usage pattern itself may dominate the performance,
	  * so switching to STREAM_DISCARD is less likely to be a good
	  * idea.)
	  *
	  * @param elm_count the number of indices/vertices to add.
	  * @param elm_size the size of one index/vertex (in bytes).
	  * @param data pointer to the new data that will be copied into
	  * the data store
	  * @param index returns the index of the first vertex/index modified.
	  * @return OpStatus::OK on success, an error otherwise.
	  */
	virtual OP_STATUS writeAnywhere(unsigned int elm_count, unsigned int elm_size, void* data, unsigned int& index) = 0;

	/** @returns the size of the buffer in bytes. */
	virtual unsigned int getSize() = 0;
};

class VEGA3dVertexLayout : public VEGARefCount
{
protected:
	VEGA3dVertexLayout()
	{}
	virtual ~VEGA3dVertexLayout(){}

public:
	/** Type of vertex data
	 * - FLOATx is x floats
	 * - USHORTx is x 16-bit unsigned integers
	 * - SHORTx is x 16-bit signed integers
	 * - UBYTEx is x 8-bit unsigned integers
	 * - BYTEx is x 8-bit signed integers */
	enum VertexType
	{
		FLOAT4,
		FLOAT3,
		FLOAT2,
		FLOAT1,
		USHORT4,
		USHORT3,
		USHORT2,
		USHORT1,
		SHORT4,
		SHORT3,
		SHORT2,
		SHORT1,
		UBYTE4,
		UBYTE3,
		UBYTE2,
		UBYTE1,
		BYTE4,
		BYTE3,
		BYTE2,
		BYTE1,
		NO_TYPE
	};

	/** Add an attribute data source at index 'index'.

	    For GL, this is the same index that's used with vertexAttrib*() and
	    enable/disableVertexAttribArray(). For D3D10, it's the semantic index
	    that will be used in the input layout specification (e.g. "foo :
	    TEXCOORD<n>". */
	virtual OP_STATUS addComponent(VEGA3dBuffer* buffer, unsigned int index, unsigned int offset, unsigned int stride, VertexType type, bool normalize) = 0;

	/* Add a constant attribute data source at index 'index'. */
	virtual OP_STATUS addConstComponent(unsigned int index, float* values) = 0;

#ifdef CANVAS3D_SUPPORT
	/** For backends that use the D3DAttribModel model, add a binding for the
	    attribute with name 'name' in the shader. */
	virtual OP_STATUS addBinding(const char *name, VEGA3dBuffer* buffer, unsigned int offset, unsigned int stride, VertexType type, bool normalize)
	{
		OP_ASSERT(!"Not implemented");
		return OpStatus::ERR;
	}

	/* For backends that use the D3DAttribModel model, add a constant binding
	   for the attribute with name 'name' in the shader. */
	virtual OP_STATUS addConstBinding(const char *name, float* values)
	{
		OP_ASSERT(!"Not implemented");
		return OpStatus::ERR;
	}
#endif // CANVAS3D_SUPPORT
};

class VEGA3dRenderState
{
public:
	/** Blend operations used by blend functions. */
	enum BlendOp
	{
		BLEND_OP_ADD = 0,
		BLEND_OP_SUB,
		BLEND_OP_REVERSE_SUB,

		NUM_BLEND_OPS
	};
	/** Blend weight used by blend functions. */
	enum BlendWeight
	{
		BLEND_ONE = 0,
		BLEND_ZERO,
		BLEND_SRC_ALPHA,
		BLEND_ONE_MINUS_SRC_ALPHA,
		BLEND_DST_ALPHA,
		BLEND_ONE_MINUS_DST_ALPHA,

		BLEND_SRC_COLOR,
		BLEND_ONE_MINUS_SRC_COLOR,
		BLEND_DST_COLOR,
		BLEND_ONE_MINUS_DST_COLOR,

		BLEND_CONSTANT_COLOR,
		BLEND_ONE_MINUS_CONSTANT_COLOR,
		BLEND_CONSTANT_ALPHA,
		BLEND_ONE_MINUS_CONSTANT_ALPHA,

		BLEND_SRC_ALPHA_SATURATE,

		BLEND_EXTENDED_SRC1_COLOR,
		BLEND_EXTENDED_SRC1_ALPHA,
		BLEND_EXTENDED_ONE_MINUS_SRC1_COLOR,
		BLEND_EXTENDED_ONE_MINUS_SRC1_ALPHA,

		NUM_BLEND_WEIGHTS
	};
	/** Depth test functions used by the 3d device. */
	enum DepthFunc
	{
		DEPTH_NEVER = 0,
		DEPTH_EQUAL,
		DEPTH_LESS,
		DEPTH_LEQUAL,
		DEPTH_GREATER,
		DEPTH_GEQUAL,
		DEPTH_NOTEQUAL,
		DEPTH_ALWAYS,

		NUM_DEPTH_FUNCS
	};
	/** Stencil test functions used by the 3d device. */
	enum StencilFunc
	{
		STENCIL_LESS = 0,
		STENCIL_LEQUAL,
		STENCIL_GREATER,
		STENCIL_GEQUAL,
		STENCIL_EQUAL,
		STENCIL_NOTEQUAL,
		STENCIL_ALWAYS,
		STENCIL_NEVER,

		NUM_STENCIL_FUNCS
	};
	/** Stencil operations used to modify stencil data. If the operation
	 * is set to increase/decrease wrap and it is not supported the
	 * non wrapping equivalent will be used instead. */
	enum StencilOp
	{
		STENCIL_KEEP = 0,
		STENCIL_INVERT,
		STENCIL_INCR,
		STENCIL_DECR,
		STENCIL_ZERO,
		STENCIL_REPLACE,
		STENCIL_INCR_WRAP,
		STENCIL_DECR_WRAP,

		NUM_STENCIL_OPS
	};
	/** Faces to cull. */
	enum Face
	{
		FACE_BACK = 0,
		FACE_FRONT,

		NUM_FACES
	};

	VEGA3dRenderState();
	virtual ~VEGA3dRenderState();

	/** Set the current blend mode. */
	void enableBlend(bool enabled);
	bool isBlendEnabled();
	/** Set the constant blend color. */
	void setBlendColor(float r, float g, float b, float a);
	void getBlendColor(float& r, float& g, float& b, float& a);
	/** Set the blend operation to modify the blend mode. */
	void setBlendEquation(BlendOp op, BlendOp opA = NUM_BLEND_OPS);
	void getBlendEquation(BlendOp& op, BlendOp& opA);
	/** Set the blend weights to modify the blend mode. */
	void setBlendFunc(BlendWeight src, BlendWeight dst, BlendWeight srcA = NUM_BLEND_WEIGHTS, BlendWeight dstA = NUM_BLEND_WEIGHTS);
	void getBlendFunc(BlendWeight& src, BlendWeight& dst, BlendWeight& srcA, BlendWeight& dstA);

	/** Set the color mask for the r, g, b and a color planes. */
	void setColorMask(bool r, bool g, bool b, bool a);
	void getColorMask(bool& r, bool& g, bool& b, bool& a);

	/** Set the culling and enable/disable culling. */
	void enableCullFace(bool enabled);
	bool isCullFaceEnabled();
	void setCullFace(Face face);
	void getCullFace(Face& face);

	/** Set the depth test function and enable/disable depth testing. */
	void enableDepthTest(bool enabled);
	bool isDepthTestEnabled();
	void setDepthFunc(DepthFunc test);
	void getDepthFunc(DepthFunc& test);
	void enableDepthWrite(bool write);
	bool isDepthWriteEnabled();

	void enableDither(bool enable);
	bool isDitherEnabled();
	void enablePolygonOffsetFill(bool enable);
	bool isPolygonOffsetFillEnabled();
	void polygonOffset(float factor, float units);
	void enableSampleToAlphaCoverage(bool enable);
	bool isSampleToAlphaCoverageEnabled();
	void enableSampleCoverage(bool enable);
	bool isSampleCoverageEnabled();
	void sampleCoverage(bool invert, float value);
	void lineWidth(float w);
	float getLineWidth();
	float getPolygonOffsetFactor();
	float getPolygonOffsetUnits();
	bool getSampleCoverageInvert();
	float getSampleCoverageValue();

	/** Change the front face. */
	void setFrontFaceCCW(bool front);
	bool isFrontFaceCCW();

	/** Set the stencil test function and enable/disable stencil testing. */
	void enableStencil(bool enabled);
	bool isStencilEnabled();
	void setStencilFunc(StencilFunc func, StencilFunc funcBack, unsigned int reference, unsigned int mask);
	void getStencilFunc(StencilFunc& func, StencilFunc& funcBack, unsigned int& reference, unsigned int& mask);
	/** Set the stencil write mask. */
	void setStencilWriteMask(unsigned int mask);
	void getStencilWriteMask(unsigned int& mask);
	/** Set the stencil update operation. */
	void setStencilOp(StencilOp fail, StencilOp zFail, StencilOp pass, StencilOp failBack = NUM_STENCIL_OPS, StencilOp zFailBack = NUM_STENCIL_OPS, StencilOp passBack = NUM_STENCIL_OPS);
	void getStencilOp(StencilOp& fail, StencilOp& zFail, StencilOp& pass, StencilOp& failBack, StencilOp& zFailBack, StencilOp& passBack);

	/** Enable/disable stencil testing. */
	void enableScissor(bool enabled);
	bool isScissorEnabled();


	void clearChanged(){m_changed = false;}
	bool isChanged(){return m_changed;}
	bool colorMaskChanged(const VEGA3dRenderState& current);

	bool blendColorChanged(const VEGA3dRenderState& current);
	bool blendEquationChanged(const VEGA3dRenderState& current);
	bool blendFuncChanged(const VEGA3dRenderState& current);

	/** @returns true if the blend has changed compared to current.
	 * Always returns false if blend is disabled.
	 * This does _not_ include enable/disable changes. */
	bool blendChanged(const VEGA3dRenderState& current);
	/** @returns true if the blend has changed compared to current.
	 * Always returns false if blend is disabled.
	 * This does _not_ include enable/disable changes. */
	bool cullFaceChanged(const VEGA3dRenderState& current);
	/** @returns true if the depth test has changed compared to current.
	 * Always returns false if depth test is disabled.
	 * This does _not_ include enable/disable changes. */
	bool depthFuncChanged(const VEGA3dRenderState& current);
	/** @retval true if either the 'offset' or the 'units' value as set
	 * by polygonOffset() has changed compared to 'current'. */
	bool polygonOffsetChanged(const VEGA3dRenderState& current) const;
	/** @returns true if the depth test has changed compared to current.
	 * Always returns false if stencil is disabled.
	 * This does _not_ include enable/disable changes. */
	bool stencilChanged(const VEGA3dRenderState& current);

	virtual bool isImplementationSpecific(){return false;}
private:
	bool m_changed;

	bool m_blendEnabled;
	BlendOp m_blendOp;
	BlendOp m_blendOpA;
	BlendWeight m_blendSrc;
	BlendWeight m_blendDst;
	BlendWeight m_blendSrcA;
	BlendWeight m_blendDstA;
	float m_blendConstColor[4];

	bool m_colorMaskR;
	bool m_colorMaskG;
	bool m_colorMaskB;
	bool m_colorMaskA;

	bool m_cullFaceEnabled;
	Face m_cullFace;

	bool m_depthTestEnabled;
	DepthFunc m_depthTest;
	bool m_depthWriteEnabled;

	bool m_isFrontFaceCCW;

	bool m_stencilEnabled;
	StencilFunc m_stencilTest;
	StencilFunc m_stencilTestBack;
	unsigned int m_stencilReference;
	unsigned int m_stencilMask;
	unsigned int m_stencilWriteMask;
	StencilOp m_stencilOpFail;
	StencilOp m_stencilOpZFail;
	StencilOp m_stencilOpPass;
	StencilOp m_stencilOpFailBack;
	StencilOp m_stencilOpZFailBack;
	StencilOp m_stencilOpPassBack;

	bool m_scissorEnabled;

	bool m_ditherEnabled;
	bool m_polygonOffsetFillEnabled;
	float m_polygonOffsetFactor;
	float m_polygonOffsetUnits;
	bool m_sampleToAlphaCoverageEnabled;
	bool m_sampleCoverageEnabled;
	bool m_sampleCoverageInvert;
	float m_sampleCoverageValue;
	float m_lineWidth;
};

#ifdef CANVAS3D_SUPPORT
/** Vertex or fragment shader */
class VEGA3dShader
{
public:
	virtual ~VEGA3dShader() {}

	virtual BOOL isFragmentShader() = 0;
	virtual BOOL isVertexShader() = 0;
};
#endif // CANVAS3D_SUPPORT

class VEGA3dShaderProgram : public VEGARefCount
{
protected:
	VEGA3dShaderProgram() : m_orthoWidth(-1), m_orthoHeight(-1), m_worldProjLocation(-1)
	{}
	virtual ~VEGA3dShaderProgram() {}
public:
	// SHADER_<S>_<T>
	enum WrapMode
	{
		WRAP_REPEAT_REPEAT = 0,
		WRAP_REPEAT_MIRROR,
		WRAP_REPEAT_CLAMP,
		WRAP_MIRROR_REPEAT,
		WRAP_MIRROR_MIRROR,
		WRAP_MIRROR_CLAMP,
		WRAP_CLAMP_REPEAT,
		WRAP_CLAMP_MIRROR,
		WRAP_CLAMP_CLAMP,

		WRAP_MODE_COUNT // last!
	};
	static WrapMode GetWrapMode(VEGA3dTexture* tex)
	{
		return tex ? (WrapMode)(3*tex->getWrapModeS() + tex->getWrapModeT()) : WRAP_CLAMP_CLAMP;
	}

	enum ShaderType
	{
		SHADER_VECTOR2D = 0,
		// 8 extra blends
		SHADER_VECTOR2DTEXGEN,
		// 8 extra blends

		SHADER_VECTOR2DTEXGENRADIAL,
		SHADER_VECTOR2DTEXGENRADIALSIMPLE,
		SHADER_TEXT2D,
		SHADER_TEXT2DTEXGEN,
		SHADER_TEXT2DEXTBLEND,
		SHADER_TEXT2DEXTBLENDTEXGEN,
		SHADER_TEXT2D_INTERPOLATE,
		SHADER_TEXT2DTEXGEN_INTERPOLATE,
		SHADER_TEXT2DEXTBLEND_INTERPOLATE,
		SHADER_TEXT2DEXTBLENDTEXGEN_INTERPOLATE,
		SHADER_COLORMATRIX,
		SHADER_COMPONENTTRANSFER,
		SHADER_DISPLACEMENT,
		SHADER_LUMINANCE_TO_ALPHA,
		SHADER_SRGB_TO_LINEARRGB,
		SHADER_LINEARRGB_TO_SRGB,

		SHADER_MERGE_ARITHMETIC,
		SHADER_MERGE_MULTIPLY,
		SHADER_MERGE_SCREEN,
		SHADER_MERGE_DARKEN,
		SHADER_MERGE_LIGHTEN,

		SHADER_LIGHTING_DISTANTLIGHT,
		SHADER_LIGHTING_POINTLIGHT,
		SHADER_LIGHTING_SPOTLIGHT,
		SHADER_LIGHTING_MAKE_BUMP,

		SHADER_CONVOLVE_GEN_16,
		// 1 extra blend
		SHADER_CONVOLVE_GEN_16_BIAS,
		// 1 extra blend
		SHADER_CONVOLVE_GEN_25_BIAS,
		// 1 extra blend

		SHADER_MORPHOLOGY_DILATE_15,
		// 1 extra blend
		SHADER_MORPHOLOGY_ERODE_15,
		// 1 extra blend

		SHADER_CUSTOM,

		NUM_SHADERS,

		NUM_ACTUAL_SHADERS = NUM_SHADERS + 8/* blends of vector2d */ + 8/* blends of vector2dtexgen */ + 3/* blends of convolve* */ + 2/* blends of morphology* */
	};
	/** returns the shader index of the combination of type and mode, which are
	 * assumed to be a valid combination. useful eg when keeping an array of
	 * NUM_ACTUAL_SHADERS shaders.
	 * @param type the shader type
	 * @param mode the shader mode
	 * @return the index of the shader with combination of type and mode
	*/
	static size_t GetShaderIndex(ShaderType type, WrapMode mode);

	enum ShaderLanguage
	{
		SHADER_GLSL = 1,
		SHADER_GLSL_ES,
		SHADER_HLSL_9,
		SHADER_HLSL_10
	};

	virtual ShaderType getShaderType() = 0;

#ifdef CANVAS3D_SUPPORT
	/** For custom shaders. */
	virtual OP_STATUS addShader(VEGA3dShader* shader) = 0;
	virtual OP_STATUS removeShader(VEGA3dShader* shader) = 0;
	/// NOTE: may overwrite global 2k temp-buffer
	virtual OP_STATUS link(OpString *info_log) = 0;

	virtual OP_STATUS setAsRenderTarget(VEGA3dRenderTarget *target) = 0;

	virtual OP_STATUS validate() = 0;
	virtual bool isValid() = 0;

	virtual unsigned int getNumInputs() = 0;
	virtual unsigned int getNumConstants() = 0;

	// Remember to update nComponentsForType as well if you change this!
	enum ConstantType
	{
		SHDCONST_FLOAT = 0,
		SHDCONST_FLOAT2,
		SHDCONST_FLOAT3,
		SHDCONST_FLOAT4,
		SHDCONST_INT,
		SHDCONST_INT2,
		SHDCONST_INT3,
		SHDCONST_INT4,
		SHDCONST_BOOL,
		SHDCONST_BOOL2,
		SHDCONST_BOOL3,
		SHDCONST_BOOL4,
		SHDCONST_MAT2,
		SHDCONST_MAT3,
		SHDCONST_MAT4,
		SHDCONST_SAMP2D,
		SHDCONST_SAMPCUBE,
		SHDCONST_NONE,

		N_SHDCONSTS
	};

	// Maps vector and matrix ConstantType's to the number of components
	// (floats or ints) in the type. Maps sampler types to 1 and SHDCONST_NONE
	// to 0.
	static const unsigned nComponentsForType[N_SHDCONSTS];

	virtual unsigned int getInputMaxLength() = 0;
	virtual unsigned int getConstantMaxLength() = 0;

	/* Returns the names of the n attributes of the program (in some arbitrary
	   order) for idx = 0..n-1.

	   @retval OpStatus::OK if idx < n and no error occurs.
	   @retval OpStatus::ERR otherwise. */
	/// NOTE: may overwrite global 2k temp-buffer
	virtual OP_STATUS indexToAttributeName(unsigned idx, const char **name) = 0;

	/** NOTE: may overwrite global 2k temp-buffer
	 *  NOTE2: this function differs from glGetActiveUniform in that the
	 *         returned name does _NOT_ end with "[0]" for array constants. */
	virtual OP_STATUS getConstantDescription(unsigned int idx, ConstantType* type, unsigned int* size, const char** name) = 0;

	virtual OP_STATUS setInputLocation(const char* name, int idx) { OP_ASSERT(!"Not implemented. Only used for OpenGL-style attributes."); return OpStatus::ERR; }
#endif // CANVAS3D_SUPPORT
	virtual int getConstantLocation(const char* name) = 0;

	/** Change the value of a shader constant (uniform). This may only be called when the
	 * shader program is bound. */
	virtual OP_STATUS setScalar(int idx, float val) = 0;
	virtual OP_STATUS setScalar(int idx, int val) = 0;
	virtual OP_STATUS setScalar(int idx, float* val, int count = 1) = 0;
	virtual OP_STATUS setScalar(int idx, int* val, int count = 1) = 0;
	virtual OP_STATUS setVector2(int idx, float* val, int count = 1) = 0;
	virtual OP_STATUS setVector2(int idx, int* val, int count = 1) = 0;
	virtual OP_STATUS setVector3(int idx, float* val, int count = 1) = 0;
	virtual OP_STATUS setVector3(int idx, int* val, int count = 1) = 0;
	virtual OP_STATUS setVector4(int idx, float* val, int count = 1) = 0;
	virtual OP_STATUS setVector4(int idx, int* val, int count = 1) = 0;
	virtual OP_STATUS setMatrix2(int idx, float* val, int count = 1, bool transpose = false) = 0;
	virtual OP_STATUS setMatrix3(int idx, float* val, int count = 1, bool transpose = false) = 0;
	virtual OP_STATUS setMatrix4(int idx, float* val, int count = 1, bool transpose = false) = 0;

#ifdef CANVAS3D_SUPPORT
	virtual OP_STATUS getConstantValue(int idx, float* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex) = 0;
	virtual OP_STATUS getConstantValue(int idx, int* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex) = 0;
#endif // CANVAS3D_SUPPORT

	OP_STATUS setOrthogonalProjection();
	OP_STATUS setOrthogonalProjection(unsigned int width, unsigned int height);
	void invalidateOrthogonalProjection(){m_orthoWidth = -1; m_orthoHeight = -1;}
	/** Sub classes may implement this in order to signal that the
	 * last projection matrix returned by
	 * VEGA3dDevice::loadOrthogonalProjection is no longer valid.
	 * @returns true if loadOrthogonalProjection needs to be called
	 * and false otherwise. */
	virtual bool orthogonalProjectionNeedsUpdate() { return false; }

#ifdef CANVAS3D_SUPPORT
	virtual bool isAttributeActive(int /* idx */)
	{
		OP_ASSERT(!"Not implemented. Only used for VEGA3dDevice::GLAttribModel.");
		return false;
	}
#endif // CANVAS3D_SUPPORT
private:
	int m_orthoWidth;
	int m_orthoHeight;
	int m_worldProjLocation;
};

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
struct VEGABlocklistFileEntry;
# define VEGA_3D_BLOCKLIST_ENTRY_DECL VEGABlocklistFileEntry* blocklist_entry
# define VEGA_3D_BLOCKLIST_ENTRY_PASS blocklist_entry
#else  // VEGA_BACKENDS_USE_BLOCKLIST
# define VEGA_3D_BLOCKLIST_ENTRY_DECL
# define VEGA_3D_BLOCKLIST_ENTRY_PASS
#endif // VEGA_BACKENDS_USE_BLOCKLIST

/** Abstract interface to a hardware 3d device. */
class VEGA3dDevice
#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	: public VEGABlocklistDevice
#endif // VEGA_BACKENDS_USE_BLOCKLIST
{
public:
	enum UseCase
	{
		For2D, ///< create/use the device unless blocked for 2D
		For3D, ///< create/use the device unless blocked for 3D
		Force  ///< create/use the device if at all possible
	};

	/** Used in the WebGL implementation.

	    Represents how samplers in shaders map to texture units for the
	    back-end.

	    In the SamplersAreTextureUnits model, each sampler corresponds to a
	    texture unit and cannot be reassigned to a different texture unit. To
	    set a sampler in this model, we set its corresponding texture unit. D3D
	    uses this model.

	    In the SamplerValuesAreTextureUnits model, samplers are variables, and
	    the value of a sampler determines the texture unit from which the
	    texture will be taken. OpenGL uses this model.

	    This information is used e.g. when mapping the "virtual" WebGL texture
	    units onto the back-end's texture units. */
	enum SamplerModel
	{
		SamplersAreTextureUnits,
		SamplerValuesAreTextureUnits
	};

	/** Used in the WebGL implementation.

	    Represents different models for handling attributes in the back-end,
	    which influence how we do things in the front-end.

	    In the GLAttribModel model, we have a number of independent attribute
	    data sources (configured with e.g. vertexAttrib[Pointer]()) that
	    shaders bind to (using e.g. bindAttribLocation()). The natural (and
	    likely most efficient) approach here is to simply sync these data
	    sources between the front-end and the back-end.

	    In the D3DAttribModel model, we lack this level of indirection, and
	    resources are bound directly to shader inputs using input layouts. The
	    natural approach here is to simply bind each data source directly to
	    the attribute (if any) that uses it.

	    To be able to accommodate both models, we virtualize the attribute data
	    sources in the WebGL front-end and delay setting up shader inputs in
	    the way appropriate for each model until draw time. */
	enum AttribModel
	{
		GLAttribModel,
		D3DAttribModel
	};

	// Functionality associated with creation
private:
	friend class VEGA3dDeviceFactory;
	/** Called from public blend of VEGA3dDeviceFactory::Create.
	 * Takes care of blocklist status and calls Construct.
	 * @param use_case under what circumstances to create a device
	 */
	OP_STATUS ConstructDevice(UseCase use_case);
protected:
	/// Creation of VEGA3dDevice is done through VEGA3dDeviceFactory
	VEGA3dDevice();
	/** First-level init - called from private blend of of
	 * VEGA3dDeviceFactory::Create. This function should initialize
	 * the device enough so that VEGABlocklistDevice::DataProvider can
	 * do what it should.
	 */
	virtual OP_STATUS Init() = 0;
	/// Second-level init - should initialize the device completely.
	virtual OP_STATUS Construct(VEGA_3D_BLOCKLIST_ENTRY_DECL) = 0;
public:

	virtual ~VEGA3dDevice();
	/// should always be called before deleting
	virtual void Destroy(){}

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	// cludge: allow VEGABlocklistDevice to call VEGA3dDevice::ConstructDevice
	virtual OP_STATUS Construct3dDevice() { return ConstructDevice(For2D); }
	friend class VEGABlocklistDevice;
#endif // VEGA_BACKENDS_USE_BLOCKLIST
	/// for opera:gpu - generate info for all available backends
	static OP_STATUS GenerateBackendInfo(class OperaGPU* gpu);

	/** Check if the device can be used for the given use case
	 * @param use_case the desired use of the device
	 */
	OP_STATUS CheckUseCase(UseCase use_case);

	void RegisterContextLostListener(VEGA3dContextLostListener* list){list->Into(&m_contextLostListeners);}
	void UnregisterContextLostListener(VEGA3dContextLostListener* list){list->Out();}

	/** @returns a short name used to describe the renderer.
	 * For example "GL"*/
	virtual const uni_char* getName() = 0;
	/** @returns a slightly longer string used to describe this renderer.
	 * For example "OpenGL ES 2.0". */
	virtual const uni_char* getDescription() const = 0;

	/// @return the device type id of the device - see VEGA3dDeviceFactory::Create
	virtual unsigned int getDeviceType() const = 0;

	/** @return the current sampler model - see VEGA3dDevice::SamplerModel */
	virtual SamplerModel getSamplerModel() const = 0;

	/** @return the current attribute model - see VEGA3dDevice::AttribModel */
	virtual AttribModel getAttribModel() const = 0;

	/** @return true if the device is capable of creating multisampled
	 * renderbuffers. */
	virtual bool supportsMultisampling() = 0;

	/** Flush makes sure the current drawing operations are finished but they do not have to be presented. */
	virtual void flush() = 0;

	enum Primitive
	{
		PRIMITIVE_TRIANGLE = 0,
		PRIMITIVE_TRIANGLE_STRIP,
		PRIMITIVE_POINT,
		PRIMITIVE_LINE,
		PRIMITIVE_LINE_LOOP,      // NEVER use this with a DX9/10 backend as it goes to a slow case.
		PRIMITIVE_LINE_STRIP,
		PRIMITIVE_TRIANGLE_FAN,   // NEVER use this with a DX10 backend as it goes to a slow case.

		NUM_PRIMITIVES
	};

	/** Create a new render state instance to keep track of and
	 * change all rendering states. If copyCurrentState is true the
	 * initial state will be the state currently used by the renderer. */
	virtual OP_STATUS createRenderState(VEGA3dRenderState** state, bool copyCurrentState) = 0;
	/** Set a new rendering state as active. If the same state is
	 * set as active and it has not changed nothing should happen. */
	virtual OP_STATUS setRenderState(VEGA3dRenderState* state) = 0;

	/** @ Returns true if the extended blend functions are available, false otherwise. */
	virtual bool hasExtendedBlend() = 0;

	/** Set the current scissor rect. This is only used when the current state
	 * has scissor enabled. */
	virtual void setScissor(int x, int y, unsigned int w, unsigned int h) = 0;

	/** Clear the buffers.
	 * @param color clear the color buffer
	 * @param depth clear the depth buffer
	 * @param stencil clear the stencil buffer */
	virtual void clear(bool color, bool depth, bool stencil, UINT32 colorVal, float depthVal, int stencilVal) = 0;

	/** Bind a texture. binding NULL will disable texture mapping. */
	virtual void setTexture(unsigned int unit, VEGA3dTexture* tex) = 0;
	virtual void setCubeTexture(unsigned int unit, VEGA3dTexture* tex) = 0;

	/** Get the maximum number of vertex attributes that can be used. */
	virtual unsigned int getMaxVertexAttribs() const = 0;

	/** Get what size of temporary vertex buffers should be used. */
	virtual unsigned int getVertexBufferSize() const {return VEGA_TEMP_VERTEX_BUFFER_SIZE;}

	virtual void getAliasedPointSizeRange(float &low, float &high) = 0;
	virtual void getAliasedLineWidthRange(float &low, float &high) = 0;
	virtual unsigned int getMaxCubeMapTextureSize() const = 0;
	virtual unsigned int getMaxFragmentUniformVectors() const = 0;
	virtual unsigned int getMaxRenderBufferSize() const = 0;
	virtual unsigned int getMaxTextureSize() const = 0;
	virtual unsigned int getMaxMipLevel() const = 0;
	virtual unsigned int getMaxVaryingVectors() const = 0;
	virtual unsigned int getMaxVertexTextureUnits() const = 0;
	virtual unsigned int getMaxFragmentTextureUnits() const = 0;
	virtual unsigned int getMaxCombinedTextureUnits() const = 0;
	virtual unsigned int getMaxVertexUniformVectors() const = 0;
	virtual void getMaxViewportDims(int &w, int &h) const = 0;
	/// max number of samples supported by GPU (for RGBA32)
	virtual unsigned int getMaxQuality() const = 0;
	unsigned int getDefaultQuality()
	{
		const unsigned int q = getMaxQuality();
		return q <= VEGA_DEFAULT_QUALITY ? q : VEGA_DEFAULT_QUALITY;
	}

	/** Return the shader language + version the programs are expected as. */
	virtual VEGA3dShaderProgram::ShaderLanguage getShaderLanguage(unsigned int &version) = 0;

	/** A (flat) namespace of extensions that a backend might be
	    asked if it supports. */
	enum Extension
	{
		OES_STANDARD_DERIVATIVES = 0,
		OES_TEXTURE_NPOT,
		MAX_EXTENSIONS
	};

	virtual bool supportsExtension(Extension e) { return false; }

	/** Create a shader of type <shdtype>
	 * SHADER_CONVOLVE_GEN_25_BIAS will fail to initialize for DirectX 10 Level 9.1 hardware*/
	virtual OP_STATUS createShaderProgram(VEGA3dShaderProgram** shader, VEGA3dShaderProgram::ShaderType shdtype, VEGA3dShaderProgram::WrapMode shdmode) = 0;
	/** Set the shader to use. If NULL then the
	 * device will switch back to fixed-function operation. */
	virtual OP_STATUS setShaderProgram(VEGA3dShaderProgram* shader) = 0;
	/** Get the current shader program used by the device.
	 * @returns the current shader program. */
	virtual VEGA3dShaderProgram* getCurrentShaderProgram() = 0;

#ifdef CANVAS3D_SUPPORT
	/** Attribute names and their known size (in 'float' units.) */
	struct AttributeData
	{
		char* name;
		unsigned int size;
	};

	virtual OP_STATUS createShader(VEGA3dShader** shader, BOOL fragmentShader, const char* source, unsigned int attribute_count, AttributeData *attributes, OpString *info_log) = 0;
#endif // CANVAS3D_SUPPORT

	/** Draw primitives from the vertex array described by the vertex
	 * layout. */
	virtual OP_STATUS drawPrimitives(Primitive type, VEGA3dVertexLayout* layout, unsigned int firstVert, unsigned int numVerts) = 0;
	/** Draw primitives from the vertex array described by the vertex
	 * layout using the indices in the index buffer. */
	virtual OP_STATUS drawIndexedPrimitives(Primitive type, VEGA3dVertexLayout* verts, VEGA3dBuffer* ibuffer, unsigned int bytesPerInd, unsigned int firstInd, unsigned int numInd) = 0;

	/** Create a texture. If width and height are not power of two they might be rounded up to nearest power of two,
	 * but the content width/height will be the specified values. */
	virtual OP_STATUS createTexture(VEGA3dTexture** texture, unsigned int width, unsigned int height, VEGA3dTexture::ColorFormat fmt, bool mipmaps = false, bool as_rendertarget = true) = 0;
	virtual OP_STATUS createCubeTexture(VEGA3dTexture** texture, unsigned int size, VEGA3dTexture::ColorFormat fmt, bool mipmaps = false, bool as_rendertarget = true) = 0;

	virtual OP_STATUS createFramebuffer(VEGA3dFramebufferObject** fbo) = 0;
	virtual OP_STATUS createRenderbuffer(VEGA3dRenderbufferObject** rbo, unsigned int width, unsigned int height, VEGA3dRenderbufferObject::ColorFormat, int multisampleQuality = 0) = 0;

	/** Copy the currently bound multisample framebuffer to the given non-multisample
	 * framebuffer. */
	virtual OP_STATUS resolveMultisample(VEGA3dRenderTarget* rt, unsigned int x, unsigned int y, unsigned int w, unsigned int h) = 0;

#ifdef VEGA_3DDEVICE
	/** Create a window which can be used as a target for 3d rendering. */
	virtual OP_STATUS createWindow(VEGA3dWindow** win, VEGAWindow* nativeWin) = 0;
#endif // VEGA_3DDEVICE

	/** Create a buffer of the specified size and usage. A buffer can be used as either a
	 * vertex buffer or an index buffer.
	 * @param buffer the created buffer.
	 * @param size the size of the buffer.
	 * @param usage the usage of the buffer (stream, dynamic or static).
	 * @param deferredUpdate the buffer may be used with non aligned data and non-float types which may cause it to be rearranged.
	 * @returns OpStatus::OK on success, an error otherwise. */
	virtual OP_STATUS createBuffer(VEGA3dBuffer** buffer, unsigned int size, VEGA3dBuffer::Usage usage, bool indexBuffer, bool deferredUpdate = false) = 0;
	/** Create a vertex layout which is used to map buffers to the shader. */
	virtual OP_STATUS createVertexLayout(VEGA3dVertexLayout** layout, VEGA3dShaderProgram* sprog) = 0;

	/** Set the current render target. */
	virtual OP_STATUS setRenderTarget(VEGA3dRenderTarget* rt, bool changeViewport = true) = 0;
	/** @returns the current render target. */
	virtual VEGA3dRenderTarget* getRenderTarget() = 0;
	/** Change the viewport used for rendering. */
	virtual void setViewport(int viewportX, int viewportY, int viewportWidth, int viewportHeight, float viewportNear, float viewportFar) = 0;
	/** Load an orthogonal transform from 0,0 to width,height for the current render target.
	 * @param transform the transform t store the projection matrix in. */
	virtual void loadOrthogonalProjection(VEGATransform3d& transform) = 0;
	/** Load an orthogonal transform from 0,0 to width,height.
	 * @param transform the transform t store the projection matrix in.
	 * @param w the width of the render target
	 * @param h the height of the render target */
	virtual void loadOrthogonalProjection(VEGATransform3d& transform, unsigned int w, unsigned int h) = 0;

	/** Framebuffer data parameters representing an area of the frame buffer. */
	struct FramebufferData
	{
		VEGAPixelStore pixels;
		unsigned int x;
		unsigned int y;
		unsigned int w;
		unsigned int h;

		void* privateData;
	};
	/** Lock a rect in the frame buffer. The data parameter passed to this
	 * function must contain x, y, w and h parameters to specify which area
	 * of the framebuffer to get.
	 * @param data area to get from the framebuffer and resulting data
	 * array.
	 * @param readPixels require the pixels to be read. It is much faster
	 * if this parameter is false, so use false if you are only going to
	 * write data.
	 * @returns OpStatus::OK on success, an error otherwise. */
	virtual OP_STATUS lockRect(FramebufferData* data, bool readPixels) = 0;
	/** Unlock a rectangle locked by lockRect.
	 * @param data the buffer to unlock.
	 * @param modified write the data back to the framebuffer. Set this to
	 * false if there is no need to update the framebuffer since updating it
	 * is slow. */
	virtual void unlockRect(FramebufferData* data, bool modified) = 0;

	/** Copy the current frame buffer to a texture.
	 * @param tex the texture to copy framebuffer data to.
	 * @param side Side in the cube map to update or CUBE_SIDE_NONE if a regular 2d texture.
	 * @param level the miplevel to copy framebuffer data to.
	 * @param texx, texy the coord in the texture which the framebuffer
	 * data will be copied to.
	 * @param x, y, w, h the rectangle in the framebuffer data to copy.
	 * @return OpStatus::OK on success, an error otherwise. */
	virtual OP_STATUS copyToTexture(VEGA3dTexture* tex, VEGA3dTexture::CubeSide side, int level, int texx, int texy, int x, int y, unsigned int w, unsigned int h) = 0;

	/** Mark the entire rendertarget as clean. Called by the platform
	* implementation of the 3d device. */
	void validate()
	{
		invalidLeft = ~0u;
		invalidTop = ~0u;
		invalidRight = 0;
		invalidBottom = 0;
	}
	/** Make a section of the rendertarget invalid. Called by vega. */
	void invalidate(unsigned int left, unsigned int top, unsigned int right, unsigned int bottom)
	{
		if (right > invalidRight)
			invalidRight = right;
		if (bottom > invalidBottom)
			invalidBottom = bottom;
		if (left < invalidLeft)
			invalidLeft = left;
		if (top < invalidTop)
			invalidTop = top;
	}

	/** Create filters using special hardware accelerated apply functions.
	 * If a filter does not support any special hardware acceleration (such
	 * as shaders), just return OpStatus::ERR to create a software fallback
	 * (or in some cases hardware accelerated fallback without shaders). */
	virtual OP_STATUS createMergeFilter(class VEGAFilterMerge** f){return OpStatus::ERR;}
	virtual OP_STATUS createConvolveFilter(class VEGAFilterConvolve** f){return OpStatus::ERR;}
	virtual OP_STATUS createMorphologyFilter(class VEGAFilterMorphology** f){return OpStatus::ERR;}
	virtual OP_STATUS createGaussianFilter(class VEGAFilterGaussian** f){return OpStatus::ERR;}
	virtual OP_STATUS createLightingFilter(class VEGAFilterLighting** f){return OpStatus::ERR;}
	virtual OP_STATUS createColorTransformFilter(class VEGAFilterColorTransform** f){return OpStatus::ERR;}
	virtual OP_STATUS createDisplaceFilter(class VEGAFilterDisplace** f){return OpStatus::ERR;}

	virtual bool fullNPotSupport() = 0;

	// Small helper functions for 3d devices
	/** Check if a number is a power of two. */
	static inline bool isPow2(unsigned int n)
	{
		return !(n&(n-1));
	}
	/** @returns the nearest power of two greater than the number. */
	static inline unsigned int pow2_ceil(unsigned int n)
	{
		if (!(n&(n-1)))
			return n;
		while (n&(n-1))
			n&=(n-1);
		return n<<1;
	}
	/** @returns the nearest power of two less than the number. */
	static inline unsigned int pow2_floor(unsigned int n)
	{
		while (n&(n-1))
			n&=(n-1);
		return n;
	}
	/** @returns the integer log2 of the number defined in the range n > 0. */
	static inline unsigned int ilog2_floor(unsigned int n)
	{
		unsigned int i = 0;
		while (n > 1)
		{
			n /= 2;
			++i;
		}
		return i;
	}

	struct Vega2dVertex
	{
		float x, y;
		float s, t;
		UINT32 color;
	};
	/** Convert a vega color to a color in the native representation used
	 * by the 3d hardware. */
	virtual UINT32 convertToNativeColor(UINT32 color) = 0;


	OP_STATUS createDefault2dObjects();
	void destroyDefault2dObjects();

	/** Get a temporary buffer of at least the requested size if one exists, otherwise returns NULL. */
	VEGA3dBuffer* getTempBuffer(unsigned int size);
	/** Return an index buffer representing quads in an order resembling triangle fans using 6 indices per quad.
	 * If the requested size is too large NULL will be returned. */
	VEGA3dBuffer* getQuadIndexBuffer(unsigned int numverts);
	/** Create a vertex layout for Vega2dVertex usable with the specified shader type.
	 * This is a helper function to allow the 3d device to cache better, so use it if possible. */
	OP_STATUS createVega2dVertexLayout(VEGA3dVertexLayout** layout, VEGA3dShaderProgram::ShaderType shdtype);
	/** Get buffers for storing triangle indices. */
	void getTriangleIndexBuffers(VEGA3dBuffer** triangleIndexBuffer, unsigned short** triangleIndices)
	{ *triangleIndexBuffer = m_triangleIndexBuffer; *triangleIndices = m_triangleIndices; }
	/** Returns the number of triangle indices the triangle index buffer can hold */
	unsigned int getTriangleIndexBufferSize() { return 3*(VEGA_TEMP_VERTEX_BUFFER_SIZE-2); }

	/** Get a temporary rgba texture which can be used as a render target
	 * with a size of at least w x h to use the copy to texture moveRect.
	 * @returns a temporary texture or NULL on failure. */
	VEGA3dTexture* getTempTexture(unsigned int minWidth, unsigned int minHeight);
	VEGA3dFramebufferObject* getTempTextureRenderTarget();
	VEGA3dTexture* getAlphaTexture();
	UINT8* getAlphaTextureBuffer();

	VEGA3dTexture* createTempTexture2(unsigned int minWidth, unsigned int minHeight);
	VEGA3dFramebufferObject* createTempTexture2RenderTarget();

	VEGA3dRenderState* getDefault2dRenderState(){return m_default2dRenderState;}
	VEGA3dRenderState* getDefault2dNoBlendRenderState(){return m_default2dNoBlendRenderState;}
	VEGA3dRenderState* getDefault2dNoBlendNoScissorRenderState(){return m_default2dNoBlendNoScissorRenderState;}

	virtual bool getHighPrecisionFragmentSupport() const { return true; }

#ifdef VEGA_ENABLE_PERF_EVENTS
	/** Insert a marker into the render command stream for debugging and performance usage. */
	virtual void SetPerfMarker(const uni_char *marker) = 0;

	/** Insert markers to bracket an event in the render command stream for debugging and performance usage. */
	virtual void BeginPerfEvent(const uni_char *marker) = 0;
	virtual void EndPerfEvent() = 0;
#endif //VEGA_ENABLE_PERF_EVENTS
protected:
	unsigned int invalidLeft, invalidTop;
	unsigned int invalidRight, invalidBottom;

	VEGA3dBuffer* m_tempBuffer;
	VEGA3dBuffer* m_ibuffer;
	VEGA3dTexture* m_tempTexture;
	VEGA3dFramebufferObject* m_tempFramebuffer;
	VEGA3dTexture* m_alphaTexture;
	UINT8* m_alphaTextureBuffer;
	VEGA3dTexture* m_tempTexture2;
	VEGA3dFramebufferObject* m_tempFramebuffer2;
	VEGA3dVertexLayout* m_2dVertexLayout;
	VEGA3dRenderState* m_default2dRenderState;
	VEGA3dRenderState* m_default2dNoBlendRenderState;
	VEGA3dRenderState* m_default2dNoBlendNoScissorRenderState;
	OpVector<VEGA3dRenderState> m_renderStateStack;

	/** GPU representation of triangle indices, used to batch indexed
	  * triangles - updated from flushBatches if needed */
	VEGA3dBuffer* m_triangleIndexBuffer;
	/** buffer for all triangle indices, synced to
	 * m_triangleIndexBuffer from flushBatches */
	unsigned short* m_triangleIndices;

	Head m_contextLostListeners;

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	VEGABlocklistDevice::BlocklistStatus m_blocklistStatus2D, m_blocklistStatus3D;
#endif
};

#ifdef VEGA_3DDEVICE
# define VEGA_3D_NATIVE_WIN_NAME nativeWin
# define VEGA_3D_NATIVE_WIN_DECL , VEGAWindow* VEGA_3D_NATIVE_WIN_NAME = NULL
# define VEGA_3D_NATIVE_WIN_PASS , VEGA_3D_NATIVE_WIN_NAME
#else // VEGA_3DDEVICE
# define VEGA_3D_NATIVE_WIN_NAME
# define VEGA_3D_NATIVE_WIN_DECL
# define VEGA_3D_NATIVE_WIN_PASS
#endif // VEGA_3DDEVICE

/** Factory class for creating VEGA3dDevices. Creation of a 3d device
 * is somewhat convoluted, mainly for two reasons:
 *
 * - Some platforms provide several possible backends (eg OpenGL, D3D).
 *
 * - Backends may be blocklisted based on information fetched from the
     backend itself, requiring a two-level init phase.
 *
 * Users of this API just call the public blend of Create, and can
 * ignore the details. Internally the creation process is as follows:
 *
 * - API user calls public blend of Create.
 *
 * - This will call private blend of Create (implemented by platform)
     requesting a specific device type/backend, which performs
     first-level initialization by calling VEGA3dDevice::Init.
 *
 * - If first-level initialization succeeds
     VEGA3dDevice::ConstructDevice is called. This checks blocklist
     status, and if not blocked calls VEGA3dDevice::Construct to
     completely initialize device.
 *
 * The internal steps are repeated until a device is created, or until
 * there are no more device types/backends to try.
 */
class VEGA3dDeviceFactory
{
public:
	/** Fetch number of blends of 3d device that can be created - to be implemented by platform.
	 * @return the number of different devices that can be created using this factory
	 */
	static unsigned int DeviceTypeCount();
	/** Fetch name of device - shown on opera:gpu
	 * @param dev the device for which to fetch name
	 */
	static const uni_char* DeviceTypeString(UINT16 device_type_index);

	/** Create a 3d device. This is only called once to create the
	 * global 3d device instance. Will call below blend of Create
	 * taking device_type_index, starting with index 0 and continuing
	 * until a device was successfully created or all valid indices
	 * have been tried.
	 * @param dev the device to create
	 * @param use_case under what circumstances to create a device
	 * @param nativeWin the native window which is used by VegaOpPainter.
	 * This is sent to make it possible to use it for initializing the 3d
	 * context.
	 */
	static OP_STATUS Create(VEGA3dDevice** dev, VEGA3dDevice::UseCase use_case
	                        VEGA_3D_NATIVE_WIN_DECL);

private:
	/** Allocate and initialize a specific blend of 3d device - to be
	 * implemented by platform. This is first-level init only, proper
	 * construction is made through Construct. Implementation should
	 * call VEGA3dDevice::Init, which should initialize enough so that
	 * VEGABlocklistDevice::DataProvider can do what it should.
	 * @param device_type_index the index of the device to create - must be less than DeviceTypeCount
	 * @param dev the device to create
	 * @param nativeWin the native window which is used by VegaOpPainter.
	 * This is sent to make it possible to use it for initializing the 3d
	 * context.
	 */
	static OP_STATUS Create(unsigned int device_type_index,
	                        VEGA3dDevice** dev VEGA_3D_NATIVE_WIN_DECL);

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	friend class VEGABlocklistDevice;
#endif // VEGA_BACKENDS_USE_BLOCKLIST
};

#ifdef VEGA_3DDEVICE
# undef  VEGA_3D_NATIVE_WIN_DECL
# define VEGA_3D_NATIVE_WIN_DECL , VEGAWindow* VEGA_3D_NATIVE_WIN_NAME
#endif // VEGA_3DDEVICE

#endif // VEGA_3DDEVICE || CANVAS3D_SUPPORT
#endif // VEGA_SUPPORT
#endif // VEGA3DDEVICE_H

