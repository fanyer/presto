/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_OPENGL
#include "platforms/vega_backends/opengl/vegaglapi.h"

#include "platforms/vega_backends/opengl/vegaglbuffer.h"
#include "platforms/vega_backends/opengl/vegagldevice.h"
#include "platforms/vega_backends/opengl/vegaglshader.h"
#include "platforms/vega_backends/opengl/vegagltexture.h"
#include "platforms/vega_backends/opengl/vegaglwindow.h"
#include "platforms/vega_backends/opengl/vegaglfbo.h"

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
#include "platforms/vega_backends/vega_blocklist_file.h"
#endif // VEGA_BACKENDS_USE_BLOCKLIST

#ifdef VEGA_GL_DEBUG_CONTEXT
void VEGA_GL_API_ENTRY VEGAGLDebugCallbackFunc(GLenum source,
                                               GLenum type,
                                               GLuint id,
                                               GLenum severity,
                                               GLsizei length,
                                               const GLchar* message,
                                               GLvoid* userParam)
{
	OP_NEW_DBG("error", "opengl");
	OP_DBG(("%s", message));
	OP_ASSERT(!"OpenGL error encountered, check debug log for details");
}
#endif // VEGA_GL_DEBUG_CONTEXT

static inline VEGAGlFramebufferObject* getFramebufferObject(VEGA3dRenderTarget* renderTarget)
{
	if (renderTarget->getType() == VEGA3dRenderTarget::VEGA3D_RT_TEXTURE)
		return static_cast<VEGAGlFramebufferObject*>(renderTarget);
	else
		return static_cast<VEGAGlFramebufferObject*>(static_cast<VEGAGlWindow*>(renderTarget)->getWindowFBO());
}

OP_STATUS VEGAGlDevice::createShaderProgram(VEGA3dShaderProgram** shader, VEGA3dShaderProgram::ShaderType shdtype, VEGA3dShaderProgram::WrapMode shdmode)
{
	if (shdtype >= VEGA3dShaderProgram::NUM_ACTUAL_SHADERS)
		return OpStatus::ERR;

	if (shdtype == VEGA3dShaderProgram::SHADER_CUSTOM)
	{
		VEGAGlShaderProgram* glshader = OP_NEW(VEGAGlShaderProgram, (this));
		if (!glshader)
			return OpStatus::ERR_NO_MEMORY;
		glshader->program_object = glCreateProgram();
		glshader->Into(&shaderPrograms);
		*shader = glshader;
		return OpStatus::OK;
	}

	// wrap workaround is only needed for GPUs without full npot
	// support
	if (fullNPotSupport())
		shdmode = VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;

	const size_t shader_index = VEGA3dShaderProgram::GetShaderIndex(shdtype, shdmode);

	if (cachedShaderProgramsPresent[shader_index])
	{
		VEGA3dShaderProgram * cached_shader = cachedShaderPrograms[shader_index];
		if (!cached_shader)
			return OpStatus::ERR;
		VEGARefCount::IncRef(cached_shader);
		*shader = cached_shader;
		return OpStatus::OK;
	}

	VEGAGlShaderProgram* glshader = OP_NEW(VEGAGlShaderProgram, (this));
	if (!glshader)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS status = glshader->loadProgram(shdtype, shdmode);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(glshader);
		cachedShaderProgramsPresent[shader_index] = true;
		return status;
	}
	glshader->Into(&shaderPrograms);
	*shader = glshader;
	VEGARefCount::IncRef(glshader);
	cachedShaderPrograms[shader_index] = glshader;
	cachedShaderProgramsPresent[shader_index] = true;

	return OpStatus::OK;
}

OP_STATUS VEGAGlDevice::setShaderProgram(VEGA3dShaderProgram* shader)
{
	if (currentShaderProgram == shader)
		return OpStatus::OK;

	VEGARefCount::DecRef(currentShaderProgram);
	VEGARefCount::IncRef(shader);
	currentShaderProgram = shader;

	if (currentShaderProgram)
	{
		glUseProgram(static_cast<VEGAGlShaderProgram*>(currentShaderProgram)->program_object);
	}
	else
	{
		glUseProgram(0); // Switch to fixed-function pipeline
	}
	// FIXME: Here be error handling
	return OpStatus::OK;
}

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGAGlDevice::createShader(VEGA3dShader** shader, BOOL fragmentShader, const char* source, unsigned int attribute_count, VEGA3dDevice::AttributeData *attributes, OpString *info_log)
{
	VEGAGlShader* sh = OP_NEW(VEGAGlShader, (this, fragmentShader));
	if (!sh)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS err = sh->Construct(source, info_log);
	if (OpStatus::IsError(err))
	{
		OP_DELETE(sh);
		return err;
	}
	*shader = sh;
	return OpStatus::OK;
}
#endif // CANVAS3D_SUPPORT


VEGAGlDevice::VEGAGlDevice() :
	renderTarget(NULL),
	currentShaderProgram(NULL), currentLayout(NULL), 
	activeTextureNum(0), activeTextures(NULL), activeCubeTextures(NULL),
	clientRenderState(NULL),
	m_scissorX(0), m_scissorY(0), m_scissorW(0), m_scissorH(0),
	m_viewportX(0), m_viewportY(0), m_viewportW(0), m_viewportH(0),
	m_depthNear(0), m_depthFar(1.f),
	m_clearColor(0), m_clearDepth(1.f), m_clearStencil(0),
	m_maxMSAA(1), m_vertexBufferSize(VEGA_TEMP_VERTEX_BUFFER_SIZE), m_cachedExtensions(0)
#ifndef VEGA_OPENGLES
	, dummyVBO(NULL)
#endif //VEGA_OPENGLES
#ifdef VEGA_BACKENDS_OPENGL_FLIPY
	, m_flipY(false), m_backupScissorY(0)
#endif // VEGA_BACKENDS_OPENGL_FLIPY
{
	fboFormatsTested = false;
	supportRGBA4_fbo = false;
	supportRGB5_A1_fbo = false;
	supportHighPrecFrag = true;
#ifdef VEGA_OPENGLES
	supportStencilAttachment = false;
#endif // VEGA_OPENGLES

	for (unsigned int i = 0; i < VEGA3dShaderProgram::NUM_ACTUAL_SHADERS; ++i)
	{
		cachedShaderPrograms[i] = NULL;
		cachedShaderProgramsPresent[i] = false;
	}

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	op_memset(m_quirks, 0, sizeof(m_quirks));
#endif // VEGA_BACKENDS_USE_BLOCKLIST
}

VEGAGlDevice::~VEGAGlDevice()
{
}

OP_STATUS VEGAGlDevice::Init()
{
#ifndef VEGA_OPENGLES
	return InitAPI();
#else
	return OpStatus::OK;
#endif // !VEGA_OPENGLES
}

void VEGAGlDevice::Destroy()
{
	while (textures.First())
		static_cast<VEGAGlTexture*>(textures.First())->clearDevice();
	while (buffers.First())
		static_cast<VEGAGlBuffer*>(buffers.First())->clearDevice();
	while (shaderPrograms.First())
		static_cast<VEGAGlShaderProgram*>(shaderPrograms.First())->clearDevice();
	while (framebuffers.First())
		static_cast<VEGAGlFramebufferObject*>(framebuffers.First())->clearDevice();
	while (renderbuffers.First())
		static_cast<VEGAGlRenderbufferObject*>(renderbuffers.First())->clearDevice();

#ifndef VEGA_OPENGLES
	VEGARefCount::DecRef(dummyVBO);
#endif //VEGA_OPENGLES

	destroyDefault2dObjects();
	if (currentShaderProgram)
		VEGARefCount::DecRef(currentShaderProgram);
	if (currentLayout)
	{
		static_cast<VEGAGlVertexLayout*>(currentLayout)->unbind();
		VEGARefCount::DecRef(currentLayout);
	}

	for (unsigned int i = 0; i < VEGA3dShaderProgram::NUM_ACTUAL_SHADERS; ++i)
	{
		VEGARefCount::DecRef(cachedShaderPrograms[i]);
	}

	OP_DELETEA(activeTextures);
	OP_DELETEA(activeCubeTextures);
}

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
# include "modules/about/operagpu.h"
OP_STATUS VEGAGlDevice::GenerateSpecificBackendInfo(OperaGPU* page, VEGABlocklistFile* blocklist, DataProvider* provider)
{
#ifndef VEGA_OPENGLES
	if (!m_glapi.get())
		RETURN_IF_ERROR(InitAPI());
#endif

	RETURN_IF_ERROR(page->OpenDefinitionList());

	OpString s;
	// vendor
	const char * tmpstr = (const char*)glGetString(GL_VENDOR);
	RETURN_IF_ERROR(s.Set(tmpstr));
	RETURN_IF_ERROR(page->ListEntry(UNI_L("GL_VENDOR"), s.CStr()));
	// renderer
	tmpstr = (const char*)glGetString(GL_RENDERER);
	RETURN_IF_ERROR(s.Set(tmpstr));
	RETURN_IF_ERROR(page->ListEntry(UNI_L("GL_RENDERER"), s.CStr()));
	// backend version
	tmpstr = (const char*)glGetString(GL_VERSION);
	RETURN_IF_ERROR(s.Set(tmpstr));
	RETURN_IF_ERROR(page->ListEntry(UNI_L("GL_VERSION"), s.CStr()));
	// extensions
	tmpstr = (const char*)glGetString(GL_EXTENSIONS);
	RETURN_IF_ERROR(s.Set(tmpstr));
	RETURN_IF_ERROR(page->ListEntry(UNI_L("GL_EXTENSIONS"), s.CStr()));
	// shading language version
	tmpstr = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
	RETURN_IF_ERROR(s.Set(tmpstr));
	RETURN_IF_ERROR(page->ListEntry(UNI_L("GL_SHADING_LANGUAGE_VERSION"), s.CStr()));

	RETURN_IF_ERROR(page->CloseDefinitionList());
	return OpStatus::OK;
}
#endif // VEGA_BACKENDS_USE_BLOCKLIST

