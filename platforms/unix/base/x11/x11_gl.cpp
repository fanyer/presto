/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/* This file (and the associated header file) provides everything that
 * is needed from GLX.  In this way, we only have to deal with the
 * mess of X11 and OpenGL includes in one very small and simple file.
 */

#if defined(VEGA_BACKEND_OPENGL)
#error This file should really be compiled on its own to minimize the impact of the include of the GLX header.
#endif

#include "core/pch.h"

#if defined(VEGA_BACKEND_OPENGL)

#include "modules/pi/OpDLL.h"
#include "platforms/unix/base/x11/x11_gl.h"
#include "platforms/utilix/dlmacros.h"
#include "platforms/utilix/x11_all.h"

#ifdef VEGA_GL_DEBUG
#define X11_GL_DEBUG_SYMBOL(ret, name, args) ;ret dbg_##name args
#else
#define X11_GL_DEBUG_SYMBOL(ret, name, args)
#endif

#define X11_GL_SYMBOL(ret, name, args) DEFINE_SYMBOL(ret, real_##name, args) X11_GL_DEBUG_SYMBOL(ret, name, args)


using namespace X11Types;
// Explicitly resolve ambiguities
using X11Types::Window;
using X11Types::XID;

#ifdef GLX_H
#error Do not include glx.h!  x11_gl.h shall provide all necessary GLX symbols.
#endif
#if 0 // Until this define is removed from core
#ifdef __gl_h_
#error Do not include gl.h! platforms/vega_backends/opengl/vegaglapi.h shall provide all necessary GL symbols.
#endif
#endif
#include <GL/glx.h>

namespace
{
	X11_GL_SYMBOL(void, glXDestroyContext, (Display *dpy, GLXContext ctx));
	X11_GL_SYMBOL(Bool, glXIsDirect, (Display *dpy, GLXContext ctx));
	X11_GL_SYMBOL(Bool, glXQueryVersion, (Display *dpy, int *major, int *minor));
	X11_GL_SYMBOL(void, glXSwapBuffers, (Display *dpy, GLXDrawable drawable));
	// GLX 1.1
	X11_GL_SYMBOL(const char *, glXGetClientString, (Display *dpy, int name));
	X11_GL_SYMBOL(const char *, glXQueryServerString, (Display *dpy, int screen, int name));
	X11_GL_SYMBOL(const char *, glXQueryExtensionsString, (Display *dpy, int screen));
	// GLX 1.3
	X11_GL_SYMBOL(GLXWindow, glXCreateWindow, (Display *dpy, GLXFBConfig config, Window win, const int *attrib_list));
	X11_GL_SYMBOL(void, glXDestroyWindow, (Display *dpy, GLXWindow win));
	X11_GL_SYMBOL(Bool, glXMakeContextCurrent, (Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx));
	X11_GL_SYMBOL(GLXContext, glXCreateNewContext, (Display *dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct));
	X11_GL_SYMBOL(GLXFBConfig *, glXChooseFBConfig, (Display *dpy, int screen, const int *attrib_list, int *nelements));
	X11_GL_SYMBOL(XVisualInfo *, glXGetVisualFromFBConfig, (Display *dpy, GLXFBConfig config));
	// GLX 1.4 (or GLX_ARB_get_proc_address)
	X11_GL_SYMBOL(GLX::voidfunc_t, glXGetProcAddress, (const GLubyte *procname));

	// GLX_AMD_gpu_association
	/* The documentation of the signatures of the functions from
	 * GLX_AMD_gpu_association are clearly copied from the WGL
	 * extension, and are obviously wrong (e.g. returning values of
	 * type HGLRC).  So these are my best guesses for what the
	 * signatures really are.
	 */
	X11_GL_SYMBOL(unsigned int, glXGetContextGPUIDAMD, (GLXContext ctx));
	X11_GL_SYMBOL(int, glXGetGPUInfoAMD, (unsigned int id, int property, GLenum dataType, unsigned int size, void *data));

	// GLX_ARB_create_context
	X11_GL_SYMBOL(GLXContext, glXCreateContextAttribsARB, (Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list));

	void *Sym(OpDLL *libGL, const char *symbol)
	{
		return libGL->GetSymbolAddress(symbol);
	}
}

