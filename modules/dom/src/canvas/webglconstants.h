/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_WEBGLCONSTANTS_H
#define DOM_WEBGLCONSTANTS_H

/* ClearBufferMask */
static const unsigned int WEBGL_DEPTH_BUFFER_BIT               = 0x00000100;
static const unsigned int WEBGL_STENCIL_BUFFER_BIT             = 0x00000400;
static const unsigned int WEBGL_COLOR_BUFFER_BIT               = 0x00004000;

/* BeginMode */
static const unsigned int WEBGL_POINTS                         = 0x0000;
static const unsigned int WEBGL_LINES                          = 0x0001;
static const unsigned int WEBGL_LINE_LOOP                      = 0x0002;
static const unsigned int WEBGL_LINE_STRIP                     = 0x0003;
static const unsigned int WEBGL_TRIANGLES                      = 0x0004;
static const unsigned int WEBGL_TRIANGLE_STRIP                 = 0x0005;
static const unsigned int WEBGL_TRIANGLE_FAN                   = 0x0006;

/* Used internally. Not part of the spec. */
static const unsigned int N_WEBGL_MODES = WEBGL_TRIANGLE_FAN + 1;

/* AlphaFunction (not supported in ES20) */
/*      GL_NEVER */
/*      GL_LESS */
/*      GL_EQUAL */
/*      GL_LEQUAL */
/*      GL_GREATER */
/*      GL_NOTEQUAL */
/*      GL_GEQUAL */
/*      GL_ALWAYS */

/* BlendingFactorDest */
static const unsigned int WEBGL_ZERO                           = 0;
static const unsigned int WEBGL_ONE                            = 1;
static const unsigned int WEBGL_SRC_COLOR                      = 0x0300;
static const unsigned int WEBGL_ONE_MINUS_SRC_COLOR            = 0x0301;
static const unsigned int WEBGL_SRC_ALPHA                      = 0x0302;
static const unsigned int WEBGL_ONE_MINUS_SRC_ALPHA            = 0x0303;
static const unsigned int WEBGL_DST_ALPHA                      = 0x0304;
static const unsigned int WEBGL_ONE_MINUS_DST_ALPHA            = 0x0305;

/* BlendingFactorSrc */
/*      GL_ZERO */
/*      GL_ONE */
static const unsigned int WEBGL_DST_COLOR                      = 0x0306;
static const unsigned int WEBGL_ONE_MINUS_DST_COLOR            = 0x0307;
static const unsigned int WEBGL_SRC_ALPHA_SATURATE             = 0x0308;
/*      GL_SRC_ALPHA */
/*      GL_ONE_MINUS_SRC_ALPHA */
/*      GL_DST_ALPHA */
/*      GL_ONE_MINUS_DST_ALPHA */

/* BlendEquationSeparate */
static const unsigned int WEBGL_FUNC_ADD                       = 0x8006;
static const unsigned int WEBGL_BLEND_EQUATION                 = 0x8009;
static const unsigned int WEBGL_BLEND_EQUATION_RGB             = 0x8009;   /* same as BLEND_EQUATION */
static const unsigned int WEBGL_BLEND_EQUATION_ALPHA           = 0x883D;

/* BlendSubtract */
static const unsigned int WEBGL_FUNC_SUBTRACT                  = 0x800A;
static const unsigned int WEBGL_FUNC_REVERSE_SUBTRACT          = 0x800B;

/* Separate Blend Functions */
static const unsigned int WEBGL_BLEND_DST_RGB                  = 0x80C8;
static const unsigned int WEBGL_BLEND_SRC_RGB                  = 0x80C9;
static const unsigned int WEBGL_BLEND_DST_ALPHA                = 0x80CA;
static const unsigned int WEBGL_BLEND_SRC_ALPHA                = 0x80CB;
static const unsigned int WEBGL_CONSTANT_COLOR                 = 0x8001;
static const unsigned int WEBGL_ONE_MINUS_CONSTANT_COLOR       = 0x8002;
static const unsigned int WEBGL_CONSTANT_ALPHA                 = 0x8003;
static const unsigned int WEBGL_ONE_MINUS_CONSTANT_ALPHA       = 0x8004;
static const unsigned int WEBGL_BLEND_COLOR                    = 0x8005;

/* Buffer Objects */
static const unsigned int WEBGL_ARRAY_BUFFER                   = 0x8892;
static const unsigned int WEBGL_ELEMENT_ARRAY_BUFFER           = 0x8893;
static const unsigned int WEBGL_ARRAY_BUFFER_BINDING           = 0x8894;
static const unsigned int WEBGL_ELEMENT_ARRAY_BUFFER_BINDING   = 0x8895;

static const unsigned int WEBGL_STREAM_DRAW                    = 0x88E0;
static const unsigned int WEBGL_STATIC_DRAW                    = 0x88E4;
static const unsigned int WEBGL_DYNAMIC_DRAW                   = 0x88E8;

static const unsigned int WEBGL_BUFFER_SIZE                    = 0x8764;
static const unsigned int WEBGL_BUFFER_USAGE                   = 0x8765;

static const unsigned int WEBGL_CURRENT_VERTEX_ATTRIB          = 0x8626;

/* CullFaceMode */
static const unsigned int WEBGL_FRONT                          = 0x0404;
static const unsigned int WEBGL_BACK                           = 0x0405;
static const unsigned int WEBGL_FRONT_AND_BACK                 = 0x0408;

/* DepthFunction */
/*      GL_NEVER */
/*      GL_LESS */
/*      GL_EQUAL */
/*      GL_LEQUAL */
/*      GL_GREATER */
/*      GL_NOTEQUAL */
/*      GL_GEQUAL */
/*      GL_ALWAYS */