#ifndef VEGA_OPENGLES
OP_STATUS VEGAGlDevice::InitAPI()
{
	OpAutoPtr<VEGAGlAPI> glapi(OP_NEW(VEGAGlAPI, ()));
	RETURN_OOM_IF_NULL(glapi.get());
	RETURN_IF_ERROR(glapi->Init(this));
	m_glapi = glapi.release();
	return OpStatus::OK;
}
#endif

#ifdef VEGA_OPENGLES
void VEGAGlDevice::testStencilAttachment()
{
	GLuint fbo;
	GLuint stencilBuffer;
	GLuint texture;

	supportStencilAttachment = true;

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// Attach a color attachment first
	glGenTextures(1, (GLuint*)&texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

	// If this assert triggers we should find another texture format that makes the fbo complete
	OP_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	glGenRenderbuffers(1, &stencilBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, stencilBuffer);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencilBuffer);

	// Consume any error state already triggered so we can test the code below.
	CheckGLError();

	glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, 1, 1);
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
		supportStencilAttachment = false;

	if (supportStencilAttachment)
		supportStencilAttachment = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteTextures(1, &texture);
	glDeleteRenderbuffers(1, &stencilBuffer);
	glDeleteFramebuffers(1, &fbo);
}
#endif // VEGA_OPENGLES

static bool HasGLError()
{
	bool error = glGetError() != GL_NO_ERROR;
	while (glGetError() != GL_NO_ERROR) { }
	return error;
}

// The GL_RGBA4 and GL_RGB5_A1 renderbuffer support is so poor on desktop we need
// to verify some of the basics or otherwise fall back on GL_RBGA.
static bool testFBOBasics()
{
	// Caller has cleared the error flag, so no need to clear it here.
	float col[4]; /* ARRAY OK 2012-05-01 emoller */
	glGetFloatv(GL_COLOR_CLEAR_VALUE, col);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(col[0], col[1], col[2], col[3]);
	unsigned char buf[4] = { 0, 0, 0, 0 }; /* ARRAY OK 2012-05-01 emoller */
	glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buf);
	return !HasGLError() && buf[0] == 255 && buf[1] == 255 && buf[2] == 255 && buf[3] == 255;
}

OP_STATUS VEGAGlDevice::testFBOFormats()
{
	if (!fboFormatsTested)
	{
		bool err = HasGLError();
		OP_ASSERT(!err); // Something left an error behind, enable TWEAK_VEGA_BACKENDS_GL_DEBUG_CONTEXT and find out where.

		GLuint fbo;
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		GLuint colorBuffer;
		glGenRenderbuffers(1, &colorBuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer);

		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1);
		err = HasGLError();
		supportRGBA4_fbo = !err && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		if (supportRGBA4_fbo)
			supportRGBA4_fbo = testFBOBasics();

		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB5_A1, 1, 1);
		err = HasGLError();
		supportRGB5_A1_fbo = !err && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		if (supportRGB5_A1_fbo)
			supportRGB5_A1_fbo = testFBOBasics();

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteRenderbuffers(1, &colorBuffer);
		glDeleteFramebuffers(1, &fbo);

#ifdef VEGA_OPENGLES
		testStencilAttachment();
#endif // VEGA_OPENGLES

		fboFormatsTested = true;
	}
	return OpStatus::OK;
}

OP_STATUS VEGAGlDevice::Construct(VEGA_3D_BLOCKLIST_ENTRY_DECL)
{
	OP_DELETEA(activeTextures);
	OP_DELETEA(activeCubeTextures);

#ifndef VEGA_OPENGLES
	if (!m_glapi.get())
		RETURN_IF_ERROR(InitAPI());
#endif

	initGlExtensions();
	initGlStateVariables();

	int numTex = getMaxCombinedTextureUnits();
	if (numTex == 0)
		return OpStatus::ERR;
	activeTextures = OP_NEWA(VEGA3dTexture *, numTex);
	activeCubeTextures = OP_NEWA(VEGA3dTexture *, numTex);
	if (!(activeTextures && activeCubeTextures))
		return OpStatus::ERR_NO_MEMORY;
	for (int i = 0; i < numTex; ++i)
	{
		activeTextures[i] = NULL;
		activeCubeTextures[i] = NULL;
	}

	RETURN_IF_ERROR(testFBOFormats());
#ifdef VEGA_OPENGLES
	supportHighPrecFrag = checkHighPrecisionFragmentSupport();
#endif

#ifndef VEGA_OPENGLES
	VEGA3dBuffer *dummyVBOtmp;
	RETURN_IF_ERROR(createBuffer(&dummyVBOtmp, N_EMPTY_BUF_DUMMY_BYTES, VEGA3dBuffer::STATIC, false, false));
	dummyVBO = static_cast<VEGAGlBuffer*>(dummyVBOtmp);
	UINT8 dummy_empty_vbo_values[N_EMPTY_BUF_DUMMY_BYTES]; /* ARRAY OK 2012-05-07 emoller */
	op_memset(dummy_empty_vbo_values, EMPTY_BUF_DUMMY_VALUE, N_EMPTY_BUF_DUMMY_BYTES);
	dummyVBO->update(0, N_EMPTY_BUF_DUMMY_BYTES, dummy_empty_vbo_values);
#endif // VEGA_OPENGLES

	applyRenderState();

	glClear(GL_COLOR_BUFFER_BIT);

	glBindTexture(GL_TEXTURE_2D, 0);
	UINT8 white[] = {255, 255, 255};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, white);

	// In OpenGL ES this is implicitly enabled allowing the use of gl_PointSize in the shader.
#ifndef VEGA_OPENGLES
	glEnable(non_gles_GL_VERTEX_PROGRAM_POINT_SIZE); //GL_VERTEX_PROGRAM_POINT_SIZE
#endif //VEGA_OPENGLES

#ifndef VEGA_OPENGLES
	if (m_glapi->m_BlitFramebuffer && m_glapi->m_RenderbufferStorageMultisample
#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	    && !(VEGA_3D_BLOCKLIST_ENTRY_PASS && VEGA_3D_BLOCKLIST_ENTRY_PASS->string_entries.Contains(UNI_L("quirks.nomultisample")))
#endif // VEGA_BACKENDS_USE_BLOCKLIST
		)
	{
		GLint maxSamples;
		glGetIntegerv(non_gles_GL_MAX_SAMPLES, &maxSamples);
		m_maxMSAA = maxSamples;
	}

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	if (VEGA_3D_BLOCKLIST_ENTRY_PASS && VEGA_3D_BLOCKLIST_ENTRY_PASS->string_entries.Contains(UNI_L("quirks.noblendfuncextended")))
		m_glapi->m_BindFragDataLocationIndexed = NULL;
#endif // VEGA_BACKENDS_USE_BLOCKLIST

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	if (VEGA_3D_BLOCKLIST_ENTRY_PASS && VEGA_3D_BLOCKLIST_ENTRY_PASS->string_entries.Contains(UNI_L("quirks.smallvertexbuffersize")))
		m_vertexBufferSize = 128;
#endif // VEGA_BACKENDS_USE_BLOCKLIST
#endif // VEGA_OPENGLES

	// Some ATI cards hang for a long time during shutdown when using
	// too big textures. This cause all kinds of havoc to spartan's
	// screenshot code.
#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	if (VEGA_3D_BLOCKLIST_ENTRY_PASS)
	{
		uni_char* s;
		if (OpStatus::IsSuccess(VEGA_3D_BLOCKLIST_ENTRY_PASS->string_entries.GetData(UNI_L("quirks.limitmaxtexturesize"), &s)))
		{
			unsigned int val = uni_atoi(s);
			OP_ASSERT(val && VEGA3dDevice::isPow2(val) && val <= stateVariables.maxTextureSize);
			if (val)
			{
				stateVariables.maxTextureSize = MIN(VEGA3dDevice::pow2_ceil(val), stateVariables.maxTextureSize);
				stateVariables.maxMipLevel = VEGA3dDevice::ilog2_floor(stateVariables.maxTextureSize);
			}
		}
	}
#endif // VEGA_BACKENDS_USE_BLOCKLIST

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	if (VEGA_3D_BLOCKLIST_ENTRY_PASS)
	{
		updateQuirks(VEGA_3D_BLOCKLIST_ENTRY_PASS->string_entries);
	}
#endif // VEGA_BACKENDS_USE_BLOCKLIST

	const OP_STATUS status = createDefault2dObjects();
#ifdef VEGA_BACKENDS_USE_BLOCKLIST
	if (OpStatus::IsError(status))
		g_vega_backends_module.SetCreationStatus(UNI_L("Failed to create objects required for hardware accelerated 2D rendering"));
#endif // VEGA_BACKENDS_USE_BLOCKLIST

#ifdef VEGA_GL_DEBUG_CONTEXT
	if (OpStatus::IsSuccess(status))
	{
		if (m_glapi.get()->m_DebugMessageCallbackARB)
		{
			void* userParam = 0;
			glDebugMessageCallbackARB(VEGAGLDebugCallbackFunc, userParam);
		}
		else
		{
			OP_ASSERT(!"debug context not available, no debug output will be produced");
		}
	}
#endif // VEGA_GL_DEBUG_CONTEXT

	return status;
}

bool VEGAGlDevice::supportsMultisampling()
{
	return (m_maxMSAA > 1);
}

bool VEGAGlDevice::fullNPotSupport()
{
	return supportsExtension(VEGA3dDevice::OES_TEXTURE_NPOT);
}

void VEGAGlDevice::flush()
{
}

static const int GLBlendOp[] = 
{
	GL_FUNC_ADD,
	GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT
};