#ifdef VEGA_GL_DEBUG
#define glXDestroyContext dbg_glXDestroyContext
#define glXIsDirect dbg_glXIsDirect
#define glXQueryVersion dbg_glXQueryVersion
#define glXSwapBuffers dbg_glXSwapBuffers
#define glXGetClientString dbg_glXGetClientString
#define glXQueryServerString dbg_glXQueryServerString
#define glXQueryExtensionsString dbg_glXQueryExtensionsString
#define glXCreateWindow dbg_glXCreateWindow
#define glXDestroyWindow dbg_glXDestroyWindow
#define glXMakeContextCurrent dbg_glXMakeContextCurrent
#define glXCreateNewContext dbg_glXCreateNewContext
#define glXChooseFBConfig dbg_glXChooseFBConfig
#define glXGetVisualFromFBConfig dbg_glXGetVisualFromFBConfig
#define glXGetProcAddress dbg_glXGetProcAddress
#define glXGetContextGPUIDAMD dbg_glXGetContextGPUIDAMD
#define glXGetGPUInfoAMD dbg_glXGetGPUInfoAMD
#define glXCreateContextAttribsARB dbg_glXCreateContextAttribsARB
#else
#define glXDestroyContext real_glXDestroyContext
#define glXIsDirect real_glXIsDirect
#define glXQueryVersion real_glXQueryVersion
#define glXSwapBuffers real_glXSwapBuffers
#define glXGetClientString real_glXGetClientString
#define glXQueryServerString real_glXQueryServerString
#define glXQueryExtensionsString real_glXQueryExtensionsString
#define glXCreateWindow real_glXCreateWindow
#define glXDestroyWindow real_glXDestroyWindow
#define glXMakeContextCurrent real_glXMakeContextCurrent
#define glXCreateNewContext real_glXCreateNewContext
#define glXChooseFBConfig real_glXChooseFBConfig
#define glXGetVisualFromFBConfig real_glXGetVisualFromFBConfig
#define glXGetProcAddress real_glXGetProcAddress
#define glXGetContextGPUIDAMD real_glXGetContextGPUIDAMD
#define glXGetGPUInfoAMD real_glXGetGPUInfoAMD
#define glXCreateContextAttribsARB real_glXCreateContextAttribsARB
#endif

// Try to ensure no other glx functions are called directly.
#define glXChooseVisual not_implemented
#define glXCreateContext not_implemented
#define glXMakeCurrent not_implemented
#define glXCopyContext not_implemented
#define glXCreateGLXPixmap not_implemented
#define glXDestroyGLXPixmap not_implemented
#define glXQueryExtension not_implemented
#define glXGetConfig not_implemented
#define glXGetCurrentContext not_implemented
#define glXGetCurrentDrawable not_implemented
#define glXWaitGL not_implemented
#define glXWaitX not_implemented
#define glXUseXFont not_implemented
#define glXGetCurrentDisplay not_implemented
#define glXGetFBConfigAttrib not_implemented
#define glXGetFBConfigs not_implemented
#define glXCreatePixmap not_implemented
#define glXDestroyPixmap not_implemented
#define glXCreatePbuffer not_implemented
#define glXDestroyPbuffer not_implemented
#define glXQueryDrawable not_implemented
#define glXGetCurrentReadDrawable not_implemented
#define glXQueryContext not_implemented
#define glXSelectEvent not_implemented
#define glXGetSelectedEvent not_implemented



#define X11_GL_LOAD_SYMBOL(f, handle, name) real_##name = (real_##name##_t)f(handle, #name)

bool GLX::LoadSymbols(OpDLL *libGL)
{
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXDestroyContext);
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXIsDirect);
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXQueryVersion);
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXSwapBuffers);
	// GLX 1.1
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXGetClientString);
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXQueryServerString);
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXQueryExtensionsString);
	// GLX 1.3
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXCreateWindow);
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXDestroyWindow);
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXMakeContextCurrent);
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXCreateNewContext);
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXChooseFBConfig);
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXGetVisualFromFBConfig);
	// GLX 1.4 (or GLX_ARB_get_proc_address)
	X11_GL_LOAD_SYMBOL(Sym, libGL, glXGetProcAddress);
	// GLX_AMD_gpu_association
	real_glXGetContextGPUIDAMD = NULL;
	real_glXGetGPUInfoAMD = NULL;
	// GLX_ARB_create_context
	real_glXCreateContextAttribsARB = NULL;

	bool result =
		real_glXDestroyContext &&
		real_glXIsDirect &&
		real_glXQueryVersion &&
		real_glXSwapBuffers &&
		real_glXGetClientString &&
		real_glXQueryServerString &&
		real_glXQueryExtensionsString &&
		real_glXCreateWindow &&
		real_glXDestroyWindow &&
		real_glXMakeContextCurrent &&
		real_glXCreateNewContext &&
		real_glXChooseFBConfig &&
		real_glXGetVisualFromFBConfig &&
		real_glXGetProcAddress;

	return result;
}

