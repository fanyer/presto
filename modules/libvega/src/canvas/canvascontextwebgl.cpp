/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT

#include "modules/libvega/src/canvas/canvascontextwebgl.h"
#include "modules/dom/src/canvas/webglconstants.h"
#include "modules/pi/OpBitmap.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegavertexbufferutils.h"
#include "modules/libvega/src/canvas/canvaswebglprogram.h"
#include "modules/libvega/src/canvas/canvaswebglshader.h"
#include "modules/webgl/src/wgl_validator.h"

#ifdef VEGA_2DDEVICE
# include "modules/libvega/src/vegabackend_hw2d.h"
#endif

#ifdef VEGA_USE_FLOATS
#define DOUBLE_TO_VEGA(x) ((VEGA_FIX)(x))
#define VEGA_TO_DOUBLE(x) (double(x))
#else
#define DOUBLE_TO_VEGA(x) ((VEGA_FIX)((x)*(double)(1<<VEGA_FIX_DECBITS) + .5))
#define VEGA_TO_DOUBLE(x) ((double)((double)(x) / (double)(1<<VEGA_FIX_DECBITS)))
#endif
#define RGB_TO_VEGA(col) ((col&0xff00ff00) | ((col>>16)&0xff) | ((col&0xff)<<16))
#define VEGA_TO_RGB(col) RGB_TO_VEGA(col)

static inline float Clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static inline bool IntAdditionOverflows(int a, int b)
{
	return (a > 0 ? (b > INT_MAX - a) : (b < INT_MIN - a));
}

static inline bool IntMultiplicationOverflows(unsigned int a, unsigned int b)
{
	unsigned product = a * b;
	return !(!a || !b || product / a == b);
}

#define RETURN_IF_WGL_ERROR(expr) \
	do { \
		unsigned int glerr = (expr); \
		if (glerr != WEBGL_NO_ERROR) { \
			raiseError(glerr); \
			return OpStatus::OK; \
		} \
	} while (0)

/** Maps a WebGL mode (e.g. WEBGL_TRIANGLES) to the corresponding VEGA
	primitive. */
static const VEGA3dDevice::Primitive GLModeToVegaPrimitive[N_WEBGL_MODES]
    = { VEGA3dDevice::PRIMITIVE_POINT,          // WEBGL_POINTS
        VEGA3dDevice::PRIMITIVE_LINE,           // WEBGL_LINES
        VEGA3dDevice::PRIMITIVE_LINE_LOOP,      // WEBGL_LINE_LOOP
        VEGA3dDevice::PRIMITIVE_LINE_STRIP,     // WEBGL_LINE_STRIP
        VEGA3dDevice::PRIMITIVE_TRIANGLE,       // WEBGL_TRIANGLES
        VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, // WEBGL_TRIANGLE_STRIP
        VEGA3dDevice::PRIMITIVE_TRIANGLE_FAN    // WEBGL_TRIANGLE_FAN
      };

/** Maps a WebGL mode (e.g. WEBGL_TRIANGLES) to the minimum number
    of vertices required to assemble a single primitive in that mode. */
static const int GLModeToMinVertexCount[N_WEBGL_MODES]
    = { 1, // WEBGL_POINTS
        2, // WEBGL_LINES
        2, // WEBGL_LINE_LOOP
        2, // WEBGL_LINE_STRIP
        3, // WEBGL_TRIANGLES
        3, // WEBGL_TRIANGLE_STRIP
        3  // WEBGL_TRIANGLE_FAN
      };

/** Maps WebGL types (e.g. WEBGL_FLOAT) to their size in bytes. */
static inline unsigned GLTypeToSize(unsigned glType)
{
	static const unsigned GLTypeToSize_[N_WEBGL_TYPES]
	    = { 1, // WEBGL_BYTE
	        1, // WEBGL_UNSIGNED_BYTE
	        2, // WEBGL_SHORT
	        2, // WEBGL_UNSIGNED_SHORT
	        4, // WEBGL_INT
	        4, // WEBGL_UNSIGNED_INT
	        4  // WEBGL_FLOAT
	      };

	glType -= WEBGL_BYTE;
	OP_ASSERT(glType < N_WEBGL_TYPES);
	return GLTypeToSize_[glType];
}

/** Maps a WebGL type (e.g. WEBGL_FLOAT) and a size in bytes to a corresponding
    VEGA vertex layout type. */
static inline VEGA3dVertexLayout::VertexType GLToVegaAttrib(unsigned glType, unsigned size)
{
	static const VEGA3dVertexLayout::VertexType GLToVegaAttrib_[N_WEBGL_TYPES][4]
	    = { // WEBGL_BYTE
	        { VEGA3dVertexLayout::BYTE1, VEGA3dVertexLayout::BYTE2,
	          VEGA3dVertexLayout::BYTE3, VEGA3dVertexLayout::BYTE4 },

	        // WEBGL_UNSIGNED_BYTE
	        { VEGA3dVertexLayout::UBYTE1, VEGA3dVertexLayout::UBYTE2,
	          VEGA3dVertexLayout::UBYTE3, VEGA3dVertexLayout::UBYTE4 },

	        // WEBGL_SHORT
	        { VEGA3dVertexLayout::SHORT1, VEGA3dVertexLayout::SHORT2,
	          VEGA3dVertexLayout::SHORT3, VEGA3dVertexLayout::SHORT4 },

	        // WEBGL_UNSIGNED_SHORT
	        { VEGA3dVertexLayout::USHORT1, VEGA3dVertexLayout::USHORT2,
	          VEGA3dVertexLayout::USHORT3, VEGA3dVertexLayout::USHORT4 },

	        /* WEBGL_INT
	           (We should never get WEBGL_INT/WEBGL_UNSIGNED_INT here since
	           vertexAttribPointer() does not accept them. Flag an internal error
	           by returning NO_TYPE.) */
	        { VEGA3dVertexLayout::NO_TYPE, VEGA3dVertexLayout::NO_TYPE,
	          VEGA3dVertexLayout::NO_TYPE, VEGA3dVertexLayout::NO_TYPE },

	        // WEBGL_UNSIGNED_INT
	        { VEGA3dVertexLayout::NO_TYPE, VEGA3dVertexLayout::NO_TYPE,
	          VEGA3dVertexLayout::NO_TYPE, VEGA3dVertexLayout::NO_TYPE },

	        // WEBGL_FLOAT
	        { VEGA3dVertexLayout::FLOAT1, VEGA3dVertexLayout::FLOAT2,
	          VEGA3dVertexLayout::FLOAT3, VEGA3dVertexLayout::FLOAT4 }
	      };

	OP_ASSERT(1 <= size && size <= 4);
	glType -= WEBGL_BYTE;
	OP_ASSERT(glType < N_WEBGL_TYPES);
	VEGA3dVertexLayout::VertexType result = GLToVegaAttrib_[glType][size - 1];
	OP_ASSERT(result != VEGA3dVertexLayout::NO_TYPE);
	return result;
}

static inline VEGA3dRenderState::BlendOp GLToVegaBlendOp(unsigned int op)
{
	switch (op)
	{
	case WEBGL_FUNC_ADD:
		return VEGA3dRenderState::BLEND_OP_ADD;
	case WEBGL_FUNC_SUBTRACT:
		return VEGA3dRenderState::BLEND_OP_SUB;
	case WEBGL_FUNC_REVERSE_SUBTRACT:
		return VEGA3dRenderState::BLEND_OP_REVERSE_SUB;
	default:
		break;
	}
	return VEGA3dRenderState::NUM_BLEND_OPS;
}
static const unsigned int VegaToGLBlendOp[] =
{
	WEBGL_FUNC_ADD,
	WEBGL_FUNC_SUBTRACT,
	WEBGL_FUNC_REVERSE_SUBTRACT
};
static inline VEGA3dRenderState::BlendWeight GLToVegaBlendWeight(unsigned int blend)
{
	switch (blend)
	{
	case WEBGL_ZERO:
		return VEGA3dRenderState::BLEND_ZERO;
	case WEBGL_ONE:
		return VEGA3dRenderState::BLEND_ONE;
	case WEBGL_SRC_COLOR:
		return VEGA3dRenderState::BLEND_SRC_COLOR;
	case WEBGL_ONE_MINUS_SRC_COLOR:
		return VEGA3dRenderState::BLEND_ONE_MINUS_SRC_COLOR;
	case WEBGL_DST_COLOR:
		return VEGA3dRenderState::BLEND_DST_COLOR;
	case WEBGL_ONE_MINUS_DST_COLOR:
		return VEGA3dRenderState::BLEND_ONE_MINUS_DST_COLOR;
	case WEBGL_SRC_ALPHA:
		return VEGA3dRenderState::BLEND_SRC_ALPHA;
	case WEBGL_ONE_MINUS_SRC_ALPHA:
		return VEGA3dRenderState::BLEND_ONE_MINUS_SRC_ALPHA;
	case WEBGL_DST_ALPHA:
		return VEGA3dRenderState::BLEND_DST_ALPHA;
	case WEBGL_ONE_MINUS_DST_ALPHA:
		return VEGA3dRenderState::BLEND_ONE_MINUS_DST_ALPHA;
	case WEBGL_CONSTANT_COLOR:
		return VEGA3dRenderState::BLEND_CONSTANT_COLOR;
	case WEBGL_ONE_MINUS_CONSTANT_COLOR:
		return VEGA3dRenderState::BLEND_ONE_MINUS_CONSTANT_COLOR;
	case WEBGL_CONSTANT_ALPHA:
		return VEGA3dRenderState::BLEND_CONSTANT_ALPHA;
	case WEBGL_ONE_MINUS_CONSTANT_ALPHA:
		return VEGA3dRenderState::BLEND_ONE_MINUS_CONSTANT_ALPHA;
	case WEBGL_SRC_ALPHA_SATURATE:
		return VEGA3dRenderState::BLEND_SRC_ALPHA_SATURATE;
	default:
		break;
	}
	return VEGA3dRenderState::NUM_BLEND_WEIGHTS;
}
static const unsigned int VegaToGLBlendWeight[] =
{
	WEBGL_ONE,
	WEBGL_ZERO,
	WEBGL_SRC_ALPHA,
	WEBGL_ONE_MINUS_SRC_ALPHA,
	WEBGL_DST_ALPHA,
	WEBGL_ONE_MINUS_DST_ALPHA,
	WEBGL_SRC_COLOR,
	WEBGL_ONE_MINUS_SRC_COLOR,
	WEBGL_DST_COLOR,
	WEBGL_ONE_MINUS_DST_COLOR,
	WEBGL_CONSTANT_COLOR,
	WEBGL_ONE_MINUS_CONSTANT_COLOR,
	WEBGL_CONSTANT_ALPHA,
	WEBGL_ONE_MINUS_CONSTANT_ALPHA,
	WEBGL_SRC_ALPHA_SATURATE
};
static inline VEGA3dRenderState::StencilFunc GLToVegaStencilFunc(unsigned int func)
{
	switch (func)
	{
	case WEBGL_NEVER:
		return VEGA3dRenderState::STENCIL_NEVER;
	case WEBGL_LESS:
		return VEGA3dRenderState::STENCIL_LESS;
	case WEBGL_LEQUAL:
		return VEGA3dRenderState::STENCIL_LEQUAL;
	case WEBGL_GREATER:
		return VEGA3dRenderState::STENCIL_GREATER;
	case WEBGL_GEQUAL:
		return VEGA3dRenderState::STENCIL_GEQUAL;
	case WEBGL_EQUAL:
		return VEGA3dRenderState::STENCIL_EQUAL;
	case WEBGL_NOTEQUAL:
		return VEGA3dRenderState::STENCIL_NOTEQUAL;
	case WEBGL_ALWAYS:
		return VEGA3dRenderState::STENCIL_ALWAYS;
	default:
		return VEGA3dRenderState::NUM_STENCIL_FUNCS;
	}
}
static const unsigned int VegaToGLStencilFunc[] =
{
	WEBGL_LESS,
	WEBGL_LEQUAL,
	WEBGL_GREATER,
	WEBGL_GEQUAL,
	WEBGL_EQUAL,
	WEBGL_NOTEQUAL,
	WEBGL_ALWAYS,
	WEBGL_NEVER
};
static inline VEGA3dRenderState::StencilOp GLToVegaStencilOp(unsigned int op)
{
	switch (op)
	{
	case WEBGL_KEEP:
		return VEGA3dRenderState::STENCIL_KEEP;
	case WEBGL_ZERO:
		return VEGA3dRenderState::STENCIL_ZERO;
	case WEBGL_REPLACE:
		return VEGA3dRenderState::STENCIL_REPLACE;
	case WEBGL_INCR:
		return VEGA3dRenderState::STENCIL_INCR;
	case WEBGL_INCR_WRAP:
		return VEGA3dRenderState::STENCIL_INCR_WRAP;
	case WEBGL_DECR:
		return VEGA3dRenderState::STENCIL_DECR;
	case WEBGL_DECR_WRAP:
		return VEGA3dRenderState::STENCIL_DECR_WRAP;
	case WEBGL_INVERT:
		return VEGA3dRenderState::STENCIL_INVERT;
	default:
		return VEGA3dRenderState::NUM_STENCIL_OPS;
	}
}
static const unsigned int VegaToGLStencilOp[] =
{
	WEBGL_KEEP,
	WEBGL_INVERT,
	WEBGL_INCR,
	WEBGL_DECR,
	WEBGL_ZERO,
	WEBGL_REPLACE,
	WEBGL_INCR_WRAP,
	WEBGL_DECR_WRAP
};
static inline VEGA3dRenderState::DepthFunc GLToVegaDepthFunc(unsigned int func)
{
	switch (func)
	{
	case WEBGL_NEVER:
		return VEGA3dRenderState::DEPTH_NEVER;
	case WEBGL_LESS:
		return VEGA3dRenderState::DEPTH_LESS;
	case WEBGL_EQUAL:
		return VEGA3dRenderState::DEPTH_EQUAL;
	case WEBGL_LEQUAL:
		return VEGA3dRenderState::DEPTH_LEQUAL;
	case WEBGL_GREATER:
		return VEGA3dRenderState::DEPTH_GREATER;
	case WEBGL_NOTEQUAL:
		return VEGA3dRenderState::DEPTH_NOTEQUAL;
	case WEBGL_GEQUAL:
		return VEGA3dRenderState::DEPTH_GEQUAL;
	case WEBGL_ALWAYS:
		return VEGA3dRenderState::DEPTH_ALWAYS;
	default:
		return VEGA3dRenderState::NUM_DEPTH_FUNCS;
	}
}
static const unsigned int VegaToGLDepthFunc[] =
{
	WEBGL_NEVER,
	WEBGL_EQUAL,
	WEBGL_LESS,
	WEBGL_LEQUAL,
	WEBGL_GREATER,
	WEBGL_GEQUAL,
	WEBGL_NOTEQUAL,
	WEBGL_ALWAYS
};

static inline OP_STATUS VegaProgramConstantToWebGLParameter(VEGA3dShaderProgram::ConstantType type, CanvasWebGLParameter::Type &wglType, BOOL &integer)
{
	switch (type)
	{
	case VEGA3dShaderProgram::SHDCONST_INT:
	case VEGA3dShaderProgram::SHDCONST_INT2:
	case VEGA3dShaderProgram::SHDCONST_INT3:
	case VEGA3dShaderProgram::SHDCONST_INT4:
	case VEGA3dShaderProgram::SHDCONST_BOOL:
	case VEGA3dShaderProgram::SHDCONST_BOOL2:
	case VEGA3dShaderProgram::SHDCONST_BOOL3:
	case VEGA3dShaderProgram::SHDCONST_BOOL4:
	case VEGA3dShaderProgram::SHDCONST_SAMP2D:
	case VEGA3dShaderProgram::SHDCONST_SAMPCUBE:
		integer = TRUE;
		break;
	default:
		integer = FALSE;
		break;
	};

	OP_STATUS status = OpStatus::OK;
	switch (type)
	{
	case VEGA3dShaderProgram::SHDCONST_FLOAT:	wglType = CanvasWebGLParameter::PARAM_FLOAT;	break;
	case VEGA3dShaderProgram::SHDCONST_FLOAT2:	wglType = CanvasWebGLParameter::PARAM_FLOAT2;	break;
	case VEGA3dShaderProgram::SHDCONST_FLOAT3:	wglType = CanvasWebGLParameter::PARAM_FLOAT3;	break;
	case VEGA3dShaderProgram::SHDCONST_FLOAT4:	wglType = CanvasWebGLParameter::PARAM_FLOAT4;	break;
	case VEGA3dShaderProgram::SHDCONST_INT:		wglType = CanvasWebGLParameter::PARAM_INT;		break;
	case VEGA3dShaderProgram::SHDCONST_INT2:	wglType = CanvasWebGLParameter::PARAM_INT2;		break;
	case VEGA3dShaderProgram::SHDCONST_INT3:	wglType = CanvasWebGLParameter::PARAM_INT3;		break;
	case VEGA3dShaderProgram::SHDCONST_INT4:	wglType = CanvasWebGLParameter::PARAM_INT4;		break;
	case VEGA3dShaderProgram::SHDCONST_BOOL:	wglType = CanvasWebGLParameter::PARAM_BOOL;		break;
	case VEGA3dShaderProgram::SHDCONST_BOOL2:	wglType = CanvasWebGLParameter::PARAM_BOOL2;	break;
	case VEGA3dShaderProgram::SHDCONST_BOOL3:	wglType = CanvasWebGLParameter::PARAM_BOOL3;	break;
	case VEGA3dShaderProgram::SHDCONST_BOOL4:	wglType = CanvasWebGLParameter::PARAM_BOOL4;	break;
	case VEGA3dShaderProgram::SHDCONST_MAT2:	wglType = CanvasWebGLParameter::PARAM_MAT2;		break;
	case VEGA3dShaderProgram::SHDCONST_MAT3:	wglType = CanvasWebGLParameter::PARAM_MAT3;		break;
	case VEGA3dShaderProgram::SHDCONST_MAT4:	wglType = CanvasWebGLParameter::PARAM_MAT4;		break;
	case VEGA3dShaderProgram::SHDCONST_SAMP2D:	wglType = CanvasWebGLParameter::PARAM_INT;		break;
	case VEGA3dShaderProgram::SHDCONST_SAMPCUBE:wglType = CanvasWebGLParameter::PARAM_INT;		break;
	default:
		status = OpStatus::ERR;
		break;
	};

	return status;
}

static const unsigned int VegaToGLFilterMode[] =
{
	WEBGL_NEAREST,
	WEBGL_LINEAR,
	WEBGL_NEAREST_MIPMAP_NEAREST,
	WEBGL_LINEAR_MIPMAP_NEAREST,
	WEBGL_NEAREST_MIPMAP_LINEAR,
	WEBGL_LINEAR_MIPMAP_LINEAR
};

static const unsigned int VegaToGlShaderConst[] =
{
	WEBGL_FLOAT,
	WEBGL_FLOAT_VEC2,
	WEBGL_FLOAT_VEC3,
	WEBGL_FLOAT_VEC4,
	WEBGL_INT,
	WEBGL_INT_VEC2,
	WEBGL_INT_VEC3,
	WEBGL_INT_VEC4,
	WEBGL_BOOL,
	WEBGL_BOOL_VEC2,
	WEBGL_BOOL_VEC3,
	WEBGL_BOOL_VEC4,
	WEBGL_FLOAT_MAT2,
	WEBGL_FLOAT_MAT3,
	WEBGL_FLOAT_MAT4,
	WEBGL_SAMPLER_2D,
	WEBGL_SAMPLER_CUBE,
	0
};

static const unsigned int VegaToGLWrapMode[] =
{
	WEBGL_REPEAT,
	WEBGL_MIRRORED_REPEAT,
	WEBGL_CLAMP_TO_EDGE
};


static inline void VegaTextureColorFormatToGL(VEGA3dTexture::ColorFormat colfmt, unsigned int &fmt, unsigned int &type)
{
	switch (colfmt)
	{
	case VEGA3dTexture::FORMAT_RGBA8888:
		fmt = WEBGL_RGBA;
		type = WEBGL_UNSIGNED_BYTE;
		break;
	case VEGA3dTexture::FORMAT_RGBA4444:
		fmt = WEBGL_RGBA;
		type = WEBGL_UNSIGNED_SHORT_4_4_4_4;
		break;
	case VEGA3dTexture::FORMAT_RGBA5551:
		fmt = WEBGL_RGBA;
		type = WEBGL_UNSIGNED_SHORT_5_5_5_1;
		break;
	case VEGA3dTexture::FORMAT_RGB888:
		fmt = WEBGL_RGB;
		type = WEBGL_UNSIGNED_BYTE;
		break;
	case VEGA3dTexture::FORMAT_RGB565:
		fmt = WEBGL_RGB;
		type = WEBGL_UNSIGNED_SHORT_5_6_5;
		break;
	case VEGA3dTexture::FORMAT_ALPHA8:
		fmt = WEBGL_ALPHA;
		type = WEBGL_UNSIGNED_BYTE;
		break;
	case VEGA3dTexture::FORMAT_LUMINANCE8:
		fmt = WEBGL_LUMINANCE;
		type = WEBGL_UNSIGNED_BYTE;
		break;
	case VEGA3dTexture::FORMAT_LUMINANCE8_ALPHA8:
		fmt = WEBGL_LUMINANCE_ALPHA;
		type = WEBGL_UNSIGNED_BYTE;
		break;
	default:
		fmt = 0;
		type = 0;
	};
}

static inline VEGA3dTexture::ColorFormat GLFormatTypeToVegaTextureColorFormat(unsigned int fmt, unsigned int type)
{
	if (fmt == WEBGL_ALPHA && type == WEBGL_UNSIGNED_BYTE)
		return VEGA3dTexture::FORMAT_ALPHA8;
	else if (fmt == WEBGL_RGB && type == WEBGL_UNSIGNED_BYTE)
		return VEGA3dTexture::FORMAT_RGB888;
	else if (fmt == WEBGL_RGB && type == WEBGL_UNSIGNED_SHORT_5_6_5)
		return VEGA3dTexture::FORMAT_RGB565;
	else if (fmt == WEBGL_RGBA && type == WEBGL_UNSIGNED_BYTE)
		return VEGA3dTexture::FORMAT_RGBA8888;
	else if (fmt == WEBGL_RGBA && type == WEBGL_UNSIGNED_SHORT_4_4_4_4)
		return VEGA3dTexture::FORMAT_RGBA4444;
	else if (fmt == WEBGL_RGBA && type == WEBGL_UNSIGNED_SHORT_5_5_5_1)
		return VEGA3dTexture::FORMAT_RGBA5551;
	else if (fmt == WEBGL_LUMINANCE && type == WEBGL_UNSIGNED_BYTE)
		return VEGA3dTexture::FORMAT_LUMINANCE8;
	else if (fmt == WEBGL_LUMINANCE_ALPHA && type == WEBGL_UNSIGNED_BYTE)
		return VEGA3dTexture::FORMAT_LUMINANCE8_ALPHA8;
	return VEGA3dTexture::FORMAT_ALPHA8;
}