static const int GLBlendWeight[] = 
{
	GL_ONE, // BLEND_ONE = 0,
	GL_ZERO, // BLEND_ZERO,
	GL_SRC_ALPHA, // BLEND_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA, // BLEND_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA, // BLEND_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA, // BLEND_ONE_MINUS_DST_ALPHA,
	GL_SRC_COLOR, // BLEND_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR, // BLEND_ONE_MINUS_SRC_COLOR,
	GL_DST_COLOR, // BLEND_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR, // BLEND_ONE_MINUS_DST_COLOR,
	GL_CONSTANT_COLOR, // BLEND_CONSTANT_COLOR,
	GL_ONE_MINUS_CONSTANT_COLOR, // BLEND_ONE_MINUS_CONSTANT_COLOR,
	GL_CONSTANT_ALPHA, // BLEND_CONSTANT_ALPHA,
	GL_ONE_MINUS_CONSTANT_ALPHA, // BLEND_ONE_MINUS_CONSTANT_ALPHA
	GL_SRC_ALPHA_SATURATE, // BLEND_SRC_ALPHA_SATURATE

#ifdef VEGA_OPENGLES
	GL_ZERO, // BLEND_EXTENDED_SRC1_COLOR - GL_SRC1_COLOR doesn't exist for OpenGL ES
	GL_ZERO, // BLEND_EXTENDED_SRC1_ALPHA - GL_SRC1_ALPHA doesn't exist for OpenGL ES
	GL_ZERO, // BLEND_EXTENDED_ONE_MINUS_SRC1_COLOR - GL_ONE_MINUS_SRC1_COLOR doesn't exist for OpenGL ES
	GL_ZERO  // BLEND_EXTENDED_ONE_MINUS_SRC1_ALPHA - GL_ONE_MINUS_SRC1_ALPHA doesn't exist for OpenGL ES
#else  // VEGA_OPENGLES
	non_gles_GL_SRC1_COLOR,           // BLEND_EXTENDED_SRC1_COLOR
	non_gles_GL_SRC1_ALPHA,           // BLEND_EXTENDED_SRC1_ALPHA
	non_gles_GL_ONE_MINUS_SRC1_COLOR, // BLEND_EXTENDED_ONE_MINUS_SRC1_COLOR
	non_gles_GL_ONE_MINUS_SRC1_ALPHA  // BLEND_EXTENDED_ONE_MINUS_SRC1_ALPHA
#endif // VEGA_OPENGLES
};

static const int GLDepthFunc[] = 
{
	GL_NEVER, // DEPTH_NEVER = 0,
	GL_EQUAL, // DEPTH_EQUAL,
	GL_LESS, // DEPTH_LESS,
	GL_LEQUAL, // DEPTH_LESSEQUAL,
	GL_GREATER, // DEPTH_GREATER,
	GL_GEQUAL, // DEPTH_GREATEREQUAL,
	GL_NOTEQUAL, // DEPTH_NOTEQUAL,
	GL_ALWAYS, // DEPTH_ALWAYS,
};

static const int GLStencilFunc[] = 
{
	GL_LESS, // STENCIL_LESS = 0,
	GL_LEQUAL, // STENCIL_LESSEQUAL,
	GL_GREATER, // STENCIL_GREATER,
	GL_GEQUAL, // STENCIL_GREATEREQUAL,
	GL_EQUAL, // STENCIL_EQUAL,
	GL_NOTEQUAL, // STENCIL_NOTEQUAL,
	GL_ALWAYS, // STENCIL_ALWAYS,
	GL_NEVER // STENCIL_NEVER
};

static const int GLStencilOp[] = 
{
	GL_KEEP, // STENCIL_KEEP = 0,
	GL_INVERT, // STENCIL_INVERT,
	GL_INCR, // STENCIL_INCREASE,
	GL_DECR, // STENCIL_DECREASE,
	GL_ZERO, // STENCIL_ZERO,
	GL_REPLACE, // STENCIL_REPLACE,
	GL_INCR_WRAP, // STENCIL_INCREASE_WRAP,
	GL_DECR_WRAP // STENCIL_DECREASE_WRAP,
};

static const int GLFace[] = 
{
	GL_BACK,
	GL_FRONT
};

static const int GLWrapMode[] = 
{
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_CLAMP_TO_EDGE
};

static const int GLFilterMode[] = 
{
	GL_NEAREST,
	GL_LINEAR,
	GL_NEAREST_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_NEAREST_MIPMAP_LINEAR,
	GL_LINEAR_MIPMAP_LINEAR
};

OP_STATUS VEGAGlDevice::createRenderState(VEGA3dRenderState** state, bool copyCurrentState)
{
	if (copyCurrentState)
		*state = OP_NEW(VEGA3dRenderState, (renderState));
	else
		*state = OP_NEW(VEGA3dRenderState, ());
	if (!(*state))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS VEGAGlDevice::setRenderState(VEGA3dRenderState* state)
{
	// Only check for changes in state if we are setting a new
	// state or the state has changed
	if (state != clientRenderState || state->isChanged())
	{
		if (state->isBlendEnabled() && !renderState.isBlendEnabled())
		{
			glEnable(GL_BLEND);
			renderState.enableBlend(true);
		}
		else if (!state->isBlendEnabled() && renderState.isBlendEnabled())
		{
			glDisable(GL_BLEND);
			renderState.enableBlend(false);
		}

		if (state->isBlendEnabled() && state->blendChanged(renderState))
		{
			if (state->blendEquationChanged(renderState))
			{
				VEGA3dRenderState::BlendOp op, opA;

				state->getBlendEquation(op, opA);
				OP_ASSERT(op < VEGA3dRenderState::NUM_BLEND_OPS && opA < VEGA3dRenderState::NUM_BLEND_OPS);
				renderState.setBlendEquation(op, opA);
				if (op == opA)
				{
					glBlendEquation(GLBlendOp[op]);
				}
				else
				{
					glBlendEquationSeparate(GLBlendOp[op], GLBlendOp[opA]);
				}
			}
			if (state->blendFuncChanged(renderState))
			{
				VEGA3dRenderState::BlendWeight src, dst, srcA, dstA;
				state->getBlendFunc(src, dst, srcA, dstA);

				OP_ASSERT(src < VEGA3dRenderState::NUM_BLEND_WEIGHTS && dst < VEGA3dRenderState::NUM_BLEND_WEIGHTS &&
					srcA < VEGA3dRenderState::NUM_BLEND_WEIGHTS && dstA < VEGA3dRenderState::NUM_BLEND_WEIGHTS);
				renderState.setBlendFunc(src, dst, srcA, dstA);
				if (src == srcA && dst == dstA)
				{
					glBlendFunc(GLBlendWeight[src], GLBlendWeight[dst]);
				}
				else
				{
					glBlendFuncSeparate(GLBlendWeight[src], GLBlendWeight[dst], GLBlendWeight[srcA], GLBlendWeight[dstA]);
				}
			}
			if (state->blendColorChanged(renderState))
			{
				float col[4];
				state->getBlendColor(col[0], col[1], col[2], col[3]);
				renderState.setBlendColor(col[0], col[1], col[2], col[3]);
				glBlendColor(col[0], col[1], col[2], col[3]);
			}
		}

		if (state->colorMaskChanged(renderState))
		{
			bool r, g, b, a;
			state->getColorMask(r, g, b, a);
			renderState.setColorMask(r, g, b, a);
			glColorMask(r, g, b, a);
		}

		if (state->isCullFaceEnabled() && !renderState.isCullFaceEnabled())
		{
			glEnable(GL_CULL_FACE);
			renderState.enableCullFace(true);
		}
		else if (!state->isCullFaceEnabled() && renderState.isCullFaceEnabled())
		{
			glDisable(GL_CULL_FACE);
			renderState.enableCullFace(false);
		}

		if (state->cullFaceChanged(renderState))
		{
			VEGA3dRenderState::Face cf;
			state->getCullFace(cf);
			OP_ASSERT(cf < VEGA3dRenderState::NUM_FACES);
			glCullFace(GLFace[cf]);
			renderState.setCullFace(cf);
		}
		
		if (state->isDepthTestEnabled() && !renderState.isDepthTestEnabled())
		{
			glEnable(GL_DEPTH_TEST);
			renderState.enableDepthTest(true);
		}
		else if (!state->isDepthTestEnabled() && renderState.isDepthTestEnabled())
		{
			glDisable(GL_DEPTH_TEST);
			renderState.enableDepthTest(false);
		}
		if (state->depthFuncChanged(renderState))
		{
			VEGA3dRenderState::DepthFunc depth;
			state->getDepthFunc(depth);
			OP_ASSERT(depth < VEGA3dRenderState::NUM_DEPTH_FUNCS);
			renderState.setDepthFunc(depth);
			glDepthFunc(GLDepthFunc[depth]);
		}
		if (state->isDepthWriteEnabled() != renderState.isDepthWriteEnabled())
		{
			glDepthMask(state->isDepthWriteEnabled()?GL_TRUE:GL_FALSE);
			renderState.enableDepthWrite(state->isDepthWriteEnabled());
		}

		if (state->isFrontFaceCCW() != renderState.isFrontFaceCCW())
		{
			glFrontFace(state->isFrontFaceCCW()?GL_CCW:GL_CW);
			renderState.setFrontFaceCCW(state->isFrontFaceCCW());
		}

		if (state->isStencilEnabled() && !renderState.isStencilEnabled())
		{
			glEnable(GL_STENCIL_TEST);
			renderState.enableStencil(true);
		}
		else if (!state->isStencilEnabled() && renderState.isStencilEnabled())
		{
			glDisable(GL_STENCIL_TEST);
			renderState.enableStencil(false);
		}
		if (state->stencilChanged(renderState))
		{
			VEGA3dRenderState::StencilFunc test, testBack;
			unsigned int reference, mask, writeMask;
			VEGA3dRenderState::StencilOp fail, zFail, pass;
			VEGA3dRenderState::StencilOp failBack, zFailBack, passBack;
			state->getStencilFunc(test, testBack, reference, mask);
			state->getStencilWriteMask(writeMask);
			state->getStencilOp(fail, zFail, pass, failBack, zFailBack, passBack);

			OP_ASSERT(test < VEGA3dRenderState::NUM_STENCIL_FUNCS && 
					  fail < VEGA3dRenderState::NUM_STENCIL_OPS && 
					  zFail < VEGA3dRenderState::NUM_STENCIL_OPS && 
					  pass < VEGA3dRenderState::NUM_STENCIL_OPS &&
					  testBack < VEGA3dRenderState::NUM_STENCIL_FUNCS && 
					  failBack < VEGA3dRenderState::NUM_STENCIL_OPS && 
					  zFailBack < VEGA3dRenderState::NUM_STENCIL_OPS && 
					  passBack < VEGA3dRenderState::NUM_STENCIL_OPS);
			renderState.setStencilFunc(test, testBack, reference, mask);
			renderState.setStencilWriteMask(writeMask);
			renderState.setStencilOp(fail, zFail, pass, failBack, zFailBack, passBack);
			if (test == testBack)
			{
				glStencilFunc(GLStencilFunc[test], reference, mask);
			}
			else
			{
				glStencilFuncSeparate(GL_FRONT, GLStencilFunc[test], reference, mask);
				glStencilFuncSeparate(GL_BACK, GLStencilFunc[testBack], reference, mask);
			}
			glStencilMask(writeMask);
			if (fail == failBack && zFail == zFailBack && pass == passBack)
			{
				glStencilOp(GLStencilOp[fail], GLStencilOp[zFail], GLStencilOp[pass]);
			}
			else
			{
				glStencilOpSeparate(GL_FRONT, GLStencilOp[fail], GLStencilOp[zFail], GLStencilOp[pass]);
				glStencilOpSeparate(GL_BACK, GLStencilOp[failBack], GLStencilOp[zFailBack], GLStencilOp[passBack]);
			}
		}

		if (state->isScissorEnabled() && !renderState.isScissorEnabled())
		{
			glEnable(GL_SCISSOR_TEST);
			renderState.enableScissor(true);
		}
		else if (!state->isScissorEnabled() && renderState.isScissorEnabled())
		{
			glDisable(GL_SCISSOR_TEST);
			renderState.enableScissor(false);
		}

		if (state->isDitherEnabled() && !renderState.isDitherEnabled())
		{
			glEnable(GL_DITHER);
			renderState.enableDither(true);
		}
		else if (!state->isDitherEnabled() && renderState.isDitherEnabled())
		{
			glDisable(GL_DITHER);
			renderState.enableDither(false);
		}

		if (state->isPolygonOffsetFillEnabled() && !renderState.isPolygonOffsetFillEnabled())
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
			renderState.enablePolygonOffsetFill(true);
		}
		else if (!state->isPolygonOffsetFillEnabled() && renderState.isPolygonOffsetFillEnabled())
		{
			glDisable(GL_POLYGON_OFFSET_FILL);
			renderState.enablePolygonOffsetFill(false);
		}

		if (state->polygonOffsetChanged(renderState))
		{
			float offsetFactor = state->getPolygonOffsetFactor();
			float offsetUnits = state->getPolygonOffsetUnits();
			glPolygonOffset(offsetFactor, offsetUnits);
			renderState.polygonOffset(offsetFactor, offsetUnits);
		}

		state->clearChanged();
		clientRenderState = state;
	}

	return OpStatus::OK;
}