/* EnableCap */
static const unsigned int WEBGL_TEXTURE_2D                     = 0x0DE1;
static const unsigned int WEBGL_CULL_FACE                      = 0x0B44;
static const unsigned int WEBGL_BLEND                          = 0x0BE2;
static const unsigned int WEBGL_DITHER                         = 0x0BD0;
static const unsigned int WEBGL_STENCIL_TEST                   = 0x0B90;
static const unsigned int WEBGL_DEPTH_TEST                     = 0x0B71;
static const unsigned int WEBGL_SCISSOR_TEST                   = 0x0C11;
static const unsigned int WEBGL_POLYGON_OFFSET_FILL            = 0x8037;
static const unsigned int WEBGL_SAMPLE_ALPHA_TO_COVERAGE       = 0x809E;
static const unsigned int WEBGL_SAMPLE_COVERAGE                = 0x80A0;

/* ErrorCode */
static const unsigned int WEBGL_NO_ERROR                       = 0;
static const unsigned int WEBGL_INVALID_ENUM                   = 0x0500;
static const unsigned int WEBGL_INVALID_VALUE                  = 0x0501;
static const unsigned int WEBGL_INVALID_OPERATION              = 0x0502;
static const unsigned int WEBGL_OUT_OF_MEMORY                  = 0x0505;

/* FrontFaceDirection */
static const unsigned int WEBGL_CW                             = 0x0900;
static const unsigned int WEBGL_CCW                            = 0x0901;

/* GetPName */
static const unsigned int WEBGL_LINE_WIDTH                     = 0x0B21;
static const unsigned int WEBGL_ALIASED_POINT_SIZE_RANGE       = 0x846D;
static const unsigned int WEBGL_ALIASED_LINE_WIDTH_RANGE       = 0x846E;
static const unsigned int WEBGL_CULL_FACE_MODE                 = 0x0B45;
static const unsigned int WEBGL_FRONT_FACE                     = 0x0B46;
static const unsigned int WEBGL_DEPTH_RANGE                    = 0x0B70;
static const unsigned int WEBGL_DEPTH_WRITEMASK                = 0x0B72;
static const unsigned int WEBGL_DEPTH_CLEAR_VALUE              = 0x0B73;
static const unsigned int WEBGL_DEPTH_FUNC                     = 0x0B74;
static const unsigned int WEBGL_STENCIL_CLEAR_VALUE            = 0x0B91;
static const unsigned int WEBGL_STENCIL_FUNC                   = 0x0B92;
static const unsigned int WEBGL_STENCIL_FAIL                   = 0x0B94;
static const unsigned int WEBGL_STENCIL_PASS_DEPTH_FAIL        = 0x0B95;
static const unsigned int WEBGL_STENCIL_PASS_DEPTH_PASS        = 0x0B96;
static const unsigned int WEBGL_STENCIL_REF                    = 0x0B97;
static const unsigned int WEBGL_STENCIL_VALUE_MASK             = 0x0B93;
static const unsigned int WEBGL_STENCIL_WRITEMASK              = 0x0B98;
static const unsigned int WEBGL_STENCIL_BACK_FUNC              = 0x8800;
static const unsigned int WEBGL_STENCIL_BACK_FAIL              = 0x8801;
static const unsigned int WEBGL_STENCIL_BACK_PASS_DEPTH_FAIL   = 0x8802;
static const unsigned int WEBGL_STENCIL_BACK_PASS_DEPTH_PASS   = 0x8803;
static const unsigned int WEBGL_STENCIL_BACK_REF               = 0x8CA3;
static const unsigned int WEBGL_STENCIL_BACK_VALUE_MASK        = 0x8CA4;
static const unsigned int WEBGL_STENCIL_BACK_WRITEMASK         = 0x8CA5;
static const unsigned int WEBGL_VIEWPORT                       = 0x0BA2;
static const unsigned int WEBGL_SCISSOR_BOX                    = 0x0C10;
/*      GL_SCISSOR_TEST */
static const unsigned int WEBGL_COLOR_CLEAR_VALUE              = 0x0C22;
static const unsigned int WEBGL_COLOR_WRITEMASK                = 0x0C23;
static const unsigned int WEBGL_UNPACK_ALIGNMENT               = 0x0CF5;
static const unsigned int WEBGL_PACK_ALIGNMENT                 = 0x0D05;
static const unsigned int WEBGL_MAX_TEXTURE_SIZE               = 0x0D33;
static const unsigned int WEBGL_MAX_VIEWPORT_DIMS              = 0x0D3A;
static const unsigned int WEBGL_SUBPIXEL_BITS                  = 0x0D50;
static const unsigned int WEBGL_RED_BITS                       = 0x0D52;
static const unsigned int WEBGL_GREEN_BITS                     = 0x0D53;
static const unsigned int WEBGL_BLUE_BITS                      = 0x0D54;
static const unsigned int WEBGL_ALPHA_BITS                     = 0x0D55;
static const unsigned int WEBGL_DEPTH_BITS                     = 0x0D56;
static const unsigned int WEBGL_STENCIL_BITS                   = 0x0D57;
static const unsigned int WEBGL_POLYGON_OFFSET_UNITS           = 0x2A00;
/*      GL_POLYGON_OFFSET_FILL */
static const unsigned int WEBGL_POLYGON_OFFSET_FACTOR          = 0x8038;
static const unsigned int WEBGL_TEXTURE_BINDING_2D             = 0x8069;
static const unsigned int WEBGL_SAMPLE_BUFFERS                 = 0x80A8;
static const unsigned int WEBGL_SAMPLES                        = 0x80A9;
static const unsigned int WEBGL_SAMPLE_COVERAGE_VALUE          = 0x80AA;
static const unsigned int WEBGL_SAMPLE_COVERAGE_INVERT         = 0x80AB;