static inline void VegaColorBufferFormatToGL(VEGA3dRenderbufferObject::ColorFormat colfmt, unsigned int &fmt, unsigned int &type)
{
	switch (colfmt)
	{
	case VEGA3dRenderbufferObject::FORMAT_RGBA8: // RGBA8 is not supported on OpenGL ES, so be very careful about using it
		fmt = WEBGL_RGBA;
		type = WEBGL_UNSIGNED_BYTE;
		break;
	case VEGA3dRenderbufferObject::FORMAT_RGBA4:
		fmt = WEBGL_RGBA;
		type = WEBGL_UNSIGNED_SHORT_4_4_4_4;
		break;
	case VEGA3dRenderbufferObject::FORMAT_RGB565:
		fmt = WEBGL_RGB;
		type = WEBGL_UNSIGNED_SHORT_5_6_5;
		break;
	case VEGA3dRenderbufferObject::FORMAT_RGB5_A1:
		fmt = WEBGL_RGBA;
		type = WEBGL_UNSIGNED_SHORT_5_5_5_1;
		break;
	default:
		fmt = 0;
		type = 0;
	};
}

bool static IsValidUniformOrAttributeName(const uni_char* source)
{
	OP_ASSERT(source != NULL);

	// Uniform and attribute names may not be more than 256 characters long (see "Maximum Uniform and Attribute Location Lengths")
	if (uni_strlen(source) > 256)
		return false;

	while (*source)
	{
		// TAB to CR and SPACE to ~ except "$'@\`
		if (*source < 9 || *source > 126 || *source > 13 && *source < 32 || *source == 34 || *source == 36 || *source == 39 || *source == 64 || *source == 92 || *source == 96)
			return false;
		++source;
	}
	return true;
}

static BOOL enoughData(unsigned int width, unsigned int height, unsigned int bpp, unsigned int alignment, unsigned int dataLen)
{
	if (height == 0 || width == 0)
		return TRUE;

	if (IntMultiplicationOverflows(width, bpp))
		return FALSE;
	unsigned int lineSize = width*bpp;

	if (IntAdditionOverflows(lineSize, (alignment-1)))
		return FALSE;
	unsigned int stride = ((lineSize+alignment-1)/alignment)*alignment;

	if (IntMultiplicationOverflows(stride, (height-1)))
		return FALSE;
	unsigned int size = stride*(height-1);

	if (IntAdditionOverflows(size, lineSize))
		return FALSE;

	return size+lineSize <= dataLen;
}


CanvasContextWebGL::CanvasContextWebGL(Canvas *can, BOOL alpha, BOOL depth, BOOL stencil, BOOL antialias, BOOL premultipliedAlpha, BOOL preserveDrawingBuffer) : m_canvas(can),
	m_renderFramebuffer(NULL),
	m_renderTexture(NULL),
	m_vegaTarget(NULL),
	m_pendingClear(false),
	m_enabledExtensions(0),
	m_ctxAlpha(alpha!=FALSE),
	m_ctxDepth(depth!=FALSE),
	m_ctxStencil(stencil!=FALSE),
	m_ctxAntialias(antialias!=FALSE),
	m_ctxPremultipliedAlpha(premultipliedAlpha!=FALSE),
	m_ctxPreserveDrawingBuffer(preserveDrawingBuffer!=FALSE),
	m_ctxFullscreen(false),
	m_renderState(NULL),
	m_maxTextures(0),
	m_activeTexture(0),
	m_scissorX(0),
	m_scissorY(0),
	m_scissorW(0),
	m_scissorH(0),
	m_clearColor(0),
	m_clearDepth(1.f),
	m_clearStencil(0),
	m_cullFace(WEBGL_BACK),
	m_stencilRef(0),
	m_stencilRefBack(0),
	m_stencilMask(~0u),
	m_stencilMaskBack(~0u),
	m_stencilWriteMask(~0u),
	m_stencilWriteMaskBack(~0u),
	m_hintMode(WEBGL_DONT_CARE),
	m_oesStdDerivativesHintMode(WEBGL_DONT_CARE),
	m_packAlignment(4),
	m_unpackAlignment(4),
	m_unpackFlipY(false),
	m_unpackPremultiplyAlpha(false),
	m_unpackColorSpaceConversion(WEBGL_BROWSER_DEFAULT_WEBGL),
	m_program(NULL),
	m_arrayBuffer(NULL),
	m_elementArrayBuffer(NULL),
	m_framebuffer(NULL),
	m_renderbuffer(NULL),
	m_textures(NULL),
	m_cubeTextures(NULL),
	m_maxVertexAttribs(0),
	m_vertexAttribs(NULL),
	m_vertexLayout(NULL),
	m_viewportX(0),
	m_viewportY(0),
	m_viewportWidth(0),
	m_viewportHeight(0),
	m_viewportNear(0.f),
	m_viewportFar(1.f),
	m_errorCode(WEBGL_NO_ERROR),
#ifdef VEGA_2DDEVICE
	m_shader2d(NULL),
	m_vlayout2d(NULL),
#endif // VEGA_2DDEVICE
	m_1x1BlackTexture(NULL),
	m_1x1BlackCubeTexture(NULL)
{
}

CanvasContextWebGL::~CanvasContextWebGL()
{
	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
	for (unsigned int tex = 0; tex < m_maxTextures; ++tex)
	{
		vega3dDevice->setTexture(tex, NULL);
		vega3dDevice->setCubeTexture(tex, NULL);
	}
	VEGARefCount::DecRef(m_vertexLayout);
	VEGARefCount::DecRef(m_renderFramebuffer);
	VEGARefCount::DecRef(m_renderTexture);
	OP_DELETE(m_renderState);
	OP_DELETEA(m_vertexAttribs);
	OP_DELETEA(m_textures);
	OP_DELETEA(m_cubeTextures);
	VEGARefCount::DecRef(m_1x1BlackTexture);
	VEGARefCount::DecRef(m_1x1BlackCubeTexture);
#ifdef VEGA_2DDEVICE
	VEGARefCount::DecRef(m_shader2d);
	VEGARefCount::DecRef(m_vlayout2d);
#endif // VEGA_2DDEVICE
}

OP_STATUS CanvasContextWebGL::Construct()
{
	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;

	m_maxTextures = vega3dDevice->getMaxCombinedTextureUnits();
	m_maxVertexAttribs = vega3dDevice->getMaxVertexAttribs();

	UINT8 buf[] = { 0, 0, 0, 255 };
	RETURN_IF_ERROR(vega3dDevice->createTexture(&m_1x1BlackTexture, 1, 1, VEGA3dTexture::FORMAT_RGBA8888, false, false));
	RETURN_IF_ERROR(m_1x1BlackTexture->update(0, 0, 1, 1, buf));

	RETURN_IF_ERROR(vega3dDevice->createCubeTexture(&m_1x1BlackCubeTexture, 1, VEGA3dTexture::FORMAT_RGBA8888, false, false));
	RETURN_IF_ERROR(m_1x1BlackCubeTexture->update(0, 0, 1, 1, buf, 1, VEGA3dTexture::CUBE_SIDE_NEG_X));
	RETURN_IF_ERROR(m_1x1BlackCubeTexture->update(0, 0, 1, 1, buf, 1, VEGA3dTexture::CUBE_SIDE_POS_X));
	RETURN_IF_ERROR(m_1x1BlackCubeTexture->update(0, 0, 1, 1, buf, 1, VEGA3dTexture::CUBE_SIDE_NEG_Y));
	RETURN_IF_ERROR(m_1x1BlackCubeTexture->update(0, 0, 1, 1, buf, 1, VEGA3dTexture::CUBE_SIDE_POS_Y));
	RETURN_IF_ERROR(m_1x1BlackCubeTexture->update(0, 0, 1, 1, buf, 1, VEGA3dTexture::CUBE_SIDE_NEG_Z));
	RETURN_IF_ERROR(m_1x1BlackCubeTexture->update(0, 0, 1, 1, buf, 1, VEGA3dTexture::CUBE_SIDE_POS_Z));

#ifdef VEGA_2DDEVICE
	RETURN_IF_ERROR(vega3dDevice->createShaderProgram(&m_shader2d, VEGA3dShaderProgram::SHADER_VECTOR2D, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP));
	RETURN_IF_ERROR(vega3dDevice->createVega2dVertexLayout(&m_vlayout2d, VEGA3dShaderProgram::SHADER_VECTOR2D));
#endif // VEGA_2DDEVICE

	return OpStatus::OK;
}

void CanvasContextWebGL::GCTrace(ES_Runtime* runtime)
{
	if (m_arrayBuffer)
		runtime->GCMark(m_arrayBuffer->getESObject());
	if (m_elementArrayBuffer)
		runtime->GCMark(m_elementArrayBuffer->getESObject());
	if (m_framebuffer)
		runtime->GCMark(m_framebuffer->getESObject());
	if (m_renderbuffer)
		runtime->GCMark(m_renderbuffer->getESObject());
	if (m_program)
		runtime->GCMark(m_program->getESObject());
	unsigned int i;
	for (i = 0; i < m_maxVertexAttribs; ++i)
	{
		if (m_vertexAttribs[i].buffer)
			runtime->GCMark(m_vertexAttribs[i].buffer->getESObject());
	}
	for (i = 0; i < m_maxTextures; ++i)
	{
		if (m_textures[i])
			runtime->GCMark(m_textures[i]->getESObject());
		if (m_cubeTextures[i])
			runtime->GCMark(m_cubeTextures[i]->getESObject());
	}
}

void CanvasContextWebGL::ClearCanvas()
{
	m_canvas = NULL;
}

Canvas *CanvasContextWebGL::GetCanvas()
{
	return m_canvas;
}

OP_STATUS CanvasContextWebGL::ensureDevice(VEGA3dDevice::UseCase use_case)
{
	// Initialize the 3d device if necessary, and check our use case
	if (g_vegaGlobals.vega3dDevice)
		RETURN_IF_ERROR(g_vegaGlobals.vega3dDevice->CheckUseCase(use_case));
	else
		RETURN_IF_ERROR(VEGA3dDeviceFactory::Create(&g_vegaGlobals.vega3dDevice, use_case));

	return OpStatus::OK;
}

OP_STATUS CanvasContextWebGL::initBuffer(VEGARenderTarget* rt, VEGA3dDevice::UseCase use_case)
{
	// Initialize the 3d device
	RETURN_IF_ERROR(ensureDevice(use_case));
	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;

	unsigned int w = rt->getWidth();
	unsigned int h = rt->getHeight();

	// This can be called before Construct so we need to set this.
	m_maxTextures = vega3dDevice->getMaxCombinedTextureUnits();
	m_maxVertexAttribs = vega3dDevice->getMaxVertexAttribs();

	if (!m_renderState)
	{
		RETURN_IF_ERROR(vega3dDevice->createRenderState(&m_renderState, false));
		m_renderState->setFrontFaceCCW(m_renderState->isFrontFaceCCW());
	}

	if (!m_vertexAttribs)
	{
		m_vertexAttribs = OP_NEWA(VertexAttrib, m_maxVertexAttribs);
		RETURN_OOM_IF_NULL(m_vertexAttribs);
		op_memset(m_vertexAttribs, 0, sizeof(VertexAttrib)*m_maxVertexAttribs);
		for (unsigned int i = 0; i < m_maxVertexAttribs; ++i)
			m_vertexAttribs[i].value[3] = 1.f;
	}
	if (!m_textures)
	{
		m_textures = OP_NEWA(CanvasWebGLTexturePtr, m_maxTextures);
		RETURN_OOM_IF_NULL(m_textures);
		for (unsigned int i = 0; i < m_maxTextures; ++i)
			m_textures[i] = NULL;
	}
	if (!m_cubeTextures)
	{
		m_cubeTextures = OP_NEWA(CanvasWebGLTexturePtr, m_maxTextures);
		RETURN_OOM_IF_NULL(m_cubeTextures);
		for (unsigned int i = 0; i < m_maxTextures; ++i)
			m_cubeTextures[i] = NULL;
	}

	if (!m_renderTexture)
	{
		// The spec says the viewport should only be auto adjusted when the canvas is created
		m_viewportWidth = w;
		m_viewportHeight = h;
		m_scissorW = w;
		m_scissorH = h;
	}
	// Make sure the rendertarget is not set when modifying it
	vega3dDevice->setRenderTarget(NULL, false);

	// FIXME: support this parameter
	m_ctxAntialias = FALSE;

	if (!m_renderFramebuffer)
		RETURN_IF_ERROR(vega3dDevice->createFramebuffer(&m_renderFramebuffer));

	// Always create a depth+stencil if stencil is requested since stencil only does not work on desktop gl (or d3d)
	if (/*m_ctxDepth &&*/ m_ctxStencil)
	{
		VEGA3dRenderbufferObject* depthStencil;
		RETURN_IF_ERROR(vega3dDevice->createRenderbuffer(&depthStencil, w, h, VEGA3dRenderbufferObject::FORMAT_DEPTH16_STENCIL8));
		OP_STATUS status = m_renderFramebuffer->attachDepthStencil(depthStencil);
		VEGARefCount::DecRef(depthStencil);
		RETURN_IF_ERROR(status);
		m_ctxDepth = true;
	}
	/*else if (m_ctxStencil)
	{
		VEGA3dRenderbufferObject* stencil;
		RETURN_IF_ERROR(vega3dDevice->createRenderbuffer(&stencil, w, h, VEGA3dRenderbufferObject::FORMAT_DEPTH16_STENCIL8));
		m_renderFramebuffer->attachStencil(stencil);
		VEGARefCount::DecRef(stencil);
	}*/
	else if (m_ctxDepth)
	{
		VEGA3dRenderbufferObject* depth;
		RETURN_IF_ERROR(vega3dDevice->createRenderbuffer(&depth, w, h, VEGA3dRenderbufferObject::FORMAT_DEPTH16));
		OP_STATUS status = m_renderFramebuffer->attachDepth(depth);
		VEGARefCount::DecRef(depth);
		RETURN_IF_ERROR(status);
	}

	VEGA3dTexture* tex;
#ifdef VEGA_CANVAS3D_USE_RGBA4444
	VEGA3dTexture::ColorFormat fmtWithAlpha = VEGA3dTexture::FORMAT_RGBA4444;
#else
	VEGA3dTexture::ColorFormat fmtWithAlpha = VEGA3dTexture::FORMAT_RGBA8888;
#endif
#ifdef VEGA_CANVAS3D_USE_RGB565
	VEGA3dTexture::ColorFormat fmtWithoutAlpha = VEGA3dTexture::FORMAT_RGB565;
#else
	VEGA3dTexture::ColorFormat fmtWithoutAlpha = VEGA3dTexture::FORMAT_RGB888;
#endif
	RETURN_IF_ERROR(vega3dDevice->createTexture(&tex, w, h, m_ctxAlpha ? fmtWithAlpha : fmtWithoutAlpha));
	OP_STATUS status = m_renderFramebuffer->attachColor(tex);
	VEGARefCount::DecRef(m_renderTexture);
	m_renderTexture = tex;
	m_vegaTarget = rt;
	RETURN_IF_ERROR(status);

	// Make sure the framebuffer is cleared after resizing
	vega3dDevice->setRenderTarget(m_renderFramebuffer, true);
	vega3dDevice->setRenderState(g_vegaGlobals.vega3dDevice->getDefault2dNoBlendNoScissorRenderState());
	vega3dDevice->clear(true, m_ctxDepth, m_ctxStencil, 0, 1.f, 0);

	return OpStatus::OK;
}

void CanvasContextWebGL::swapBuffers(VEGARenderer* renderer, bool clear, VEGARenderTarget* backbufferRT)
{
	if (!m_ctxPreserveDrawingBuffer)
	{
		if (m_pendingClear)
		{
			OP_ASSERT(!clear && "Doing a proper swap when a clear is pending means the canvas was invalidated when it should not be.");
			VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
			vega3dDevice->setRenderState(vega3dDevice->getDefault2dNoBlendNoScissorRenderState());
			vega3dDevice->setRenderTarget(m_renderFramebuffer);
			vega3dDevice->clear(true, m_ctxDepth, m_ctxStencil, 0, 1.f, 0);
			m_pendingClear = false;
		}
		m_pendingClear |= clear;
	}

#ifdef VEGA_2DDEVICE
	VEGARenderTarget* destRT = backbufferRT ? backbufferRT : m_vegaTarget;
	if (destRT->GetBackingStore()->IsA(VEGABackingStore::SURFACE))
	{
		destRT->GetBackingStore()->Validate(); // Makes sure it is not in sw-render-mode
		VEGA2dSurface* surf = static_cast<VEGABackingStore_Surface*>(destRT->GetBackingStore())->GetSurface();
		VEGA3dFramebufferObject* surfaceFbo = surf->lock3d();
		if (surfaceFbo)
		{
			// We can render directly to the surface so lets do that.
			VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
			dev->setRenderState(dev->getDefault2dNoBlendNoScissorRenderState());
			VEGA3dRenderTarget* oldRT = dev->getRenderTarget();
			dev->setRenderTarget(surfaceFbo);
			dev->setTexture(0, m_renderTexture);

			VEGA3dDevice::Vega2dVertex verts[] =
				{
					{0, 0, 0, 0, 0xffffffff},
					{(float)m_renderTexture->getWidth(), 0, 1.0f, 0, 0xffffffff},
					{0, (float)m_renderTexture->getHeight(), 0, 1.0f, 0xffffffff},
					{(float)m_renderTexture->getWidth(), (float)m_renderTexture->getHeight(), 1.0f, 1.0f, 0xffffffff}
				};

			VEGA3dBuffer* vbuffer = dev->getTempBuffer(4*sizeof(VEGA3dDevice::Vega2dVertex));
			if (vbuffer && OpStatus::IsSuccess(vbuffer->update(0, 4*sizeof(VEGA3dDevice::Vega2dVertex), verts)))
			{
				dev->setShaderProgram(m_shader2d);
				m_shader2d->setOrthogonalProjection();
				dev->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, m_vlayout2d, 0, 4);
			}

			// It is very important that the order below is kept, ie first call unlock3d
			// and then reset the rendertarget. The reason is that on some devices when
			// WebGL rendering is done onto a pixmap we must call glClear between each
			// frame to flush the renderqueue (Check MALI_GPU_Application_Optimization_Guide.pdf
			// for more information). The call to glClear must come while the surfaceFbo is still
			// bound but after the render result has been copied to screen.
			surf->unlock3d();
			dev->setRenderTarget(oldRT);
			return;
		}
	}
#endif // VEGA_2DDEVICE

	VEGAFill* fill;
	RETURN_VOID_IF_ERROR(renderer->createImage(&fill, m_renderTexture, m_ctxPremultipliedAlpha)); // FIXME: OOM

	// Flip the texture since it is upside down
	VEGATransform trans, itrans, temp;

	temp.loadScale(VEGA_INTTOFIX(1), VEGA_INTTOFIX(-1));
	trans.loadTranslate(0, VEGA_INTTOFIX(m_vegaTarget->getHeight()));
	trans.multiply(temp);
	itrans.loadScale(VEGA_INTTOFIX(1), VEGA_INTTOFIX(-1));
	temp.loadTranslate(0, -VEGA_INTTOFIX(m_vegaTarget->getHeight()));
	itrans.multiply(temp);

	fill->setTransform(trans, itrans);

	renderer->setRenderTarget(backbufferRT?backbufferRT:m_vegaTarget);
	renderer->setFill(fill);
	renderer->fillRect(0, 0, m_renderTexture->getWidth(), m_renderTexture->getHeight(), NULL, false);
	renderer->setFill(NULL);
	OP_DELETE(fill);
}

OP_STATUS CanvasContextWebGL::lockBufferContent(VEGA3dDevice::FramebufferData* data)
{
	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
	dev->setRenderState(dev->getDefault2dNoBlendNoScissorRenderState());
	dev->setRenderTarget(m_renderFramebuffer);
	data->x = 0;
	data->y = 0;
	data->w = m_renderTexture->getWidth();
	data->h = m_renderTexture->getHeight();
	return dev->lockRect(data, true);
}

void CanvasContextWebGL::unlockBufferContent(VEGA3dDevice::FramebufferData* data)
{
	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
	dev->unlockRect(data, false);
}