void VEGAGlDevice::setScissor(int x, int y, unsigned int w, unsigned int h)
{
#ifdef VEGA_BACKENDS_OPENGL_FLIPY
	m_backupScissorY = y;
	if (m_flipY)
	{
		y = renderTarget->getHeight() - (y + h);
	}
#endif //VEGA_BACKENDS_OPENGL_FLIPY

	if (x != m_scissorX || y != m_scissorY || w != m_scissorW || h != m_scissorH)
	{
		glScissor(x, y, w, h);
		m_scissorX = x;
		m_scissorY = y;
		m_scissorW = w;
		m_scissorH = h;
	}
}

void VEGAGlDevice::applyRenderState()
{
	if (renderState.isBlendEnabled())
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}

	VEGA3dRenderState::BlendOp op, opA;
	renderState.getBlendEquation(op, opA);
	OP_ASSERT(op < VEGA3dRenderState::NUM_BLEND_OPS && opA < VEGA3dRenderState::NUM_BLEND_OPS);
	if (op == opA)
	{
		glBlendEquation(GLBlendOp[op]);
	}
	else
	{
		glBlendEquationSeparate(GLBlendOp[op], GLBlendOp[opA]);
	}

	VEGA3dRenderState::BlendWeight src, dst, srcA, dstA;
	renderState.getBlendFunc(src, dst, srcA, dstA);
	OP_ASSERT(src < VEGA3dRenderState::NUM_BLEND_WEIGHTS && dst < VEGA3dRenderState::NUM_BLEND_WEIGHTS &&
		srcA < VEGA3dRenderState::NUM_BLEND_WEIGHTS && dstA < VEGA3dRenderState::NUM_BLEND_WEIGHTS);
	if (src == srcA && dst == dstA)
	{
		glBlendFunc(GLBlendWeight[src], GLBlendWeight[dst]);
	}
	else
	{
		glBlendFuncSeparate(GLBlendWeight[src], GLBlendWeight[dst], GLBlendWeight[srcA], GLBlendWeight[dstA]);
	}

	float col[4];
	renderState.getBlendColor(col[0], col[1], col[2], col[3]);
	glBlendColor(col[0], col[1], col[2], col[3]);

	bool r, g, b, a;
	renderState.getColorMask(r, g, b, a);
	glColorMask(r, g, b, a);

	if (renderState.isCullFaceEnabled())
	{
		glEnable(GL_CULL_FACE);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}

	if (renderState.isDitherEnabled())
	{
		glEnable(GL_DITHER);
	}
	else
	{
		glDisable(GL_DITHER);
	}

	VEGA3dRenderState::Face cf;
	renderState.getCullFace(cf);
	OP_ASSERT(cf < VEGA3dRenderState::NUM_FACES);
	glCullFace(GLFace[cf]);

	if (renderState.isDepthTestEnabled())
	{
		glEnable(GL_DEPTH_TEST);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);
	}

	VEGA3dRenderState::DepthFunc dt;
	renderState.getDepthFunc(dt);
	OP_ASSERT(dt < VEGA3dRenderState::NUM_DEPTH_FUNCS);
	glDepthFunc(GLDepthFunc[dt]);

	glDepthMask(renderState.isDepthWriteEnabled()?GL_TRUE:GL_FALSE);

	glFrontFace(renderState.isFrontFaceCCW()?GL_CCW:GL_CW);

	if (renderState.isStencilEnabled())
	{
		glEnable(GL_STENCIL_TEST);
	}
	else if (!renderState.isStencilEnabled())
	{
		glDisable(GL_STENCIL_TEST);
	}

	VEGA3dRenderState::StencilFunc test, testBack;
	unsigned int reference, mask, writeMask;
	VEGA3dRenderState::StencilOp fail, zFail, pass;
	VEGA3dRenderState::StencilOp failBack, zFailBack, passBack;
	renderState.getStencilFunc(test, testBack, reference, mask);
	renderState.getStencilWriteMask(writeMask);
	renderState.getStencilOp(fail, zFail, pass, failBack, zFailBack, passBack);
	OP_ASSERT(test < VEGA3dRenderState::NUM_STENCIL_FUNCS && 
			  fail < VEGA3dRenderState::NUM_STENCIL_OPS && 
			  zFail < VEGA3dRenderState::NUM_STENCIL_OPS && 
			  pass < VEGA3dRenderState::NUM_STENCIL_OPS &&
			  testBack < VEGA3dRenderState::NUM_STENCIL_FUNCS && 
			  failBack < VEGA3dRenderState::NUM_STENCIL_OPS && 
			  zFailBack < VEGA3dRenderState::NUM_STENCIL_OPS && 
			  passBack < VEGA3dRenderState::NUM_STENCIL_OPS);
	if (test == testBack)
	{
		glStencilFunc(GLStencilFunc[test], reference, mask);
	}
	else
	{
		glStencilFuncSeparate(GL_FRONT, GLStencilFunc[test], reference, mask);
		glStencilFuncSeparate(GL_BACK, GLStencilFunc[testBack], reference, mask);
	}
	glStencilMask(writeMask);
	if (fail == failBack && zFail == zFailBack && pass == passBack)
	{
		glStencilOp(GLStencilOp[fail], GLStencilOp[zFail], GLStencilOp[pass]);
	}
	else
	{
		glStencilOpSeparate(GL_FRONT, GLStencilOp[fail], GLStencilOp[zFail], GLStencilOp[pass]);
		glStencilOpSeparate(GL_BACK, GLStencilOp[failBack], GLStencilOp[zFailBack], GLStencilOp[passBack]);
	}

	if (renderState.isScissorEnabled())
	{
		glEnable(GL_SCISSOR_TEST);
	}
	else
	{
		glDisable(GL_SCISSOR_TEST);
	}
	glScissor(m_scissorX, m_scissorY, m_scissorW, m_scissorH);

	if (renderState.isPolygonOffsetFillEnabled())
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
	}
	else
	{
		glDisable(GL_POLYGON_OFFSET_FILL);
	}
	glPolygonOffset(renderState.getPolygonOffsetFactor(), renderState.getPolygonOffsetUnits());
}


void VEGAGlDevice::clear(bool color, bool depth, bool stencil, UINT32 colorVal, float depthVal, int stencilVal)
{
	unsigned int mask = 0;
	if (color)
	{
		if (colorVal != m_clearColor)
		{
			glClearColor(((colorVal>>16)&0xff)/255.f, 
						((colorVal>>8)&0xff)/255.f, 
						(colorVal&0xff)/255.f, 
						(colorVal>>24)/255.f);
			m_clearColor = colorVal;
		}
		mask |= GL_COLOR_BUFFER_BIT;
	}
	if (depth)
	{
		if (depthVal != m_clearDepth)
		{
			glClearDepthf(depthVal);
			m_clearDepth = depthVal;
		}
		mask |= GL_DEPTH_BUFFER_BIT;
	}
	if (stencil)
	{
		if (stencilVal != m_clearStencil)
		{
			glClearStencil(stencilVal);
			m_clearStencil = stencilVal;
		}
		mask |= GL_STENCIL_BUFFER_BIT;
	}
	glClear(mask);
}


void VEGAGlDevice::clearActiveTexture(VEGA3dTexture* tex)
{
	for (unsigned int i = 0; i < getMaxCombinedTextureUnits(); ++i)
	{
		if (activeTextures[i] == tex)
			setTexture(i, NULL);
		if (activeCubeTextures[i] == tex)
			setCubeTexture(i, NULL);
	}
}

bool VEGAGlDevice::hasExtendedBlend()
{
#ifdef VEGA_OPENGLES
	return false;
#else
	return m_glapi->m_BindFragDataLocationIndexed != NULL;
#endif
}

