/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CANVASWEBGLRENDERBUFFER_H
#define CANVASWEBGLRENDERBUFFER_H

#ifdef CANVAS3D_SUPPORT

#include "modules/libvega/src/canvas/webglutils.h"

class CanvasWebGLRenderbuffer;
class CanvasWebGLRenderbufferPtr : public WebGLSmartPointer<CanvasWebGLRenderbuffer>
{
public:
	CanvasWebGLRenderbufferPtr(CanvasWebGLRenderbuffer *p = NULL) : WebGLSmartPointer<CanvasWebGLRenderbuffer>(p) { m_initId = 0; }
	CanvasWebGLRenderbufferPtr(const CanvasWebGLRenderbufferPtr &s) : WebGLSmartPointer<CanvasWebGLRenderbuffer>(s) { m_initId = 0; }
	void operator =(const CanvasWebGLRenderbufferPtr &s)  { m_initId = 0; WebGLSmartPointer<CanvasWebGLRenderbuffer>::operator=(s); }
	void operator =(CanvasWebGLRenderbuffer *p)  { m_initId = 0; WebGLSmartPointer<CanvasWebGLRenderbuffer>::operator=(p); }

	bool hasBeenInvalidated() const;
	void validate();
private:
	unsigned int m_initId;
};


class CanvasWebGLRenderbuffer : public WebGLRefCounted
{
public:
	CanvasWebGLRenderbuffer();
	~CanvasWebGLRenderbuffer();

	OP_STATUS create(VEGA3dRenderbufferObject::ColorFormat fmt, unsigned int width, unsigned int height);
	void destroyInternal();

	VEGA3dRenderbufferObject* getBuffer(){return m_renderbuffer;}
	bool hasBeenBound() const     { return m_hasBeenBound; }
	bool isInitialized() const { return m_initialized; }
	void setInitialized() { m_initialized = TRUE; }
	void setIsBound() { m_hasBeenBound = true; }
	bool isZeroSized() const { return m_zeroSizedBuffer; }
	unsigned int getInitId() const { return m_initId; }
private:
	VEGA3dRenderbufferObject* m_renderbuffer;

	bool m_initialized;
	bool m_hasBeenBound;
	bool m_zeroSizedBuffer;
	unsigned int m_initId;  // Incremented every time the renderbuffer is reinitialized.
};


#endif //CANVAS3D_SUPPORT
#endif //CANVASWEBGLRENDERBUFFER_H