void CanvasContextWebGL::activeTexture(unsigned int tex)
{
	if (tex < WEBGL_TEXTURE0 || tex >= WEBGL_TEXTURE0+m_maxTextures)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_activeTexture = tex-WEBGL_TEXTURE0;
}
void CanvasContextWebGL::blendColor(float red, float green, float blue, float alpha)
{
	m_renderState->setBlendColor(red, green, blue, alpha);
}
void CanvasContextWebGL::blendEquation(unsigned int eq)
{
	VEGA3dRenderState::BlendOp op = GLToVegaBlendOp(eq);
	if (op >= VEGA3dRenderState::NUM_BLEND_OPS)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_renderState->setBlendEquation(op);
}
void CanvasContextWebGL::blendEquationSeparate(unsigned int eq, unsigned int eqA)
{
	VEGA3dRenderState::BlendOp op = GLToVegaBlendOp(eq);
	VEGA3dRenderState::BlendOp opA = GLToVegaBlendOp(eqA);
	if (op >= VEGA3dRenderState::NUM_BLEND_OPS || opA >= VEGA3dRenderState::NUM_BLEND_OPS)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_renderState->setBlendEquation(op, opA);
}
void CanvasContextWebGL::blendFunc(unsigned int src, unsigned int dst)
{
	VEGA3dRenderState::BlendWeight srcw = GLToVegaBlendWeight(src);
	VEGA3dRenderState::BlendWeight dstw = GLToVegaBlendWeight(dst);
	if (srcw >= VEGA3dRenderState::NUM_BLEND_WEIGHTS || dstw >= VEGA3dRenderState::NUM_BLEND_WEIGHTS || dstw == VEGA3dRenderState::BLEND_SRC_ALPHA_SATURATE)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (((srcw == VEGA3dRenderState::BLEND_CONSTANT_COLOR || srcw == VEGA3dRenderState::BLEND_ONE_MINUS_CONSTANT_COLOR) && (dstw == VEGA3dRenderState::BLEND_CONSTANT_ALPHA || dstw == VEGA3dRenderState::BLEND_ONE_MINUS_CONSTANT_ALPHA)) ||
		((dstw == VEGA3dRenderState::BLEND_CONSTANT_COLOR || dstw == VEGA3dRenderState::BLEND_ONE_MINUS_CONSTANT_COLOR) && (srcw == VEGA3dRenderState::BLEND_CONSTANT_ALPHA || srcw == VEGA3dRenderState::BLEND_ONE_MINUS_CONSTANT_ALPHA)))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}
	m_renderState->setBlendFunc(srcw, dstw);
}
void CanvasContextWebGL::blendFuncSeparate(unsigned int src, unsigned int dst, unsigned int srcA, unsigned int dstA)
{
	VEGA3dRenderState::BlendWeight srcw = GLToVegaBlendWeight(src);
	VEGA3dRenderState::BlendWeight dstw = GLToVegaBlendWeight(dst);
	VEGA3dRenderState::BlendWeight srcwA = GLToVegaBlendWeight(srcA);
	VEGA3dRenderState::BlendWeight dstwA = GLToVegaBlendWeight(dstA);
	if (srcw >= VEGA3dRenderState::NUM_BLEND_WEIGHTS || dstw >= VEGA3dRenderState::NUM_BLEND_WEIGHTS  || dstw == VEGA3dRenderState::BLEND_SRC_ALPHA_SATURATE ||
		srcwA >= VEGA3dRenderState::NUM_BLEND_WEIGHTS || dstwA >= VEGA3dRenderState::NUM_BLEND_WEIGHTS  || dstwA == VEGA3dRenderState::BLEND_SRC_ALPHA_SATURATE)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (((srcw == VEGA3dRenderState::BLEND_CONSTANT_COLOR || srcw == VEGA3dRenderState::BLEND_ONE_MINUS_CONSTANT_COLOR) && (dstw == VEGA3dRenderState::BLEND_CONSTANT_ALPHA || dstw == VEGA3dRenderState::BLEND_ONE_MINUS_CONSTANT_ALPHA)) ||
		((dstw == VEGA3dRenderState::BLEND_CONSTANT_COLOR || dstw == VEGA3dRenderState::BLEND_ONE_MINUS_CONSTANT_COLOR) && (srcw == VEGA3dRenderState::BLEND_CONSTANT_ALPHA || srcw == VEGA3dRenderState::BLEND_ONE_MINUS_CONSTANT_ALPHA)))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}
	m_renderState->setBlendFunc(srcw, dstw, srcwA, dstwA);
}
void CanvasContextWebGL::clearColor(float red, float green, float blue, float alpha)
{
	UINT8 r = (UINT8)(red*255.f+.5f);
	UINT8 g = (UINT8)(green*255.f+.5f);
	UINT8 b = (UINT8)(blue*255.f+.5f);
	UINT8 a = (UINT8)(alpha*255.f+.5f);

	m_clearColor = (a<<24)|(r<<16)|(g<<8)|b;
}
void CanvasContextWebGL::clearDepth(float depth)
{
	m_clearDepth = depth;
}
void CanvasContextWebGL::clearStencil(int sten)
{
	m_clearStencil = sten;
}
void CanvasContextWebGL::colorMask(BOOL red, BOOL green, BOOL blue, BOOL alpha)
{
	m_renderState->setColorMask(red!=FALSE, green!=FALSE, blue!=FALSE, alpha!=FALSE);
}
void CanvasContextWebGL::cullFace(unsigned int face)
{
	switch (face)
	{
	case WEBGL_FRONT:
		m_renderState->setCullFace(VEGA3dRenderState::FACE_FRONT);
		m_cullFace = face;
		break;
	case WEBGL_BACK:
		m_renderState->setCullFace(VEGA3dRenderState::FACE_BACK);
		m_cullFace = face;
		break;
	case WEBGL_FRONT_AND_BACK:
		// Only lines and points will be drawn. This is handled in drawArrays/drawElements.
		m_cullFace = face;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
}
void CanvasContextWebGL::depthFunc(unsigned int func)
{
	VEGA3dRenderState::DepthFunc df = GLToVegaDepthFunc(func);
	if (df >= VEGA3dRenderState::NUM_DEPTH_FUNCS)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_renderState->setDepthFunc(df);
}
void CanvasContextWebGL::depthMask(BOOL depth)
{
	m_renderState->enableDepthWrite(depth?true:false);
}
void CanvasContextWebGL::depthRange(float zNear, float zFar)
{
	if (zNear > zFar)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}
	m_viewportNear = Clamp01(zNear);
	m_viewportFar = Clamp01(zFar);
}
void CanvasContextWebGL::disable(unsigned int cap)
{
	switch (cap)
	{
	case WEBGL_BLEND:
		m_renderState->enableBlend(false);
		break;
	case WEBGL_CULL_FACE:
		m_renderState->enableCullFace(false);
		break;
	case WEBGL_DEPTH_TEST:
		m_renderState->enableDepthTest(false);
		break;
	case WEBGL_DITHER:
		m_renderState->enableDither(false);
		break;
	case WEBGL_POLYGON_OFFSET_FILL:
		m_renderState->enablePolygonOffsetFill(false);
		break;
	case WEBGL_SAMPLE_ALPHA_TO_COVERAGE:
		m_renderState->enableSampleToAlphaCoverage(false);
		break;
	case WEBGL_SAMPLE_COVERAGE:
		m_renderState->enableSampleCoverage(false);
		break;
	case WEBGL_SCISSOR_TEST:
		m_renderState->enableScissor(false);
		break;
	case WEBGL_STENCIL_TEST:
		m_renderState->enableStencil(false);
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
}
void CanvasContextWebGL::enable(unsigned int cap)
{
	switch (cap)
	{
	case WEBGL_BLEND:
		m_renderState->enableBlend(true);
		break;
	case WEBGL_CULL_FACE:
		m_renderState->enableCullFace(true);
		break;
	case WEBGL_DEPTH_TEST:
		m_renderState->enableDepthTest(true);
		break;
	case WEBGL_DITHER:
		m_renderState->enableDither(true);
		break;
	case WEBGL_POLYGON_OFFSET_FILL:
		m_renderState->enablePolygonOffsetFill(true);
		break;
	case WEBGL_SAMPLE_ALPHA_TO_COVERAGE:
		m_renderState->enableSampleToAlphaCoverage(true);
		break;
	case WEBGL_SAMPLE_COVERAGE:
		m_renderState->enableSampleCoverage(true);
		break;
	case WEBGL_SCISSOR_TEST:
		m_renderState->enableScissor(true);
		break;
	case WEBGL_STENCIL_TEST:
		m_renderState->enableStencil(true);
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
}
void CanvasContextWebGL::frontFace(unsigned int face)
{
	if (face != WEBGL_CW && face != WEBGL_CCW)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_renderState->setFrontFaceCCW(face == WEBGL_CCW);
}

void CanvasContextWebGL::getFramebufferColorFormat(unsigned int &framebufferFmt, unsigned int &framebufferType)
{
	framebufferFmt = 0;
	framebufferType = 0;
	VEGA3dFramebufferObject* curFBO = m_framebuffer?m_framebuffer->getBuffer():m_renderFramebuffer;
	if (VEGA3dTexture* tex = curFBO->getAttachedColorTexture())
		VegaTextureColorFormatToGL(tex->getFormat(), framebufferFmt, framebufferType);
	else if (VEGA3dRenderbufferObject* buf = curFBO->getAttachedColorBuffer())
		VegaColorBufferFormatToGL(buf->getFormat(), framebufferFmt, framebufferType);
}

CanvasWebGLParameter CanvasContextWebGL::getParameter(unsigned int param)
{
	VEGA3dFramebufferObject* curFBO = m_framebuffer?m_framebuffer->getBuffer():m_renderFramebuffer;
	CanvasWebGLParameter p;
	// Return NULL on error
	p.type = CanvasWebGLParameter::PARAM_OBJECT;
	p.value.object_param = NULL;

	switch (param)
	{
	case WEBGL_ACTIVE_TEXTURE:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = m_activeTexture+WEBGL_TEXTURE0;
		break;
	case WEBGL_ALIASED_LINE_WIDTH_RANGE:
		p.type = CanvasWebGLParameter::PARAM_FLOAT2;
		g_vegaGlobals.vega3dDevice->getAliasedLineWidthRange(p.value.float_param[0], p.value.float_param[1]);
		break;
	case WEBGL_ALIASED_POINT_SIZE_RANGE:
		p.type = CanvasWebGLParameter::PARAM_FLOAT2;
		g_vegaGlobals.vega3dDevice->getAliasedPointSizeRange(p.value.float_param[0], p.value.float_param[1]);
		break;
	case WEBGL_ALPHA_BITS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = curFBO->getAlphaBits();
		break;
	case WEBGL_ARRAY_BUFFER_BINDING:
		p.type = CanvasWebGLParameter::PARAM_OBJECT;
		p.value.object_param = m_arrayBuffer?m_arrayBuffer->getESObject():NULL;
		break;
	case WEBGL_BLEND:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_renderState->isBlendEnabled();
		break;
	case WEBGL_BLEND_COLOR:
		p.type = CanvasWebGLParameter::PARAM_FLOAT4;
		m_renderState->getBlendColor(p.value.float_param[0], p.value.float_param[1], p.value.float_param[2], p.value.float_param[3]);
		break;
	case WEBGL_BLEND_DST_ALPHA:
		{
		VEGA3dRenderState::BlendWeight src, dst, srcA, dstA;
		m_renderState->getBlendFunc(src, dst, srcA, dstA);
		OP_ASSERT(dstA < VEGA3dRenderState::NUM_BLEND_WEIGHTS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLBlendWeight[dstA];
		break;
		}
	case WEBGL_BLEND_DST_RGB:
		{
		VEGA3dRenderState::BlendWeight src, dst, srcA, dstA;
		m_renderState->getBlendFunc(src, dst, srcA, dstA);
		OP_ASSERT(dst < VEGA3dRenderState::NUM_BLEND_WEIGHTS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLBlendWeight[dst];
		break;
		}
	case WEBGL_BLEND_EQUATION_ALPHA:
		{
		VEGA3dRenderState::BlendOp op, opA;
		m_renderState->getBlendEquation(op, opA);
		OP_ASSERT(opA < VEGA3dRenderState::NUM_BLEND_OPS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLBlendOp[opA];
		break;
		}
	case WEBGL_BLEND_EQUATION_RGB:
		{
		VEGA3dRenderState::BlendOp op, opA;
		m_renderState->getBlendEquation(op, opA);
		OP_ASSERT(op < VEGA3dRenderState::NUM_BLEND_OPS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLBlendOp[op];
		break;
		}
	case WEBGL_BLEND_SRC_ALPHA:
		{
		VEGA3dRenderState::BlendWeight src, dst, srcA, dstA;
		m_renderState->getBlendFunc(src, dst, srcA, dstA);
		OP_ASSERT(srcA < VEGA3dRenderState::NUM_BLEND_WEIGHTS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLBlendWeight[srcA];
		break;
		}
	case WEBGL_BLEND_SRC_RGB:
		{
		VEGA3dRenderState::BlendWeight src, dst, srcA, dstA;
		m_renderState->getBlendFunc(src, dst, srcA, dstA);
		OP_ASSERT(src < VEGA3dRenderState::NUM_BLEND_WEIGHTS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLBlendWeight[src];
		break;
		}
	case WEBGL_BLUE_BITS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = curFBO->getBlueBits();
		break;
	case WEBGL_COLOR_CLEAR_VALUE:
		p.type = CanvasWebGLParameter::PARAM_FLOAT4;
		p.value.float_param[0] = (float)((m_clearColor>>16)&0xff)/255.f;
		p.value.float_param[1] = (float)((m_clearColor>>8)&0xff)/255.f;
		p.value.float_param[2] = (float)(m_clearColor&0xff)/255.f;
		p.value.float_param[3] = (float)(m_clearColor>>24)/255.f;
		break;
	case WEBGL_COLOR_WRITEMASK:
	{
		bool r, g, b, a;
		m_renderState->getColorMask(r, g, b, a);

		p.type = CanvasWebGLParameter::PARAM_BOOL4;
		p.value.int_param[0] = r?1:0;
		p.value.int_param[1] = g?1:0;
		p.value.int_param[2] = b?1:0;
		p.value.int_param[3] = a?1:0;
		break;
	}
	case WEBGL_COMPRESSED_TEXTURE_FORMATS:
		OP_ASSERT(!"Use getNumCompressedTextureFormats() and getCompressedTextureFormat()");
		break;
	case WEBGL_CULL_FACE:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_renderState->isCullFaceEnabled();
		break;
	case WEBGL_CULL_FACE_MODE:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = m_cullFace;
		break;
	case WEBGL_CURRENT_PROGRAM:
		p.type = CanvasWebGLParameter::PARAM_OBJECT;
		p.value.object_param = m_program?m_program->getESObject():NULL;
		break;
	case WEBGL_DEPTH_BITS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = curFBO->getDepthBits();
		break;
	case WEBGL_DEPTH_CLEAR_VALUE:
		p.type = CanvasWebGLParameter::PARAM_FLOAT;
		p.value.float_param[0] = m_clearDepth;
		break;
	case WEBGL_DEPTH_FUNC:
		{
		VEGA3dRenderState::DepthFunc df;
		m_renderState->getDepthFunc(df);
		OP_ASSERT(df < VEGA3dRenderState::NUM_DEPTH_FUNCS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLDepthFunc[df];
		break;
		}
	case WEBGL_DEPTH_RANGE:
		p.type = CanvasWebGLParameter::PARAM_FLOAT2;
		p.value.float_param[0] = m_viewportNear;
		p.value.float_param[1] = m_viewportFar;
		break;
	case WEBGL_DEPTH_TEST:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_renderState->isDepthTestEnabled();
		break;
	case WEBGL_DEPTH_WRITEMASK:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_renderState->isDepthWriteEnabled();
		break;
	case WEBGL_DITHER:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_renderState->isDitherEnabled();
		break;
	case WEBGL_ELEMENT_ARRAY_BUFFER_BINDING:
		p.type = CanvasWebGLParameter::PARAM_OBJECT;
		p.value.object_param = m_elementArrayBuffer?m_elementArrayBuffer->getESObject():NULL;
		break;
	case WEBGL_FRAMEBUFFER_BINDING:
		p.type = CanvasWebGLParameter::PARAM_OBJECT;
		p.value.object_param = m_framebuffer && ! m_framebuffer->isDestroyed() ? m_framebuffer->getESObject() : NULL;
		break;
	case WEBGL_FRONT_FACE:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = m_renderState->isFrontFaceCCW()?WEBGL_CCW:WEBGL_CW;
		break;
	case WEBGL_GENERATE_MIPMAP_HINT:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = m_hintMode;
		break;
	case WEBGL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
		if ((m_enabledExtensions&WEBGL_EXT_OES_STANDARD_DERIVATIVES)!=0)
		{
			p.type = CanvasWebGLParameter::PARAM_UINT;
			p.value.uint_param[0] = m_oesStdDerivativesHintMode;
		}
		else
			raiseError(WEBGL_INVALID_ENUM);
		break;
	case WEBGL_GREEN_BITS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = curFBO->getGreenBits();
		break;
	case WEBGL_LINE_WIDTH:
		p.type = CanvasWebGLParameter::PARAM_FLOAT;
		p.value.float_param[0] = m_renderState->getLineWidth();
		break;
	case WEBGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = m_maxTextures;
		break;
	case WEBGL_MAX_CUBE_MAP_TEXTURE_SIZE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = g_vegaGlobals.vega3dDevice->getMaxCubeMapTextureSize();
		break;
	case WEBGL_MAX_FRAGMENT_UNIFORM_VECTORS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = g_vegaGlobals.vega3dDevice->getMaxFragmentUniformVectors();
		break;
	case WEBGL_MAX_RENDERBUFFER_SIZE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = g_vegaGlobals.vega3dDevice->getMaxRenderBufferSize();
		break;
	case WEBGL_MAX_TEXTURE_IMAGE_UNITS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = g_vegaGlobals.vega3dDevice->getMaxFragmentTextureUnits();
		break;
	case WEBGL_MAX_TEXTURE_SIZE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = g_vegaGlobals.vega3dDevice->getMaxTextureSize();
		break;
	case WEBGL_MAX_VARYING_VECTORS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = g_vegaGlobals.vega3dDevice->getMaxVaryingVectors();
		break;
	case WEBGL_MAX_VERTEX_ATTRIBS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = g_vegaGlobals.vega3dDevice->getMaxVertexAttribs();
		break;
	case WEBGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = g_vegaGlobals.vega3dDevice->getMaxVertexTextureUnits();
		break;
	case WEBGL_MAX_VERTEX_UNIFORM_VECTORS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = g_vegaGlobals.vega3dDevice->getMaxVertexUniformVectors();
		break;
	case WEBGL_MAX_VIEWPORT_DIMS:
		g_vegaGlobals.vega3dDevice->getMaxViewportDims(p.value.int_param[0], p.value.int_param[1]);
		p.type = CanvasWebGLParameter::PARAM_INT2;
		break;
	case WEBGL_PACK_ALIGNMENT:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = m_packAlignment;
		break;
	case WEBGL_POLYGON_OFFSET_FACTOR:
		p.type = CanvasWebGLParameter::PARAM_FLOAT;
		p.value.float_param[0] = m_renderState->getPolygonOffsetFactor();
		break;
	case WEBGL_POLYGON_OFFSET_FILL:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_renderState->isPolygonOffsetFillEnabled();
		break;
	case WEBGL_POLYGON_OFFSET_UNITS:
		p.type = CanvasWebGLParameter::PARAM_FLOAT;
		p.value.float_param[0] = m_renderState->getPolygonOffsetUnits();
		break;
	case WEBGL_RED_BITS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = curFBO->getRedBits();
		break;
	case WEBGL_RENDERBUFFER_BINDING:
		p.type = CanvasWebGLParameter::PARAM_OBJECT;
		p.value.object_param = m_renderbuffer && !m_renderbuffer->isDestroyed() ? m_renderbuffer->getESObject() : NULL;
		break;
	case WEBGL_SAMPLE_BUFFERS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = curFBO->getSampleBuffers();
		break;
	case WEBGL_SAMPLE_COVERAGE_INVERT:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_renderState->getSampleCoverageInvert();
		break;
	case WEBGL_SAMPLE_COVERAGE_VALUE:
		p.type = CanvasWebGLParameter::PARAM_FLOAT;
		p.value.float_param[0] = m_renderState->getSampleCoverageValue();
		break;
	case WEBGL_SAMPLES:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = curFBO->getSamples();
		break;
	case WEBGL_SCISSOR_BOX:
		{
		p.type = CanvasWebGLParameter::PARAM_INT4;
		p.value.int_param[0] = m_scissorX;
		p.value.int_param[1] = m_scissorY;
		p.value.int_param[2] = m_scissorW;
		p.value.int_param[3] = m_scissorH;
		break;
		}
	case WEBGL_SCISSOR_TEST:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_renderState->isScissorEnabled();
		break;
	case WEBGL_STENCIL_BACK_FAIL:
		{
		VEGA3dRenderState::StencilOp fail, zFail, pass, failb, zFailb, passb;
		m_renderState->getStencilOp(fail, zFail, pass, failb, zFailb, passb);
		OP_ASSERT(failb < VEGA3dRenderState::NUM_STENCIL_OPS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLStencilOp[failb];
		break;
		}
	case WEBGL_STENCIL_BACK_FUNC:
		{
		VEGA3dRenderState::StencilFunc func, funcb;
		unsigned int ref, mask;
		m_renderState->getStencilFunc(func, funcb, ref, mask);
		OP_ASSERT(funcb < VEGA3dRenderState::NUM_STENCIL_FUNCS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLStencilFunc[funcb];
		break;
		}
	case WEBGL_STENCIL_BACK_PASS_DEPTH_FAIL:
		{
		VEGA3dRenderState::StencilOp fail, zFail, pass, failb, zFailb, passb;
		m_renderState->getStencilOp(fail, zFail, pass, failb, zFailb, passb);
		OP_ASSERT(zFailb < VEGA3dRenderState::NUM_STENCIL_OPS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLStencilOp[zFailb];
		break;
		}
	case WEBGL_STENCIL_BACK_PASS_DEPTH_PASS:
		{
		VEGA3dRenderState::StencilOp fail, zFail, pass, failb, zFailb, passb;
		m_renderState->getStencilOp(fail, zFail, pass, failb, zFailb, passb);
		OP_ASSERT(passb < VEGA3dRenderState::NUM_STENCIL_OPS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLStencilOp[passb];
		break;
		}
	case WEBGL_STENCIL_BACK_REF:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = m_stencilRefBack;
		break;
	case WEBGL_STENCIL_BACK_VALUE_MASK:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = m_stencilMaskBack;
		break;
	case WEBGL_STENCIL_BACK_WRITEMASK:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = m_stencilWriteMaskBack;
		break;
	case WEBGL_STENCIL_BITS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = curFBO->getStencilBits();
		break;
	case WEBGL_STENCIL_CLEAR_VALUE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = m_clearStencil;
		break;
	case WEBGL_STENCIL_FAIL:
		{
		VEGA3dRenderState::StencilOp fail, zFail, pass, failb, zFailb, passb;
		m_renderState->getStencilOp(fail, zFail, pass, failb, zFailb, passb);
		OP_ASSERT(fail < VEGA3dRenderState::NUM_STENCIL_OPS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLStencilOp[fail];
		break;
		}
	case WEBGL_STENCIL_FUNC:
		{
		VEGA3dRenderState::StencilFunc func, funcb;
		unsigned int ref, mask;
		m_renderState->getStencilFunc(func, funcb, ref, mask);
		OP_ASSERT(func < VEGA3dRenderState::NUM_STENCIL_FUNCS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLStencilFunc[func];
		break;
		}
	case WEBGL_STENCIL_PASS_DEPTH_FAIL:
		{
		VEGA3dRenderState::StencilOp fail, zFail, pass, failb, zFailb, passb;
		m_renderState->getStencilOp(fail, zFail, pass, failb, zFailb, passb);
		OP_ASSERT(zFail < VEGA3dRenderState::NUM_STENCIL_OPS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLStencilOp[zFail];
		break;
		}
	case WEBGL_STENCIL_PASS_DEPTH_PASS:
		{
		VEGA3dRenderState::StencilOp fail, zFail, pass, failb, zFailb, passb;
		m_renderState->getStencilOp(fail, zFail, pass, failb, zFailb, passb);
		OP_ASSERT(pass < VEGA3dRenderState::NUM_STENCIL_OPS);
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = VegaToGLStencilOp[pass];
		break;
		}
	case WEBGL_STENCIL_REF:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = m_stencilRef;
		break;
	case WEBGL_STENCIL_TEST:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_renderState->isStencilEnabled();
		break;
	case WEBGL_STENCIL_VALUE_MASK:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = m_stencilMask;
		break;
	case WEBGL_STENCIL_WRITEMASK:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = m_stencilWriteMask;
		break;
	case WEBGL_SUBPIXEL_BITS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = curFBO->getSubpixelBits();
		break;
	case WEBGL_TEXTURE_BINDING_2D:
		p.type = CanvasWebGLParameter::PARAM_OBJECT;
		p.value.object_param = m_textures[m_activeTexture] && !m_textures[m_activeTexture]->isDestroyed() ? m_textures[m_activeTexture]->getESObject() : NULL;
		break;
	case WEBGL_TEXTURE_BINDING_CUBE_MAP:
		p.type = CanvasWebGLParameter::PARAM_OBJECT;
		p.value.object_param = m_cubeTextures[m_activeTexture] && !m_cubeTextures[m_activeTexture]->isDestroyed() ? m_cubeTextures[m_activeTexture]->getESObject() : NULL;
		break;
	case WEBGL_UNPACK_ALIGNMENT:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = m_unpackAlignment;
		break;
	case WEBGL_UNPACK_FLIP_Y_WEBGL:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_unpackFlipY;
		break;
	case WEBGL_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = m_unpackPremultiplyAlpha;
		break;
	case WEBGL_UNPACK_COLORSPACE_CONVERSION_WEBGL:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = m_unpackColorSpaceConversion;
		break;
	case WEBGL_VIEWPORT:
		p.type = CanvasWebGLParameter::PARAM_FLOAT4;
		p.value.float_param[0] = (float)m_viewportX;
		p.value.float_param[1] = (float)m_viewportY;
		p.value.float_param[2] = (float)m_viewportWidth;
		p.value.float_param[3] = (float)m_viewportHeight;
		break;
	case WEBGL_VERSION:
		p.type = CanvasWebGLParameter::PARAM_STRING;
		p.value.string_param = UNI_L("WebGL 1.0");
		break;
	case WEBGL_SHADING_LANGUAGE_VERSION:
		p.type = CanvasWebGLParameter::PARAM_STRING;
		p.value.string_param = UNI_L("WebGL GLSL ES 1.0");
		break;
	case WEBGL_VENDOR:
		p.type = CanvasWebGLParameter::PARAM_STRING;
		p.value.string_param = UNI_L("Opera Software");
		break;
	case WEBGL_RENDERER:
		p.type = CanvasWebGLParameter::PARAM_STRING;
		p.value.string_param = UNI_L("Vega");
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}

	return p;
}

unsigned int CanvasContextWebGL::getNumCompressedTextureFormats()
{
	return 0;
}

unsigned int CanvasContextWebGL::getCompressedTextureFormat(unsigned int index)
{
	OP_ASSERT(!"Out of range since the number of supported formats are 0.");
	return 0;
}

void CanvasContextWebGL::hint(unsigned int target, unsigned int mode)
{
	if (target != WEBGL_GENERATE_MIPMAP_HINT &&
		(target != WEBGL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES || (m_enabledExtensions&WEBGL_EXT_OES_STANDARD_DERIVATIVES)==0))
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}

	switch (mode)
	{
	case WEBGL_DONT_CARE:
	case WEBGL_FASTEST:
	case WEBGL_NICEST:
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return;
	};

	if (target == WEBGL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES)
		m_oesStdDerivativesHintMode = mode;
	else
		m_hintMode = mode;
}

BOOL CanvasContextWebGL::isEnabled(unsigned int cap)
{
	BOOL enabled = FALSE;
	switch (cap)
	{
	case WEBGL_BLEND:
		enabled = m_renderState->isBlendEnabled();
		break;
	case WEBGL_CULL_FACE:
		enabled = m_renderState->isCullFaceEnabled();
		break;
	case WEBGL_DEPTH_TEST:
		enabled = m_renderState->isDepthTestEnabled();
		break;
	case WEBGL_DITHER:
		enabled = m_renderState->isDitherEnabled();
		break;
	case WEBGL_POLYGON_OFFSET_FILL:
		enabled = m_renderState->isPolygonOffsetFillEnabled();
		break;
	case WEBGL_SAMPLE_ALPHA_TO_COVERAGE:
		enabled = m_renderState->isSampleToAlphaCoverageEnabled();
		break;
	case WEBGL_SAMPLE_COVERAGE:
		enabled = m_renderState->isSampleCoverageEnabled();
		break;
	case WEBGL_SCISSOR_TEST:
		enabled = m_renderState->isScissorEnabled();
		break;
	case WEBGL_STENCIL_TEST:
		enabled = m_renderState->isStencilEnabled();
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
	return enabled;
}
void CanvasContextWebGL::lineWidth(float width)
{
	m_renderState->lineWidth(width);
}
void CanvasContextWebGL::pixelStorei(unsigned int name, unsigned int param)
{
	switch (name)
	{
	case WEBGL_UNPACK_ALIGNMENT:
		if (param != 1 && param != 2 && param != 4 && param != 8)
			raiseError(WEBGL_INVALID_VALUE);
		else
			m_unpackAlignment = param;
		break;
	case WEBGL_PACK_ALIGNMENT:
		if (param != 1 && param != 2 && param != 4 && param != 8)
			raiseError(WEBGL_INVALID_VALUE);
		else
			m_packAlignment = param;
		break;
	case WEBGL_UNPACK_FLIP_Y_WEBGL:
		m_unpackFlipY = param?true:false;
		break;
	case WEBGL_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
		m_unpackPremultiplyAlpha = param?true:false;
		break;
	case WEBGL_UNPACK_COLORSPACE_CONVERSION_WEBGL:
		m_unpackColorSpaceConversion = param;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
}
void CanvasContextWebGL::polygonOffset(float factor, float offset)
{
	m_renderState->polygonOffset(factor, offset);
}
void CanvasContextWebGL::sampleCoverage(float value, BOOL invert)
{
	m_renderState->sampleCoverage(invert!=FALSE, value);
}
void CanvasContextWebGL::stencilFunc(unsigned int func, int ref, unsigned int mask)
{
	VEGA3dRenderState::StencilFunc vf = GLToVegaStencilFunc(func);
	if (vf >= VEGA3dRenderState::NUM_STENCIL_FUNCS)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_stencilRef = m_stencilRefBack = ref;
	m_stencilMask = m_stencilMaskBack = mask;
	m_renderState->setStencilFunc(vf, vf, ref, mask);
}
void CanvasContextWebGL::stencilFuncSeparate(unsigned int face, unsigned int func, int ref, unsigned int mask)
{
	VEGA3dRenderState::StencilFunc vf = GLToVegaStencilFunc(func);
	VEGA3dRenderState::StencilFunc vfback = vf;
	VEGA3dRenderState::StencilFunc vftemp;
	if (vf >= VEGA3dRenderState::NUM_STENCIL_FUNCS)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	unsigned int reftemp, masktemp;
	switch (face)
	{
	case WEBGL_FRONT:
		m_renderState->getStencilFunc(vftemp, vfback, reftemp, masktemp);
		m_stencilRef = ref;
		m_stencilMask = mask;
		break;
	case WEBGL_BACK:
		m_renderState->getStencilFunc(vf, vftemp, reftemp, masktemp);
		m_stencilRefBack = ref;
		m_stencilMaskBack = mask;
		break;
	case WEBGL_FRONT_AND_BACK:
		m_stencilRef = m_stencilRefBack = ref;
		m_stencilMask = m_stencilMaskBack = mask;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_renderState->setStencilFunc(vf, vfback, ref, mask);
}
void CanvasContextWebGL::stencilMask(unsigned int mask)
{
	m_stencilWriteMask = m_stencilWriteMaskBack = mask;
	m_renderState->setStencilWriteMask(mask);
}
void CanvasContextWebGL::stencilMaskSeparate(unsigned int face, unsigned int mask)
{
	switch (face)
	{
	case WEBGL_FRONT:
		m_stencilWriteMask = mask;
		break;
	case WEBGL_BACK:
		m_stencilWriteMaskBack = mask;
		break;
	case WEBGL_FRONT_AND_BACK:
		m_stencilWriteMask = m_stencilWriteMaskBack = mask;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_renderState->setStencilWriteMask(mask);
}
void CanvasContextWebGL::stencilOp(unsigned int fail, unsigned int zfail, unsigned int zpass)
{
	VEGA3dRenderState::StencilOp vfail = GLToVegaStencilOp(fail);
	VEGA3dRenderState::StencilOp vzfail = GLToVegaStencilOp(zfail);
	VEGA3dRenderState::StencilOp vzpass = GLToVegaStencilOp(zpass);
	if (vfail >= VEGA3dRenderState::NUM_STENCIL_OPS ||
		vzfail >= VEGA3dRenderState::NUM_STENCIL_OPS ||
		vzpass >= VEGA3dRenderState::NUM_STENCIL_OPS)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_renderState->setStencilOp(vfail, vzfail, vzpass);
}
void CanvasContextWebGL::stencilOpSeparate(unsigned int face, unsigned int fail, unsigned int zfail, unsigned int zpass)
{
	VEGA3dRenderState::StencilOp vfail = GLToVegaStencilOp(fail);
	VEGA3dRenderState::StencilOp vzfail = GLToVegaStencilOp(zfail);
	VEGA3dRenderState::StencilOp vzpass = GLToVegaStencilOp(zpass);
	VEGA3dRenderState::StencilOp vfailb = vfail;
	VEGA3dRenderState::StencilOp vzfailb = vzfail;
	VEGA3dRenderState::StencilOp vzpassb = vzpass;
	VEGA3dRenderState::StencilOp vfailtemp, vzfailtemp, vzpasstemp;
	if (vfail >= VEGA3dRenderState::NUM_STENCIL_OPS ||
		vzfail >= VEGA3dRenderState::NUM_STENCIL_OPS ||
		vzpass >= VEGA3dRenderState::NUM_STENCIL_OPS)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	switch (face)
	{
	case WEBGL_FRONT:
		m_renderState->getStencilOp(vfailtemp, vzfailtemp, vzpasstemp, vfailb, vzfailb, vzpassb);
		break;
	case WEBGL_BACK:
		m_renderState->getStencilOp(vfail, vzfail, vzpass, vfailtemp, vzfailtemp, vzpasstemp);
		break;
	case WEBGL_FRONT_AND_BACK:
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_renderState->setStencilOp(vfail, vzfail, vzpass, vfailb, vzfailb, vzpassb);
}

void CanvasContextWebGL::clearRenderFramebuffer(UINT32 clearColor /* = 0 */,
                                                float clearDepth /* = 1.0 */,
                                                int clearStencil /* = 0 */)
{
	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
	vega3dDevice->setRenderState(vega3dDevice->getDefault2dNoBlendNoScissorRenderState());
	vega3dDevice->setRenderTarget(m_renderFramebuffer);
	vega3dDevice->clear(true, m_ctxDepth, m_ctxStencil, clearColor, clearDepth, clearStencil);
}

void CanvasContextWebGL::clearIfNeeded()
{
	if (m_pendingClear && !m_framebuffer)
	{
		clearRenderFramebuffer();
		m_pendingClear = false;
	}
}

void CanvasContextWebGL::clear(unsigned int mask)
{
	/* The ES 2.0 spec is a bit vague here, but the man pages indicate that 0
	   is a valid mask */
	if (!mask)
		return;

	if (mask & ~(WEBGL_COLOR_BUFFER_BIT|WEBGL_DEPTH_BUFFER_BIT|WEBGL_STENCIL_BUFFER_BIT))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}

	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;

	bool colorBit = (mask & WEBGL_COLOR_BUFFER_BIT) != 0;
	bool depthBit = (mask & WEBGL_DEPTH_BUFFER_BIT) != 0;
	bool stencilBit = (mask & WEBGL_STENCIL_BUFFER_BIT) != 0;

	if (m_framebuffer)
	{
		unsigned int fboStatus;
		OpStatus::Ignore(m_framebuffer->updateBindings(fboStatus));
		if (fboStatus != WEBGL_FRAMEBUFFER_COMPLETE)
		{
			raiseError(WEBGL_INVALID_FRAMEBUFFER_OPERATION);
			return;
		}
	}
	else if (m_pendingClear)
	{
		if (m_renderState->isScissorEnabled() &&
		    (m_scissorX != 0 || m_scissorY != 0 ||
		     m_scissorW != m_renderFramebuffer->getWidth() || m_scissorH != m_renderFramebuffer->getHeight()))
		{
			/* We need to clear the entire framebuffer in a separate step if a
			   scissor is set, since the user-initiated clear won't touch the
			   scissored parts */
			clearRenderFramebuffer();
			m_pendingClear = false;
		}
		else
		{
			/* If a clear is pending and no scissors are set, we combine the
			   the automatic clear to (color, depth, stencil) = (0, 1.0, 0)
			   with the values and masks specified by the user into a single
			   clear operation. */

			// Calculate combined color

			UINT32 clearColor;
			if (!colorBit)
				clearColor = 0;
			else
			{
				clearColor = m_clearColor;
				bool cmask_r, cmask_g, cmask_b, cmask_a;
				m_renderState->getColorMask(cmask_r, cmask_g, cmask_b, cmask_a);
				if (!cmask_r)
					clearColor &= 0xFF00FFFFu;
				if (!cmask_g)
					clearColor &= 0xFFFF00FFu;
				if (!cmask_b)
					clearColor &= 0xFFFFFF00u;
				if (!cmask_a)
					clearColor &= 0x00FFFFFFu;
			}

			// Calculate combined depth

			float clearDepth = (depthBit && m_renderState->isDepthWriteEnabled()) ? m_clearDepth : 1.f;

			// Calculate combined stencil
			// ES 2.0 spec: "The clear operation always uses the front stencil write mask
			// when clearing the stencil buffer."

			int clearStencil = stencilBit ? (m_clearStencil & m_stencilWriteMask) : 0;

			clearRenderFramebuffer(clearColor, clearDepth, clearStencil);
			m_pendingClear = false;

			return;
		}
	}

	vega3dDevice->setRenderTarget(m_framebuffer ? m_framebuffer->getBuffer() : m_renderFramebuffer, false);
	vega3dDevice->setViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_viewportNear, m_viewportFar);
	vega3dDevice->setRenderState(m_renderState);
	if (m_renderState->isScissorEnabled())
		vega3dDevice->setScissor(m_scissorX, m_scissorY, m_scissorW, m_scissorH);
	vega3dDevice->clear(colorBit, depthBit, stencilBit, m_clearColor, m_clearDepth, m_clearStencil);
}

OP_STATUS CanvasContextWebGL::updateVertexLayout()
{
	VEGA3dDevice::AttribModel attribModel = g_vegaGlobals.vega3dDevice->getAttribModel();
	OP_ASSERT(attribModel == VEGA3dDevice::GLAttribModel || attribModel == VEGA3dDevice::D3DAttribModel);

	if (m_vertexLayout || !m_program)
		return OpStatus::OK;

	/* We need to check that all enabled array attributes have a buffer here -
	   not just the attributes used by the shader. (According to the spec., "If
	   a vertex attribute is enabled as an array via enableVertexAttribArray
	   but no buffer is bound to that attribute via bindBuffer and
	   vertexAttribPointer, then calls to drawArrays or drawElements will
	   generate an INVALID_OPERATION error.") This is tested by
	   gl-enable-vertex-attrib.html. */
	for (unsigned i = 0; i < m_maxVertexAttribs; ++i)
		if (m_vertexAttribs[i].isFromArray && !m_vertexAttribs[i].buffer)
		{
			raiseError(WEBGL_INVALID_OPERATION);
			return OpStatus::OK;
		}

	RETURN_IF_ERROR(g_vegaGlobals.vega3dDevice->createVertexLayout(&m_vertexLayout, m_program->getProgram()));
	// Start with "infinity" vertex count
	m_maxVertexCount = ~0u;
	OP_STATUS status = OpStatus::OK;

	// We only update the attributes actually used by the shader

	const char *name;
	unsigned location;
	StringHash<unsigned>::Iterator iter = m_program->m_active_attrib_bindings.get_iterator();
	while (iter.get_next(name, location) && OpStatus::IsSuccess(status))
	{
		if (attribModel == VEGA3dDevice::GLAttribModel && !m_program->getProgram()->isAttributeActive(location))
			continue;

		VertexAttrib *attrib = &m_vertexAttribs[location];
		if (!attrib->isFromArray)
		{
			if (attribModel == VEGA3dDevice::GLAttribModel)
				status = m_vertexLayout->addConstComponent(location, attrib->value);
			else
				status = m_vertexLayout->addConstBinding(name, attrib->value);
		}
		else
		{
			VEGA3dVertexLayout::VertexType vegaType = GLToVegaAttrib(attrib->type, attrib->size);
			// Size in bytes of each attribute
			unsigned int attributeSize = attrib->size * GLTypeToSize(attrib->type);
			unsigned int stride = (attrib->stride > 0) ? attrib->stride : attributeSize;

			if (attrib->offset > attrib->buffer->getSize())
				m_maxVertexCount = 0;
			else
			{
				unsigned numVertices = maxVerticesForBuffer(attrib->buffer->getSize(), attrib->offset,
				                                            stride, attributeSize);
				m_maxVertexCount = MIN(m_maxVertexCount, numVertices);
			}

			if (attribModel == VEGA3dDevice::GLAttribModel)
				status = m_vertexLayout->addComponent(attrib->buffer->getBuffer(), location,
				                                      attrib->offset, stride, vegaType,
				                                      attrib->normalize != FALSE);
			else
				status = m_vertexLayout->addBinding(name, attrib->buffer->getBuffer(),
				                                    attrib->offset, stride, vegaType,
				                                    attrib->normalize != FALSE);
		}
	}
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(m_vertexLayout);
		m_vertexLayout = NULL;
	}
	return status;
}

BOOL CanvasContextWebGL::isCurrentPow2(unsigned int target)
{
	CanvasWebGLTexturePtr texture;
	if (target == WEBGL_TEXTURE_2D)
		texture = m_textures[m_activeTexture];
	else
		texture = m_cubeTextures[m_activeTexture];
	return texture && texture->getTexture() && VEGA3dDevice::isPow2(texture->getTexture()->getHeight()) && VEGA3dDevice::isPow2(texture->getTexture()->getWidth());
}

bool ShouldUseBlackTex(const CanvasWebGLTexturePtr& cTex)
{
	return ((cTex->getMinFilter() != VEGA3dTexture::FILTER_NEAREST && cTex->getMinFilter() != VEGA3dTexture::FILTER_LINEAR && (!cTex->isComplete() || cTex->isNPOT())) ||
			((cTex->getWrapModeS() == VEGA3dTexture::WRAP_REPEAT || cTex->getWrapModeS() == VEGA3dTexture::WRAP_REPEAT_MIRROR) && cTex->isNPOT()));
}

void CanvasContextWebGL::setTextures()
{
	// TODO: Is this needed?
	OP_ASSERT(m_program);
	if (!m_program)
		return;

	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;

	/* For each sampler, we

	    1. Fetch the texture from the virtual WebGL texture unit it is assigned
	    to.

	    2. Bind the texture to the back-end ("real") texture unit for the
	    sampler. This is handled slightly differently depending on the sampler
	    model; see the documentation for
	    SamplersAreTextureUnits/SamplerValuesAreTextureUnits. */

	OpAutoPtr<OpHashIterator> iter(m_program->m_sampler_values.GetIterator());
	RETURN_VOID_IF_ERROR(iter->First());
	do {
		// We need two casts here since no cast seems to allow for casting
		// directly from a pointer type to an integer type of different size
		UINT32 id = (UINT32)(UINTPTR)iter->GetKey();
		UINT32 gl_texunit = (UINT32)(UINTPTR)iter->GetData();

		/* TODO: The WebGL spec does not currently (20 Dec. 2011) appear to
		   specify what should happen if you bind a non-existent textures unit
		   to a sampler. For now, treat all invalid texture units as texture
		   unit 0. */
		if (gl_texunit >= m_maxTextures)
			gl_texunit = 0;

		VEGA3dTexture *vega_tex = NULL;
		CanvasWebGLTexturePtr wgl_tex;

		bool isCubeSampler = (id & IS_SAMPLER_CUBE_BIT) != 0;

		if (isCubeSampler)
		{
			wgl_tex = m_cubeTextures[gl_texunit];
			if (wgl_tex && wgl_tex->getTexture() && !ShouldUseBlackTex(wgl_tex))
				vega_tex = wgl_tex->getTexture();
			else
				vega_tex = m_1x1BlackCubeTexture;
			id = id & ~IS_SAMPLER_CUBE_BIT;
		}
		else
		{
			wgl_tex = m_textures[gl_texunit];
			if (wgl_tex && wgl_tex->getTexture() && !ShouldUseBlackTex(wgl_tex))
				vega_tex = wgl_tex->getTexture();
			else
				vega_tex = m_1x1BlackTexture;
		}

		switch(vega3dDevice->getSamplerModel())
		{
		case VEGA3dDevice::SamplersAreTextureUnits:
			if(isCubeSampler)
				vega3dDevice->setCubeTexture(id, vega_tex);
			else
				vega3dDevice->setTexture(id, vega_tex);
			break;

		case VEGA3dDevice::SamplerValuesAreTextureUnits:
			{
			if (isCubeSampler)
				vega3dDevice->setCubeTexture(gl_texunit, vega_tex);
			else
				vega3dDevice->setTexture(gl_texunit, vega_tex);
			// Undefined if the value of gl_texunit does not fit in an int
			int gl_texunit_int = gl_texunit;
			m_program->getProgram()->setScalar(id, &gl_texunit_int, 1);
			break;
			}

		default:
			OP_ASSERT(!"Unhandled sampler model");
			break;
		}
	} while(OpStatus::IsSuccess(iter->Next()));
}

OP_STATUS CanvasContextWebGL::drawArrays(unsigned int mode, int firstVert, int numVerts)
{
	clearIfNeeded();

	if (firstVert < 0 || numVerts < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	if (m_stencilRef != m_stencilRefBack || m_stencilMask != m_stencilMaskBack || m_stencilWriteMask != m_stencilWriteMaskBack)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (numVerts == 0)
		return OpStatus::OK;

	RETURN_IF_ERROR(updateVertexLayout());
	if (!m_vertexLayout)
		return OpStatus::OK;
	if (m_framebuffer)
	{
		unsigned int fboStatus;
		RETURN_IF_ERROR(m_framebuffer->updateBindings(fboStatus));
		if (fboStatus != WEBGL_FRAMEBUFFER_COMPLETE)
		{
			raiseError(WEBGL_INVALID_FRAMEBUFFER_OPERATION);
			return OpStatus::OK;
		}
	}
	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
	vega3dDevice->setRenderTarget(m_framebuffer?m_framebuffer->getBuffer():m_renderFramebuffer, false);
	vega3dDevice->setViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_viewportNear, m_viewportFar);
	vega3dDevice->setRenderState(m_renderState);
	if (m_renderState->isScissorEnabled())
		vega3dDevice->setScissor(m_scissorX, m_scissorY, m_scissorW, m_scissorH);
	vega3dDevice->setShaderProgram(m_program->getProgram());

	if (mode >= N_WEBGL_MODES)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return OpStatus::OK;
	}

	if (m_cullFace == WEBGL_FRONT_AND_BACK && (mode == WEBGL_TRIANGLES || mode == WEBGL_TRIANGLE_STRIP || mode == WEBGL_TRIANGLE_FAN))
		return OpStatus::OK;

	setTextures();

	if (IntAdditionOverflows(firstVert, numVerts) || (unsigned int)(firstVert + numVerts) > m_maxVertexCount)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (numVerts < GLModeToMinVertexCount[mode])
		return OpStatus::OK;

	vega3dDevice->drawPrimitives(GLModeToVegaPrimitive[mode], m_vertexLayout, firstVert, numVerts);

	return OpStatus::OK;
}

OP_STATUS CanvasContextWebGL::drawElements(unsigned int mode, int numVert, unsigned int type, int offset)
{
	clearIfNeeded();

	if (offset < 0 || numVert < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	if (m_stencilRef != m_stencilRefBack || m_stencilMask != m_stencilMaskBack || m_stencilWriteMask != m_stencilWriteMaskBack)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (numVert == 0)
		return OpStatus::OK;

	unsigned int bytesPerInd = 0;
	int numVertSize = numVert;
	switch (type)
	{
	case WEBGL_UNSIGNED_BYTE:
		bytesPerInd = 1;
		break;
	case WEBGL_UNSIGNED_SHORT:
		bytesPerInd = 2;
		// Check if multiplying by two overflows
		if (numVertSize & 0x40000000)
		{
			raiseError(WEBGL_INVALID_OPERATION);
			return OpStatus::OK;
		}
		numVertSize *= 2;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return OpStatus::OK;
	}

	if (IntAdditionOverflows(offset, numVertSize))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (!m_elementArrayBuffer)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}
	RETURN_IF_ERROR(updateVertexLayout());
	if (!m_vertexLayout)
		return OpStatus::OK;
	if (m_framebuffer)
	{
		unsigned int fboStatus;
		RETURN_IF_ERROR(m_framebuffer->updateBindings(fboStatus));
		if (fboStatus != WEBGL_FRAMEBUFFER_COMPLETE)
		{
			raiseError(WEBGL_INVALID_FRAMEBUFFER_OPERATION);
			return OpStatus::OK;
		}
	}
	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
	vega3dDevice->setRenderTarget(m_framebuffer?m_framebuffer->getBuffer():m_renderFramebuffer, false);
	vega3dDevice->setViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_viewportNear, m_viewportFar);
	vega3dDevice->setRenderState(m_renderState);
	if (m_renderState->isScissorEnabled())
		vega3dDevice->setScissor(m_scissorX, m_scissorY, m_scissorW, m_scissorH);
	vega3dDevice->setShaderProgram(m_program->getProgram());

	if (mode >= N_WEBGL_MODES)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return OpStatus::OK;
	}

	if (offset%bytesPerInd)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}
	if ((unsigned int)(offset+numVertSize) > m_elementArrayBuffer->getSize())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}
	if (m_elementArrayBuffer->getMaxIndex(type) >= m_maxVertexCount)
	{
		if (offset == 0 && (unsigned int)numVertSize == m_elementArrayBuffer->getSize())
		{
			raiseError(WEBGL_INVALID_OPERATION);
			return OpStatus::OK;
		}
		// Do a slower check for this specific case
		if (type == WEBGL_UNSIGNED_BYTE)
		{
			unsigned char* data = m_elementArrayBuffer->getData()+offset;
			for (int i = 0; i < numVert; ++i)
			{
				if (data[i] >= m_maxVertexCount)
				{
					raiseError(WEBGL_INVALID_OPERATION);
					return OpStatus::OK;
				}
			}
		}
		else
		{
			unsigned short* data = (unsigned short*)(m_elementArrayBuffer->getData()+offset);
			for (int i = 0; i < numVert; ++i)
			{
				if (data[i] >= m_maxVertexCount)
				{
					raiseError(WEBGL_INVALID_OPERATION);
					return OpStatus::OK;
				}
			}
		}
	}

	if (m_cullFace == WEBGL_FRONT_AND_BACK && (mode == WEBGL_TRIANGLES || mode == WEBGL_TRIANGLE_STRIP || mode == WEBGL_TRIANGLE_FAN))
		return OpStatus::OK;

	if (numVert < GLModeToMinVertexCount[mode])
		return OpStatus::OK;

	setTextures();

	vega3dDevice->drawIndexedPrimitives(GLModeToVegaPrimitive[mode], m_vertexLayout, m_elementArrayBuffer->getBuffer(), bytesPerInd, offset/bytesPerInd, numVert);

	return OpStatus::OK;
}

void CanvasContextWebGL::finish()
{
	// FIXME: should finish do anything?
}

void CanvasContextWebGL::flush()
{
	// FIXME: should flush do anything?
}

OP_STATUS CanvasContextWebGL::readPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, UINT8* data, unsigned int dataLen)
{
	clearIfNeeded();

	if (width < 0 || height < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	if (IntAdditionOverflows(x, width) || IntAdditionOverflows(y, height))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	if (type == WEBGL_UNSIGNED_SHORT_5_6_5 && format != WEBGL_RGB)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if ((type == WEBGL_UNSIGNED_SHORT_4_4_4_4 || type == WEBGL_UNSIGNED_SHORT_5_5_5_1) && format != WEBGL_RGBA)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (type != WEBGL_UNSIGNED_BYTE || format != WEBGL_RGBA)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return OpStatus::OK;
	}

	if (m_framebuffer)
	{
		unsigned int fboStatus;
		RETURN_IF_ERROR(m_framebuffer->updateBindings(fboStatus));
		if (fboStatus != WEBGL_FRAMEBUFFER_COMPLETE)
		{
			raiseError(WEBGL_INVALID_FRAMEBUFFER_OPERATION);
			return OpStatus::OK;
		}
	}

	if (width == 0 || height == 0)
	{
		return OpStatus::OK;
	}

	// make sure the buffer is big enough
	unsigned int bpp = 4;
	if (!enoughData(width, height, bpp, m_packAlignment, dataLen))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	VEGA3dFramebufferObject* curFBO = m_framebuffer?m_framebuffer->getBuffer():m_renderFramebuffer;
	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
	vega3dDevice->setRenderTarget(curFBO, false);
	vega3dDevice->setViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_viewportNear, m_viewportFar);
	vega3dDevice->setRenderState(m_renderState);
	if (m_renderState->isScissorEnabled())
		vega3dDevice->setScissor(m_scissorX, m_scissorY, m_scissorW, m_scissorH);

	VEGA3dDevice::FramebufferData fbdata;
	fbdata.x = x;
	fbdata.y = y;
	fbdata.w = width;
	fbdata.h = height;
	if (x < 0)
	{
		fbdata.x = 0;
		if (x+width < 0)
			fbdata.w = 0;
		else
			fbdata.w += x;
	}
	if (y < 0)
	{
		fbdata.y = 0;
		if (y+height < 0)
			fbdata.h = 0;
		else
			fbdata.h += y;
	}
	unsigned int fboWidth = curFBO->getWidth();
	unsigned int fboHeight = curFBO->getHeight();
	if (fbdata.x + fbdata.w > fboWidth)
	{
		if (fboWidth <= fbdata.x)
			fbdata.w = 0;
		else
			fbdata.w = fboWidth-fbdata.x;
	}
	if (fbdata.y + fbdata.h > fboHeight)
	{
		if (fboHeight <= fbdata.y)
			fbdata.h = 0;
		else
			fbdata.h = fboHeight-fbdata.y;
	}

	// Format conversion stuff here
	UINT8* dstp = data;
	unsigned int stride = ((width*bpp+m_packAlignment-1)/m_packAlignment)*m_packAlignment;

	if (!fbdata.w || !fbdata.h)
	{
		for (int i = 0; i < height; ++i)
		{
			op_memset(dstp, 0, width*bpp);
			dstp += stride;
		}
		OP_ASSERT(dstp == data + height*stride);
		return OpStatus::OK;
	}

	int syp;
	int yedge = MIN(y+height, 0);
	for (syp = y; syp < yedge; ++syp)
	{
		op_memset(dstp, 0, width*bpp);
		dstp += stride;
	}

	VEGASWBuffer buf;
	RETURN_IF_ERROR(vega3dDevice->lockRect(&fbdata, true));
	OP_STATUS err = buf.Bind(&fbdata.pixels);
	if (OpStatus::IsError(err))
	{
		vega3dDevice->unlockRect(&fbdata, false);
		return err;
	}

	for (unsigned int yp = 0; yp < fbdata.h; ++yp)
	{
#ifdef DEBUG_ENABLE_OPASSERT
		const UINT8* const old_dstp = dstp;
#endif // DEBUG_ENABLE_OPASSERT

		VEGAConstPixelAccessor srcp = buf.GetConstAccessor(0, yp);
		// Actual format conversion
		if ((int)fbdata.x > x)
		{
			op_memset(dstp, 0, (fbdata.x-x)*bpp);
			dstp += (fbdata.x-x)*bpp;
		}
		for (unsigned int xp = 0; xp < fbdata.w; ++xp)
		{
			int sa, sr, sg, sb;
			srcp.LoadUnpack(sa, sr, sg, sb);

			if (format == WEBGL_RGBA)
			{
				dstp[0] = (UINT8)sr;
				dstp[1] = (UINT8)sg;
				dstp[2] = (UINT8)sb;
				dstp[3] = (UINT8)sa;
			}
			else if (format == WEBGL_RGB)
			{
				dstp[0] = (UINT8)sr;
				dstp[1] = (UINT8)sg;
				dstp[2] = (UINT8)sb;
			}
			else
			{
				dstp[0] = (UINT8)sa;
			}
			dstp += bpp;
			srcp++;
		}
		if ((int)(fbdata.x+fbdata.w) < x+width)
		{
			op_memset(dstp, 0, ((x+width)-(fbdata.x+fbdata.w))*bpp);
			dstp += ((x+width)-(fbdata.x+fbdata.w))*bpp;
		}
		dstp += stride-width*bpp;

		OP_ASSERT(dstp == old_dstp + stride);
	}
	buf.Unbind(&fbdata.pixels);
	vega3dDevice->unlockRect(&fbdata, false);

	yedge = MAX((int)fboHeight, y);
	for (syp = yedge; syp < y+height; ++syp)
	{
		op_memset(dstp, 0, width*bpp);
		dstp += stride;
	}

	OP_ASSERT(dstp == data + stride*height);

	return OpStatus::OK;
}

void CanvasContextWebGL::scissor(int x, int y, int width, int height)
{
	if (width < 0 || height < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	m_scissorX = x;
	m_scissorY = y;
	m_scissorW = (unsigned int)width;
	m_scissorH = (unsigned int)height;
}

void CanvasContextWebGL::viewport(int x, int y, int width, int height)
{
	if (width < 0 || height < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	m_viewportX = x;
	m_viewportY = y;
	m_viewportWidth = width;
	m_viewportHeight = height;
}


void CanvasContextWebGL::bindBuffer(unsigned int target, CanvasWebGLBuffer* buffer)
{
	if (buffer && buffer->isDestroyed())
		return;
	if (buffer && buffer->getBinding() != 0 && buffer->getBinding() != target)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}
	switch (target)
	{
	case WEBGL_ARRAY_BUFFER:
		m_arrayBuffer = buffer;
		break;
	case WEBGL_ELEMENT_ARRAY_BUFFER:
		m_elementArrayBuffer = buffer;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (buffer)
		buffer->setBinding(target);
}

OP_STATUS CanvasContextWebGL::bufferData(unsigned int target, unsigned int usage, int size, void* data)
{
	CanvasWebGLBufferPtr buf;
	VEGA3dBuffer::Usage u;
	switch (target)
	{
	case WEBGL_ARRAY_BUFFER:
		buf = m_arrayBuffer;
		break;
	case WEBGL_ELEMENT_ARRAY_BUFFER:
		buf = m_elementArrayBuffer;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		// Using WebGL error only
		return OpStatus::OK;
	}
	if (!buf)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}
	switch (usage)
	{
	case WEBGL_STATIC_DRAW:
		u = VEGA3dBuffer::STATIC;
		break;
	case WEBGL_DYNAMIC_DRAW:
		u = VEGA3dBuffer::DYNAMIC;
		break;
	case WEBGL_STREAM_DRAW:
		u = VEGA3dBuffer::STREAM;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		// Using WebGL error only
		return OpStatus::OK;
	}
	if (size < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		// Using WebGL error only
		return OpStatus::OK;
	}
	// FIXME: only required if buf is bound in a vertex attrib when this happens
	VEGARefCount::DecRef(m_vertexLayout);
	m_vertexLayout = NULL;

	RETURN_IF_ERROR(buf->create(u, size));
	if (data)
	{
		buf->getBuffer()->writeAtOffset(0, size, data);
		if (buf->getData())
			op_memcpy(buf->getData(), data, size);
		buf->resetMaxIndex();
	}
	else if (size)
	{
		void* d = op_malloc(size);
		if (!d)
		{
			buf->destroy();
			return OpStatus::ERR_NO_MEMORY;
		}
		op_memset(d, 0, size);
		buf->getBuffer()->writeAtOffset(0, size, d);
		op_free(d);
		if (buf->getData())
			op_memset(buf->getData(), 0, size);
		buf->resetMaxIndex();
	}
	return OpStatus::OK;
}

OP_STATUS CanvasContextWebGL::bufferSubData(unsigned int target, int offset, int size, void* data)
{
	CanvasWebGLBufferPtr buf;
	switch (target)
	{
	case WEBGL_ARRAY_BUFFER:
		buf = m_arrayBuffer;
		break;
	case WEBGL_ELEMENT_ARRAY_BUFFER:
		buf = m_elementArrayBuffer;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		// Using WebGL error only
		return OpStatus::OK;
	}
	if (!buf || !buf->getBuffer())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		// Using WebGL error only
		return OpStatus::OK;
	}
	if (offset < 0 || size < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		// Using WebGL error only
		return OpStatus::OK;
	}
	if ((unsigned int)(offset+size) > buf->getSize())
	{
		raiseError(WEBGL_INVALID_VALUE);
		// Using WebGL error only
		return OpStatus::OK;
	}
	if (size == 0)
		return OpStatus::OK;
	buf->getBuffer()->writeAtOffset(offset, size, data);
	if (buf->getData())
		op_memcpy(buf->getData()+offset, data, size);
	buf->resetMaxIndex();
	return OpStatus::OK;
}

void CanvasContextWebGL::deleteBuffer(CanvasWebGLBuffer* buffer)
{
	if (m_arrayBuffer == buffer)
		m_arrayBuffer = NULL;
	if (m_elementArrayBuffer == buffer)
		m_elementArrayBuffer = NULL;
	for (unsigned int i = 0; i < m_maxVertexAttribs; ++i)
	{
		if (m_vertexAttribs[i].buffer == buffer)
			m_vertexAttribs[i].buffer = NULL;
	}
	buffer->destroy();
}

CanvasWebGLParameter CanvasContextWebGL::getBufferParameter(unsigned int target, unsigned int param)
{
	CanvasWebGLParameter p;
	// Return NULL on error
	p.type = CanvasWebGLParameter::PARAM_OBJECT;
	p.value.object_param = NULL;
	CanvasWebGLBufferPtr buf;
	switch (target)
	{
	case WEBGL_ARRAY_BUFFER:
		buf = m_arrayBuffer;
		break;
	case WEBGL_ELEMENT_ARRAY_BUFFER:
		buf = m_elementArrayBuffer;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return p;
	}
	switch (param)
	{
	case WEBGL_BUFFER_SIZE:
		if (!buf)
		{
			raiseError(WEBGL_INVALID_OPERATION);
		}
		else if (buf->getBuffer())
		{
			p.type = CanvasWebGLParameter::PARAM_INT;
			p.value.int_param[0] = buf->getSize();
		}
		break;
	case WEBGL_BUFFER_USAGE:
		if (!buf)
		{
			raiseError(WEBGL_INVALID_OPERATION);
		}
		else if (buf->getBuffer())
		{
			p.type = CanvasWebGLParameter::PARAM_UINT;
			p.value.uint_param[0] = WEBGL_STATIC_DRAW;
			switch (buf->getUsage())
			{
			case VEGA3dBuffer::STREAM:
				p.value.uint_param[0] = WEBGL_STREAM_DRAW;
				break;
			case VEGA3dBuffer::DYNAMIC:
				p.value.uint_param[0] = WEBGL_DYNAMIC_DRAW;
				break;
			case VEGA3dBuffer::STATIC:
				p.value.uint_param[0] = WEBGL_STATIC_DRAW;
				break;
			default:
				break;
			}
		}
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
	return p;
}

// Framebuffer objects
void CanvasContextWebGL::bindFramebuffer(unsigned int target, CanvasWebGLFramebuffer* framebuffer)
{
	if (target != WEBGL_FRAMEBUFFER)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (m_framebuffer != framebuffer)
	{
		m_framebuffer = framebuffer;
		if (m_framebuffer)
			m_framebuffer->setIsBound();
	}
}

unsigned int CanvasContextWebGL::checkFramebufferStatus(unsigned int target)
{
	if (target != WEBGL_FRAMEBUFFER)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return 0;
	}
	if (m_framebuffer)
		return m_framebuffer->checkStatus();
	return WEBGL_FRAMEBUFFER_COMPLETE;
}

void CanvasContextWebGL::deleteFramebuffer(CanvasWebGLFramebuffer* framebuffer)
{
	if (framebuffer)
	{
		// If we're deleting the bound FBO we need to set the current back to the default FBO.
		if (m_framebuffer == framebuffer)
			m_framebuffer = NULL;

		framebuffer->destroy();
	}
}

void CanvasContextWebGL::framebufferRenderbuffer(unsigned int target, unsigned int attachment, unsigned int renderbuffertarget, CanvasWebGLRenderbuffer* renderbuffer)
{
	if (target != WEBGL_FRAMEBUFFER)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (renderbuffer && renderbuffertarget != WEBGL_RENDERBUFFER)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (attachment != WEBGL_COLOR_ATTACHMENT0 && attachment != WEBGL_DEPTH_ATTACHMENT && attachment != WEBGL_STENCIL_ATTACHMENT && attachment != WEBGL_DEPTH_STENCIL_ATTACHMENT)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (!m_framebuffer || m_framebuffer->isDestroyed() || (renderbuffer && renderbuffer->isDestroyed()))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	switch (attachment)
	{
	case WEBGL_COLOR_ATTACHMENT0:
		m_framebuffer->setColorBuffer(renderbuffer);
		break;
	case WEBGL_DEPTH_ATTACHMENT:
		m_framebuffer->setDepthBuffer(renderbuffer);
		break;
	case WEBGL_STENCIL_ATTACHMENT:
		m_framebuffer->setStencilBuffer(renderbuffer);
		break;
	case WEBGL_DEPTH_STENCIL_ATTACHMENT:
		m_framebuffer->setDepthStencilBuffer(renderbuffer);
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}

	unsigned int fboStatus;
	OpStatus::Ignore(m_framebuffer->updateBindings(fboStatus));
	if (fboStatus == WEBGL_FRAMEBUFFER_COMPLETE)
	{
		initializeRenderbufferData();
	}
}

void CanvasContextWebGL::initializeRenderbufferData()
{
	bool color = false;
	bool depth = false;
	bool stencil = false;

	if (CanvasWebGLRenderbuffer* buf = m_framebuffer->getColorBuffer())
	{
		color = !buf->isInitialized();
		buf->setInitialized();
	}

	if (CanvasWebGLRenderbuffer* buf = m_framebuffer->getDepthBuffer())
	{
		depth = !buf->isInitialized();
		buf->setInitialized();
	}

	if (CanvasWebGLRenderbuffer* buf = m_framebuffer->getStencilBuffer())
	{
		stencil = !buf->isInitialized();
		buf->setInitialized();
	}

	if (CanvasWebGLRenderbuffer* buf = m_framebuffer->getDepthStencilBuffer())
	{
		depth = !buf->isInitialized();
		stencil = !buf->isInitialized();
		buf->setInitialized();
	}

	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
	VEGA3dRenderTarget *originalRT = vega3dDevice->getRenderTarget();

	// We will be calling clear() below, but we have no guarantee that m_framebuffer contains
	// complete Vega fbo. Depth, Stencil and Color buffers were attached, but it was only
	// attachement on CanvasWebGLRenderbuffer level, not on vega (and OpenGL) level.
	// So we must attach lower-level buffers here:
	unsigned int fboStatus;
	OpStatus::Ignore(m_framebuffer->updateBindings(fboStatus));
	OP_ASSERT(fboStatus == WEBGL_FRAMEBUFFER_COMPLETE);

	vega3dDevice->setRenderTarget(m_framebuffer->getBuffer());

	vega3dDevice->setRenderState(vega3dDevice->getDefault2dNoBlendNoScissorRenderState());

	if (color || depth || stencil)
		vega3dDevice->clear(color, depth, stencil, 0, 1.f, 0);

	vega3dDevice->setRenderTarget(originalRT);
}

void CanvasContextWebGL::framebufferTexture2D(unsigned int target, unsigned int attachment, unsigned int textarget, CanvasWebGLTexture* texture, int level)
{
	if (target != WEBGL_FRAMEBUFFER)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (texture && textarget != WEBGL_TEXTURE_2D &&
		textarget != WEBGL_TEXTURE_CUBE_MAP_POSITIVE_X && textarget != WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_X &&
		textarget != WEBGL_TEXTURE_CUBE_MAP_POSITIVE_Y && textarget != WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_Y &&
		textarget != WEBGL_TEXTURE_CUBE_MAP_POSITIVE_Z && textarget != WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (level != 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	if (!m_framebuffer || m_framebuffer->isDestroyed() || (texture && texture->isDestroyed()))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}
	switch (attachment)
	{
	case WEBGL_COLOR_ATTACHMENT0:
		m_framebuffer->setColorTexture(texture, textarget);
		break;
	case WEBGL_DEPTH_ATTACHMENT:
		m_framebuffer->setDepthTexture(texture, textarget);
		break;
	case WEBGL_STENCIL_ATTACHMENT:
		m_framebuffer->setStencilTexture(texture, textarget);
		break;
	case WEBGL_DEPTH_STENCIL_ATTACHMENT:
		m_framebuffer->setDepthStencilTexture(texture, textarget);
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
}

CanvasWebGLParameter CanvasContextWebGL::getFramebufferAttachmentParameter(unsigned int target, unsigned int attachment, unsigned int param)
{
	CanvasWebGLParameter p;
	// Return NULL on error
	p.type = CanvasWebGLParameter::PARAM_OBJECT;
	p.value.object_param = NULL;
	if (target != WEBGL_FRAMEBUFFER)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return p;
	}
	if (!m_framebuffer || m_framebuffer->isDestroyed())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return p;
	}
	CanvasWebGLRenderbuffer* renderbuffer = NULL;
	CanvasWebGLTexture* texture = NULL;
	unsigned int textureFace;
	switch (attachment)
	{
	case WEBGL_COLOR_ATTACHMENT0:
		renderbuffer = m_framebuffer->getColorBuffer();
		texture = m_framebuffer->getColorTexture();
		textureFace = m_framebuffer->getColorTextureFace();
		break;
	case WEBGL_DEPTH_ATTACHMENT:
		renderbuffer = m_framebuffer->getDepthBuffer();
		texture = m_framebuffer->getDepthTexture();
		textureFace = m_framebuffer->getDepthTextureFace();
		break;
	case WEBGL_STENCIL_ATTACHMENT:
		renderbuffer = m_framebuffer->getStencilBuffer();
		texture = m_framebuffer->getStencilTexture();
		textureFace = m_framebuffer->getStencilTextureFace();
		break;
	case WEBGL_DEPTH_STENCIL_ATTACHMENT:
		renderbuffer = m_framebuffer->getDepthStencilBuffer();
		texture = m_framebuffer->getDepthStencilTexture();
		textureFace = m_framebuffer->getDepthStencilTextureFace();
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return p;
	}
	switch (param)
	{
	case WEBGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		if (renderbuffer)
			p.value.uint_param[0] = WEBGL_RENDERBUFFER;
		else if (texture)
			p.value.uint_param[0] = WEBGL_TEXTURE;
		else
			p.value.uint_param[0] = WEBGL_NONE;
		break;
	case WEBGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
		if (renderbuffer)
		{
			p.type = CanvasWebGLParameter::PARAM_OBJECT;
			p.value.object_param = renderbuffer->isDestroyed() ? NULL : renderbuffer->getESObject();
		}
		else if (texture)
		{
			p.type = CanvasWebGLParameter::PARAM_OBJECT;
			p.value.object_param = texture->isDestroyed() ? NULL : texture->getESObject();
		}
		else
		{
			raiseError(WEBGL_INVALID_ENUM);
		}
		break;
	case WEBGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = 0;
		// FIXME:
		/*if (texture)
		{
			p.value.int_param[0] = m_framebuffer->getTextureLevel();
		}*/
		if (!texture)
			raiseError(WEBGL_INVALID_ENUM);
		break;
	case WEBGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		if (texture && texture->isCubeMap())
		{
			p.value.uint_param[0] = textureFace;
		}
		else
		{
			p.value.uint_param[0] = 0;
		}
		if (!texture)
			raiseError(WEBGL_INVALID_ENUM);
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return p;
	}
	return p;
}

// Renderbuffer objects
void CanvasContextWebGL::bindRenderbuffer(unsigned int target, CanvasWebGLRenderbuffer* renderbuffer)
{
	if (target != WEBGL_RENDERBUFFER)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (m_renderbuffer != renderbuffer)
	{
		m_renderbuffer = renderbuffer;
		if (m_renderbuffer)
			m_renderbuffer->setIsBound();
	}
}

void CanvasContextWebGL::deleteRenderbuffer(CanvasWebGLRenderbuffer* renderbuffer)
{
	if (renderbuffer)
	{
		if (m_framebuffer)
		{
			if (m_framebuffer->getColorBuffer() == renderbuffer)
				m_framebuffer->setColorBuffer(NULL);
			if (m_framebuffer->getDepthBuffer() == renderbuffer)
				m_framebuffer->setDepthBuffer(NULL);
			if (m_framebuffer->getStencilBuffer() == renderbuffer)
				m_framebuffer->setStencilBuffer(NULL);
			if (m_framebuffer->getDepthStencilBuffer() == renderbuffer)
			{
				m_framebuffer->setDepthStencilBuffer(NULL);
				m_framebuffer->setStencilBuffer(NULL);
			}
		}
		if (m_renderbuffer == renderbuffer)
			m_renderbuffer = NULL;
		renderbuffer->destroy();
	}
}

CanvasWebGLParameter CanvasContextWebGL::getRenderbufferParameter(unsigned int target, unsigned int param)
{
	CanvasWebGLParameter p;
	// Return NULL on error
	p.type = CanvasWebGLParameter::PARAM_OBJECT;
	p.value.object_param = NULL;
	if (target != WEBGL_RENDERBUFFER)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return p;
	}
	if (!m_renderbuffer || m_renderbuffer->isDestroyed())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return p;
	}
	VEGA3dRenderbufferObject *buffer = m_renderbuffer->getBuffer();
	switch (param)
	{
	case WEBGL_RENDERBUFFER_WIDTH:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = buffer ? buffer->getWidth() : 0;
		break;
	case WEBGL_RENDERBUFFER_HEIGHT:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = buffer ? buffer->getHeight() : 0;
		break;
	case WEBGL_RENDERBUFFER_INTERNAL_FORMAT:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		if (!buffer)
		{
			p.value.uint_param[0] = WEBGL_RGB565;
			return p;
		}
		switch (buffer->getFormat())
		{
		case VEGA3dRenderbufferObject::FORMAT_RGB565:
			p.value.uint_param[0] = WEBGL_RGB565;
			break;
		case VEGA3dRenderbufferObject::FORMAT_RGB5_A1:
			p.value.uint_param[0] = WEBGL_RGB5_A1;
			break;
		case VEGA3dRenderbufferObject::FORMAT_RGBA4:
			p.value.uint_param[0] = WEBGL_RGBA4;
			break;
		case VEGA3dRenderbufferObject::FORMAT_DEPTH16:
			p.value.uint_param[0] = WEBGL_DEPTH_COMPONENT16;
			break;
		case VEGA3dRenderbufferObject::FORMAT_STENCIL8:
			p.value.uint_param[0] = WEBGL_STENCIL_INDEX8;
			break;
		case VEGA3dRenderbufferObject::FORMAT_DEPTH16_STENCIL8:
			p.value.uint_param[0] = WEBGL_DEPTH_STENCIL;
			break;
		}
		break;
	case WEBGL_RENDERBUFFER_RED_SIZE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = buffer ? buffer->getRedBits() : 0;
		break;
	case WEBGL_RENDERBUFFER_GREEN_SIZE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = buffer ? buffer->getGreenBits() : 0;
		break;
	case WEBGL_RENDERBUFFER_BLUE_SIZE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = buffer ? buffer->getBlueBits() : 0;
		break;
	case WEBGL_RENDERBUFFER_ALPHA_SIZE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = buffer ? buffer->getAlphaBits() : 0;
		break;
	case WEBGL_RENDERBUFFER_DEPTH_SIZE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = buffer ? buffer->getDepthBits() : 0;
		break;
	case WEBGL_RENDERBUFFER_STENCIL_SIZE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = buffer ? buffer->getStencilBits() : 0;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
	return p;
}

OP_STATUS CanvasContextWebGL::renderbufferStorage(unsigned int target, unsigned int internalformat, int width, int height)
{
	if (target != WEBGL_RENDERBUFFER)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return OpStatus::OK;
	}
	VEGA3dRenderbufferObject::ColorFormat fmt;
	switch (internalformat)
	{
	case WEBGL_RGBA4:
		fmt = VEGA3dRenderbufferObject::FORMAT_RGBA4;
		break;
	case WEBGL_RGB565:
		fmt = VEGA3dRenderbufferObject::FORMAT_RGB565;
		break;
	case WEBGL_RGB5_A1:
		fmt = VEGA3dRenderbufferObject::FORMAT_RGB5_A1;
		break;
	case WEBGL_DEPTH_COMPONENT16:
		fmt = VEGA3dRenderbufferObject::FORMAT_DEPTH16;
		break;
	case WEBGL_STENCIL_INDEX8:
		fmt = VEGA3dRenderbufferObject::FORMAT_STENCIL8;
		break;
	case WEBGL_DEPTH_STENCIL:
		fmt = VEGA3dRenderbufferObject::FORMAT_DEPTH16_STENCIL8;
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return OpStatus::OK;
	}
	if (width < 0 || height < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}
	if (!m_renderbuffer || m_renderbuffer->isDestroyed())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}
	return m_renderbuffer->create(fmt, width, height);
}

// Texture objects
void CanvasContextWebGL::bindTexture(unsigned int target, CanvasWebGLTexture* texture)
{
	switch (target)
	{
	case WEBGL_TEXTURE_2D:
		if (texture && texture->getTexture() && texture->isCubeMap())
		{
			raiseError(WEBGL_INVALID_OPERATION);
			return;
		}
		if (m_textures[m_activeTexture] != texture)
		{
			m_textures[m_activeTexture] = texture;
			if (m_textures[m_activeTexture])
				m_textures[m_activeTexture]->setIsBound();
		}
		break;
	case WEBGL_TEXTURE_CUBE_MAP:
		if (texture && texture->getTexture() && !texture->isCubeMap())
		{
			raiseError(WEBGL_INVALID_OPERATION);
			return;
		}
		if (m_cubeTextures[m_activeTexture] != texture)
		{
			m_cubeTextures[m_activeTexture] = texture;
			if (m_cubeTextures[m_activeTexture])
				m_cubeTextures[m_activeTexture]->setIsBound();
		}
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
}

OP_STATUS CanvasContextWebGL::copyTexImage2D(unsigned int target, int level, unsigned int internalfmt, int x, int y, int width, int height, int border)
{
	clearIfNeeded();

	if (IntAdditionOverflows(x, width) || IntAdditionOverflows(y, height))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	if (m_framebuffer)
	{
		unsigned int fboStatus;
		RETURN_IF_ERROR(m_framebuffer->updateBindings(fboStatus));
		if (fboStatus != WEBGL_FRAMEBUFFER_COMPLETE)
		{
			raiseError(WEBGL_INVALID_FRAMEBUFFER_OPERATION);
			return OpStatus::OK;
		}
	}
	CanvasWebGLTexturePtr texture;
	VEGA3dTexture::CubeSide cubeSide;
	RETURN_IF_WGL_ERROR(texImage2DCommon(target, level, width, height, border, texture, cubeSide));

	unsigned int framebufferFmt = 0;
	unsigned int framebufferType = 0;
	getFramebufferColorFormat(framebufferFmt, framebufferType);

	if (framebufferFmt == 0 || framebufferType == 0)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	// Check if the framebuffer format is a superset. If it is set it to match the internal format.
	if (framebufferFmt == WEBGL_RGBA)
	{
		if (internalfmt == WEBGL_RGB || internalfmt == WEBGL_ALPHA)
			framebufferFmt = internalfmt;
	}
	else if (framebufferFmt == WEBGL_LUMINANCE_ALPHA)
	{
		if (internalfmt == WEBGL_LUMINANCE || internalfmt == WEBGL_ALPHA)
			framebufferFmt = internalfmt;
	}

	RETURN_IF_WGL_ERROR(texImage2DFormatCheck(target, internalfmt, framebufferFmt, framebufferType));

	VEGA3dTexture::ColorFormat colFmt = GLFormatTypeToVegaTextureColorFormat(framebufferFmt, framebufferType);

	VEGA3dFramebufferObject *curFBO = m_framebuffer ? m_framebuffer->getBuffer() : m_renderFramebuffer;

	bool readingOutsideFBO = false;
	int fboWidth = curFBO->getWidth(),
		fboHeight = curFBO->getHeight(),
		srcOffsetX = x,
		srcOffsetY = y,
		srcWidth = width,
		srcHeight = height,
		dstOffsetX = 0,
		dstOffsetY = 0;

	// Check if any of the pixel data to be copied is outside of the valid area.
	if (srcOffsetX < 0 || srcOffsetY < 0 || srcOffsetX + srcWidth > fboWidth || srcOffsetY + srcHeight > fboHeight)
	{
		readingOutsideFBO = true;
		if (srcOffsetX < 0)
		{
			dstOffsetX = -srcOffsetX;
			srcOffsetX = 0;
		}
		if (srcOffsetY < 0)
		{
			dstOffsetY = -srcOffsetY;
			srcOffsetY = 0;
		}
		srcWidth = MIN(srcWidth - dstOffsetX, fboWidth-srcOffsetX);
		srcHeight = MIN(srcHeight - dstOffsetY, fboHeight-srcOffsetY);
	}

	CanvasWebGLTexture::MipChain *mipChain = texture->getMipChain(width, height, level, colFmt);
	// Any mip level > 0 which has at least one of width or height equal to 1 need special treatment as
	// we don't know which MipChain it belongs to.
	if ((width == 1 || height == 1) && level != 0)
	{
		// Get hold of the MipData for the specified level, either overwriting an old or allocating a new one.
		CanvasWebGLTexture::MipData *mipData = NULL;
		RETURN_IF_ERROR(texture->getMipData(level, cubeSide, mipData));

		// Set all the data which we'll need to slot this in to a MipChain created at a later stage.
		mipData->dataType = CanvasWebGLTexture::MipData::MIPDATA_SWBUFFER;
		mipData->fmt = colFmt;
		mipData->cubeSide = cubeSide;
		mipData->level = level;
		mipData->width = width;
		mipData->height = height;
		mipData->unpackAlignment = m_unpackAlignment;
		mipData->unpackFlipY = m_unpackFlipY;
		mipData->unpackPremultiplyAlpha = m_unpackPremultiplyAlpha;
		// Initialize the swbuffer to hold all the data.
		RETURN_IF_ERROR(mipData->swbuffer.Create((unsigned int)width, (unsigned int)height));

		// If we're reading outside of the FBO we need to initialize the buffer to zeroes first.
		if (readingOutsideFBO)
			mipData->swbuffer.FillRect(0,0, width, height, (VEGA_PIXEL)0);

		// Read out the data from the FBO.
		VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
		vega3dDevice->setRenderTarget(curFBO, false);
		vega3dDevice->setViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_viewportNear, m_viewportFar);
		vega3dDevice->setRenderState(m_renderState);
		if (m_renderState->isScissorEnabled())
			vega3dDevice->setScissor(m_scissorX, m_scissorY, m_scissorW, m_scissorH);
		VEGA3dDevice::FramebufferData fbdata;
		fbdata.x = srcOffsetX;
		fbdata.y = srcOffsetY;
		fbdata.w = srcWidth;
		fbdata.h = srcHeight;
		RETURN_IF_ERROR(vega3dDevice->lockRect(&fbdata, true));

		// Create a sub-view for the data we actually read and copy the pixelbuffer content into it.
		VEGASWBuffer subBuffer = mipData->swbuffer.CreateSubset(dstOffsetX, dstOffsetY, srcWidth, srcHeight);
		subBuffer.CopyFromPixelStore(&fbdata.pixels);
	}
	else
	{
		// Clear any MipData set at this level and any MipChain that this level is set in.
		texture->clearMipLevel(level, cubeSide, mipChain, false);

		// If there's no matching MipChain.
		if (!mipChain)
		{
			// Create a new MipChain.
			unsigned int level0Width = width << level;
			unsigned int level0Height = height << level;
			RETURN_IF_ERROR(texture->create(colFmt, level0Width, level0Height, cubeSide != VEGA3dTexture::CUBE_SIDE_NONE, &mipChain));

			// Apply any matching MipData to it.
			RETURN_IF_ERROR(texture->applyMipData(mipChain));
		}
	}

	if (mipChain)
	{
		// If we're reading outside of the FBO we blank out the texture with zeroes first.
		if (readingOutsideFBO)
		{
			VEGASWBuffer blackBuf;
			RETURN_IF_ERROR(blackBuf.Create((unsigned int)width, (unsigned int)height));
			blackBuf.FillRect(0, 0, width, height, (VEGA_PIXEL)0);
			OP_STATUS err = texture->getTexture()->update(0, 0, &blackBuf, 4, cubeSide, level);
			blackBuf.Destroy();
			RETURN_IF_ERROR(err);
		}

		// Copy the (cropped) data to the texture.
		if (srcWidth != 0 && srcHeight != 0)
		{
			VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
			vega3dDevice->setRenderTarget(curFBO, false);
			vega3dDevice->setViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_viewportNear, m_viewportFar);
			vega3dDevice->setRenderState(m_renderState);
			if (m_renderState->isScissorEnabled())
				vega3dDevice->setScissor(m_scissorX, m_scissorY, m_scissorW, m_scissorH);
			vega3dDevice->copyToTexture(mipChain->getTexture(), cubeSide, level, dstOffsetX, dstOffsetY, srcOffsetX, srcOffsetY, srcWidth, srcHeight);
		}

		// This is now a valid mip level.
		mipChain->setMipLevel(level, cubeSide, true);
	}
	if (level == 0)
		texture->setActiveMipChain(mipChain);

	return OpStatus::OK;
}

OP_STATUS CanvasContextWebGL::copyTexSubImage2D(unsigned int target, int level, int xofs, int yofs, int x, int y, int width, int height)
{
	clearIfNeeded();

	if (IntAdditionOverflows(x, width) || IntAdditionOverflows(y, height) ||
		IntAdditionOverflows(xofs, width) || IntAdditionOverflows(yofs, height))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	if (m_framebuffer)
	{
		unsigned int fboStatus;
		RETURN_IF_ERROR(m_framebuffer->updateBindings(fboStatus));
		if (fboStatus != WEBGL_FRAMEBUFFER_COMPLETE)
		{
			raiseError(WEBGL_INVALID_FRAMEBUFFER_OPERATION);
			return OpStatus::OK;
		}
	}
	CanvasWebGLTexturePtr texture;
	VEGA3dTexture::CubeSide cubeSide;
	RETURN_IF_WGL_ERROR(texImage2DCommon(target, level, width, height, 0, texture, cubeSide));

	if (!texture->getTexture())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (xofs < 0 || yofs < 0 || (unsigned int)(xofs+width) > texture->getTexture()->getWidth() || (unsigned int)(yofs+height) > texture->getTexture()->getHeight())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	unsigned int texFormat, texType;
	VegaTextureColorFormatToGL(texture->getTexture()->getFormat(), texFormat, texType);

	unsigned int framebufferFmt = 0;
	unsigned int framebufferType = 0;
	getFramebufferColorFormat(framebufferFmt, framebufferType);

	if (framebufferFmt == 0 || framebufferType == 0)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	// Check if the framebuffer format is a superset. If it is set it to match the internal format.
	if (framebufferFmt == WEBGL_RGBA)
	{
		if (texFormat == WEBGL_RGB || texFormat == WEBGL_ALPHA)
			framebufferFmt = texFormat;
	}
	else if (framebufferFmt == WEBGL_LUMINANCE_ALPHA)
	{
		if (texFormat == WEBGL_LUMINANCE || texFormat == WEBGL_ALPHA)
			framebufferFmt = texFormat;
	}

	RETURN_IF_WGL_ERROR(texImage2DFormatCheck(target, texFormat, framebufferFmt, framebufferType));

	VEGA3dFramebufferObject* curFBO = m_framebuffer?m_framebuffer->getBuffer():m_renderFramebuffer;

	// Check if any of the pixel data to be copied is outside of the valid area.
	int fboWidth = curFBO->getWidth(),
		fboHeight = curFBO->getHeight();
	if (x < 0 || y < 0 || x + width > fboWidth || y + height > fboHeight)
	{
		// If it is make sure to initialize the destination with zeroes first.
		VEGASWBuffer blackBuf;
		RETURN_IF_ERROR(blackBuf.Create((unsigned int)width, (unsigned int)height));
		blackBuf.FillRect(0,0, width, height, (VEGA_PIXEL)0);
		OP_STATUS err = texture->getTexture()->update(0, 0, &blackBuf, 4, cubeSide, level);
		blackBuf.Destroy();
		RETURN_IF_ERROR(err);

		if (x < 0)
		{
			xofs += -x;
			x = 0;
		}
		if (y < 0)
		{
			yofs += -y;
			y = 0;
		}
		width = MIN(width - xofs, fboWidth-x);
		height = MIN(height - yofs, fboHeight-y);
	}

	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
	vega3dDevice->setRenderTarget(curFBO, false);
	vega3dDevice->setViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_viewportNear, m_viewportFar);
	vega3dDevice->setRenderState(m_renderState);
	if (m_renderState->isScissorEnabled())
		vega3dDevice->setScissor(m_scissorX, m_scissorY, m_scissorW, m_scissorH);
	vega3dDevice->copyToTexture(texture->getTexture(), cubeSide, level, xofs, yofs, x, y, width, height);
	return OpStatus::OK;
}

void CanvasContextWebGL::deleteTexture(CanvasWebGLTexture* texture)
{
	if (m_framebuffer)
	{
		if (m_framebuffer->getColorTexture() == texture)
			m_framebuffer->setColorTexture(NULL, 0);
		if (m_framebuffer->getDepthTexture() == texture)
			m_framebuffer->setDepthTexture(NULL, 0);
		if (m_framebuffer->getStencilTexture() == texture)
			m_framebuffer->setStencilTexture(NULL, 0);
		if (m_framebuffer->getDepthStencilTexture() == texture)
		{
			m_framebuffer->setDepthStencilTexture(NULL, 0);
			m_framebuffer->setStencilTexture(NULL, 0);
		}
	}
	for (unsigned int i = 0; i < m_maxTextures; ++i)
	{
		if (m_textures[i] == texture)
			m_textures[i] = NULL;
		if (m_cubeTextures[i] == texture)
			m_cubeTextures[i] = NULL;
	}
	texture->destroy();
}

void CanvasContextWebGL::generateMipmap(unsigned int target)
{
	CanvasWebGLTexturePtr texture;
	switch (target)
	{
	case WEBGL_TEXTURE_2D:
		texture = m_textures[m_activeTexture];
		break;
	case WEBGL_TEXTURE_CUBE_MAP:
		texture = m_cubeTextures[m_activeTexture];
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return;
		break;
	};

	if (texture)
	{
		if (!isCurrentPow2(target) || (target != WEBGL_TEXTURE_2D && !texture->isCubeComplete()))
		{
			raiseError(WEBGL_INVALID_OPERATION);
			return;
		}
		texture->generateMipMaps(m_hintMode);
	}
}


CanvasWebGLParameter CanvasContextWebGL::getTexParameter(unsigned int target, unsigned int param)
{
	CanvasWebGLParameter p;
	// Return NULL on error
	p.type = CanvasWebGLParameter::PARAM_OBJECT;
	p.value.object_param = NULL;

	if (target != WEBGL_TEXTURE_2D && target != WEBGL_TEXTURE_CUBE_MAP)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return p;
	}

	CanvasWebGLTexturePtr tex = target == WEBGL_TEXTURE_2D ? m_textures[m_activeTexture] : m_cubeTextures[m_activeTexture];
	if (!tex)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return p;
	}

	switch (param)
	{
	case WEBGL_TEXTURE_MAG_FILTER:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = VegaToGLFilterMode[tex->getMinFilter()];
		break;
	case WEBGL_TEXTURE_MIN_FILTER:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = VegaToGLFilterMode[tex->getMagFilter()];
		break;
	case WEBGL_TEXTURE_WRAP_S:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = VegaToGLWrapMode[tex->getWrapModeS()];
		break;
	case WEBGL_TEXTURE_WRAP_T:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = VegaToGLWrapMode[tex->getWrapModeT()];
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}

	return p;
}

unsigned int CanvasContextWebGL::texImage2DCommon(unsigned int target, int level, int width, int height, int border, CanvasWebGLTexturePtr& texture, VEGA3dTexture::CubeSide& cubeSide)
{
	switch (target)
	{
	case WEBGL_TEXTURE_2D:
		texture = m_textures[m_activeTexture];
		cubeSide = VEGA3dTexture::CUBE_SIDE_NONE;
		break;
	case WEBGL_TEXTURE_CUBE_MAP_POSITIVE_X:
		texture = m_cubeTextures[m_activeTexture];
		cubeSide = VEGA3dTexture::CUBE_SIDE_POS_X;
		break;
	case WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		texture = m_cubeTextures[m_activeTexture];
		cubeSide = VEGA3dTexture::CUBE_SIDE_NEG_X;
		break;
	case WEBGL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		texture = m_cubeTextures[m_activeTexture];
		cubeSide = VEGA3dTexture::CUBE_SIDE_POS_Y;
		break;
	case WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		texture = m_cubeTextures[m_activeTexture];
		cubeSide = VEGA3dTexture::CUBE_SIDE_NEG_Y;
		break;
	case WEBGL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		texture = m_cubeTextures[m_activeTexture];
		cubeSide = VEGA3dTexture::CUBE_SIDE_POS_Z;
		break;
	case WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		texture = m_cubeTextures[m_activeTexture];
		cubeSide = VEGA3dTexture::CUBE_SIDE_NEG_Z;
		break;
	default:
		return WEBGL_INVALID_ENUM;
	}
	if (!texture)
		return WEBGL_INVALID_OPERATION;
	VEGA3dDevice *vega3dDevice = g_vegaGlobals.vega3dDevice;
	if (level < 0 || (unsigned int)level > vega3dDevice->getMaxMipLevel())
		return WEBGL_INVALID_VALUE;
	unsigned int maxSize = target == WEBGL_TEXTURE_2D ? vega3dDevice->getMaxTextureSize() : vega3dDevice->getMaxCubeMapTextureSize();
	if ((unsigned int)width > maxSize || (unsigned int)height > maxSize)
		return WEBGL_INVALID_VALUE;
	if (border != 0 || width < 0 || height < 0 || (cubeSide != VEGA3dTexture::CUBE_SIDE_NONE && width != height))
		return WEBGL_INVALID_VALUE;
	if (level > 0 && !(VEGA3dDevice::isPow2(width) && VEGA3dDevice::isPow2(height)))
		return WEBGL_INVALID_VALUE;
	return WEBGL_NO_ERROR;
}

unsigned int CanvasContextWebGL::texImage2DFormatCheck(unsigned int target, unsigned int internalfmt, unsigned int fmt, unsigned int type)
{
	if ((fmt != WEBGL_ALPHA && fmt != WEBGL_LUMINANCE && fmt != WEBGL_LUMINANCE_ALPHA && fmt != WEBGL_RGB && fmt != WEBGL_RGBA) ||
		(type != WEBGL_UNSIGNED_BYTE && type != WEBGL_UNSIGNED_SHORT_5_6_5 && type != WEBGL_UNSIGNED_SHORT_4_4_4_4 && type !=WEBGL_UNSIGNED_SHORT_5_5_5_1))
		return WEBGL_INVALID_ENUM;

	if (internalfmt != WEBGL_ALPHA && internalfmt != WEBGL_LUMINANCE && internalfmt != WEBGL_LUMINANCE_ALPHA && internalfmt != WEBGL_RGB && internalfmt != WEBGL_RGBA)
		return WEBGL_INVALID_ENUM;

	if ((fmt == WEBGL_ALPHA && type != WEBGL_UNSIGNED_BYTE) ||
		(fmt == WEBGL_LUMINANCE && type != WEBGL_UNSIGNED_BYTE) ||
		(fmt == WEBGL_LUMINANCE_ALPHA && type != WEBGL_UNSIGNED_BYTE) ||
		(fmt == WEBGL_RGB && type != WEBGL_UNSIGNED_BYTE && type != WEBGL_UNSIGNED_SHORT_5_6_5) ||
		(fmt == WEBGL_RGBA && type != WEBGL_UNSIGNED_BYTE && type != WEBGL_UNSIGNED_SHORT_5_5_5_1 && type != WEBGL_UNSIGNED_SHORT_4_4_4_4))
		return WEBGL_INVALID_OPERATION;

	if (internalfmt != fmt)
		return WEBGL_INVALID_OPERATION;

	return WEBGL_NO_ERROR;
}

OP_STATUS CanvasContextWebGL::texImage2D(unsigned int target, int level, unsigned int internalfmt, unsigned int fmt, unsigned int type, OpBitmap* data, VEGASWBuffer *nonPremultipliedData)
{
	unsigned data_width = data ? data->Width() : nonPremultipliedData->width;
	unsigned data_height = data ? data->Height() : nonPremultipliedData->height;

	// Do common format verifications etc.
	CanvasWebGLTexturePtr texture;
	VEGA3dTexture::CubeSide cubeSide;
	RETURN_IF_WGL_ERROR(texImage2DCommon(target, level, data_width, data_height, 0, texture, cubeSide));
	RETURN_IF_WGL_ERROR(texImage2DFormatCheck(target, internalfmt, fmt, type));

	VEGA3dTexture::ColorFormat colFmt = GLFormatTypeToVegaTextureColorFormat(fmt, type);

	VEGASWBuffer *buffer = nonPremultipliedData;
	VEGASWBuffer swbuf;
	bool destroy = false;
	bool usingPtr = false;
	if (!buffer)
	{
		destroy = true;
		RETURN_IF_ERROR(swbuf.CreateFromBitmap(data, usingPtr));
		buffer = &swbuf;
	}

	if (m_unpackPremultiplyAlpha && nonPremultipliedData)
		nonPremultipliedData->Premultiply();

	OP_STATUS err = texImage2DInternal(texture, level, buffer->width, buffer->height, colFmt, cubeSide, buffer, 0, CanvasWebGLTexture::MipData::MIPDATA_SWBUFFER, false);

	if (destroy)
	{
		if (usingPtr)
			data->ReleasePointer(FALSE);
		else
			swbuf.Destroy();
	}

	return err;
}

OP_STATUS CanvasContextWebGL::texImage2D(unsigned int target, int level, unsigned int internalfmt, int width, int height, int border, unsigned int fmt, unsigned int type, UINT8* data, unsigned int dataLen)
{
	// Do common format verifications etc.
	CanvasWebGLTexturePtr texture;
	VEGA3dTexture::CubeSide cubeSide;
	RETURN_IF_WGL_ERROR(texImage2DCommon(target, level, width, height, border, texture, cubeSide));
	RETURN_IF_WGL_ERROR(texImage2DFormatCheck(target, internalfmt, fmt, type));

	VEGA3dTexture::ColorFormat colFmt = GLFormatTypeToVegaTextureColorFormat(fmt, type);

	// Make sure this version is called with the correct type.
	if (type != WEBGL_UNSIGNED_BYTE)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	// Check that we're getting enough data.
	unsigned int bpp = fmt == WEBGL_RGBA ? 4 : fmt == WEBGL_RGB ? 3 : fmt == WEBGL_LUMINANCE_ALPHA ? 2 : 1;
	if (!enoughData(width, height, bpp, m_unpackAlignment, dataLen))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	return texImage2DInternal(texture, level, width, height, colFmt, cubeSide, data, dataLen, CanvasWebGLTexture::MipData::MIPDATA_UINT8);
}

OP_STATUS CanvasContextWebGL::texImage2DInternal(CanvasWebGLTexturePtr texture, int level, int width, int height, VEGA3dTexture::ColorFormat colFmt, VEGA3dTexture::CubeSide cubeSide, void* data, unsigned int dataLen, CanvasWebGLTexture::MipData::DataType dataType, bool usePremultiplyFlag)
{
	bool premultiplyAlpha = m_unpackPremultiplyAlpha && usePremultiplyFlag;
	CanvasWebGLTexture::MipChain *mipChain = texture->getMipChain(width, height, level, colFmt);

	// Any mip level > 0 which has at least one of width or height equal to 1 need special treatment as
	// we don't know which MipChain it belongs to.
	if ((width == 1 || height == 1) && level != 0)
	{
		// Get hold of the MipData for the specified level, either overwriting an old or allocating a new one.
		CanvasWebGLTexture::MipData *mipData = NULL;
		RETURN_IF_ERROR(texture->getMipData(level, cubeSide, mipData));

		// Set all the data which we'll need to slot this in to a MipChain created at a later stage.
		mipData->dataType = CanvasWebGLTexture::MipData::MIPDATA_UINT8;
		mipData->fmt = colFmt;
		mipData->cubeSide = cubeSide;
		mipData->level = level;
		mipData->width = width;
		mipData->height = height;
		mipData->unpackAlignment = m_unpackAlignment;
		mipData->unpackFlipY = m_unpackFlipY;
		mipData->unpackPremultiplyAlpha = premultiplyAlpha;
		if (dataType != CanvasWebGLTexture::MipData::MIPDATA_SWBUFFER)
		{
			OP_DELETEA(mipData->data);
			mipData->data = OP_NEWA(unsigned char, dataLen);
			RETURN_OOM_IF_NULL(mipData->data);
			op_memcpy(mipData->data, data, dataLen);
		}
		else
		{
			VEGASWBuffer *buf = (VEGASWBuffer *)data;
			RETURN_IF_ERROR(buf->Clone(mipData->swbuffer));
		}
	}
	else
	{
		// Clear any MipData set at this level and any MipChain that this level is set in.
		texture->clearMipLevel(level, cubeSide, mipChain, false);

		// If there's no matching MipChain.
		if (!mipChain)
		{
			// Create a new MipChain.
			unsigned int level0Width = width << level;
			unsigned int level0Height = height << level;
			RETURN_IF_ERROR(texture->create(colFmt, level0Width, level0Height, cubeSide != VEGA3dTexture::CUBE_SIDE_NONE, &mipChain));

			// Apply any matching MipData to it.
			RETURN_IF_ERROR(texture->applyMipData(mipChain));
		}
	}

	if (mipChain)
	{
		if (width != 0 && height != 0)
		{
			switch (dataType)
			{
			case CanvasWebGLTexture::MipData::MIPDATA_UINT8:
				RETURN_IF_ERROR(mipChain->getTexture()->update(0, 0, width, height, (UINT8*)data, m_unpackAlignment, cubeSide, level, m_unpackFlipY, premultiplyAlpha));
				break;
			case CanvasWebGLTexture::MipData::MIPDATA_UINT16:
				RETURN_IF_ERROR(mipChain->getTexture()->update(0, 0, width, height, (UINT16*)data, m_unpackAlignment, cubeSide, level, m_unpackFlipY, premultiplyAlpha));
				break;
			case CanvasWebGLTexture::MipData::MIPDATA_SWBUFFER:
				RETURN_IF_ERROR(mipChain->getTexture()->update(0, 0, (VEGASWBuffer *)data, m_unpackAlignment, cubeSide, level, m_unpackFlipY, premultiplyAlpha));
				break;
			}
		}
		mipChain->setMipLevel(level, cubeSide, true);
	}
	if (level == 0)
		texture->setActiveMipChain(mipChain);

	return OpStatus::OK;
}


OP_STATUS CanvasContextWebGL::texImage2D(unsigned int target, int level, unsigned int internalfmt, int width, int height, int border, unsigned int fmt, unsigned int type, float* data, unsigned int dataLen)
{
	// Do common format verifications etc.
	CanvasWebGLTexturePtr texture;
	VEGA3dTexture::CubeSide cubeSide;
	RETURN_IF_WGL_ERROR(texImage2DCommon(target, level, width, height, border, texture, cubeSide));
	RETURN_IF_WGL_ERROR(texImage2DFormatCheck(target, internalfmt, fmt, type));

	// FIXME: Needed to support OES-texture-float extension.
	raiseError(WEBGL_INVALID_OPERATION);
	return OpStatus::OK;
}

OP_STATUS CanvasContextWebGL::texImage2D(unsigned int target, int level, unsigned int internalfmt, int width, int height, int border, unsigned int fmt, unsigned int type, UINT16* data, unsigned int dataLen)
{
	// Do common format verifications etc.
	CanvasWebGLTexturePtr texture;
	VEGA3dTexture::CubeSide cubeSide;
	RETURN_IF_WGL_ERROR(texImage2DCommon(target, level, width, height, border, texture, cubeSide));
	RETURN_IF_WGL_ERROR(texImage2DFormatCheck(target, internalfmt, fmt, type));

	VEGA3dTexture::ColorFormat colFmt = GLFormatTypeToVegaTextureColorFormat(fmt, type);

	// Make sure this version is called with the correct type.
	if (type != WEBGL_UNSIGNED_SHORT_5_6_5 && type != WEBGL_UNSIGNED_SHORT_5_5_5_1 && type != WEBGL_UNSIGNED_SHORT_4_4_4_4)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return OpStatus::OK;
	}

	// Check that we're getting enough data.
	unsigned int bpp = 2;
	if (!enoughData(width, height, bpp, m_unpackAlignment, dataLen))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	return texImage2DInternal(texture, level, width, height, colFmt, cubeSide, data, dataLen, CanvasWebGLTexture::MipData::MIPDATA_UINT16);
}

OP_STATUS CanvasContextWebGL::texImage2D(unsigned int target, int level, unsigned int internalfmt, int width, int height, int border, unsigned int fmt, unsigned int type)
{
	OP_STATUS err = OpStatus::OK;

	// Need to check this before allocating any buffers to avoid running out of memeory
	if (width < 0 || height < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	unsigned int bpp = 0;

	if (type == WEBGL_UNSIGNED_BYTE)
		bpp = fmt == WEBGL_RGBA ? 4 : fmt == WEBGL_RGB ? 3 : fmt == WEBGL_LUMINANCE_ALPHA ? 2 : 1;
	else if (type == WEBGL_UNSIGNED_SHORT_5_6_5 || type == WEBGL_UNSIGNED_SHORT_5_5_5_1 || type == WEBGL_UNSIGNED_SHORT_4_4_4_4)
		bpp = 2;
	else
	{
		raiseError(WEBGL_INVALID_ENUM);
		return OpStatus::OK;
	}

	UINT8 *data = OP_NEWA_WHD(UINT8, width, height, bpp);
	RETURN_OOM_IF_NULL(data);
	unsigned int size = width * height * bpp;
	op_memset(data, 0, size);

	// Store old unpack alignment and temporarily set it to 1.
	unsigned int align = m_unpackAlignment;
	m_unpackAlignment = 1;

	// Store old unpack y-flip flag and set it to false as we don't want to rearrange zeroes.
	bool flipY = m_unpackFlipY;
	m_unpackFlipY = false;

	// Store old premultiply alpha flag and set it to false as we don't want to multiply zeroes.
	bool premult = m_unpackPremultiplyAlpha;
	m_unpackPremultiplyAlpha = false;

	if (type == WEBGL_UNSIGNED_BYTE)
		err = texImage2D(target, level, internalfmt, width, height, border, fmt, type, data, size);
	else
		err = texImage2D(target, level, internalfmt, width, height, border, fmt, type, (UINT16 *)data, size);

	OP_DELETEA(data);

	// Restore the unpack values.
	m_unpackFlipY = flipY;
	m_unpackAlignment = align;
	m_unpackPremultiplyAlpha = premult;

	return err;
}

void CanvasContextWebGL::texParameterf(unsigned int target, unsigned int param, float value)
{
	texParameteri(target, param, (int)value);
}

void CanvasContextWebGL::texParameteri(unsigned int target, unsigned int param, int value)
{
	CanvasWebGLTexturePtr texture;
	switch (target)
	{
	case WEBGL_TEXTURE_2D:
		texture = m_textures[m_activeTexture];
		break;
	case WEBGL_TEXTURE_CUBE_MAP:
		texture = m_cubeTextures[m_activeTexture];
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	if (!texture || texture->isDestroyed())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}
	switch (param)
	{
	case WEBGL_TEXTURE_MIN_FILTER:
		VEGA3dTexture::FilterMode minFilter;
		switch (value)
		{
		case WEBGL_NEAREST:
			minFilter = VEGA3dTexture::FILTER_NEAREST;
			break;
		case WEBGL_LINEAR:
			minFilter = VEGA3dTexture::FILTER_LINEAR;
			break;
		case WEBGL_NEAREST_MIPMAP_NEAREST:
			minFilter = VEGA3dTexture::FILTER_NEAREST_MIPMAP_NEAREST;
			break;
		case WEBGL_LINEAR_MIPMAP_NEAREST:
			minFilter = VEGA3dTexture::FILTER_LINEAR_MIPMAP_NEAREST;
			break;
		case WEBGL_NEAREST_MIPMAP_LINEAR:
			minFilter = VEGA3dTexture::FILTER_NEAREST_MIPMAP_LINEAR;
			break;
		case WEBGL_LINEAR_MIPMAP_LINEAR:
			minFilter = VEGA3dTexture::FILTER_LINEAR_MIPMAP_LINEAR;
			break;
		default:
			raiseError(WEBGL_INVALID_ENUM);
			return;
		}
		texture->setFilterMode(minFilter, texture->getMagFilter());
		break;
	case WEBGL_TEXTURE_MAG_FILTER:
		VEGA3dTexture::FilterMode magFilter;
		switch (value)
		{
		case WEBGL_NEAREST:
			magFilter = VEGA3dTexture::FILTER_NEAREST;
			break;
		case WEBGL_LINEAR:
			magFilter = VEGA3dTexture::FILTER_LINEAR;
			break;
		default:
			raiseError(WEBGL_INVALID_ENUM);
			return;
		}
		texture->setFilterMode(texture->getMinFilter(), magFilter);
		break;
	case WEBGL_TEXTURE_WRAP_S:
		VEGA3dTexture::WrapMode swrap;
		switch (value)
		{
		case WEBGL_CLAMP_TO_EDGE:
			swrap = VEGA3dTexture::WRAP_CLAMP_EDGE;
			break;
		case WEBGL_REPEAT:
			swrap = VEGA3dTexture::WRAP_REPEAT;
			break;
		case WEBGL_MIRRORED_REPEAT:
			swrap = VEGA3dTexture::WRAP_REPEAT_MIRROR;
			break;
		default:
			raiseError(WEBGL_INVALID_ENUM);
			return;
		}
		texture->setWrapMode(swrap, texture->getWrapModeT());
		break;
	case WEBGL_TEXTURE_WRAP_T:
		VEGA3dTexture::WrapMode twrap;
		switch (value)
		{
		case WEBGL_CLAMP_TO_EDGE:
			twrap = VEGA3dTexture::WRAP_CLAMP_EDGE;
			break;
		case WEBGL_REPEAT:
			twrap = VEGA3dTexture::WRAP_REPEAT;
			break;
		case WEBGL_MIRRORED_REPEAT:
			twrap = VEGA3dTexture::WRAP_REPEAT_MIRROR;
			break;
		default:
			raiseError(WEBGL_INVALID_ENUM);
			return;
		}
		texture->setWrapMode(texture->getWrapModeS(), twrap);
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
}

OP_STATUS CanvasContextWebGL::texSubImage2D(unsigned int target, int level, int xofs, int yofs, unsigned int fmt, unsigned int type, OpBitmap* data, VEGASWBuffer* nonPremultipliedData)
{
	unsigned data_width = data ? data->Width() : nonPremultipliedData->width;
	unsigned data_height = data ? data->Height() : nonPremultipliedData->height;
	CanvasWebGLTexturePtr texture;
	VEGA3dTexture::CubeSide cubeSide;
	RETURN_IF_WGL_ERROR(texImage2DCommon(target, level, data_width, data_height, 0, texture, cubeSide));

	if (!texture->getTexture())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	unsigned int texFormat, texType;
	VegaTextureColorFormatToGL(texture->getTexture()->getFormat(), texFormat, texType);
	RETURN_IF_WGL_ERROR(texImage2DFormatCheck(target, texFormat, fmt, type));

	if (fmt != texFormat || type != texType)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (xofs < 0 || yofs < 0 || xofs+data_width > texture->getTexture()->getWidth() || yofs+data_height > texture->getTexture()->getHeight())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	if (nonPremultipliedData)
	{
		if (m_unpackPremultiplyAlpha)
			nonPremultipliedData->Premultiply();
		RETURN_IF_ERROR(texture->getTexture()->update(xofs, yofs, nonPremultipliedData, m_unpackAlignment, cubeSide, level, m_unpackFlipY, false));
	}
	else
	{
		OP_ASSERT(m_unpackPremultiplyAlpha); // This path should only be taken if we want premultiplied data.
		VEGASWBuffer swbuf;
		bool usingPtr;
		RETURN_IF_ERROR(swbuf.CreateFromBitmap(data, usingPtr));
		OP_STATUS err = texture->getTexture()->update(xofs, yofs, &swbuf, m_unpackAlignment, cubeSide, level, m_unpackFlipY, false);
		if (usingPtr)
			data->ReleasePointer(FALSE);
		else
			swbuf.Destroy();
		return err;
	}
	return OpStatus::OK;
}

OP_STATUS CanvasContextWebGL::texSubImage2D(unsigned int target, int level, int xofs, int yofs, int width, int height, unsigned int fmt, unsigned int type, UINT8* data, unsigned int dataLen)
{
	if (IntAdditionOverflows(xofs, width) || IntAdditionOverflows(yofs, height))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	CanvasWebGLTexturePtr texture;
	VEGA3dTexture::CubeSide cubeSide;
	RETURN_IF_WGL_ERROR(texImage2DCommon(target, level, width, height, 0, texture, cubeSide));

	if (!texture->getTexture())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	unsigned int texFormat, texType;
	VegaTextureColorFormatToGL(texture->getTexture()->getFormat(), texFormat, texType);
	RETURN_IF_WGL_ERROR(texImage2DFormatCheck(target, texFormat, fmt, type));

	if (fmt != texFormat || type != texType)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (type != WEBGL_UNSIGNED_BYTE)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (xofs < 0 || yofs < 0 || (unsigned int)(xofs+width) > texture->getTexture()->getWidth() || (unsigned int)(yofs+height) > texture->getTexture()->getHeight())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	// Check that we're getting enough data.
	unsigned int bpp = fmt == WEBGL_RGBA ? 4 : fmt == WEBGL_RGB ? 3 : fmt == WEBGL_LUMINANCE_ALPHA ? 2 : 1;
	if (!enoughData(width, height, bpp, m_unpackAlignment, dataLen))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	return texture->getTexture()->update(xofs, yofs, width, height, data, m_unpackAlignment, cubeSide, level, m_unpackFlipY, m_unpackPremultiplyAlpha);
}

OP_STATUS CanvasContextWebGL::texSubImage2D(unsigned int target, int level, int xofs, int yofs, int width, int height, unsigned int fmt, unsigned int type, UINT16* data, unsigned int dataLen)
{
	if (IntAdditionOverflows(xofs, width) || IntAdditionOverflows(yofs, height))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	CanvasWebGLTexturePtr texture;
	VEGA3dTexture::CubeSide cubeSide;
	RETURN_IF_WGL_ERROR(texImage2DCommon(target, level, width, height, 0, texture, cubeSide));

	if (!texture->getTexture())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	unsigned int texFormat, texType;
	VegaTextureColorFormatToGL(texture->getTexture()->getFormat(), texFormat, texType);
	RETURN_IF_WGL_ERROR(texImage2DFormatCheck(target, texFormat, fmt, type));

	if (fmt != texFormat || type != texType)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (type != WEBGL_UNSIGNED_SHORT_5_6_5 && type != WEBGL_UNSIGNED_SHORT_4_4_4_4 && type !=WEBGL_UNSIGNED_SHORT_5_5_5_1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	if (xofs < 0 || yofs < 0 || (unsigned int)(xofs+width) > texture->getTexture()->getWidth() || (unsigned int)(yofs+height) > texture->getTexture()->getHeight())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	// Check that we're getting enough data.
	unsigned int bpp = 2;
	if (!enoughData(width, height, bpp, m_unpackAlignment, dataLen))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}

	return texture->getTexture()->update(xofs, yofs, width, height, data, m_unpackAlignment, cubeSide, level, m_unpackFlipY, m_unpackPremultiplyAlpha);
}

OP_STATUS CanvasContextWebGL::attachShader(CanvasWebGLProgram* program, CanvasWebGLShader* shader)
{
	if (!program || !shader || program->isDestroyed() || shader->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}
	if (program->hasShader(shader))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}
	if (shader->isFragmentShader() && program->hasFragmentShader() || !shader->isFragmentShader() && program->hasVertexShader())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}
	return program->addShader(shader);
}

OP_STATUS CanvasContextWebGL::bindAttribLocation(CanvasWebGLProgram* program, const uni_char* name, unsigned int index)
{
	if (!program || program->isDestroyed() || index >= m_maxVertexAttribs || !IsValidUniformOrAttributeName(name))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}
	if (name[0] == 'g' && name[1] == 'l' && name[2] == '_')
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}
	OpString8 n;
	RETURN_IF_ERROR(n.Set(name));
	RETURN_IF_ERROR(program->m_next_attrib_bindings.set(n.CStr(), index));
	return OpStatus::OK;
}