void VEGAGlDevice::setTexture(unsigned int unit, VEGA3dTexture* tex)
{
	OP_ASSERT(unit < getMaxCombinedTextureUnits());
	if (activeTextureNum != unit)
	{
		glActiveTexture(GL_TEXTURE0+unit);
		activeTextureNum = unit;
	}
	if (tex != activeTextures[unit])
	{
		glBindTexture(GL_TEXTURE_2D, tex?static_cast<VEGAGlTexture*>(tex)->getTextureId():0);
		activeTextures[unit] = tex;
	}

#ifdef DEBUG_ENABLE_OPASSERT
	// check that state caching works
	GLint tmp;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &tmp);
	OP_ASSERT(tmp == static_cast<GLint>(GL_TEXTURE0+unit));
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &tmp);
	OP_ASSERT(tmp == static_cast<GLint>((tex?static_cast<VEGAGlTexture*>(tex)->getTextureId():0)));
#endif // DEBUG_ENABLE_OPASSERT

	if (tex)
	{
		VEGAGlTexture* glTex = static_cast<VEGAGlTexture*>(tex);

		if (glTex->isWrapChanged())
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLWrapMode[glTex->getWrapModeS()]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLWrapMode[glTex->getWrapModeT()]);
		}
		if (glTex->isFilterChanged())
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLFilterMode[glTex->getMinFilter()]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLFilterMode[glTex->getMagFilter()]);
		}

#ifdef DEBUG_ENABLE_OPASSERT
		// check that state caching works
		GLint tmp;
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &tmp);
		OP_ASSERT(tmp == GLWrapMode[glTex->getWrapModeS()]);
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &tmp);
		OP_ASSERT(tmp == GLWrapMode[glTex->getWrapModeT()]);
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &tmp);
		OP_ASSERT(tmp == GLFilterMode[glTex->getMinFilter()]);
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &tmp);
		OP_ASSERT(tmp == GLFilterMode[glTex->getMagFilter()]);
#endif // DEBUG_ENABLE_OPASSERT
	}
}

void VEGAGlDevice::setCubeTexture(unsigned int unit, VEGA3dTexture* tex)
{
	OP_ASSERT(unit < getMaxCombinedTextureUnits());
	if (activeTextureNum != unit)
	{
		glActiveTexture(GL_TEXTURE0+unit);
		activeTextureNum = unit;
	}
	if (tex != activeCubeTextures[unit])
	{
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex?static_cast<VEGAGlTexture*>(tex)->getTextureId():0);
		activeCubeTextures[unit] = tex;
	}

#ifdef DEBUG_ENABLE_OPASSERT
	// check that state caching works
	GLint tmp;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &tmp);
	OP_ASSERT(tmp == static_cast<GLint>(GL_TEXTURE0+unit));
	glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &tmp);
	OP_ASSERT(tmp == static_cast<GLint>((tex?static_cast<VEGAGlTexture*>(tex)->getTextureId():0)));
#endif // DEBUG_ENABLE_OPASSERT

	if (tex)
	{
		VEGAGlTexture* glTex = static_cast<VEGAGlTexture*>(tex);

		if (glTex->isWrapChanged())
		{
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GLWrapMode[glTex->getWrapModeS()]);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GLWrapMode[glTex->getWrapModeT()]);
		}
		if (glTex->isFilterChanged())
		{
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GLFilterMode[glTex->getMinFilter()]);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GLFilterMode[glTex->getMagFilter()]);
		}

#ifdef DEBUG_ENABLE_OPASSERT
		// check that state caching works
		GLint tmp;
		glGetTexParameteriv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, &tmp);
		OP_ASSERT(tmp == GLWrapMode[glTex->getWrapModeS()]);
		glGetTexParameteriv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, &tmp);
		OP_ASSERT(tmp == GLWrapMode[glTex->getWrapModeT()]);
		glGetTexParameteriv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, &tmp);
		OP_ASSERT(tmp == GLFilterMode[glTex->getMinFilter()]);
		glGetTexParameteriv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, &tmp);
		OP_ASSERT(tmp == GLFilterMode[glTex->getMagFilter()]);
#endif // DEBUG_ENABLE_OPASSERT
	}
}

void VEGAGlDevice::bindBuffer(bool indexBuffer, UINT32 buffer)
{
	size_t idx;
	GLenum tgt;
	if (indexBuffer)
	{
		idx = 1;
		tgt = GL_ELEMENT_ARRAY_BUFFER;
	}
	else
	{
		idx = 0;
		tgt = GL_ARRAY_BUFFER;
	}

	if (stateVariables.boundBuffer[idx] != buffer)
	{
		glBindBuffer(tgt, buffer);
		stateVariables.boundBuffer[idx] = buffer;
	}
}

void VEGAGlDevice::deleteBuffers(bool indexBuffers, unsigned int n, const UINT32* buffers)
{
	size_t idx = indexBuffers ? 1 : 0;

	for (unsigned int i = 0; i < n; i++)
	{
		if (stateVariables.boundBuffer[idx] == buffers[i])
		{
			bindBuffer(indexBuffers, 0);
			break;
		}
	}

	glDeleteBuffers(n, buffers);
}

void VEGAGlDevice::initGlExtensions()
{
#ifdef VEGA_OPENGLES
	const char* extStr = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
#endif

	// GL_IMG_read_format (NOTE: We would like to use GL_EXT_read_format_bgra instead,
	// but GL_IMG_read_format is supported on a wider range of devices, and is a
	// subset of GL_EXT_read_format_bgra).
#ifndef VEGA_OPENGLES
	m_extensions.IMG_read_format = true;
#else
	m_extensions.IMG_read_format = extStr && (op_strstr(extStr, "GL_IMG_read_format") ||
											  op_strstr(extStr, "GL_EXT_read_format_bgra"));
#endif

	// GL_EXT_texture_format_BGRA8888 (same as GL_IMG/APPLE_texture_format_BGRA8888).
#ifndef VEGA_OPENGLES
	m_extensions.EXT_texture_format_BGRA8888 = true;
#else
	m_extensions.EXT_texture_format_BGRA8888 = extStr && (op_strstr(extStr, "GL_EXT_texture_format_BGRA8888") ||
														  op_strstr(extStr, "GL_APPLE_texture_format_BGRA8888") ||
														  op_strstr(extStr, "GL_IMG_texture_format_BGRA8888"));
#endif

	// GL_OES_packed_depth_stencil
#ifndef VEGA_OPENGLES
	m_extensions.OES_packed_depth_stencil = true;
#else
	m_extensions.OES_packed_depth_stencil = extStr && (op_strstr(extStr, "GL_OES_packed_depth_stencil"));
#endif
}

void VEGAGlDevice::initGlStateVariables()
{
	// buggy driver may not set the value when oom, this value
	// is used to iterate array so must initialize the variable
	GLint param = 0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &param);
	stateVariables.maxFragmentTextureUnits = param;

	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &param);
	stateVariables.maxVertexAttribs = param;

	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &param);
	stateVariables.maxCombinedTextureUnits = param;

	glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &param);
	stateVariables.maxCubeMapTextureSize = param;

#ifdef VEGA_OPENGLES
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &param);
#else // VEGA_OPENGLES
	glGetIntegerv(non_gles_GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &param);
	param /= 4;
#endif // VEGA_OPENGLES
	stateVariables.maxFragmentUniformVectors = param;

	glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &param);
	stateVariables.maxRenderBufferSize = param;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &param);
	stateVariables.maxTextureSize = param;

	stateVariables.maxMipLevel = VEGA3dDevice::ilog2_floor(stateVariables.maxTextureSize);

#ifdef VEGA_OPENGLES
	glGetIntegerv(GL_MAX_VARYING_VECTORS, &param);
#else // VEGA_OPENGLES
	glGetIntegerv(non_gles_GL_MAX_VARYING_FLOATS, &param);
	param /= 4;
#endif // VEGA_OPENGLES
	stateVariables.maxVaryingVectors = param;

	glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &param);
	stateVariables.maxVertexTextureUnits = param;

#ifdef VEGA_OPENGLES
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &param);
#else // VEGA_OPENGLES
	glGetIntegerv(non_gles_GL_MAX_VERTEX_UNIFORM_COMPONENTS, &param);
	param /= 4;
#endif // VEGA_OPENGLES
	stateVariables.maxVertexUniformVectors = param;

	glGetIntegerv(GL_SUBPIXEL_BITS, &param);
	stateVariables.subpixelBits = param;

	GLfloat range[2];
	glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
	stateVariables.aliasedPointSizeRange[0] = range[0];
	stateVariables.aliasedPointSizeRange[1] = range[1];

	glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);
	stateVariables.aliasedLineWidthRange[0] = range[0];
	stateVariables.aliasedLineWidthRange[1] = range[1];

	GLint dim[2];
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dim);
	stateVariables.maxViewportDims[0] = dim[0];
	stateVariables.maxViewportDims[1] = dim[1];

	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, dim);
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, dim+1);
	stateVariables.boundBuffer[0] = dim[0];
	stateVariables.boundBuffer[1] = dim[1];
}

unsigned int VEGAGlDevice::getMaxVertexAttribs() const
{
	return stateVariables.maxVertexAttribs;
}

void VEGAGlDevice::getAliasedPointSizeRange(float &low, float &high)
{
	low = stateVariables.aliasedPointSizeRange[0];
	high = stateVariables.aliasedPointSizeRange[1];
}

void VEGAGlDevice::getAliasedLineWidthRange(float &low, float &high)
{
	low = stateVariables.aliasedLineWidthRange[0];
	high = stateVariables.aliasedLineWidthRange[1];
}

unsigned int VEGAGlDevice::getMaxCubeMapTextureSize() const
{
	return stateVariables.maxCubeMapTextureSize;
}

unsigned int VEGAGlDevice::getMaxFragmentUniformVectors() const
{
	return stateVariables.maxFragmentUniformVectors;
}

unsigned int VEGAGlDevice::getMaxRenderBufferSize() const
{
	return stateVariables.maxRenderBufferSize;
}

unsigned int VEGAGlDevice::getMaxTextureSize() const
{
	return stateVariables.maxTextureSize;
}

unsigned int VEGAGlDevice::getMaxMipLevel() const
{
	return stateVariables.maxMipLevel;
}

unsigned int VEGAGlDevice::getMaxVaryingVectors() const
{
	return stateVariables.maxVaryingVectors;
}

