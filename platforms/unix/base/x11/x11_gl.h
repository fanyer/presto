/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef X11_GL_H
#define X11_GL_H

/* This file provides all the necessary functionality from GLX without
 * including the header file.  This way we avoid cluttering the
 * namespace, and have a single point to check or debug the glx code.
 *
 * We don't really need to use much from GLX, so this is entirely
 * feasible to maintain by hand.
 */

#include "platforms/utilix/x11_all.h"

namespace GLX
{
	typedef void * FBConfig;
	typedef void * Context;
	typedef X11Types::XID Window;

	const int VENDOR = 1;
	const int VERSION = 2;
	const int EXTENSIONS = 3;

	const int DOUBLEBUFFER = 5;
	const int RED_SIZE = 8;
	const int GREEN_SIZE = 9;
	const int BLUE_SIZE = 10;
	const int ALPHA_SIZE = 11;
	const int DEPTH_SIZE = 12;
	const int STENCIL_SIZE = 13;
	const int RENDER_TYPE = 0x8011;
	const int X_RENDERABLE = 0x8012;
	const int RGBA_TYPE = 0x8014;
	const int RGBA_BIT = 0x00000001;

	const int GPU_RAM_AMD = 0x21A3;

	const int CONTEXT_FLAGS_ARB = 0x2094;
	const int CONTEXT_DEBUG_BIT_ARB =0x0001;

	typedef void(*voidfunc_t)();

	bool LoadSymbols(OpDLL *libGL);

	void DestroyContext(X11Types::Display *dpy, Context ctx);
	X11Types::Bool IsDirect(X11Types::Display *dpy, Context ctx);
	X11Types::Bool QueryVersion(X11Types::Display *dpy, int *major, int *minor);
	/* 'drawable' is actually a GLXDrawable, but we only use windows
	 * in opera.  Easy to fix should we need the extra generality.
	 */
	void SwapBuffers(X11Types::Display *dpy, Window drawable);

	// GLX 1.1
	const char * GetClientString(X11Types::Display *dpy, int name);
	const char * QueryServerString(X11Types::Display *dpy, int screen, int name);
	const char * QueryExtensionsString(X11Types::Display *dpy, int screen);

	// GLX 1.3
	Window CreateWindow(X11Types::Display *dpy, FBConfig config, X11Types::Window win, const int *attrib_list);
	void DestroyWindow(X11Types::Display *dpy, Window win);
	/* draw/read are actually GLXDrawables, but we only use windows in
	 * opera.  Easy to fix should we need the extra generality.
	 */
	X11Types::Bool MakeContextCurrent(X11Types::Display *dpy, Window draw, Window read, Context ctx);
	Context CreateNewContext(X11Types::Display *dpy, FBConfig config, int render_type, Context share_list, X11Types::Bool direct);
	FBConfig * ChooseFBConfig(X11Types::Display *dpy, int screen, const int *attrib_list, int *nelements);
	XVisualInfo * GetVisualFromFBConfig(X11Types::Display *dpy, FBConfig config);

	// GLX 1.4 (or GLX_ARB_get_proc_address)
	voidfunc_t GetProcAddress(const char *procname);


	// GLX_AMD_gpu_association
	/** Checks for the presence of, and initializes the
	 * GLX_AMD_gpu_association extension.  Returns true on success,
	 * false on failure.
	 */
	bool HasExtensionAMDGpuAssociation(X11Types::Display *dpy, int screen);
	/** Make sure HasExtensionAMDGpuAssociation() has been called (and
	 * returned success) before calling this function.
	 */
	unsigned int GetContextGPUIDAMD(GLX::Context ctx);
	/** Make sure HasExtensionAMDGpuAssociation() has been called (and
	 * returned success) before calling this function.
	 */
	int GetGPUInfoAMD(unsigned int id, int property, int dataType, unsigned int size, void *data);

	// GLX_ARB_create_context
	/** Checks for the presence of, and initializes the
	 * GLX_ARB_create_context extension.  Returns true on success,
	 * false on failure.
	 */
	bool HasExtensionARBCreateContext(X11Types::Display * dpy, int screen);
	/** Make sure HasExtensionARBCreateContext() has been called (and
	 * returned success) before calling this function.
	 */
	Context CreateContextAttribsARB(X11Types::Display * dpy, FBConfig config, Context share_context, X11Types::Bool direct, const int * attrib_list);
}


#endif // !X11_GL_H