OP_STATUS CanvasContextWebGL::compileShader(CanvasWebGLShader* shader, const uni_char *url, BOOL validate)
{
	if (!shader || shader->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}
	return shader->compile(url, validate, m_enabledExtensions);
}

void CanvasContextWebGL::deleteProgram(CanvasWebGLProgram* program)
{
	if (program->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	program->destroy();
}

void CanvasContextWebGL::deleteShader(CanvasWebGLShader* shader)
{
	shader->destroy();
}

OP_STATUS CanvasContextWebGL::detachShader(CanvasWebGLProgram* program, CanvasWebGLShader* shader)
{
	if (!program || !shader || program->isDestroyed() || shader->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}
	if (!program->hasShader(shader))
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::OK;
	}
	return program->removeShader(shader);
}

CanvasWebGLParameter CanvasContextWebGL::getProgramParameter(CanvasWebGLProgram* program, unsigned int param)
{
	CanvasWebGLParameter p;
	// Return NULL on error
	p.type = CanvasWebGLParameter::PARAM_OBJECT;
	p.value.object_param = NULL;
	if (!program || program->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return p;
	}
	switch (param)
	{
	case WEBGL_DELETE_STATUS:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = !!program->isPendingDestroy();
		break;
	case WEBGL_LINK_STATUS:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = !!program->isLinked();
		break;
	case WEBGL_VALIDATE_STATUS:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = !!program->getProgram()->isValid();
		break;
	case WEBGL_ATTACHED_SHADERS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = program->getNumAttachedShaders();
		break;
	case WEBGL_ACTIVE_ATTRIBUTES:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = program->getProgram()->getNumInputs();
		break;
	case WEBGL_ACTIVE_UNIFORMS:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = program->getProgram()->getNumConstants();
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
	return p;
}

const uni_char* CanvasContextWebGL::getProgramInfoLog(CanvasWebGLProgram* program)
{
	if (!program || program->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return UNI_L("");
	}
	return program->getInfoLog();
}

CanvasWebGLParameter CanvasContextWebGL::getShaderParameter(CanvasWebGLShader* shader, unsigned int param)
{
	CanvasWebGLParameter p;
	// Return NULL on error
	p.type = CanvasWebGLParameter::PARAM_OBJECT;
	p.value.object_param = NULL;
	if (!shader || shader->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return p;
	}
	switch (param)
	{
	case WEBGL_SHADER_TYPE:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = shader->isFragmentShader()?WEBGL_FRAGMENT_SHADER:WEBGL_VERTEX_SHADER;
		break;
	case WEBGL_DELETE_STATUS:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = !!shader->isPendingDestroy();
		break;
	case WEBGL_COMPILE_STATUS:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = !!shader->isCompiled();
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
	return p;
}

const uni_char* CanvasContextWebGL::getShaderInfoLog(CanvasWebGLShader* shader)
{
	if (!shader || shader->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return UNI_L("");
	}
	return shader->getInfoLog();
}

const uni_char* CanvasContextWebGL::getShaderSource(CanvasWebGLShader* shader)
{
	if (!shader || shader->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return UNI_L("");
	}
	return shader->getSource();
}

OP_STATUS CanvasContextWebGL::linkProgram(CanvasWebGLProgram* program)
{
	if (!program || program->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}
	if (!(program->hasFragmentShader() && program->hasVertexShader()))
		return OpStatus::OK;
	return program->link(m_enabledExtensions, m_maxVertexAttribs);
}

OP_STATUS CanvasContextWebGL::shaderSource(CanvasWebGLShader* shader, const uni_char* source)
{
	if (!shader || shader->isDestroyed() || !WGL_Validator::ValidateSource(source))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}
	return shader->setSource(source);
}

void CanvasContextWebGL::useProgram(CanvasWebGLProgram* program)
{
	if (program && program->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	if (program && !program->isLinked())
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}
	VEGARefCount::DecRef(m_vertexLayout);
	m_vertexLayout = NULL;
	m_program = program;
}

OP_STATUS CanvasContextWebGL::validateProgram(CanvasWebGLProgram* program)
{
	if (!program || program->isDestroyed())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}
	return program->getProgram()->validate();
}

