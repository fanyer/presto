/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CANVASCONTEXTWEBGL_H
#define CANVASCONTEXTWEBGL_H

#ifdef CANVAS3D_SUPPORT

class Canvas;
class ES_Object;

class VEGARenderTarget;

#include "modules/util/stringhash.h"

#include "modules/libvega/vega3ddevice.h"
#include "modules/libvega/vegarefcount.h"
#include "modules/libvega/src/vegaswbuffer.h"
#include "modules/libvega/src/canvas/webglutils.h"
#include "modules/libvega/src/canvas/canvaswebglrenderbuffer.h"
#include "modules/libvega/src/canvas/canvaswebglbuffer.h"
#include "modules/libvega/src/canvas/canvaswebglframebuffer.h"
#include "modules/libvega/src/canvas/canvaswebgltexture.h"

class VEGARenderer;
class CanvasWebGLProgram;
class CanvasWebGLShader;

/** The context which is used for painting opengl content on the
 * canvas. */
class CanvasContextWebGL : public VEGARefCount
{
public:
	CanvasContextWebGL(Canvas *can, BOOL alpha, BOOL depth, BOOL stencil, BOOL antialias, BOOL premultipliedAlpha, BOOL preserveDrawingBuffer);
	~CanvasContextWebGL();

	/** Do any post constructor initialization needed. */
	OP_STATUS Construct();

	/** GCTrace all the DOM objects referenced by the context. */
	void GCTrace(ES_Runtime* runtime);

	/** Remove all references to the owning canvas. */
	void ClearCanvas();
	/** Get teh owning canvas insteance. */
	Canvas *GetCanvas();

	/** Make sure that we have a 3d device that is capable of WebGL rendering. */
	OP_STATUS ensureDevice(VEGA3dDevice::UseCase use_case);

	/** Initialize this context with a buffer to paint on. */
	OP_STATUS initBuffer(VEGARenderTarget* rt, VEGA3dDevice::UseCase use_case);

	/** Function to check if the context is rendering to the canvas or a user bound buffer. */
	bool isRenderingToCanvas() { return !m_framebuffer; }

	void swapBuffers(VEGARenderer* renderer, bool clear, VEGARenderTarget* backbufferRT = NULL);
	OP_STATUS lockBufferContent(VEGA3dDevice::FramebufferData* data);
	void unlockBufferContent(VEGA3dDevice::FramebufferData* data);

	// Setting and getting state
	void activeTexture(unsigned int tex);
	void blendColor(float red, float green, float blue, float alpha);
	void blendEquation(unsigned int eq);
	void blendEquationSeparate(unsigned int eq, unsigned int eqA);
	void blendFunc(unsigned int src, unsigned int dst);
	void blendFuncSeparate(unsigned int src, unsigned int dst, unsigned int srcA, unsigned int dstA);
	void clearColor(float red, float green, float blue, float alpha);
	void clearDepth(float depth);
	void clearStencil(int sten);
	void colorMask(BOOL red, BOOL green, BOOL blue, BOOL alpha);
	void cullFace(unsigned int face);
	void depthFunc(unsigned int func);
	void depthMask(BOOL depth);
	void depthRange(float zNear, float zFar);
	void disable(unsigned int cap);
	void enable(unsigned int cap);
	void frontFace(unsigned int face);
	CanvasWebGLParameter getParameter(unsigned int param);
	unsigned int getNumCompressedTextureFormats();
	unsigned int getCompressedTextureFormat(unsigned int index);
	void hint(unsigned int target, unsigned int mode);
	BOOL isEnabled(unsigned int cap);
	void lineWidth(float width);
	void pixelStorei(unsigned int name, unsigned int param);
	void polygonOffset(float factor, float offset);
	void sampleCoverage(float value, BOOL invert);
	void stencilFunc(unsigned int func, int ref, unsigned int mask);
	void stencilFuncSeparate(unsigned int face, unsigned int func, int ref, unsigned int mask);
	void stencilMask(unsigned int mask);
	void stencilMaskSeparate(unsigned int face, unsigned int mask);
	void stencilOp(unsigned int fail, unsigned int zfail, unsigned int zpass);
	void stencilOpSeparate(unsigned int face, unsigned int fail, unsigned int zfail, unsigned int zpass);