/* GetTextureParameter */
/*      GL_TEXTURE_MAG_FILTER */
/*      GL_TEXTURE_MIN_FILTER */
/*      GL_TEXTURE_WRAP_S */
/*      GL_TEXTURE_WRAP_T */

static const unsigned int WEBGL_COMPRESSED_TEXTURE_FORMATS     = 0x86A3;

/* HintMode */
static const unsigned int WEBGL_DONT_CARE                      = 0x1100;
static const unsigned int WEBGL_FASTEST                        = 0x1101;
static const unsigned int WEBGL_NICEST                         = 0x1102;

/* HintTarget */
static const unsigned int WEBGL_GENERATE_MIPMAP_HINT            = 0x8192;

/* DataType */
static const unsigned int WEBGL_BYTE                           = 0x1400;
static const unsigned int WEBGL_UNSIGNED_BYTE                  = 0x1401;
static const unsigned int WEBGL_SHORT                          = 0x1402;
static const unsigned int WEBGL_UNSIGNED_SHORT                 = 0x1403;
static const unsigned int WEBGL_INT                            = 0x1404;
static const unsigned int WEBGL_UNSIGNED_INT                   = 0x1405;
static const unsigned int WEBGL_FLOAT                          = 0x1406;

/* Used internally. Not part of the spec. */
static const unsigned int N_WEBGL_TYPES = WEBGL_FLOAT - 0x1400 + 1;

/* PixelFormat */
static const unsigned int WEBGL_DEPTH_COMPONENT                = 0x1902;
static const unsigned int WEBGL_ALPHA                          = 0x1906;
static const unsigned int WEBGL_RGB                            = 0x1907;
static const unsigned int WEBGL_RGBA                           = 0x1908;
static const unsigned int WEBGL_LUMINANCE                      = 0x1909;
static const unsigned int WEBGL_LUMINANCE_ALPHA                = 0x190A;

/* PixelType */
/*      GL_UNSIGNED_BYTE */
static const unsigned int WEBGL_UNSIGNED_SHORT_4_4_4_4         = 0x8033;
static const unsigned int WEBGL_UNSIGNED_SHORT_5_5_5_1         = 0x8034;
static const unsigned int WEBGL_UNSIGNED_SHORT_5_6_5           = 0x8363;

/* Shaders */
static const unsigned int WEBGL_FRAGMENT_SHADER                  = 0x8B30;
static const unsigned int WEBGL_VERTEX_SHADER                    = 0x8B31;
static const unsigned int WEBGL_MAX_VERTEX_ATTRIBS               = 0x8869;
static const unsigned int WEBGL_MAX_VERTEX_UNIFORM_VECTORS       = 0x8DFB;
static const unsigned int WEBGL_MAX_VARYING_VECTORS              = 0x8DFC;
static const unsigned int WEBGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS = 0x8B4D;
static const unsigned int WEBGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS   = 0x8B4C;
static const unsigned int WEBGL_MAX_TEXTURE_IMAGE_UNITS          = 0x8872;
static const unsigned int WEBGL_MAX_FRAGMENT_UNIFORM_VECTORS     = 0x8DFD;
static const unsigned int WEBGL_SHADER_TYPE                      = 0x8B4F;
static const unsigned int WEBGL_DELETE_STATUS                    = 0x8B80;
static const unsigned int WEBGL_LINK_STATUS                      = 0x8B82;
static const unsigned int WEBGL_VALIDATE_STATUS                  = 0x8B83;
static const unsigned int WEBGL_ATTACHED_SHADERS                 = 0x8B85;
static const unsigned int WEBGL_ACTIVE_UNIFORMS                  = 0x8B86;
static const unsigned int WEBGL_ACTIVE_ATTRIBUTES                = 0x8B89;
static const unsigned int WEBGL_SHADING_LANGUAGE_VERSION         = 0x8B8C;
static const unsigned int WEBGL_CURRENT_PROGRAM                  = 0x8B8D;

/* StencilFunction */
static const unsigned int WEBGL_NEVER                          = 0x0200;
static const unsigned int WEBGL_LESS                           = 0x0201;
static const unsigned int WEBGL_EQUAL                          = 0x0202;
static const unsigned int WEBGL_LEQUAL                         = 0x0203;
static const unsigned int WEBGL_GREATER                        = 0x0204;
static const unsigned int WEBGL_NOTEQUAL                       = 0x0205;
static const unsigned int WEBGL_GEQUAL                         = 0x0206;
static const unsigned int WEBGL_ALWAYS                         = 0x0207;

/* StencilOp */
/*      GL_ZERO */
static const unsigned int WEBGL_KEEP                           = 0x1E00;
static const unsigned int WEBGL_REPLACE                        = 0x1E01;
static const unsigned int WEBGL_INCR                           = 0x1E02;
static const unsigned int WEBGL_DECR                           = 0x1E03;
static const unsigned int WEBGL_INVERT                         = 0x150A;
static const unsigned int WEBGL_INCR_WRAP                      = 0x8507;
static const unsigned int WEBGL_DECR_WRAP                      = 0x8508;

/* StringName */
static const unsigned int WEBGL_VENDOR                         = 0x1F00;
static const unsigned int WEBGL_RENDERER                       = 0x1F01;
static const unsigned int WEBGL_VERSION                        = 0x1F02;

/* TextureMagFilter */
static const unsigned int WEBGL_NEAREST                        = 0x2600;
static const unsigned int WEBGL_LINEAR                         = 0x2601;