BOOL CanvasContextWebGL::supportsExtension(unsigned int e)
{
	switch (e)
	{
	case WEBGL_EXT_OES_STANDARD_DERIVATIVES:
		return g_vegaGlobals.vega3dDevice->supportsExtension(VEGA3dDevice::OES_STANDARD_DERIVATIVES);
	default:
		OP_ASSERT(!"Unexpected extension");
	case WEBGL_EXT_OES_TEXTURE_FLOAT:
		// FIXME: not supported yet
		return FALSE;
	}
}

void CanvasContextWebGL::enableExtension(unsigned int e)
{
	m_enabledExtensions |= e;
}

void CanvasContextWebGL::disableVertexAttribArray(unsigned int idx)
{
	if (idx >= m_maxVertexAttribs)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	if (m_vertexAttribs[idx].isFromArray)
	{
		m_vertexAttribs[idx].isFromArray = FALSE;
		VEGARefCount::DecRef(m_vertexLayout);
		m_vertexLayout = NULL;
	}
}

void CanvasContextWebGL::enableVertexAttribArray(unsigned int idx)
{
	if (idx >= m_maxVertexAttribs)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	if (!m_vertexAttribs[idx].isFromArray)
	{
		m_vertexAttribs[idx].isFromArray = TRUE;
		VEGARefCount::DecRef(m_vertexLayout);
		m_vertexLayout = NULL;
	}
}