unsigned int VEGAGlDevice::getMaxVertexTextureUnits() const
{
	return stateVariables.maxVertexTextureUnits;
}

unsigned int VEGAGlDevice::getMaxFragmentTextureUnits() const
{
	return stateVariables.maxFragmentTextureUnits;
}

unsigned int VEGAGlDevice::getMaxCombinedTextureUnits() const
{
	return stateVariables.maxCombinedTextureUnits;
}

unsigned int VEGAGlDevice::getMaxVertexUniformVectors() const
{
	return stateVariables.maxVertexUniformVectors;
}

void VEGAGlDevice::getMaxViewportDims(int &w, int &h) const
{
	w = stateVariables.maxViewportDims[0];
	h = stateVariables.maxViewportDims[1];
}

unsigned int VEGAGlDevice::getMaxQuality() const
{
	return m_maxMSAA;
}

unsigned int VEGAGlDevice::getSubpixelBits() const
{
	return stateVariables.subpixelBits;
}

void VEGAGlDevice::getRenderbufferParameters(int format, VEGAGlRenderbufferParameters &params)
{
	if (format != renderbufferParameters.format)
	{
		renderbufferParameters.format = format;
		int bits;
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE, &bits);
		renderbufferParameters.redBits = (unsigned char)bits;
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_GREEN_SIZE, &bits);
		renderbufferParameters.greenBits = (unsigned char)bits;
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_BLUE_SIZE, &bits);
		renderbufferParameters.blueBits = (unsigned char)bits;
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE, &bits);
		renderbufferParameters.alphaBits = (unsigned char)bits;
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &bits);
		renderbufferParameters.depthBits = (unsigned char)bits;
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_STENCIL_SIZE, &bits);
		renderbufferParameters.stencilBits = (unsigned char)bits;
	}

	params.redBits = renderbufferParameters.redBits;
	params.greenBits = renderbufferParameters.greenBits;
	params.blueBits = renderbufferParameters.blueBits;
	params.alphaBits = renderbufferParameters.alphaBits;
	params.depthBits = renderbufferParameters.depthBits;
	params.stencilBits = renderbufferParameters.stencilBits;
}

#ifdef VEGA_OPENGLES
bool VEGAGlDevice::checkHighPrecisionFragmentSupport() const
{
	// Compile a simple fragment shader with high precision and check if the compile fails.
	GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
	const char *fsource = "precision highp float; void main() { gl_FragColor = vec4(1.0,1.0,1.0,1.0); }";
	glShaderSource(fshader, 1, &fsource, NULL);
	glCompileShader(fshader);
	GLint status;
	glGetShaderiv(fshader, GL_COMPILE_STATUS, &status);
	if (status == GL_TRUE)
	{
		// Even if it succeeds it's valid for a ES2 implementation to defer compile
		// errors until link time so we need to compile a vertex shader and try to
		// link the program to be sure.
		GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
		const char *vsource = "void main() { gl_Position = vec4(0,0,0,1); }";
		glShaderSource(vshader, 1, &vsource, NULL);
		glCompileShader(vshader);
		GLuint program = glCreateProgram();
		glAttachShader(program, vshader);
		glAttachShader(program, fshader);
		glLinkProgram(program);
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		glDeleteShader(vshader);
		glDeleteProgram(program);
	}
	glDeleteShader(fshader);
	return status == GL_TRUE;
}
#endif //VEGA_OPENGLES

VEGA3dShaderProgram::ShaderLanguage VEGAGlDevice::getShaderLanguage(unsigned &version)
{
#ifdef VEGA_OPENGLES
	version = 100;
	return VEGA3dShaderProgram::SHADER_GLSL_ES;
#else
	version = 120;
	return VEGA3dShaderProgram::SHADER_GLSL;
#endif
}

bool VEGAGlDevice::supportsExtension(VEGA3dDevice::Extension e)
{
	if (m_cachedExtensions & (1 << (MAX_EXTENSIONS + e)))
		return (m_cachedExtensions & (1 << e)) != 0;

	bool res = false;
	switch (e)
	{
	case VEGA3dDevice::OES_STANDARD_DERIVATIVES:
#ifndef VEGA_OPENGLES
		// Standard derivatives are not optional on desktop gl, so it is always supported
		res = true;
#else
	{
		const char * extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
		res = extensions && op_strstr(extensions, "GL_OES_standard_derivatives");
	}
#endif
		break;
	case VEGA3dDevice::OES_TEXTURE_NPOT:
#ifndef VEGA_OPENGLES
		// full non-power-of-two support is not optional on desktop gl, so it is always supported
		res = true;
#else
	{
		const char * extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
		res = extensions && op_strstr(extensions, "GL_OES_texture_npot");
	}
#endif
		break;
	}

	m_cachedExtensions |= (1 << (MAX_EXTENSIONS+e));
	if (res)
		m_cachedExtensions |= (1 << e);
	return res;
}

static const int GLPrimitive[] = 
{
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_POINTS,
	GL_LINES,
	GL_LINE_LOOP,
	GL_LINE_STRIP,
	GL_TRIANGLE_FAN
};

void VEGAGlDevice::applyWrapHack()
{
	// wrap workaround is only needed for GPUs without full npot support
	if (!fullNPotSupport() &&
	    currentShaderProgram &&
	    currentShaderProgram->getShaderType() != VEGA3dShaderProgram::SHADER_CUSTOM)
	{
		unsigned int active = activeTextureNum;
		for (unsigned int i = 0; i < 3; ++i) // number of textures used by our shaders
		{
			if (activeTextures[i] &&
			    (activeTextures[i]->getWrapModeS() != VEGA3dTexture::WRAP_CLAMP_EDGE ||
			     activeTextures[i]->getWrapModeT() != VEGA3dTexture::WRAP_CLAMP_EDGE))
			{
				if (active != i)
				{
					glActiveTexture(GL_TEXTURE0+i);
					active = i;
				}
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLWrapMode[VEGA3dTexture::WRAP_CLAMP_EDGE]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLWrapMode[VEGA3dTexture::WRAP_CLAMP_EDGE]);

				// make sure wrong wrap mode isn't cached
				activeTextures[i]->setWrapMode(VEGA3dTexture::WRAP_CLAMP_EDGE, VEGA3dTexture::WRAP_CLAMP_EDGE);
				((VEGAGlTexture*)activeTextures[i])->isWrapChanged();
			}
		}
		if (active != activeTextureNum)
			glActiveTexture(GL_TEXTURE0+activeTextureNum);
	}
}

OP_STATUS VEGAGlDevice::drawPrimitives(Primitive type, VEGA3dVertexLayout* layout, unsigned int firstVert, unsigned int numVerts)
{
	applyWrapHack();

	if (layout != currentLayout)
	{
		if (currentLayout)
		{
			static_cast<VEGAGlVertexLayout*>(currentLayout)->unbind();
			VEGARefCount::DecRef(currentLayout);
		}
		static_cast<VEGAGlVertexLayout*>(layout)->bind();
		VEGARefCount::IncRef(layout);
		currentLayout = layout;
	}

#ifndef VEGA_OPENGLES
	// POINT_SPRITE is required in order to handle gl_PointCoord properly, 
	// but some amd drivers crash if it is enabled when drawing something other than points
	if (type == PRIMITIVE_POINT)
		glEnable(non_gles_GL_POINT_SPRITE);
#endif //VEGA_OPENGLES

	OP_ASSERT(type < NUM_PRIMITIVES);
	glDrawArrays(GLPrimitive[type], firstVert, numVerts);

#ifndef VEGA_OPENGLES
	// POINT_SPRITE is required in order to handle gl_PointCoord properly, 
	// but some amd drivers crash if it is enabled when drawing something other than points
	if (type == PRIMITIVE_POINT)
		glDisable(non_gles_GL_POINT_SPRITE);
#endif //VEGA_OPENGLES

	return OpStatus::OK;
}

OP_STATUS VEGAGlDevice::drawIndexedPrimitives(Primitive type, VEGA3dVertexLayout* verts, VEGA3dBuffer* ibuffer, unsigned int bytesPerInd, unsigned int firstInd, unsigned int numInd)
{
	applyWrapHack();

	if (verts != currentLayout)
	{
		if (currentLayout)
		{
			static_cast<VEGAGlVertexLayout*>(currentLayout)->unbind();
			VEGARefCount::DecRef(currentLayout);
		}
		static_cast<VEGAGlVertexLayout*>(verts)->bind();
		VEGARefCount::IncRef(verts);
		currentLayout = verts;
	}
	static_cast<VEGAGlBuffer*>(ibuffer)->bind();

#ifndef VEGA_OPENGLES
	// POINT_SPRITE is required in order to handle gl_PointCoord properly, 
	// but some amd drivers crash if it is enabled when drawing something other than points
	if (type == PRIMITIVE_POINT)
		glEnable(non_gles_GL_POINT_SPRITE);
#endif //VEGA_OPENGLES

	OP_ASSERT(type < NUM_PRIMITIVES);
	GLenum itype = GL_UNSIGNED_BYTE;
	if (bytesPerInd == 2)
		itype = GL_UNSIGNED_SHORT;
	glDrawElements(GLPrimitive[type], numInd, itype, INT_TO_PTR(firstInd*bytesPerInd));

#ifndef VEGA_OPENGLES
	// POINT_SPRITE is required in order to handle gl_PointCoord properly, 
	// but some amd drivers crash if it is enabled when drawing something other than points
	if (type == PRIMITIVE_POINT)
		glDisable(non_gles_GL_POINT_SPRITE);
#endif //VEGA_OPENGLES

	static_cast<VEGAGlBuffer*>(ibuffer)->unbind();

	return OpStatus::OK;
}


OP_STATUS VEGAGlDevice::createTexture(VEGA3dTexture** texture, unsigned int width, unsigned int height,
									  VEGA3dTexture::ColorFormat fmt, bool mipmaps, bool as_rendertarget)
{
	if (width > this->getMaxTextureSize() || height > this->getMaxTextureSize())
		return OpStatus::ERR;

	VEGAGlTexture* tex = OP_NEW(VEGAGlTexture, (width, height, fmt));
	if (!tex)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err = tex->Construct(this);
	if (OpStatus::IsError(err))
	{
		VEGARefCount::DecRef(tex);
		return err;
	}
	tex->Into(&textures);

	*texture = tex;
	return OpStatus::OK;
}

