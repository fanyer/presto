/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT

#include "modules/libvega/src/canvas/canvaswebglshader.h"
#include "modules/webgl/src/wgl_validator.h"
#include "modules/dom/src/canvas/webglconstants.h"

CanvasWebGLShader::CanvasWebGLShader(BOOL fragmentShader) : m_shader(NULL), m_fragmentShader(fragmentShader), m_compiled(FALSE), m_variables(NULL)
{}

CanvasWebGLShader::~CanvasWebGLShader()
{
	OP_DELETE(m_shader);
	OP_DELETE(m_variables);
}

OP_STATUS CanvasWebGLShader::compile(const uni_char *url, BOOL validate, unsigned int extensions_enabled)
{
	OP_NEW_DBG("CanvasWebGLShader::compile()", "webgl.shaders");

	m_compiled = FALSE;

	/* Receives the validated/translated shader source */
	OpString8 src;

	if (validate)
	{
		m_info_log.Set("");

		WGL_Validator::Configuration config;
		config.for_vertex = !isFragmentShader();
		config.console_logging = TRUE;
		config.output_log = &m_info_log;
		config.output_source = &src;
		config.output_format = g_vegaGlobals.vega3dDevice->getShaderLanguage(config.output_version);
		config.support_higp_fragment = g_vegaGlobals.vega3dDevice->getHighPrecisionFragmentSupport();
		/* Both are defaulted to TRUE, but modify these to disable array indexing checks: */
		config.fragment_shader_constant_uniform_array_indexing = TRUE;
		config.clamp_out_of_bound_uniform_array_indexing = TRUE;
		config.support_oes_derivatives = (extensions_enabled & WEBGL_EXT_OES_STANDARD_DERIVATIVES) != 0;

		OP_BOOLEAN result;
		WGL_ShaderVariables *variables = NULL;

		/* Validate and translate the shader program, using the configuration
		   from 'config', and recording the variables used by the shader
		   (uniforms, varying, and attributes) in m_variables. Even when
		   compiling for platforms that use GLSL, we still generate new code,
		   containing e.g. additional runtime checks. */

		/* Need to pass uni_char[]'s to get the format correct; %S seems
		   unreliable. Also, put in horizontal bars to make these a bit easier
		   to pick out. */
		OP_DBG((UNI_L("\n%s shader before validation and translation\n=================================================\n%s\n"),
		        isFragmentShader() ? UNI_L("Fragment") : UNI_L("Vertex"),
		        m_source.CStr()));

		RETURN_IF_ERROR(result = WGL_Validator::Validate(config, url, m_source.CStr(),
		                                                 m_source.Length(), &variables));

		OP_DBG(("\n%s shader after validation and translation to %s\n=================================================================\n%s\n",
		        isFragmentShader() ? "Fragment" : "Vertex",
		        config.output_format == VEGA3D_SHADER_LANGUAGE(SHADER_GLSL)    ? "GLSL (non-ES)" :
		        config.output_format == VEGA3D_SHADER_LANGUAGE(SHADER_GLSL_ES) ? "GLSL ES"       :
		        config.output_format == VEGA3D_SHADER_LANGUAGE(SHADER_HLSL_9)  ? "HLSL 9"        :
		        config.output_format == VEGA3D_SHADER_LANGUAGE(SHADER_HLSL_10) ? "HLSL 10"       :
		                                                                         "unknown shader language",
		        src.CStr()));

#ifdef _DEBUG
		if (!m_info_log.IsEmpty())
			OP_DBG((UNI_L("\nMessages from %s shader validation\n==========================================\n%s\n"),
			        isFragmentShader() ? UNI_L("fragment") : UNI_L("vertex"),
			        m_info_log.CStr()));
#endif // _DEBUG

		/* Remove records of shader variables from any previous compilation (as
		   the source might have been changed with gl.shaderSource()) */
		OP_DELETE(m_variables);
		m_variables = variables;
		if (result == OpBoolean::IS_FALSE)
			return OpStatus::ERR;
	}
	else
		RETURN_IF_ERROR(src.Set(m_source));

	VEGA3dShader* shader;
	unsigned int attribute_count = 0;
	VEGA3dDevice::AttributeData *attributes = NULL;
	if (!isFragmentShader() && m_variables)
		RETURN_IF_ERROR(m_variables->GetAttributeData(attribute_count, attributes));

	OP_STATUS status = g_vegaGlobals.vega3dDevice->createShader(&shader, m_fragmentShader,
	                                                            src.CStr(), attribute_count,
	                                                            attributes, &m_info_log);

#ifdef _DEBUG
	if (!m_info_log.IsEmpty())
		OP_DBG((UNI_L("\nMessages from %s shader compilation\n===========================================\n%s\n"),
		        isFragmentShader() ? UNI_L("fragment") : UNI_L("vertex"),
		        m_info_log.CStr()));
#endif // _DEBUG

	WGL_ShaderVariables::ReleaseAttributeData(attribute_count, attributes);

	if (OpStatus::IsError(status) && !OpStatus::IsMemoryError(status))
		OP_ASSERT(!"Shader compiler should not be passed incompatible input.");

	RETURN_IF_ERROR(status);

	m_compiled = TRUE;
	OP_DELETE(m_shader);
	m_shader = shader;
	return OpStatus::OK;
}

void CanvasWebGLShader::destroyInternal()
{
	OP_DELETE(m_shader);
	m_shader = NULL;
}

#endif // CANVAS3D_SUPPORT