#ifdef VEGA_GL_DEBUG
namespace
{
	void dbg_glXDestroyContext (Display *dpy, GLXContext ctx)
	{
		OP_NEW_DBG("glXDestroyContext", "opengl");
		OP_DBG(("(%p)", ctx));
		real_glXDestroyContext(dpy, ctx);
	}
	Bool dbg_glXIsDirect (Display *dpy, GLXContext ctx)
	{
		OP_NEW_DBG("glXIsDirect", "opengl");
		OP_DBG(("(%p)", ctx));
		return real_glXIsDirect(dpy, ctx);
	}
	Bool dbg_glXQueryVersion (Display *dpy, int *major, int *minor)
	{
		OP_NEW_DBG("glXQueryVersion()", "opengl");
		return real_glXQueryVersion(dpy, major, minor);
	}
	void dbg_glXSwapBuffers (Display *dpy, GLXDrawable drawable)
	{
		OP_NEW_DBG("glXSwapBuffers", "opengl");
		OP_DBG(("(%p)", drawable));
		real_glXSwapBuffers(dpy, drawable);
	}
	// GLX 1.1
	const char * dbg_glXGetClientString (Display *dpy, int name)
	{
		OP_NEW_DBG("glXGetClientString", "opengl");
		OP_DBG(("(%d)", name));
		return real_glXGetClientString(dpy, name);
	}
	const char * dbg_glXQueryServerString (Display *dpy, int screen, int name)
	{
		OP_NEW_DBG("glXQueryServerString", "opengl");
		OP_DBG(("(%d, %d)", screen, name));
		return real_glXQueryServerString(dpy, screen, name);
	}
	const char * dbg_glXQueryExtensionsString (Display *dpy, int screen)
	{
		OP_NEW_DBG("glXQueryExtensionsString", "opengl");
		OP_DBG(("(%d)", screen));
		return real_glXQueryExtensionsString(dpy, screen);
	}
	// GLX 1.3
	GLXWindow dbg_glXCreateWindow (Display *dpy, GLXFBConfig config, Window win, const int *attrib_list)
	{
		OP_NEW_DBG("glXCreateWindow", "opengl");
		OP_DBG(("(%d, %p, (attrib_list))", config, win));
		GLXWindow ret = real_glXCreateWindow(dpy, config, win, attrib_list);
		OP_DBG((" -> %x", ret));
		return ret;
	}
	void dbg_glXDestroyWindow (Display *dpy, GLXWindow win)
	{
		OP_NEW_DBG("glXDestroyWindow", "opengl");
		OP_DBG(("(%p)", win));
		real_glXDestroyWindow(dpy, win);
	}
	Bool dbg_glXMakeContextCurrent (Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx)
	{
		OP_NEW_DBG("glXMakeContextCurrent", "opengl");
		OP_DBG(("(%p, %p, %p)", draw, read, ctx));
		return real_glXMakeContextCurrent(dpy, draw, read, ctx);
	}
	GLXContext dbg_glXCreateNewContext (Display *dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct)
	{
		OP_NEW_DBG("glXCreateNewContext", "opengl");
		OP_DBG(("(%d, %d, %p, %d)", config, render_type, share_list, direct));
		return real_glXCreateNewContext(dpy, config, render_type, share_list, direct);
	}
	GLXFBConfig * dbg_glXChooseFBConfig (Display *dpy, int screen, const int *attrib_list, int *nelements)
	{
		OP_NEW_DBG("glXChooseFBConfig", "opengl");
		OP_DBG(("(%d)", screen));
		return real_glXChooseFBConfig(dpy, screen, attrib_list, nelements);
	}
	XVisualInfo * dbg_glXGetVisualFromFBConfig (Display *dpy, GLXFBConfig config)
	{
		OP_NEW_DBG("glXGetVisualFromFBConfig", "opengl");
		OP_DBG(("(%d)", config));
		return real_glXGetVisualFromFBConfig(dpy, config);
	}
	// GLX 1.4 (or GLX_ARB_get_proc_address)
	GLX::voidfunc_t dbg_glXGetProcAddress (const GLubyte *procname)
	{
		OP_NEW_DBG("glXGetProcAddress", "opengl");
		OP_DBG(("(%s)", procname));
		return real_glXGetProcAddress(procname);
	}

	unsigned int dbg_glXGetContextGPUIDAMD(GLXContext ctx)
	{
		OP_NEW_DBG("glXGetContextGPUIDAMD", "opengl");
		OP_DBG(("(%p)", ctx));
		unsigned int ret = real_glXGetContextGPUIDAMD(ctx);
		OP_DBG((" -> %u", ret));
		return ret;
	}