OP_STATUS VEGAGlDevice::createCubeTexture(VEGA3dTexture** texture, unsigned int size,
										  VEGA3dTexture::ColorFormat fmt, bool mipmaps, bool as_rendertarget)
{
	VEGAGlTexture* tex = OP_NEW(VEGAGlTexture, (size, fmt));
	if (!tex)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err = tex->Construct(this);
	if (OpStatus::IsError(err))
	{
		VEGARefCount::DecRef(tex);
		return err;
	}
	tex->Into(&textures);

	*texture = tex;
	return OpStatus::OK;
}

OP_STATUS VEGAGlDevice::createFramebuffer(VEGA3dFramebufferObject** fbo)
{
	VEGAGlFramebufferObject* fb = OP_NEW(VEGAGlFramebufferObject, ());
	if (!fb)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS status = fb->Construct();
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(fb);
		return status;
	}
	fb->Into(&framebuffers);
	*fbo = fb;
	return OpStatus::OK;
}

OP_STATUS VEGAGlDevice::createRenderbuffer(VEGA3dRenderbufferObject** rbo, unsigned int width, unsigned int height, VEGA3dRenderbufferObject::ColorFormat fmt, int multisampleQuality)
{
	if (multisampleQuality > m_maxMSAA)
		multisampleQuality = m_maxMSAA;

	VEGAGlRenderbufferObject* rb = OP_NEW(VEGAGlRenderbufferObject, (width, height, fmt, multisampleQuality));
	if (!rb)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS status = rb->Construct();
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(rb);
		return status;
	}
	rb->Into(&renderbuffers);
	*rbo = rb;
	return OpStatus::OK;
}

OP_STATUS VEGAGlDevice::resolveMultisample(VEGA3dRenderTarget* rt, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
#ifdef VEGA_OPENGLES
	return OpStatus::ERR;
#else
	VEGAGlFramebufferObject* fbo = getFramebufferObject(rt);
	glBindFramebuffer(non_gles_GL_DRAW_FRAMEBUFFER, fbo->fbo);
	glBlitFramebuffer(x, y, x+w, y+h, x, y, x+w, y+h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	GLuint fboId = 0;
	if (renderTarget)
	{
		VEGAGlFramebufferObject* rtfbo = getFramebufferObject(renderTarget);
		fboId = rtfbo->fbo;
	}

	glBindFramebuffer(non_gles_GL_DRAW_FRAMEBUFFER, fboId);

	return OpStatus::OK;
#endif
}

OP_STATUS VEGAGlDevice::createBuffer(VEGA3dBuffer** buffer, unsigned int size, VEGA3dBuffer::Usage usage, bool indexBuffer, bool deferredUpdate)
{
	VEGAGlBuffer* buf = OP_NEW(VEGAGlBuffer, ());
	if (!buf)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err = buf->Construct(size, usage, indexBuffer);
	if (OpStatus::IsError(err))
	{
		VEGARefCount::DecRef(buf);
		return err;
	}

	buf->Into(&buffers);

	*buffer = buf;
	return OpStatus::OK;
}
OP_STATUS VEGAGlDevice::createVertexLayout(VEGA3dVertexLayout** layout, VEGA3dShaderProgram* sprog)
{
	VEGAGlVertexLayout* l = OP_NEW(VEGAGlVertexLayout, ());
	if (!l)
		return OpStatus::ERR_NO_MEMORY;
	*layout = l;
	return OpStatus::OK;
}

OP_STATUS VEGAGlDevice::setRenderTarget(VEGA3dRenderTarget* rt, bool changeViewport)
{
	if (renderTarget == rt)
		return OpStatus::OK;

	renderTarget = rt;

#ifdef VEGA_BACKENDS_OPENGL_FLIPY
	// Since only window rts use flipped y axis we need to reset the scissor
	// coordinates if we change between non-flipped and flipped rts.
	bool oldFlipY = m_flipY;
	m_flipY = renderTarget && renderTarget->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW;
	if (m_flipY != oldFlipY)
	{
		setScissor(m_scissorX, oldFlipY ? m_backupScissorY : m_scissorY, m_scissorW, m_scissorH);
	}
#endif // VEGA_BACKENDS_OPENGL_FLIPY

	VEGAGlFramebufferObject* fbo = NULL;
	if (renderTarget)
	{
		fbo = getFramebufferObject(renderTarget);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return OpStatus::OK;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);

	if (changeViewport)
		setViewport(0, 0, rt->getWidth(), rt->getHeight(), 0, 1.f);

	return OpStatus::OK;
}

void VEGAGlDevice::setViewport(int x, int y, int w, int h, float depthNear, float depthFar)
{
	if (x != m_viewportX || y != m_viewportY || w != m_viewportW || h != m_viewportH)
	{
		glViewport(x, y, w, h);
		m_viewportX = x;
		m_viewportY = y;
		m_viewportW = w;
		m_viewportH = h;
	}
	if (depthNear != m_depthNear || depthFar != m_depthFar)
	{
		glDepthRangef(depthNear, depthFar);
		m_depthNear = depthNear;
		m_depthFar = depthFar;
	}
}

void VEGAGlDevice::loadOrthogonalProjection(VEGATransform3d& transform)
{
	OP_ASSERT(renderTarget);
	loadOrthogonalProjection(transform, renderTarget->getWidth(), renderTarget->getHeight());
}

void VEGAGlDevice::loadOrthogonalProjection(VEGATransform3d& transform, unsigned int w, unsigned int h)
{
	VEGA_FIX left = 0;
	VEGA_FIX right = VEGA_INTTOFIX(w);
	VEGA_FIX top = VEGA_INTTOFIX(h);
	VEGA_FIX bottom = 0;
	VEGA_FIX znear = VEGA_INTTOFIX(-1);
	VEGA_FIX zfar = VEGA_INTTOFIX(1);

#ifdef VEGA_BACKENDS_OPENGL_FLIPY
	if (m_flipY)
	{
		top = 0;
		bottom = VEGA_INTTOFIX(h);
	}
	static_cast<VEGAGlShaderProgram*>(currentShaderProgram)->setFlipY(m_flipY);
#endif //VEGA_BACKENDS_OPENGL_FLIPY

	transform.loadIdentity();

	transform[0] = 2.f/(right-left); // 2/(r-l)
	transform[3] = (left+right)/(left-right); // (l+r)/(l-r)
	transform[5] = 2.f/(top-bottom); // 2/(t-b)
	transform[7] = (top+bottom)/(bottom-top); // (t+b)/(b-t)
	transform[10] = -2.f/(zfar-znear); // 1/(zf-zn) or 1/(zn-zf) for right handed
	transform[11] = (zfar+znear)/(zfar-znear); // zn/(zn-zf)
}

void VEGAGlDevice::getOptimalReadFormat(VEGAPixelStoreFormat* targetFormat, int* format, int* type,  int* bytesPerPixel)
{
	// Get the color format for the render target.
	OP_ASSERT(renderTarget);
	*targetFormat = VPSF_RGBA8888;
	VEGAGlFramebufferObject* fbo = getFramebufferObject(renderTarget);
	VEGA3dTexture* tex = fbo->getAttachedColorTexture();
	if (tex)
	{
		switch (tex->getFormat())
		{
		case VEGA3dTexture::FORMAT_RGB888:
			*targetFormat = VPSF_RGB888;
			break;
		case VEGA3dTexture::FORMAT_RGBA4444:
			*targetFormat = VPSF_ABGR4444;
			break;
		case VEGA3dTexture::FORMAT_RGBA5551:
			*targetFormat = VPSF_RGBA5551;
			break;
		case VEGA3dTexture::FORMAT_RGB565:
			*targetFormat = VPSF_RGB565;
			break;
		default:
		case VEGA3dTexture::FORMAT_RGBA8888:
			*targetFormat = VPSF_RGBA8888;
			break;
		}
	}
	else
	{
		VEGA3dRenderbufferObject* rb = fbo->getAttachedColorBuffer();
		if (rb)
		{
			switch (rb->getFormat())
			{
			case VEGA3dRenderbufferObject::FORMAT_RGBA4:
				*targetFormat = VPSF_ABGR4444;
				break;
			case VEGA3dRenderbufferObject::FORMAT_RGB5_A1:
				*targetFormat = VPSF_RGBA5551;
				break;
			case VEGA3dRenderbufferObject::FORMAT_RGB565:
				*targetFormat = VPSF_RGB565;
				break;
			default:
			case VEGA3dRenderbufferObject::FORMAT_RGBA8:
				*targetFormat = VPSF_RGBA8888;
				break;
			}
		}
	}

	// Choose an optimal format for reading the buffer.
	// Rationale: The transcoding overhead in the OpenGL driver can otherwise
	// be quite significant. For instance, here are some frame rate results for
	// a simple WebGL demo running on a Tegra 2 device:
	//
	//  Frame buffer format  | data->pixels.format  | Frames per second (FPS)
	// ----------------------+----------------------+------------------------
	//  RGB565               | RGB565               | 46
	//  RGBA8888             | RGBA8888             | 37
	//  RGB565               | RGBA8888             | 18 <- conversion hit
	//  RGB888               | RGBA8888             | 18 <- conversion hit
	// ----------------------+----------------------+------------------------
#ifdef VEGA_OPENGLES
	// Get implementation supported read format (except for this combination,
	// the only supported combination is GL_RGBA/GL_UNSIGNED_BYTE).
	glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, format);
	glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, type);

	// Select optimal & correct read back format.
	// Note: We only support formats that we can upload using glTexSubImage2D().
	if (m_extensions.IMG_read_format && m_extensions.EXT_texture_format_BGRA8888 && *format == GL_BGRA_IMG && *type == GL_UNSIGNED_BYTE)
	{
		*targetFormat = VPSF_BGRA8888;
		*bytesPerPixel = 4;
	}
	else if (*format == GL_RGB && *type == GL_UNSIGNED_BYTE && (*targetFormat == VPSF_RGB888 || *targetFormat == VPSF_RGB565))
	{
		*targetFormat = VPSF_RGB888;
		*bytesPerPixel = 3;
	}
	else if (*format == GL_RGBA && *type == GL_UNSIGNED_SHORT_4_4_4_4 && *targetFormat == VPSF_ABGR4444)
	{
		*targetFormat = VPSF_ABGR4444;
		*bytesPerPixel = 2;
	}
	else if (*format == GL_RGB && *type == GL_UNSIGNED_SHORT_5_6_5 && *targetFormat == VPSF_RGB565)
	{
		*targetFormat = VPSF_RGB565;
		*bytesPerPixel = 2;
	}