OP_STATUS CanvasContextWebGL::getActiveAttrib(CanvasWebGLProgram* program, unsigned int idx, unsigned int* type, unsigned int* size, OpString* name)
{
	name->Empty();
	*type = 0;
	*size = 0;
	// NOTE: Can't find in spec what to return for !linked, but desktop GL returns GL_INVALID_VALUE.
	if (!program || program->isDestroyed() || !program->isLinked() || idx >= program->getProgram()->getNumInputs())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	VEGA3dShaderProgram::ConstantType vegatype;
	const char* name8;
	const uni_char *original_name = NULL;

	/* TODO: We wouldn't need to talk to the backend at all if we checked which
	   attributes are active at link time */
	RETURN_IF_ERROR(program->getProgram()->indexToAttributeName(idx, &name8));
	OP_BOOLEAN result = program->lookupShaderAttribute(name8, vegatype, *size, &original_name);
	if (result != OpBoolean::IS_TRUE)
		return result;
	/* 'size' is in units of 'type', and ES 2.0 does not allow attributes to be arrays. Hence
	   it should always be 1. */
	OP_ASSERT(*size == 1);
	*type = VegaToGlShaderConst[vegatype];
	if (original_name)
		RETURN_IF_ERROR(name->Set(original_name));
	else
		RETURN_IF_ERROR(name->Set(name8));
	return OpStatus::OK;
}