	int dbg_glXGetGPUInfoAMD(unsigned int id, int property, GLenum dataType, unsigned int size, void *data)
	{
		OP_NEW_DBG("glXGetGPUInfoAMD", "opengl");
		OP_DBG(("(%u, %d, %d, %u, %p)", id, property, dataType, size, data));
		int ret = real_glXGetGPUInfoAMD(id, property, dataType, size, data);
		OP_DBG((" -> %d, data: [", ret));
		void * next = data;
		for (int i = 0; i < ret; i++)
		{
			if (i != 0)
				OP_DBG((", "));
			switch (dataType)
			{
				case GL_UNSIGNED_INT:
				{
					GLuint * d = reinterpret_cast<GLuint*>(next);
					OP_DBG(("%u", *d));
					d ++;
					next = d;
				}
				break;
				default:
					OP_DBG(("?"));
					break;
			}
		}
		OP_DBG(("]"));
		return ret;
	}

	GLXContext dbg_glXCreateContextAttribsARB(Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list)
	{
		OP_NEW_DBG("glXCreateContextAttribsARB", "opengl");
		OP_DBG(("(%d, %p, %d, [", config, share_context, direct));
		if (attrib_list)
			for (size_t i = 0; attrib_list[i] != None; i++)
			{
				if (i == 0)
					OP_DBG(("%d", attrib_list[i]));
				else
					OP_DBG((", %d", attrib_list[i]));
			}
		OP_DBG(("])"));

		GLXContext ret = real_glXCreateContextAttribsARB(dpy, config, share_context, direct, attrib_list);
		OP_DBG((" -> %p", ret));
		return ret;
	}
} // local namespace for dbg_ versions
#endif // VEGA_GL_DEBUG


/* Type conversions between x11_gl.h's types and GL/glx.h's types.
 *
 * All type conversions (except GLXWindow) shall go through one of
 * these functions!  (So that if we need to change it, they are easy
 * to find).
 *
 * GLXContext is typedeffed as a pointer to a struct, while
 * GLX::Context is a void*.  Straight casts should be fine.
 *
 * GLXFBConfig is typedeffed as a pointer to a struct, while
 * GLX::FBConfig is a void*.  Straight casts should be fine.
 *
 * Both GLXWindow and GLX::Window are typedeffed to XID.
 * These are just used as-is in the code below.
 */

static GLXContext glxcontext(GLX::Context ctx)
{
	return reinterpret_cast<GLXContext>(ctx);
}

static GLX::Context operaglxcontext(GLXContext ctx)
{
	return ctx;
}

static GLXFBConfig glxfbconfig(GLX::FBConfig config)
{
	return reinterpret_cast<GLXFBConfig>(config);
}

static GLX::FBConfig * operaglxfbconfigarray(GLXFBConfig * confarray)
{
	return reinterpret_cast<GLX::FBConfig*>(confarray);
}


void GLX::DestroyContext(X11Types::Display *dpy, GLX::Context ctx)
{
	glXDestroyContext(dpy, glxcontext(ctx));
}

X11Types::Bool GLX::IsDirect(X11Types::Display *dpy, GLX::Context ctx)
{
	return glXIsDirect(dpy, glxcontext(ctx));
}

X11Types::Bool GLX::QueryVersion(X11Types::Display *dpy, int *major, int *minor)
{
	return glXQueryVersion(dpy, major, minor);
}

void GLX::SwapBuffers(X11Types::Display *dpy, GLX::Window drawable)
{
	glXSwapBuffers(dpy, drawable);
}

const char * GLX::GetClientString(X11Types::Display *dpy, int name)
{
	return glXGetClientString(dpy, name);
}

const char * GLX::QueryServerString(X11Types::Display *dpy, int screen, int name)
{
	return glXQueryServerString(dpy, screen, name);
}

const char * GLX::QueryExtensionsString(X11Types::Display *dpy, int screen)
{
	return glXQueryExtensionsString(dpy, screen);
}

GLX::Window GLX::CreateWindow(X11Types::Display *dpy, GLX::FBConfig config, X11Types::Window win, const int *attrib_list)
{
	return glXCreateWindow(dpy, glxfbconfig(config), win, attrib_list);
}

void GLX::DestroyWindow(X11Types::Display *dpy, GLX::Window win)
{
	glXDestroyWindow(dpy, win);
}

X11Types::Bool GLX::MakeContextCurrent(X11Types::Display *dpy, GLX::Window draw, GLX::Window read, GLX::Context ctx)
{
	return glXMakeContextCurrent(dpy, draw, read, glxcontext(ctx));
}