/* TextureMinFilter */
/*      GL_NEAREST */
/*      GL_LINEAR */
static const unsigned int WEBGL_NEAREST_MIPMAP_NEAREST         = 0x2700;
static const unsigned int WEBGL_LINEAR_MIPMAP_NEAREST          = 0x2701;
static const unsigned int WEBGL_NEAREST_MIPMAP_LINEAR          = 0x2702;
static const unsigned int WEBGL_LINEAR_MIPMAP_LINEAR           = 0x2703;

/* TextureParameterName */
static const unsigned int WEBGL_TEXTURE_MAG_FILTER             = 0x2800;
static const unsigned int WEBGL_TEXTURE_MIN_FILTER             = 0x2801;
static const unsigned int WEBGL_TEXTURE_WRAP_S                 = 0x2802;
static const unsigned int WEBGL_TEXTURE_WRAP_T                 = 0x2803;

/* TextureTarget */
/*      GL_TEXTURE_2D */
static const unsigned int WEBGL_TEXTURE                        = 0x1702;

static const unsigned int WEBGL_TEXTURE_CUBE_MAP               = 0x8513;
static const unsigned int WEBGL_TEXTURE_BINDING_CUBE_MAP       = 0x8514;
static const unsigned int WEBGL_TEXTURE_CUBE_MAP_POSITIVE_X    = 0x8515;
static const unsigned int WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_X    = 0x8516;
static const unsigned int WEBGL_TEXTURE_CUBE_MAP_POSITIVE_Y    = 0x8517;
static const unsigned int WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_Y    = 0x8518;
static const unsigned int WEBGL_TEXTURE_CUBE_MAP_POSITIVE_Z    = 0x8519;
static const unsigned int WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_Z    = 0x851A;
static const unsigned int WEBGL_MAX_CUBE_MAP_TEXTURE_SIZE      = 0x851C;

/* TextureUnit */
static const unsigned int WEBGL_TEXTURE0                       = 0x84C0;
static const unsigned int WEBGL_TEXTURE1                       = 0x84C1;
static const unsigned int WEBGL_TEXTURE2                       = 0x84C2;
static const unsigned int WEBGL_TEXTURE3                       = 0x84C3;
static const unsigned int WEBGL_TEXTURE4                       = 0x84C4;
static const unsigned int WEBGL_TEXTURE5                       = 0x84C5;
static const unsigned int WEBGL_TEXTURE6                       = 0x84C6;
static const unsigned int WEBGL_TEXTURE7                       = 0x84C7;
static const unsigned int WEBGL_TEXTURE8                       = 0x84C8;
static const unsigned int WEBGL_TEXTURE9                       = 0x84C9;
static const unsigned int WEBGL_TEXTURE10                      = 0x84CA;
static const unsigned int WEBGL_TEXTURE11                      = 0x84CB;
static const unsigned int WEBGL_TEXTURE12                      = 0x84CC;
static const unsigned int WEBGL_TEXTURE13                      = 0x84CD;
static const unsigned int WEBGL_TEXTURE14                      = 0x84CE;
static const unsigned int WEBGL_TEXTURE15                      = 0x84CF;
static const unsigned int WEBGL_TEXTURE16                      = 0x84D0;
static const unsigned int WEBGL_TEXTURE17                      = 0x84D1;
static const unsigned int WEBGL_TEXTURE18                      = 0x84D2;
static const unsigned int WEBGL_TEXTURE19                      = 0x84D3;
static const unsigned int WEBGL_TEXTURE20                      = 0x84D4;
static const unsigned int WEBGL_TEXTURE21                      = 0x84D5;
static const unsigned int WEBGL_TEXTURE22                      = 0x84D6;
static const unsigned int WEBGL_TEXTURE23                      = 0x84D7;
static const unsigned int WEBGL_TEXTURE24                      = 0x84D8;
static const unsigned int WEBGL_TEXTURE25                      = 0x84D9;
static const unsigned int WEBGL_TEXTURE26                      = 0x84DA;
static const unsigned int WEBGL_TEXTURE27                      = 0x84DB;
static const unsigned int WEBGL_TEXTURE28                      = 0x84DC;
static const unsigned int WEBGL_TEXTURE29                      = 0x84DD;
static const unsigned int WEBGL_TEXTURE30                      = 0x84DE;
static const unsigned int WEBGL_TEXTURE31                      = 0x84DF;
static const unsigned int WEBGL_ACTIVE_TEXTURE                 = 0x84E0;

/* TextureWrapMode */
static const unsigned int WEBGL_REPEAT                         = 0x2901;
static const unsigned int WEBGL_CLAMP_TO_EDGE                  = 0x812F;
static const unsigned int WEBGL_MIRRORED_REPEAT                = 0x8370;

/* Uniform Types */
static const unsigned int WEBGL_FLOAT_VEC2                     = 0x8B50;
static const unsigned int WEBGL_FLOAT_VEC3                     = 0x8B51;
static const unsigned int WEBGL_FLOAT_VEC4                     = 0x8B52;
static const unsigned int WEBGL_INT_VEC2                       = 0x8B53;
static const unsigned int WEBGL_INT_VEC3                       = 0x8B54;
static const unsigned int WEBGL_INT_VEC4                       = 0x8B55;
static const unsigned int WEBGL_BOOL                           = 0x8B56;
static const unsigned int WEBGL_BOOL_VEC2                      = 0x8B57;
static const unsigned int WEBGL_BOOL_VEC3                      = 0x8B58;
static const unsigned int WEBGL_BOOL_VEC4                      = 0x8B59;
static const unsigned int WEBGL_FLOAT_MAT2                     = 0x8B5A;
static const unsigned int WEBGL_FLOAT_MAT3                     = 0x8B5B;
static const unsigned int WEBGL_FLOAT_MAT4                     = 0x8B5C;
static const unsigned int WEBGL_SAMPLER_2D                     = 0x8B5E;
static const unsigned int WEBGL_SAMPLER_CUBE                   = 0x8B60;