OP_STATUS CanvasContextWebGL::getActiveUniform(CanvasWebGLProgram* program, unsigned int idx, unsigned int* type, unsigned int* size, OpString* name)
{
	name->Empty();
	*type = 0;
	*size = 0;

	// NOTE: Delegate the check idx < getNumConstants() to getConstantDescription()
	//       as OpenGL handles it a bit differently. Uniform arrays count as one when
	//       getting their info.
	// NOTE: Can't find in spec what to return for !linked, but desktop GL returns GL_INVALID_VALUE.
	if (!program || program->isDestroyed() || !program->isLinked())
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}
#if 0
	VEGA3dShaderProgram::ConstantType vegatype_valid;
	unsigned int size_valid = 0;
	const uni_char *name_valid = NULL;
	BOOL release_name = FALSE;
	OP_BOOLEAN result = program->lookupShaderUniform(idx, &vegatype_valid, &size_valid, &name_valid, release_name);
	RETURN_IF_ERROR(result);
	if (result != OpBoolean::IS_TRUE)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}
	*type = VegaToGlShaderConst[vegatype_valid];
	*size = size_valid;
	OP_STATUS err = name->Set(name_valid);
	if (release_name && name_valid)
		OP_DELETEA(name_valid);
	return err;
#else
	VEGA3dShaderProgram::ConstantType vegatype;
	const char* name8;
	OP_STATUS err = program->getProgram()->getConstantDescription(idx, &vegatype, size, &name8);
	if (OpStatus::IsError(err))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::OK;
	}

	*type = VegaToGlShaderConst[vegatype];
	VEGA3dShaderProgram::ConstantType vegatype_valid;

	// TODO: More size vs. array length confusion going on below?

	unsigned int size_valid = 0;
	BOOL is_array = FALSE;
	const uni_char *original_name = NULL;
	OP_BOOLEAN result = program->lookupShaderUniform(name8, vegatype_valid, size_valid, is_array, &original_name);
	if (OpStatus::IsMemoryError(result))
		return result;
	/* TODO: The last test here seems bogus, if it's to be interpreted as
	   checking if the "used length" of the array as reported by the backend
	   (the highest index used, plus one) is the same as the length reported by
	   the validator. */
	OP_ASSERT(result == OpBoolean::IS_TRUE && vegatype_valid == vegatype /* && *size == size_valid */);
	if (original_name)
		RETURN_IF_ERROR(name->Set(original_name));
		/* We know that there won't be a "[]" suffix in what the validator
		   returned, so no need to check. */
	else
	{
		/* Special case for arrays. If the array name is returned without an element
		   index, we should append one. */
		RETURN_IF_ERROR(name->Set(name8));
		if (is_array)
			is_array = name8[op_strlen(name8) - 1] != ']';
	}
	if (is_array)
		RETURN_IF_ERROR(name->Append("[0]"));
	return OpStatus::OK;