GLX::Context GLX::CreateNewContext(X11Types::Display *dpy, GLX::FBConfig config, int render_type, GLX::Context share_list, X11Types::Bool direct)
{
	return operaglxcontext(glXCreateNewContext(dpy, glxfbconfig(config), render_type, glxcontext(share_list), direct));
}

GLX::FBConfig * GLX::ChooseFBConfig(X11Types::Display *dpy, int screen, const int *attrib_list, int *nelements)
{
	return operaglxfbconfigarray(glXChooseFBConfig(dpy, screen, attrib_list, nelements));
}

XVisualInfo * GLX::GetVisualFromFBConfig(X11Types::Display *dpy, GLX::FBConfig config)
{
	return glXGetVisualFromFBConfig(dpy, glxfbconfig(config));
}

GLX::voidfunc_t GLX::GetProcAddress(const char *procname)
{
	return glXGetProcAddress(reinterpret_cast<const GLubyte*>(procname));
}

static bool has_extension(const char * haystack, const char * needle)
{
	if (haystack == NULL)
		return false;
	const char * c_haystack = (const char *)haystack;
	int needlelen = op_strlen(needle);
	const char * cand = op_strstr(c_haystack, needle);
	while (cand != NULL)
	{
		if ((cand == c_haystack || cand[-1] == ' ') &&
			(cand[needlelen] == 0 || cand[needlelen] == ' '))
			return true;
		cand = op_strstr(cand + 1, needle);
	}
	return false;
}

bool GLX::HasExtensionAMDGpuAssociation(X11Types::Display *dpy, int screen)
{
	static bool has_tested = false;
	if (has_tested)
		return real_glXGetContextGPUIDAMD != NULL;
	/* GLX_AMD_gpu_association seems to be a pure client extension
	 * (i.e. it is sufficient that the string occurs in the client
	 * extension string.  It does not need to be present in
	 * GLXQueryExtensionsString()).  It would be nice if this was
	 * actually documented somewhere.
	 */
	const char * exts = GetClientString(dpy, GLX_EXTENSIONS);
	if (!has_extension(exts, "GLX_AMD_gpu_association"))
	{
		has_tested = true;
		return false;
	}
	real_glXGetContextGPUIDAMD = (unsigned int(*)(GLXContext))GetProcAddress("glXGetContextGPUIDAMD");
	real_glXGetGPUInfoAMD = (int(*)(unsigned int, int, GLenum, unsigned int, void*))GetProcAddress("glXGetGPUInfoAMD");
	if (!real_glXGetContextGPUIDAMD ||
		!real_glXGetGPUInfoAMD)
	{
		real_glXGetContextGPUIDAMD = NULL;
		real_glXGetGPUInfoAMD = NULL;
	}
	has_tested = true;
	return real_glXGetContextGPUIDAMD != NULL;
}

unsigned int GLX::GetContextGPUIDAMD(GLX::Context ctx)
{
	// Always check for the existence of the extension before using it
	OP_ASSERT(real_glXGetContextGPUIDAMD);
	return glXGetContextGPUIDAMD(glxcontext(ctx));
}

int GLX::GetGPUInfoAMD(unsigned int id, int property, int dataType, unsigned int size, void *data)
{
	// Always check for the existence of the extension before using it
	OP_ASSERT(real_glXGetContextGPUIDAMD);
	return glXGetGPUInfoAMD(id, property, dataType, size, data);
}


bool GLX::HasExtensionARBCreateContext(X11Types::Display * dpy, int screen)
{
	static bool has_tested = false;
	if (has_tested)
		return real_glXCreateContextAttribsARB != NULL;
	const char * exts = glXQueryExtensionsString(dpy, screen);
	if (!has_extension(exts, "GLX_ARB_create_context"))
	{
		has_tested = true;
		return false;
	}
	real_glXCreateContextAttribsARB = (GLXContext(*)(X11Types::Display*,GLXFBConfig,GLXContext,X11Types::Bool,const int*))GetProcAddress("glXCreateContextAttribsARB");
	has_tested = true;
	return real_glXCreateContextAttribsARB != NULL;
}

GLX::Context GLX::CreateContextAttribsARB(X11Types::Display * dpy, GLX::FBConfig config, GLX::Context share_context, X11Types::Bool direct, const int * attrib_list)
{
	OP_ASSERT(real_glXCreateContextAttribsARB);
	return operaglxcontext(glXCreateContextAttribsARB(dpy, glxfbconfig(config), glxcontext(share_context), direct, attrib_list));
}

#endif // defined(VEGA_BACKEND_OPENGL)