/* Vertex Arrays */
static const unsigned int WEBGL_VERTEX_ATTRIB_ARRAY_ENABLED        = 0x8622;
static const unsigned int WEBGL_VERTEX_ATTRIB_ARRAY_SIZE           = 0x8623;
static const unsigned int WEBGL_VERTEX_ATTRIB_ARRAY_STRIDE         = 0x8624;
static const unsigned int WEBGL_VERTEX_ATTRIB_ARRAY_TYPE           = 0x8625;
static const unsigned int WEBGL_VERTEX_ATTRIB_ARRAY_NORMALIZED     = 0x886A;
static const unsigned int WEBGL_VERTEX_ATTRIB_ARRAY_POINTER        = 0x8645;
static const unsigned int WEBGL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING = 0x889F;

/* Read Format (Not supported in WebGL) */
/*static const unsigned int WEBGL_IMPLEMENTATION_COLOR_READ_TYPE   = 0x8B9A;*/
/*static const unsigned int WEBGL_IMPLEMENTATION_COLOR_READ_FORMAT = 0x8B9B;*/

/* Shader Source */
static const unsigned int WEBGL_COMPILE_STATUS                 = 0x8B81;
static const unsigned int WEBGL_SHADER_COMPILER                = 0x8DFA;

/* Shader Precision-Specified Types */
static const unsigned int WEBGL_LOW_FLOAT                      = 0x8DF0;
static const unsigned int WEBGL_MEDIUM_FLOAT                   = 0x8DF1;
static const unsigned int WEBGL_HIGH_FLOAT                     = 0x8DF2;
static const unsigned int WEBGL_LOW_INT                        = 0x8DF3;
static const unsigned int WEBGL_MEDIUM_INT                     = 0x8DF4;
static const unsigned int WEBGL_HIGH_INT                       = 0x8DF5;

/* Framebuffer Object. */
static const unsigned int WEBGL_FRAMEBUFFER                    = 0x8D40;
static const unsigned int WEBGL_RENDERBUFFER                   = 0x8D41;

static const unsigned int WEBGL_RGBA4                          = 0x8056;
static const unsigned int WEBGL_RGB5_A1                        = 0x8057;
static const unsigned int WEBGL_RGB565                         = 0x8D62;
static const unsigned int WEBGL_DEPTH_COMPONENT16              = 0x81A5;
static const unsigned int WEBGL_STENCIL_INDEX                  = 0x1901;
static const unsigned int WEBGL_STENCIL_INDEX8                 = 0x8D48;
static const unsigned int WEBGL_DEPTH_STENCIL                  = 0x84F9;

static const unsigned int WEBGL_RENDERBUFFER_WIDTH             = 0x8D42;
static const unsigned int WEBGL_RENDERBUFFER_HEIGHT            = 0x8D43;
static const unsigned int WEBGL_RENDERBUFFER_INTERNAL_FORMAT   = 0x8D44;
static const unsigned int WEBGL_RENDERBUFFER_RED_SIZE          = 0x8D50;
static const unsigned int WEBGL_RENDERBUFFER_GREEN_SIZE        = 0x8D51;
static const unsigned int WEBGL_RENDERBUFFER_BLUE_SIZE         = 0x8D52;
static const unsigned int WEBGL_RENDERBUFFER_ALPHA_SIZE        = 0x8D53;
static const unsigned int WEBGL_RENDERBUFFER_DEPTH_SIZE        = 0x8D54;
static const unsigned int WEBGL_RENDERBUFFER_STENCIL_SIZE      = 0x8D55;

static const unsigned int WEBGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE           = 0x8CD0;
static const unsigned int WEBGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME           = 0x8CD1;
static const unsigned int WEBGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL         = 0x8CD2;
static const unsigned int WEBGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE = 0x8CD3;

static const unsigned int WEBGL_COLOR_ATTACHMENT0              = 0x8CE0;
static const unsigned int WEBGL_DEPTH_ATTACHMENT               = 0x8D00;
static const unsigned int WEBGL_STENCIL_ATTACHMENT             = 0x8D20;
static const unsigned int WEBGL_DEPTH_STENCIL_ATTACHMENT       = 0x821A;

static const unsigned int WEBGL_NONE                           = 0;

static const unsigned int WEBGL_FRAMEBUFFER_COMPLETE                      = 0x8CD5;
static const unsigned int WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT         = 0x8CD6;
static const unsigned int WEBGL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT = 0x8CD7;
static const unsigned int WEBGL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS         = 0x8CD9;
static const unsigned int WEBGL_FRAMEBUFFER_UNSUPPORTED                   = 0x8CDD;

static const unsigned int WEBGL_FRAMEBUFFER_BINDING            = 0x8CA6;
static const unsigned int WEBGL_RENDERBUFFER_BINDING           = 0x8CA7;
static const unsigned int WEBGL_MAX_RENDERBUFFER_SIZE          = 0x84E8;

static const unsigned int WEBGL_INVALID_FRAMEBUFFER_OPERATION  = 0x0506;