#endif
}

int CanvasContextWebGL::getAttribLocation(CanvasWebGLProgram* program, const uni_char* name)
{
	if (!program || program->isDestroyed() || !program->isLinked() || !IsValidUniformOrAttributeName(name))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return -1;
	}
	OpString8 charName;
	if (OpStatus::IsError(charName.Set(name)))
		return -1;
	unsigned location;
	VEGA3dShaderProgram::ConstantType dummyType;
	unsigned int dummySize;
	const uni_char* originalName;
	// Look up the original name as it might've been aliased.
	if (OpBoolean::IS_TRUE == program->lookupShaderAttribute(charName.CStr(), dummyType, dummySize, &originalName))
		if (originalName != NULL && OpStatus::IsError(charName.Set(originalName)))
			return -1;
	return program->m_active_attrib_bindings.get(charName.CStr(), location) ? location : -1;
}

CanvasWebGLParameter CanvasContextWebGL::getUniform(CanvasWebGLProgram* program, unsigned int idx, const char* name)
{
	CanvasWebGLParameter p;
	p.type = CanvasWebGLParameter::PARAM_OBJECT;
	p.value.object_param = NULL;

	VEGA3dShaderProgram::ConstantType type;
	BOOL isArray;

	int arrayIndex = -1;
	size_t stringLength = op_strlen(name);
	if (stringLength > 0)
	{
		const char *str = name + stringLength - 1;
		if (*str == ']')
		{
			// TODO: This value is _only_ used when using a DX backend for WebGL. It should be tidied
			// up and the index should be grabbed from the shader validator instead of this quick hack.
			while (--str != name)
			{
				if (*str == '[')
				{
					arrayIndex = op_atoi(str + 1);
					break;
				}
			}
		}
	}

	const uni_char *original_name = NULL;
	unsigned int dummy_length;
	OP_BOOLEAN res = program->lookupShaderUniform(name, type, dummy_length, isArray, &original_name);
	RETURN_VALUE_IF_ERROR(res, p);
	if (res != OpBoolean::IS_TRUE)
		return p;

	BOOL integer;
	if (OpStatus::IsSuccess(VegaProgramConstantToWebGLParameter(type, p.type, integer)))
	{
		const bool is_cube_sampler = (type == VEGA3dShaderProgram::SHDCONST_SAMPCUBE);

		if (type == VEGA3dShaderProgram::SHDCONST_SAMP2D || is_cube_sampler)
		{
			if (is_cube_sampler)
				idx |= IS_SAMPLER_CUBE_BIT;

			UINT32 texture_unit;
			if (OpStatus::IsSuccess(program->m_sampler_values.GetData(idx, &texture_unit)))
				p.value.int_param[0] = texture_unit;
			else
				raiseError(WEBGL_INVALID_OPERATION);
		}
		else
		{
			OP_STATUS status;
			if (integer)
				status = program->getProgram()->getConstantValue(idx, p.value.int_param, type, arrayIndex);
			else
				status = program->getProgram()->getConstantValue(idx, p.value.float_param, type, arrayIndex);
			if (OpStatus::IsError(status))
				raiseError(WEBGL_INVALID_OPERATION);
		}
	}

	return p;
}

int CanvasContextWebGL::getUniformLocation(CanvasWebGLProgram* program, const uni_char* name)
{
	if (!program || program->isDestroyed() || !program->isLinked() || !IsValidUniformOrAttributeName(name))
	{
		raiseError(WEBGL_INVALID_VALUE);
		return -1;
	}
	OpString8 n;
	if (OpStatus::IsError(n.Set(name)))
		return -1;
	int idx = program->getProgram()->getConstantLocation(n.CStr());
	if (idx == -1)
	{
		// Try again, but this time with the aliased name (if there is one).
		VEGA3dShaderProgram::ConstantType type;
		BOOL isArray;
		const uni_char *original_name = NULL;
		unsigned int dummy_length;
		OP_BOOLEAN res = program->lookupShaderUniform(n.CStr(), type, dummy_length, isArray, &original_name);
		if (res == OpBoolean::IS_TRUE && original_name != NULL && OpStatus::IsSuccess(n.Set(original_name)))
		{
			// lookupShaderUniform only provides the original name, so we need to append any compound names.
			while (*name && *name != '.' && *name != '[')
			{
				name++;
			}

			if (*name != '\0')
			{
				OpString8 rest;
				RETURN_VALUE_IF_ERROR(rest.Set(name), -1);
				RETURN_VALUE_IF_ERROR(n.Append(rest), -1);
			}
			return program->getProgram()->getConstantLocation(n.CStr());
		}
	}

	return idx;
}

CanvasWebGLParameter CanvasContextWebGL::getVertexAttrib(unsigned int idx, unsigned int param)
{
	CanvasWebGLParameter p;
	p.type = CanvasWebGLParameter::PARAM_OBJECT;
	p.value.object_param = NULL;
	if (idx >= m_maxVertexAttribs)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return p;
	}
	switch (param)
	{
	case WEBGL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
		p.type = CanvasWebGLParameter::PARAM_OBJECT;
		p.value.object_param = m_vertexAttribs[idx].buffer?m_vertexAttribs[idx].buffer->getESObject():NULL;
		break;
	case WEBGL_VERTEX_ATTRIB_ARRAY_ENABLED:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = !!m_vertexAttribs[idx].isFromArray;
		break;
	case WEBGL_VERTEX_ATTRIB_ARRAY_SIZE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = m_vertexAttribs[idx].size;
		break;
	case WEBGL_VERTEX_ATTRIB_ARRAY_STRIDE:
		p.type = CanvasWebGLParameter::PARAM_INT;
		p.value.int_param[0] = m_vertexAttribs[idx].stride;
		break;
	case WEBGL_VERTEX_ATTRIB_ARRAY_TYPE:
		p.type = CanvasWebGLParameter::PARAM_UINT;
		p.value.uint_param[0] = m_vertexAttribs[idx].type;
		break;
	case WEBGL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
		p.type = CanvasWebGLParameter::PARAM_BOOL;
		p.value.bool_param = !!m_vertexAttribs[idx].normalize;
		break;
	case WEBGL_CURRENT_VERTEX_ATTRIB:
		p.type = CanvasWebGLParameter::PARAM_FLOAT4;
		op_memcpy(p.value.float_param, m_vertexAttribs[idx].value, sizeof(float)*4);
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		break;
	}
	return p;
}

unsigned int CanvasContextWebGL::getVertexAttribOffset(unsigned int idx, unsigned int param)
{
	if (idx >= m_maxVertexAttribs)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return 0;
	}
	if (param != WEBGL_VERTEX_ATTRIB_ARRAY_POINTER)
	{
		raiseError(WEBGL_INVALID_ENUM);
		return 0;
	}
	return m_vertexAttribs[idx].offset;
}

int CanvasContextWebGL::getDrawingBufferWidth()
{
	// TODO: We should make sure we can initialize the size of buffer requested on startup and if
	// not fall back to a smaller buffer and report that size here.
	return m_renderFramebuffer->getWidth();
}

int CanvasContextWebGL::getDrawingBufferHeight()
{
	// TODO: We should make sure we can initialize the size of buffer requested on startup and if
	// not fall back to a smaller buffer and report that size here.
	return m_renderFramebuffer->getHeight();
}

OP_STATUS CanvasContextWebGL::VerifyUniformType(int idx, const char *name,
                                                unsigned int specifiedType, int specifiedLength,
                                                unsigned int *actualType,
                                                unsigned int *actualLength)
{
	unsigned int type;
	unsigned int length;
	BOOL isArray;

	VEGA3dShaderProgram::ConstantType vegatype;

	if (specifiedLength < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return OpStatus::ERR;
	}

	unsigned int specifiedLengthValue = (unsigned int)specifiedLength;

	// If the uniform is an array element, extract the array name (before the
	// "[") from 'name'.

	char* temp = NULL;
	unsigned int len = op_strlen(name);
	bool requireArray = false;
	if (len > 0 && name[len-1] == ']')
	{
		temp = OP_NEWA(char, len+1);
		RETURN_OOM_IF_NULL(temp);
		op_strcpy(temp, name);
		--len;
		while (len > 0 && temp[len] != '[')
			--len;
		if (!len)
		{
			OP_ASSERT(!"Uniform name not handled by shader validator");
			OP_DELETEA(temp);
			return OpStatus::ERR;
		}
		temp[len] = '\0';
		requireArray = true;
		name = temp;
	}

	const uni_char *original_name = NULL;
	OP_BOOLEAN res = m_program->lookupShaderUniform(name, vegatype, length, isArray, &original_name);
	OP_DELETEA(temp);
	RETURN_IF_ERROR(res);
	if (res != OpBoolean::IS_TRUE || (requireArray && !isArray))
		return OpStatus::ERR;

	type = VegaToGlShaderConst[vegatype];

	if (actualType)
		*actualType = type;

	if (actualLength)
		*actualLength = length;

	if (length == specifiedLengthValue)
	{
		if (type == WEBGL_BOOL      && (specifiedType == WEBGL_FLOAT      || specifiedType == WEBGL_INT     ) ||
		    type == WEBGL_BOOL_VEC2 && (specifiedType == WEBGL_FLOAT_VEC2 || specifiedType == WEBGL_INT_VEC2) ||
		    type == WEBGL_BOOL_VEC3 && (specifiedType == WEBGL_FLOAT_VEC3 || specifiedType == WEBGL_INT_VEC3) ||
		    type == WEBGL_BOOL_VEC4 && (specifiedType == WEBGL_FLOAT_VEC4 || specifiedType == WEBGL_INT_VEC4))
			return OpStatus::OK;
	}

	// Return an error if it's not the expected type and not an int when expecting a sampler.
	if (type != specifiedType)
	{
		bool isSampler = (type == WEBGL_SAMPLER_2D || type == WEBGL_SAMPLER_CUBE);
		if (!(isSampler && specifiedType == WEBGL_INT))
		{
			raiseError(WEBGL_INVALID_OPERATION);
			return OpStatus::ERR;
		}
	}

	// If it's an array, it's ok
	if (length != specifiedLengthValue && !isArray)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

void CanvasContextWebGL::uniform1(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_FLOAT, length, NULL, &actualLength));

	g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
	m_program->getProgram()->setScalar(idx, values, MIN((unsigned)length, actualLength));
}

void CanvasContextWebGL::uniform1(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, int* values, int length)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_INT, length, NULL, &actualLength));

	// We defer the setting of sampler uniforms until draw time

	if (m_program->m_sampler_values.Contains(idx))
	{
		// Range checking is performed in setTextures()
		m_program->m_sampler_values.Update(idx, values[0]);
	}
	else if (m_program->m_sampler_values.Contains(idx | IS_SAMPLER_CUBE_BIT))
	{
		// Range checking is performed in setTextures()
		m_program->m_sampler_values.Update(idx | IS_SAMPLER_CUBE_BIT, values[0]);
	}
	else
	{
		g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
		m_program->getProgram()->setScalar(idx, values, MIN((unsigned)length, actualLength));
	}
}

void CanvasContextWebGL::uniform2(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_FLOAT_VEC2, length, NULL, &actualLength));

	g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
	m_program->getProgram()->setVector2(idx, values, MIN((unsigned)length, actualLength));
}

void CanvasContextWebGL::uniform2(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, int* values, int length)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_INT_VEC2, length, NULL, &actualLength));

	g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
	m_program->getProgram()->setVector2(idx, values, MIN((unsigned)length, actualLength));
}

void CanvasContextWebGL::uniform3(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_FLOAT_VEC3, length, NULL, &actualLength));

	g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
	m_program->getProgram()->setVector3(idx, values, MIN((unsigned)length, actualLength));
}

void CanvasContextWebGL::uniform3(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, int* values, int length)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_INT_VEC3, length, NULL, &actualLength));

	g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
	m_program->getProgram()->setVector3(idx, values, MIN((unsigned)length, actualLength));
}

void CanvasContextWebGL::uniform4(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_FLOAT_VEC4, length, NULL, &actualLength));

	g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
	m_program->getProgram()->setVector4(idx, values, MIN((unsigned)length, actualLength));
}

void CanvasContextWebGL::uniform4(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, int* values, int length)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_INT_VEC4, length, NULL, &actualLength));

	g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
	m_program->getProgram()->setVector4(idx, values, MIN((unsigned)length, actualLength));
}

void CanvasContextWebGL::uniformMatrix2(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values,int length, BOOL transpose)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_FLOAT_MAT2, length, NULL, &actualLength));

	g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
	m_program->getProgram()->setMatrix2(idx, values, MIN((unsigned)length, actualLength), transpose!=FALSE);
}

void CanvasContextWebGL::uniformMatrix3(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length, BOOL transpose)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_FLOAT_MAT3, length, NULL, &actualLength));

	g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
	m_program->getProgram()->setMatrix3(idx, values, MIN((unsigned)length, actualLength), transpose!=FALSE);
}

void CanvasContextWebGL::uniformMatrix4(CanvasWebGLProgram* program, unsigned int linkId, int idx, const char* name, float* values, int length, BOOL transpose)
{
	if (!m_program || m_program != program || linkId != m_program->getLinkId() || idx == -1)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}

	unsigned int actualLength;
	RETURN_VOID_IF_ERROR(VerifyUniformType(idx, name, WEBGL_FLOAT_MAT4, length, &actualLength));

	g_vegaGlobals.vega3dDevice->setShaderProgram(m_program->getProgram());
	m_program->getProgram()->setMatrix4(idx, values, MIN((unsigned)length, actualLength), transpose!=FALSE);
}

void CanvasContextWebGL::vertexAttrib(unsigned int idx, float* values)
{
	if (idx >= m_maxVertexAttribs)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	op_memcpy(m_vertexAttribs[idx].value, values, sizeof(float)*4);

	/* Throw away the current layout and recreate it later if the
	   newly assigned generic attribute value is in use */
	if (!m_vertexAttribs[idx].isFromArray)
	{
		VEGARefCount::DecRef(m_vertexLayout);
		m_vertexLayout = NULL;
	}
}

void CanvasContextWebGL::vertexAttribPointer(unsigned int idx, int size, unsigned int type, BOOL normalized, int stride, int offset)
{
	if (!m_arrayBuffer)
	{
		raiseError(WEBGL_INVALID_OPERATION);
		return;
	}
	if (idx >= m_maxVertexAttribs)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	if (size < 1 || size > 4)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	if (stride < 0 || stride > 255 || offset < 0)
	{
		raiseError(WEBGL_INVALID_VALUE);
		return;
	}
	switch (type)
	{
	case WEBGL_UNSIGNED_BYTE:
	case WEBGL_BYTE:
		break;
	case WEBGL_UNSIGNED_SHORT:
	case WEBGL_SHORT:
		if (stride&1 || offset&1)
		{
			raiseError(WEBGL_INVALID_OPERATION);
			return;
		}
		break;
	case WEBGL_FLOAT:
		if (stride&3 || offset&3)
		{
			raiseError(WEBGL_INVALID_OPERATION);
			return;
		}
		break;
	default:
		raiseError(WEBGL_INVALID_ENUM);
		return;
	}
	m_vertexAttribs[idx].size = size;
	m_vertexAttribs[idx].type = type;
	m_vertexAttribs[idx].normalize = normalized;
	m_vertexAttribs[idx].stride = stride;
	m_vertexAttribs[idx].offset = offset;
	m_vertexAttribs[idx].buffer = m_arrayBuffer;

	/* Throw away the current layout and recreate it later if the
	   vertex attribute array is active */
	if (m_vertexAttribs[idx].isFromArray)
	{
		VEGARefCount::DecRef(m_vertexLayout);
		m_vertexLayout = NULL;
	}
}

void CanvasContextWebGL::raiseError(unsigned int code)
{
	if (m_errorCode == WEBGL_NO_ERROR)
		m_errorCode = code;
}

unsigned int CanvasContextWebGL::getError()
{
	unsigned int err = m_errorCode;
	m_errorCode = WEBGL_NO_ERROR;
	return err;
}

#endif // CANVAS3D_SUPPORT