	void clear(unsigned int mask);
	OP_STATUS drawArrays(unsigned int mode, int firstVert, int numVerts);
	OP_STATUS drawElements(unsigned int mode, int numVerts, unsigned int type, int offset);
	void finish();
	void flush();
	OP_STATUS readPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, UINT8* data, unsigned int dataLen);

	// Viewing and clipping
	void scissor(int x, int y, int width, int height);
	void viewport(int x, int y, int width, int height);

	// Buffer objects
	void bindBuffer(unsigned int target, CanvasWebGLBuffer* buffer);
	OP_STATUS bufferData(unsigned int target, unsigned int usage, int size, void* data);
	OP_STATUS bufferSubData(unsigned int target, int offset, int size, void* data);
	void deleteBuffer(CanvasWebGLBuffer* buffer);
	CanvasWebGLParameter getBufferParameter(unsigned int target, unsigned int param);

	// Framebuffer objects
	void bindFramebuffer(unsigned int target, CanvasWebGLFramebuffer* framebuffer);
	unsigned int checkFramebufferStatus(unsigned int target);
	void deleteFramebuffer(CanvasWebGLFramebuffer* framebuffer);
	void framebufferRenderbuffer(unsigned int target, unsigned int attachment, unsigned int renderbuffertarget, CanvasWebGLRenderbuffer* renderbuffer);
	void framebufferTexture2D(unsigned int target, unsigned int attachment, unsigned int textarget, CanvasWebGLTexture* texture, int level);
	CanvasWebGLParameter getFramebufferAttachmentParameter(unsigned int target, unsigned int attachment, unsigned int param);

	// Renderbuffer objects
	void bindRenderbuffer(unsigned int target, CanvasWebGLRenderbuffer* renderbuffer);
	void deleteRenderbuffer(CanvasWebGLRenderbuffer* renderbuffer);
	CanvasWebGLParameter getRenderbufferParameter(unsigned int target, unsigned int param);
	OP_STATUS renderbufferStorage(unsigned int target, unsigned int internalformat, int width, int height);

	// Texture objects
	void bindTexture(unsigned int target, CanvasWebGLTexture* texture);
	OP_STATUS copyTexImage2D(unsigned int target, int level, unsigned int internalfmt, int x, int y, int width, int height, int border);
	OP_STATUS copyTexSubImage2D(unsigned int target, int level, int xofs, int yofs, int x, int y, int width, int height);
	void deleteTexture(CanvasWebGLTexture* texture);
	void generateMipmap(unsigned int target);
	CanvasWebGLParameter getTexParameter(unsigned int target, unsigned int param);
	OP_STATUS texImage2D(unsigned int target, int level, unsigned int internalfmt, unsigned int fmt, unsigned int type, OpBitmap* data, VEGASWBuffer* nonPremultipliedData);
	OP_STATUS texImage2D(unsigned int target, int level, unsigned int internalfmt, int width, int height, int border, unsigned int fmt, unsigned int type, UINT8* data, unsigned int dataLen);
	OP_STATUS texImage2D(unsigned int target, int level, unsigned int internalfmt, int width, int height, int border, unsigned int fmt, unsigned int type, UINT16* data, unsigned int dataLen);
	OP_STATUS texImage2D(unsigned int target, int level, unsigned int internalfmt, int width, int height, int border, unsigned int fmt, unsigned int type, float* data, unsigned int dataLen);
	OP_STATUS texImage2D(unsigned int target, int level, unsigned int internalfmt, int width, int height, int border, unsigned int fmt, unsigned int type);
	void texParameterf(unsigned int target, unsigned int param, float value);
	void texParameteri(unsigned int target, unsigned int param, int value);
	OP_STATUS texSubImage2D(unsigned int target, int level, int xofs, int yofs, unsigned int fmt, unsigned int type, OpBitmap* data, VEGASWBuffer* nonPremultipliedData);
	OP_STATUS texSubImage2D(unsigned int target, int level, int xofs, int yofs, int width, int height, unsigned int fmt, unsigned int type, UINT8* data, unsigned int dataLen);
	OP_STATUS texSubImage2D(unsigned int target, int level, int xofs, int yofs, int width, int height, unsigned int fmt, unsigned int type, UINT16* data, unsigned int dataLen);

	// Programs and Shaders
	OP_STATUS attachShader(CanvasWebGLProgram* program, CanvasWebGLShader* shader);
	OP_STATUS bindAttribLocation(CanvasWebGLProgram* program, const uni_char* name, unsigned int index);
	OP_STATUS compileShader(CanvasWebGLShader* shader, const uni_char *url, BOOL validate);
	void deleteProgram(CanvasWebGLProgram* program);
	void deleteShader(CanvasWebGLShader* shader);
	OP_STATUS detachShader(CanvasWebGLProgram* program, CanvasWebGLShader* shader);
	CanvasWebGLParameter getProgramParameter(CanvasWebGLProgram* program, unsigned int param);
	const uni_char* getProgramInfoLog(CanvasWebGLProgram* program);
	CanvasWebGLParameter getShaderParameter(CanvasWebGLShader* shader, unsigned int param);
	const uni_char* getShaderInfoLog(CanvasWebGLShader* shader);
	const uni_char* getShaderSource(CanvasWebGLShader* shader);
	OP_STATUS linkProgram(CanvasWebGLProgram* program);
	OP_STATUS shaderSource(CanvasWebGLShader* shader, const uni_char* source);
	void useProgram(CanvasWebGLProgram* program);
	OP_STATUS validateProgram(CanvasWebGLProgram* program);
	
	BOOL supportsExtension(unsigned int e);
	void enableExtension(unsigned int e);

	// Uniforms and attributes
	void disableVertexAttribArray(unsigned int idx);
	void enableVertexAttribArray(unsigned int idx);
	OP_STATUS getActiveAttrib(CanvasWebGLProgram* program, unsigned int idx, unsigned int* type, unsigned int* size, OpString* name);
	OP_STATUS getActiveUniform(CanvasWebGLProgram* program, unsigned int idx, unsigned int* type, unsigned int* size, OpString* name);
	int getAttribLocation(CanvasWebGLProgram* program, const uni_char* name);
	CanvasWebGLParameter getUniform(CanvasWebGLProgram* program, unsigned int idx, const char* name);
	int getUniformLocation(CanvasWebGLProgram* program, const uni_char* name);
	CanvasWebGLParameter getVertexAttrib(unsigned int idx, unsigned int param);
	unsigned int getVertexAttribOffset(unsigned int idx, unsigned int param);

	void uniform1(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length);
	void uniform1(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, int* values, int length);
	void uniform2(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length);
	void uniform2(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, int* values, int length);
	void uniform3(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length);
	void uniform3(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, int* values, int length);
	void uniform4(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length);
	void uniform4(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, int* values, int length);
	void uniformMatrix2(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length, BOOL transpose);
	void uniformMatrix3(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length, BOOL transpose);
	void uniformMatrix4(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length, BOOL transpose);
	void vertexAttrib(unsigned int idx, float* values);
	void vertexAttribPointer(unsigned int idx, int size, unsigned int type, BOOL normalized, int stride, int offset);

	void raiseError(unsigned int code);
	unsigned int getError();
	OP_STATUS VerifyUniformType(int idx, const char* name, unsigned int specifiedType, int specifiedLength, unsigned int *actualType = NULL, unsigned int *actualLength = NULL);

	int getDrawingBufferWidth();
	int getDrawingBufferHeight();

	struct VertexAttrib
	{
		BOOL isFromArray;
		CanvasWebGLBufferPtr buffer;
		unsigned int offset;
		unsigned int stride;
		unsigned int type;
		unsigned int size;
		BOOL normalize;

		float value[4];
	};

	BOOL HasAlpha(){return m_ctxAlpha;}
	BOOL HasDepth(){return m_ctxDepth;}
	BOOL HasStencil(){return m_ctxStencil;}
	BOOL HasAntialias(){return m_ctxAntialias;}
	BOOL HasPremultipliedAlpha(){return m_ctxPremultipliedAlpha;}
	BOOL HasPreserveDrawingBuffer(){return m_ctxPreserveDrawingBuffer;}

	/* The m_ctxFullscreen flag is set to tell the context that the WebGL
	   content is painted fullscreen, so that optimizations can be done.
	   Can be used to redirect painting directly to the screen. */
	void SetFullscreen(bool fullscreen) { m_ctxFullscreen = fullscreen; }
	bool GetFullscreen() const { return m_ctxFullscreen; }

	BOOL UnpackPremultipliedAlpha() const { return m_unpackPremultiplyAlpha; }
	unsigned int UnpackColorSpaceConversion() const { return m_unpackColorSpaceConversion; }