/* WebGL specific */
static const unsigned int WEBGL_UNPACK_FLIP_Y_WEBGL                 = 0x9240;
static const unsigned int WEBGL_UNPACK_PREMULTIPLY_ALPHA_WEBGL      = 0x9241;
static const unsigned int WEBGL_CONTEXT_LOST_WEBGL                  = 0x9242;
static const unsigned int WEBGL_UNPACK_COLORSPACE_CONVERSION_WEBGL  = 0x9243;
static const unsigned int WEBGL_BROWSER_DEFAULT_WEBGL               = 0x9244;

/* WebGL extension defines */
static const unsigned int WEBGL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES	= 0x8b8b;

/* WebGL extensions (as flags) */
static const unsigned int WEBGL_EXT_OES_TEXTURE_FLOAT = 0x1;
static const unsigned int WEBGL_EXT_OES_STANDARD_DERIVATIVES = 0x2;

#endif // !DOM_WEBGLCONSTANTS_H

/* AddWebGLConstants() is put here to be close to the relevant #defines and
   hence not as likely to be neglected. Only the location that needs to call
   AddWebGLConstants() should define WEBGL_CONSTANTS_CREATE_FUNCTION prior to
   including the header.

   We put this outside the include guards so that

   #include "modules/dom/src/canvas/webglconstants.h"
   ...
   #define WEBGL_CONSTANTS_CREATE_FUNCTION
   #include "modules/dom/src/canvas/webglconstants.h"

   works as intended. */

#ifdef WEBGL_CONSTANTS_CREATE_FUNCTION

#define DOM_PUT_WEBGL_CONSTANT_L(obj, constant) DOM_Object::PutNumericConstantL(obj, #constant, WEBGL_##constant, runtime)