#else
	// Use the format that closest matches the render target.
	if (*targetFormat == VPSF_RGB888)
	{
		*format = GL_RGB;
		*type = GL_UNSIGNED_BYTE;
		*bytesPerPixel = 3;
	}
	else if (*targetFormat == VPSF_ABGR4444 && supportRGBA4_fbo)
	{
		*format = GL_RGBA;
		*type = GL_UNSIGNED_SHORT_4_4_4_4;
		*bytesPerPixel = 2;
	}
	else if (*targetFormat == VPSF_RGBA5551 && supportRGB5_A1_fbo)
	{
		*format = GL_RGBA;
		*type = GL_UNSIGNED_SHORT_5_5_5_1;
		*bytesPerPixel = 2;
	}
	else if (*targetFormat == VPSF_RGB565)
	{
		*format = GL_RGB;
		*type = GL_UNSIGNED_SHORT_5_6_5;
		*bytesPerPixel = 2;
	}
#endif
	else
	{
		// Fall back to RGBA8888, which is always supported by glReadPixels,
		// and prevents any information loss.
		// NOTE: This is especially important on OpenGL ES, which only supports
		// RGBA8888 and the format given by the implementation read format/type.
		*targetFormat = VPSF_RGBA8888;
		*format = GL_RGBA;
		*type = GL_UNSIGNED_BYTE;
		*bytesPerPixel = 4;
	}
}

OP_STATUS VEGAGlDevice::pixelStoreFormatToGLFormat(VEGAPixelStoreFormat storeFormat, int* format, int* type)
{
	switch (storeFormat)
	{
	case VPSF_RGBA8888:
		*format = GL_RGBA;
		*type = GL_UNSIGNED_BYTE;
		return OpStatus::OK;
	case VPSF_BGRA8888:
#ifndef VEGA_OPENGLES
		*format = non_gles_GL_BGRA;
		*type = GL_UNSIGNED_BYTE;
		return OpStatus::OK;
#else
		OP_ASSERT(m_extensions.EXT_texture_format_BGRA8888);
		*format = GL_BGRA_EXT;
		*type = GL_UNSIGNED_BYTE;
		return OpStatus::OK;
#endif
	case VPSF_RGB888:
		*format = GL_RGB;
		*type = GL_UNSIGNED_BYTE;
		return OpStatus::OK;
	case VPSF_ABGR4444:
		*format = GL_RGBA;
		*type = GL_UNSIGNED_SHORT_4_4_4_4;
		return OpStatus::OK;
	case VPSF_RGBA5551:
		*format = GL_RGBA;
		*type = GL_UNSIGNED_SHORT_5_5_5_1;
		return OpStatus::OK;
	case VPSF_RGB565:
		*format = GL_RGB;
		*type = GL_UNSIGNED_SHORT_5_6_5;
		return OpStatus::OK;
	default:
		return OpStatus::ERR;
	}
}

OP_STATUS VEGAGlDevice::lockRect(FramebufferData* data, bool readPixels)
{
#ifdef _DEBUG
	// There seems to have been an implicit assumption that
	// (UN)PACK_ALIGNMENT = 4 for pixel uploads/downloads. Make it explicit.
	int pack_alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment);
	OP_ASSERT(pack_alignment == 4);
#endif // _DEBUG

	if (!renderTarget)
		return OpStatus::ERR;

	// Get the optimal read back format for the currently bound render target.
	VEGAPixelStoreFormat targetFormat;
	GLint format, type;
	int bytesPerPixel;
	getOptimalReadFormat(&targetFormat, &format, &type, &bytesPerPixel);

	// Allocate memory for frame buffer read-back.
	data->pixels.width = data->w;
	data->pixels.height = data->h;
	// Round upwards to the nearest multiple of 4 (the assumed value of
	// PACK_ALIGNMENT).
	data->pixels.stride = (data->w*bytesPerPixel + 3) & ~3u;
	data->pixels.format = targetFormat;
	data->pixels.buffer = OP_NEWA(UINT8, data->h*data->pixels.stride);
	RETURN_OOM_IF_NULL(data->pixels.buffer);

	// Read back pixels.
	if (readPixels)
		glReadPixels(data->x, data->y, data->w, data->h, format, type, data->pixels.buffer);

	return OpStatus::OK;
}

void VEGAGlDevice::unlockRect(FramebufferData* data, bool modified)
{
#ifdef _DEBUG
	// There seems to have been an implicit assumption that
	// (UN)PACK_ALIGNMENT = 4 for pixel uploads/downloads. Make it explicit.
	int unpack_alignment;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
	OP_ASSERT(unpack_alignment == 4);
#endif // _DEBUG

	if (modified && renderTarget)
	{
		VEGAGlFramebufferObject* fbo = getFramebufferObject(renderTarget);
		// FIXME: not sure this works properly
		setRenderTarget(NULL);
		setTexture(0, fbo->getAttachedColorTexture());

		// Note: The pixel format should be set by lockRect (i.e. getOptimalReadFormat),
		// so we should always be able to find a valid format/type pair.
		GLint format, type;
		OP_STATUS validBufferFormat = pixelStoreFormatToGLFormat(data->pixels.format, &format, &type);
		OP_ASSERT(OpStatus::IsSuccess(validBufferFormat));
		OpStatus::Ignore(validBufferFormat);

		// FIXME: probably needs to set pixel stride etc
		glTexSubImage2D(GL_TEXTURE_2D, 0, data->x, data->y, data->w, data->h, format, type, data->pixels.buffer);
		setTexture(0, NULL);
		setRenderTarget(renderTarget);
	}
	UINT32* buffer = (UINT32*)data->pixels.buffer;
	OP_DELETEA(buffer);
}


OP_STATUS VEGAGlDevice::copyToTexture(VEGA3dTexture* tex, VEGA3dTexture::CubeSide side, int level, int texx, int texy,
									  int x, int y, unsigned int w, unsigned int h)
{
	if (!renderTarget)
		return OpStatus::ERR;

	if (renderState.isScissorEnabled())
		glDisable(GL_SCISSOR_TEST);

	static_cast<VEGAGlTexture*>(tex)->createTexture();
	// FIXME: stencil
	unsigned int target = GL_TEXTURE_2D;
	switch (side)
	{
	case VEGA3dTexture::CUBE_SIDE_NEG_X: target = GL_TEXTURE_CUBE_MAP_NEGATIVE_X; break;
	case VEGA3dTexture::CUBE_SIDE_POS_X: target = GL_TEXTURE_CUBE_MAP_POSITIVE_X; break;
	case VEGA3dTexture::CUBE_SIDE_NEG_Y: target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y; break;
	case VEGA3dTexture::CUBE_SIDE_POS_Y: target = GL_TEXTURE_CUBE_MAP_POSITIVE_Y; break;
	case VEGA3dTexture::CUBE_SIDE_NEG_Z: target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; break;
	case VEGA3dTexture::CUBE_SIDE_POS_Z: target = GL_TEXTURE_CUBE_MAP_POSITIVE_Z; break;
	default: break;
	};
	if (target != GL_TEXTURE_2D)
		setCubeTexture(0, static_cast<VEGA3dTexture*>(tex));
	else
		setTexture(0, static_cast<VEGA3dTexture*>(tex));

	if (activeTextureNum != 0)
	{
		glActiveTexture(GL_TEXTURE0);
		activeTextureNum = 0;
	}

	/* Some Linux NVIDIA drivers (at least) will sometimes generate a GL
	   error if glCopyTexSubImage2D() is called with either w = 0 or h = 0, or
	   both, even if all conditions from the spec are satisfied. As the call
	   will not have any effect in that case anyway, simply skip it. */
	if (w != 0 && h != 0)
		glCopyTexSubImage2D(target, level, texx, texy, x, y, w, h);

	if (renderState.isScissorEnabled())
		glEnable(GL_SCISSOR_TEST);

	return OpStatus::OK;
}

#ifdef VEGA_ENABLE_PERF_EVENTS
void VEGAGlDevice::SetPerfMarker(const uni_char *marker)
{
	if (m_glapi->m_StringMarkerGREMEDY)
	{
		if (char *asciiMarker = uni_down_strdup(marker))
		{
			glStringMarkerGREMEDY(0, asciiMarker);
			op_free(asciiMarker);
		}
	}
}

void VEGAGlDevice::BeginPerfEvent(const uni_char *marker)
{
	if (m_glapi->m_StringMarkerGREMEDY)
	{
		if (char *asciiMarker = uni_down_strdup(marker))
		{
			glStringMarkerGREMEDY(0, asciiMarker);
			op_free(asciiMarker);
		}
	}
}

void  VEGAGlDevice::EndPerfEvent()
{
	if (m_glapi->m_StringMarkerGREMEDY)
		glStringMarkerGREMEDY(0, "end");
}

#endif //VEGA_ENABLE_PERF_EVENTS

#ifdef VEGA_BACKENDS_USE_BLOCKLIST
void VEGAGlDevice::updateQuirks(const OpStringHashTable<uni_char> &entries)
{
	struct QuirkEntry
	{
		const enum Quirk quirk;
		const uni_char* quirkString;
	};

	const struct QuirkEntry quirkEntries[] = {
		{ QUIRK_DONT_DETACH_SHADERS, UNI_L("quirks.keepshadersworkaround") }
	};

	for (unsigned int i = 0; i < ARRAY_SIZE(quirkEntries); i++)
	{
		if (entries.Contains(quirkEntries[i].quirkString))
		{
			const enum Quirk quirk = quirkEntries[i].quirk;
			m_quirks[quirk / 32] |= (1 << (quirk % 32));
		}
	}
}
#endif // VEGA_BACKENDS_USE_BLOCKLIST

#endif // VEGA_BACKEND_OPENGL