private:
	unsigned int texImage2DCommon(unsigned int target, int level, int width, int height, int border, CanvasWebGLTexturePtr& texture, VEGA3dTexture::CubeSide& cubeSide);
	unsigned int texImage2DFormatCheck(unsigned int target, unsigned int internalfmt, unsigned int fmt, unsigned int type);

	OP_STATUS texImage2DInternal(CanvasWebGLTexturePtr texture, int level, int width, int height, VEGA3dTexture::ColorFormat colFmt, VEGA3dTexture::CubeSide cubeSide, void* data, unsigned int dataLen, CanvasWebGLTexture::MipData::DataType dataType, bool usePremultiplyFlag = true);

	/** Clears all buffers of the WebGL framebuffer to the specified values.
	    The default values are those that should be used for the automatic
	    clear prior to compositing.
	    Note: Sets a new render state to bypass scissors, masks, etc. */
	void clearRenderFramebuffer(UINT32 clearColor = 0, float clearDepth = 1.0, int clearStencil = 0);

	/** Clears all buffers of the WebGL framebuffer to their defualt values if
	    a clear is pending, and unsets the pending clear flag.
	    Note: Sets a new render state to bypass scissors, masks, etc. */
	void clearIfNeeded();

	OP_STATUS updateVertexLayout();

	Canvas *m_canvas;

	void getFramebufferColorFormat(unsigned int &framebufferFmt, unsigned int &framebufferType);

	BOOL isCurrentPow2(unsigned int target);

	void setTextures();

	void initializeRenderbufferData();

	/** The main (non-FBO, from WebGL's perspective) WebGL framebuffer. */
	VEGA3dFramebufferObject* m_renderFramebuffer;

	VEGA3dTexture* m_renderTexture;
	VEGARenderTarget* m_vegaTarget;

	/** If true, we need to clear the WebGL framebuffer before the next
	    draw or read operation that touches it (e.g., because we're compositing
	    a new frame). We defer the clear to avoid doing double work if the user
	    manually clears one or more buffers for us. */
	bool m_pendingClear;

	unsigned int m_enabledExtensions;

	// context attributes
	bool m_ctxAlpha;
	bool m_ctxDepth;
	bool m_ctxStencil;
	bool m_ctxAntialias;
	bool m_ctxPremultipliedAlpha;
	bool m_ctxPreserveDrawingBuffer;
	bool m_ctxFullscreen;

	// State
	VEGA3dRenderState* m_renderState;
	unsigned int m_maxTextures;
	unsigned int m_activeTexture;
	int m_scissorX;
	int m_scissorY;
	unsigned int m_scissorW;
	unsigned int m_scissorH;
	UINT32 m_clearColor;
	float m_clearDepth;
	int m_clearStencil;
	unsigned int m_cullFace;
	unsigned int m_stencilRef;
	unsigned int m_stencilRefBack;
	unsigned int m_stencilMask;
	unsigned int m_stencilMaskBack;
	unsigned int m_stencilWriteMask;
	unsigned int m_stencilWriteMaskBack;
	unsigned int m_hintMode;
	unsigned int m_oesStdDerivativesHintMode;

	unsigned int m_packAlignment;
	unsigned int m_unpackAlignment;
	bool m_unpackFlipY;
	bool m_unpackPremultiplyAlpha;
	unsigned int m_unpackColorSpaceConversion;

	// Programs and shaders
	typedef WebGLSmartPointer<CanvasWebGLProgram> CanvasWebGLProgramPtr;
	CanvasWebGLProgramPtr m_program;

	// Buffer objects
	CanvasWebGLBufferPtr m_arrayBuffer;
	CanvasWebGLBufferPtr m_elementArrayBuffer;

	// Framebuffer objects
	CanvasWebGLFramebufferPtr m_framebuffer;

	// Renderbuffer objects
	CanvasWebGLRenderbufferPtr m_renderbuffer;

	// Texture objects
	CanvasWebGLTexturePtr* m_textures;
	CanvasWebGLTexturePtr* m_cubeTextures;
	// Uniforms and attributes
	unsigned int m_maxVertexAttribs;
	VertexAttrib* m_vertexAttribs;
	VEGA3dVertexLayout* m_vertexLayout;
	unsigned int m_maxVertexCount;

	// Viewport and depth range
	int m_viewportX;
	int m_viewportY;
	int m_viewportWidth;
	int m_viewportHeight;
	float m_viewportNear;
	float m_viewportFar;

	unsigned int m_errorCode;

#ifdef VEGA_2DDEVICE
	VEGA3dVertexLayout* m_vlayout2d;
	VEGA3dShaderProgram* m_shader2d;
#endif // VEGA_2DDEVICE

	VEGA3dTexture* m_1x1BlackTexture;
	VEGA3dTexture* m_1x1BlackCubeTexture;
};

#endif // CANVAS3D_SUPPORT
#endif // CANVASCONTEXTWEBGL_H