static void AddWebGLConstantsL(ES_Object *obj, DOM_Runtime *runtime)
{
	/* ClearBufferMask */
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_BUFFER_BIT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_BUFFER_BIT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, COLOR_BUFFER_BIT);

	/* BeginMode */
	DOM_PUT_WEBGL_CONSTANT_L(obj, POINTS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LINES);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LINE_LOOP);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LINE_STRIP);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TRIANGLES);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TRIANGLE_STRIP);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TRIANGLE_FAN);

	/* AlphaFunction (not supported in ES20) */
	/*      GL_NEVER */
	/*      GL_LESS */
	/*      GL_EQUAL */
	/*      GL_LEQUAL */
	/*      GL_GREATER */
	/*      GL_NOTEQUAL */
	/*      GL_GEQUAL */
	/*      GL_ALWAYS */

	/* BlendingFactorDest */
	DOM_PUT_WEBGL_CONSTANT_L(obj, ZERO);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ONE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SRC_COLOR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ONE_MINUS_SRC_COLOR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SRC_ALPHA);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ONE_MINUS_SRC_ALPHA);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DST_ALPHA);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ONE_MINUS_DST_ALPHA);

	/* BlendingFactorSrc */
	/*      GL_ZERO */
	/*      GL_ONE */
	DOM_PUT_WEBGL_CONSTANT_L(obj, DST_COLOR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ONE_MINUS_DST_COLOR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SRC_ALPHA_SATURATE);
	/*      GL_SRC_ALPHA */
	/*      GL_ONE_MINUS_SRC_ALPHA */
	/*      GL_DST_ALPHA */
	/*      GL_ONE_MINUS_DST_ALPHA */

	/* BlendEquationSeparate */
	DOM_PUT_WEBGL_CONSTANT_L(obj, FUNC_ADD);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BLEND_EQUATION);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BLEND_EQUATION_RGB);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BLEND_EQUATION_ALPHA);

	/* BlendSubtract */
	DOM_PUT_WEBGL_CONSTANT_L(obj, FUNC_SUBTRACT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FUNC_REVERSE_SUBTRACT);

	/* Separate Blend Functions */
	DOM_PUT_WEBGL_CONSTANT_L(obj, BLEND_DST_RGB);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BLEND_SRC_RGB);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BLEND_DST_ALPHA);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BLEND_SRC_ALPHA);
	DOM_PUT_WEBGL_CONSTANT_L(obj, CONSTANT_COLOR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ONE_MINUS_CONSTANT_COLOR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, CONSTANT_ALPHA);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ONE_MINUS_CONSTANT_ALPHA);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BLEND_COLOR);

	/* Buffer Objects */
	DOM_PUT_WEBGL_CONSTANT_L(obj, ARRAY_BUFFER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ELEMENT_ARRAY_BUFFER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ARRAY_BUFFER_BINDING);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ELEMENT_ARRAY_BUFFER_BINDING);

	DOM_PUT_WEBGL_CONSTANT_L(obj, STREAM_DRAW);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STATIC_DRAW);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DYNAMIC_DRAW);

	DOM_PUT_WEBGL_CONSTANT_L(obj, BUFFER_SIZE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BUFFER_USAGE);

	DOM_PUT_WEBGL_CONSTANT_L(obj, CURRENT_VERTEX_ATTRIB);

	/* CullFaceMode */
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRONT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BACK);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRONT_AND_BACK);

	/* DepthFunction */
	/*      GL_NEVER */
	/*      GL_LESS */
	/*      GL_EQUAL */
	/*      GL_LEQUAL */
	/*      GL_GREATER */
	/*      GL_NOTEQUAL */
	/*      GL_GEQUAL */
	/*      GL_ALWAYS */

	/* EnableCap */
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_2D);
	DOM_PUT_WEBGL_CONSTANT_L(obj, CULL_FACE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BLEND);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DITHER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_TEST);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_TEST);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SCISSOR_TEST);
	DOM_PUT_WEBGL_CONSTANT_L(obj, POLYGON_OFFSET_FILL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SAMPLE_ALPHA_TO_COVERAGE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SAMPLE_COVERAGE);

	/* ErrorCode */
	DOM_PUT_WEBGL_CONSTANT_L(obj, NO_ERROR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, INVALID_ENUM);
	DOM_PUT_WEBGL_CONSTANT_L(obj, INVALID_VALUE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, INVALID_OPERATION);
	DOM_PUT_WEBGL_CONSTANT_L(obj, OUT_OF_MEMORY);

	/* FrontFaceDirection */
	DOM_PUT_WEBGL_CONSTANT_L(obj, CW);
	DOM_PUT_WEBGL_CONSTANT_L(obj, CCW);

	/* GetPName */
	DOM_PUT_WEBGL_CONSTANT_L(obj, LINE_WIDTH);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ALIASED_POINT_SIZE_RANGE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ALIASED_LINE_WIDTH_RANGE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, CULL_FACE_MODE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRONT_FACE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_RANGE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_WRITEMASK);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_CLEAR_VALUE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_FUNC);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_CLEAR_VALUE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_FUNC);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_FAIL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_PASS_DEPTH_FAIL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_PASS_DEPTH_PASS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_REF);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_VALUE_MASK);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_WRITEMASK);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_BACK_FUNC);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_BACK_FAIL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_BACK_PASS_DEPTH_FAIL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_BACK_PASS_DEPTH_PASS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_BACK_REF);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_BACK_VALUE_MASK);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_BACK_WRITEMASK);
	DOM_PUT_WEBGL_CONSTANT_L(obj, VIEWPORT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SCISSOR_BOX);
	/*      GL_SCISSOR_TEST */
	DOM_PUT_WEBGL_CONSTANT_L(obj, COLOR_CLEAR_VALUE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, COLOR_WRITEMASK);
	DOM_PUT_WEBGL_CONSTANT_L(obj, UNPACK_ALIGNMENT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, PACK_ALIGNMENT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_TEXTURE_SIZE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_VIEWPORT_DIMS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SUBPIXEL_BITS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RED_BITS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, GREEN_BITS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BLUE_BITS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ALPHA_BITS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_BITS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_BITS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, POLYGON_OFFSET_UNITS);
	/*      GL_POLYGON_OFFSET_FILL */
	DOM_PUT_WEBGL_CONSTANT_L(obj, POLYGON_OFFSET_FACTOR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_BINDING_2D);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SAMPLE_BUFFERS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SAMPLES);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SAMPLE_COVERAGE_VALUE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SAMPLE_COVERAGE_INVERT);

	/* GetTextureParameter */
	/*      GL_TEXTURE_MAG_FILTER */
	/*      GL_TEXTURE_MIN_FILTER */
	/*      GL_TEXTURE_WRAP_S */
	/*      GL_TEXTURE_WRAP_T */

	DOM_PUT_WEBGL_CONSTANT_L(obj, COMPRESSED_TEXTURE_FORMATS);

	/* HintMode */
	DOM_PUT_WEBGL_CONSTANT_L(obj, DONT_CARE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FASTEST);
	DOM_PUT_WEBGL_CONSTANT_L(obj, NICEST);

	/* HintTarget */
	DOM_PUT_WEBGL_CONSTANT_L(obj, GENERATE_MIPMAP_HINT);

	/* DataType */
	DOM_PUT_WEBGL_CONSTANT_L(obj, BYTE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, UNSIGNED_BYTE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SHORT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, UNSIGNED_SHORT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, INT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, UNSIGNED_INT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FLOAT);

	/* PixelFormat */
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_COMPONENT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ALPHA);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RGB);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RGBA);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LUMINANCE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LUMINANCE_ALPHA);

	/* PixelType */
	/*      GL_UNSIGNED_BYTE */
	DOM_PUT_WEBGL_CONSTANT_L(obj, UNSIGNED_SHORT_4_4_4_4);
	DOM_PUT_WEBGL_CONSTANT_L(obj, UNSIGNED_SHORT_5_5_5_1);
	DOM_PUT_WEBGL_CONSTANT_L(obj, UNSIGNED_SHORT_5_6_5);

	/* Shaders */
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAGMENT_SHADER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, VERTEX_SHADER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_VERTEX_ATTRIBS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_VERTEX_UNIFORM_VECTORS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_VARYING_VECTORS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_VERTEX_TEXTURE_IMAGE_UNITS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_TEXTURE_IMAGE_UNITS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_FRAGMENT_UNIFORM_VECTORS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SHADER_TYPE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DELETE_STATUS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LINK_STATUS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, VALIDATE_STATUS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ATTACHED_SHADERS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ACTIVE_UNIFORMS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ACTIVE_ATTRIBUTES);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SHADING_LANGUAGE_VERSION);
	DOM_PUT_WEBGL_CONSTANT_L(obj, CURRENT_PROGRAM);

	/* StencilFunction */
	DOM_PUT_WEBGL_CONSTANT_L(obj, NEVER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LESS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, EQUAL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LEQUAL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, GREATER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, NOTEQUAL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, GEQUAL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ALWAYS);

	/* StencilOp */
	/*      GL_ZERO */
	DOM_PUT_WEBGL_CONSTANT_L(obj, KEEP);
	DOM_PUT_WEBGL_CONSTANT_L(obj, REPLACE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, INCR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DECR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, INVERT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, INCR_WRAP);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DECR_WRAP);

	/* StringName */
	DOM_PUT_WEBGL_CONSTANT_L(obj, VENDOR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, VERSION);

	/* TextureMagFilter */
	DOM_PUT_WEBGL_CONSTANT_L(obj, NEAREST);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LINEAR);

	/* TextureMinFilter */
	/*      GL_NEAREST */
	/*      GL_LINEAR */
	DOM_PUT_WEBGL_CONSTANT_L(obj, NEAREST_MIPMAP_NEAREST);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LINEAR_MIPMAP_NEAREST);
	DOM_PUT_WEBGL_CONSTANT_L(obj, NEAREST_MIPMAP_LINEAR);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LINEAR_MIPMAP_LINEAR);

	/* TextureParameterName */
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_MAG_FILTER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_MIN_FILTER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_WRAP_S);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_WRAP_T);

	/* TextureTarget */
	/*      GL_TEXTURE_2D */
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE);

	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_CUBE_MAP);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_BINDING_CUBE_MAP);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_CUBE_MAP_POSITIVE_X);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_CUBE_MAP_NEGATIVE_X);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_CUBE_MAP_POSITIVE_Y);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_CUBE_MAP_NEGATIVE_Y);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_CUBE_MAP_POSITIVE_Z);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE_CUBE_MAP_NEGATIVE_Z);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_CUBE_MAP_TEXTURE_SIZE);

	/* TextureUnit */
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE0);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE1);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE2);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE3);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE4);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE5);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE6);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE7);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE8);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE9);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE10);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE11);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE12);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE13);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE14);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE15);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE16);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE17);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE18);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE19);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE20);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE21);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE22);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE23);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE24);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE25);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE26);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE27);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE28);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE29);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE30);
	DOM_PUT_WEBGL_CONSTANT_L(obj, TEXTURE31);
	DOM_PUT_WEBGL_CONSTANT_L(obj, ACTIVE_TEXTURE);

	/* TextureWrapMode */
	DOM_PUT_WEBGL_CONSTANT_L(obj, REPEAT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, CLAMP_TO_EDGE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MIRRORED_REPEAT);

	/* Uniform Types */
	DOM_PUT_WEBGL_CONSTANT_L(obj, FLOAT_VEC2);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FLOAT_VEC3);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FLOAT_VEC4);
	DOM_PUT_WEBGL_CONSTANT_L(obj, INT_VEC2);
	DOM_PUT_WEBGL_CONSTANT_L(obj, INT_VEC3);
	DOM_PUT_WEBGL_CONSTANT_L(obj, INT_VEC4);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BOOL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BOOL_VEC2);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BOOL_VEC3);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BOOL_VEC4);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FLOAT_MAT2);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FLOAT_MAT3);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FLOAT_MAT4);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SAMPLER_2D);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SAMPLER_CUBE);

	/* Vertex Arrays */
	DOM_PUT_WEBGL_CONSTANT_L(obj, VERTEX_ATTRIB_ARRAY_ENABLED);
	DOM_PUT_WEBGL_CONSTANT_L(obj, VERTEX_ATTRIB_ARRAY_SIZE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, VERTEX_ATTRIB_ARRAY_STRIDE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, VERTEX_ATTRIB_ARRAY_TYPE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, VERTEX_ATTRIB_ARRAY_NORMALIZED);
	DOM_PUT_WEBGL_CONSTANT_L(obj, VERTEX_ATTRIB_ARRAY_POINTER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, VERTEX_ATTRIB_ARRAY_BUFFER_BINDING);

	/* Shader Source */
	DOM_PUT_WEBGL_CONSTANT_L(obj, COMPILE_STATUS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, SHADER_COMPILER);

	/* Shader Precision-Specified Types */
	DOM_PUT_WEBGL_CONSTANT_L(obj, LOW_FLOAT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MEDIUM_FLOAT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, HIGH_FLOAT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, LOW_INT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MEDIUM_INT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, HIGH_INT);

	/* Framebuffer Object. */
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER);

	DOM_PUT_WEBGL_CONSTANT_L(obj, RGBA4);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RGB5_A1);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RGB565);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_COMPONENT16);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_INDEX);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_INDEX8);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_STENCIL);

	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER_WIDTH);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER_HEIGHT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER_INTERNAL_FORMAT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER_RED_SIZE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER_GREEN_SIZE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER_BLUE_SIZE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER_ALPHA_SIZE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER_DEPTH_SIZE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER_STENCIL_SIZE);

	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE);

	DOM_PUT_WEBGL_CONSTANT_L(obj, COLOR_ATTACHMENT0);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_ATTACHMENT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, STENCIL_ATTACHMENT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, DEPTH_STENCIL_ATTACHMENT);

	DOM_PUT_WEBGL_CONSTANT_L(obj, NONE);

	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER_COMPLETE);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER_INCOMPLETE_DIMENSIONS);
	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER_UNSUPPORTED);

	DOM_PUT_WEBGL_CONSTANT_L(obj, FRAMEBUFFER_BINDING);
	DOM_PUT_WEBGL_CONSTANT_L(obj, RENDERBUFFER_BINDING);
	DOM_PUT_WEBGL_CONSTANT_L(obj, MAX_RENDERBUFFER_SIZE);

	DOM_PUT_WEBGL_CONSTANT_L(obj, INVALID_FRAMEBUFFER_OPERATION);

	/* WebGL specific */
	DOM_PUT_WEBGL_CONSTANT_L(obj, UNPACK_FLIP_Y_WEBGL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, UNPACK_PREMULTIPLY_ALPHA_WEBGL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, CONTEXT_LOST_WEBGL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, UNPACK_COLORSPACE_CONVERSION_WEBGL);
	DOM_PUT_WEBGL_CONSTANT_L(obj, BROWSER_DEFAULT_WEBGL);
}

#undef DOM_PUT_WEBGL_CONSTANT_L

#endif // WEBGL_CONSTANTS_CREATE_FUNCTION
