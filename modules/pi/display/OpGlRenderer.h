/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPGLRENDERER_H
#define OPGLRENDERER_H

#ifdef CANVAS3D_SUPPORT

/** Interface to render to a memory buffer using OpenGL. */
class OpGlRenderer
{
public:
	/** Create and OpenGL renderer which renderes to the specified buffer
	 * of the specified size and stride. */
	static OP_STATUS Create(OpGlRenderer** renderer, UINT32* buffer, UINT32 width, UINT32 height, UINT32 stride);

	virtual ~OpGlRenderer() {}

#ifdef LINK_WITHOUT_OPENGL
	/** Get the OpenGL dll loaded by the renderer. */
	virtual class OpDLL* GetDLL() = 0;
#endif // LINK_WITHOUT_OPENGL

	/** Resize the buffer to which OpenGL renders. */
	virtual OP_STATUS Resize(UINT32* buffer, UINT32 width, UINT32 height, UINT32 stride) = 0;

	/** Activate the current OpenGL renderer. If there are several this must
	 * be called for OpenGL to know where to render. */
	virtual OP_STATUS Activate() = 0;

	/** Update the buffer. Will cause all OpenGL drawing to be copied to the buffer. */
	virtual OP_STATUS UpdateBuffer() = 0;
};

#endif // CANVAS3D_SUPPORT

#endif // !OPGLRENDERER_H
