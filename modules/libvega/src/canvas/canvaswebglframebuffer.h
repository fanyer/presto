/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CANVASWEBGLFRAMEBUFFER_H
#define CANVASWEBGLFRAMEBUFFER_H

#ifdef CANVAS3D_SUPPORT

#include "modules/libvega/src/canvas/webglutils.h"
#include "modules/libvega/src/canvas/canvaswebglrenderbuffer.h"
#include "modules/libvega/src/canvas/canvaswebgltexture.h"

class CanvasWebGLFramebuffer : public WebGLRefCounted
{
public:
	CanvasWebGLFramebuffer();
	~CanvasWebGLFramebuffer();

	OP_STATUS Construct();
	void destroyInternal();

	void setColorBuffer(CanvasWebGLRenderbuffer* buf){m_colorBuf = buf;m_colorTex = NULL;m_bindingsChanged=TRUE;}
	void setDepthBuffer(CanvasWebGLRenderbuffer* buf){m_depthBuf = buf;m_depthTex = NULL;m_bindingsChanged=TRUE;}
	void setStencilBuffer(CanvasWebGLRenderbuffer* buf){m_stencilBuf = buf;m_stencilTex = NULL;m_bindingsChanged=TRUE;}
	void setDepthStencilBuffer(CanvasWebGLRenderbuffer* buf){m_depthStencilBuf = buf;m_depthStencilTex = NULL;m_bindingsChanged=TRUE;}
	void setColorTexture(CanvasWebGLTexture* tex, unsigned int face){m_colorTex = tex;m_colorTexFace = face;m_colorBuf = NULL;m_bindingsChanged=TRUE;}
	void setDepthTexture(CanvasWebGLTexture* tex, unsigned int face){m_depthTex = tex;m_depthTexFace = face;m_depthBuf = NULL;m_bindingsChanged=TRUE;}
	void setStencilTexture(CanvasWebGLTexture* tex, unsigned int face){m_stencilTex = tex;m_stencilTexFace = face;m_stencilBuf = NULL;m_bindingsChanged=TRUE;}
	void setDepthStencilTexture(CanvasWebGLTexture* tex, unsigned int face){m_depthStencilTex = tex;m_depthStencilTexFace = face;m_depthStencilBuf = NULL;m_bindingsChanged=TRUE;}

	CanvasWebGLRenderbuffer* getColorBuffer()         { return m_colorBuf.GetPointer(); }
	CanvasWebGLRenderbuffer* getDepthBuffer()         { return m_depthBuf.GetPointer(); }
	CanvasWebGLRenderbuffer* getStencilBuffer()       { return m_stencilBuf.GetPointer(); }
	CanvasWebGLRenderbuffer* getDepthStencilBuffer()  { return m_depthStencilBuf.GetPointer(); }

	CanvasWebGLTexture* getColorTexture()         { return m_colorTex.GetPointer(); }
	CanvasWebGLTexture* getDepthTexture()         { return m_depthTex.GetPointer(); }
	CanvasWebGLTexture* getStencilTexture()       { return m_stencilTex.GetPointer(); }
	CanvasWebGLTexture* getDepthStencilTexture()  { return m_depthStencilTex.GetPointer(); }

	unsigned int getColorTextureFace(){return m_colorTexFace;}
	unsigned int getDepthTextureFace(){return m_depthTexFace;}
	unsigned int getStencilTextureFace(){return m_stencilTexFace;}
	unsigned int getDepthStencilTextureFace(){return m_depthStencilTexFace;}

	VEGA3dFramebufferObject* getBuffer() { return m_framebuffer; }

	BOOL hasBeenBound() const  { return m_hasBeenBound; }
	void setIsBound()      { m_hasBeenBound = true; }

	unsigned int checkStatus();
	OP_STATUS updateBindings(unsigned int& status);
private:

	virtual void releaseHandles();

	VEGA3dFramebufferObject* m_framebuffer;

	CanvasWebGLRenderbufferPtr m_colorBuf;
	CanvasWebGLRenderbufferPtr m_depthBuf;
	CanvasWebGLRenderbufferPtr m_stencilBuf;
	CanvasWebGLRenderbufferPtr m_depthStencilBuf;

	CanvasWebGLTexturePtr m_colorTex;
	CanvasWebGLTexturePtr m_depthTex;
	CanvasWebGLTexturePtr m_stencilTex;
	CanvasWebGLTexturePtr m_depthStencilTex;

	unsigned int m_colorTexFace;
	unsigned int m_depthTexFace;
	unsigned int m_stencilTexFace;
	unsigned int m_depthStencilTexFace;

	BOOL m_hasBeenBound;
	BOOL m_bindingsChanged;
};

typedef WebGLSmartPointer<CanvasWebGLFramebuffer> CanvasWebGLFramebufferPtr;

#endif //CANVAS3D_SUPPORT
#endif //CANVASWEBGLFRAMEBUFFER_H
