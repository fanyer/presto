/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAGLAPI_H
#define VEGAGLAPI_H

#ifdef VEGA_OPENGLES

# include VEGA_OPENGLES_HEADER
# include VEGA_OPENGLES_EXTENSIONS_HEADER

# define VEGA_GLREALSYM(sym) gl##sym

#else

# if defined GL_VERSION_1_1 || defined GL_TRIANGLES
/* Most likely <GL/gl.h> defines at least one of the above.  In
 * principle, neither need to be defined (GL_TRIANGLES is pretty much
 * necessary, but could be an enum or a const int).  Feel free to
 * suggest better tests for whether any OpenGL headers have been
 * included.
 *
 * vegaglapi.h is intended to supply all of the OpenGL symbols needed.
 * This allows us to more easily check that we're doing the right
 * thing.  Both by visual inspection and by instrumenting.
 *
 * However, if the platform's OpenGL headers are included as well,
 * then code may (accidentally) circumvent whatever scaffolding we
 * have in place.  Which is a Bad Thing.
 *
 * The correct solution to this problem is to not include any OpenGL
 * headers anywhere in opera.  This is typically not feasible, since
 * the platform code needs to set up the OpenGL context.  However, in
 * most cases the code that needs to include OpenGL headers can be
 * made very small and easily controlled.
 */
#  error "There should be no need for including OpenGL header files."
# endif

/* I think most, maybe all, of the type declarations for OpenGL and
 * GLX are in the gl.spec file and its companions.  Thus they should
 * probably be generated from those files.  --eirik 2012-02-27
 */

typedef unsigned int    GLenum;
typedef unsigned char   GLboolean;
typedef unsigned int    GLbitfield;
typedef void            GLvoid;
typedef signed char     GLbyte;         /* 1-byte signed */
typedef short           GLshort;        /* 2-byte signed */
typedef int             GLint;          /* 4-byte signed */
typedef unsigned char   GLubyte;        /* 1-byte unsigned */
typedef unsigned short  GLushort;       /* 2-byte unsigned */
typedef unsigned int    GLuint;         /* 4-byte unsigned */
typedef int             GLsizei;        /* 4-byte signed */
typedef float           GLfloat;        /* single precision float */
typedef float           GLclampf;       /* single precision float in [0,1] */
typedef double          GLdouble;       /* double precision float */
typedef double          GLclampd;       /* double precision float in [0,1] */
typedef char            GLchar;

// Apple provides GLintptr & GLsizeiptr in its OpenGL implementation
// and they define them as long. On other platforms those types are defined as ptrdiff_t.
#ifdef _MACINTOSH_
typedef long            GLintptr;
typedef long            GLsizeiptr;
#else
typedef ptrdiff_t       GLintptr;
typedef ptrdiff_t       GLsizeiptr;
#endif

#ifdef OPENGL_REGISTRY
// This is copied from gl.spec, and thus requires the ifdef.
typedef struct __GLsync *GLsync;
#endif

// If we're compiling with OpenGL GL_RGB565 doesn't exist so we use GL_RGB instead.
# define GL_RGB565 GL_RGB

// These functions have different names in OpenGL compared to OpenGL
// ES. In our code we use the OpenGL ES versions.

# define glClearDepthf glClearDepth
# define glDepthRangef glDepthRange

# define VEGA_GLREALSYM(sym) (VEGAGlDevice::GetGlAPI()->m_##sym)

#endif // !VEGA_OPENGLES

# ifdef VEGA_GL_DEBUG
#  define VEGA_GLSYM(sym) (VEGA_GLREALSYM(sym) ? VEGAGlAPI::debug_##sym : NULL)
# else
#  define VEGA_GLSYM(sym) VEGA_GLREALSYM(sym)
# endif // VEGA_GL_DEBUG

OP_STATUS CheckGLError();

# ifdef VEGA_BACKENDS_OPENGL_REGISTRY
#include "vegaglapi_h.inc"
# endif // VEGA_BACKENDS_OPENGL_REGISTRY

#endif // VEGAGLAPI_H
