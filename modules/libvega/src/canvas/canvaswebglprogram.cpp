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

#include "modules/dom/src/canvas/webglconstants.h"
#include "modules/libvega/src/canvas/canvaswebglprogram.h"
#include "modules/libvega/src/canvas/canvaswebglshader.h"
#include "modules/webgl/src/wgl_validator.h"

CanvasWebGLProgram::CanvasWebGLProgram() : m_program(NULL), m_shaderVariables(NULL), m_linked(FALSE), m_linkId(0)
{
}

CanvasWebGLProgram::~CanvasWebGLProgram()
{
	VEGARefCount::DecRef(m_program);
	while (m_attachedShaders.GetCount() > 0)
	{
		CanvasWebGLShader* shader = m_attachedShaders.Remove(0);
		shader->removeRef();
	}
	OP_DELETE(m_shaderVariables);
}

OP_STATUS CanvasWebGLProgram::Construct()
{
	return g_vegaGlobals.vega3dDevice->createShaderProgram(&m_program, VEGA3dShaderProgram::SHADER_CUSTOM, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP);
}

BOOL CanvasWebGLProgram::hasShader(CanvasWebGLShader* shader)
{
	return m_attachedShaders.Find(shader) >= 0;
}

OP_STATUS CanvasWebGLProgram::addShader(CanvasWebGLShader* shader)
{
	RETURN_IF_ERROR(m_attachedShaders.Add(shader));
	shader->addRef();
	return OpStatus::OK;
}

OP_STATUS CanvasWebGLProgram::removeShader(CanvasWebGLShader* shader)
{
	RETURN_IF_ERROR(m_attachedShaders.RemoveByItem(shader));
	shader->removeRef();
	return OpStatus::OK;
}

unsigned int CanvasWebGLProgram::getNumAttachedShaders()
{
	return m_attachedShaders.GetCount();
}

ES_Object* CanvasWebGLProgram::getAttachedShaderESObject(unsigned int i)
{
	return m_attachedShaders.Get(i)->getESObject();
}

OP_STATUS CanvasWebGLProgram::link(unsigned int extensions_enabled, unsigned int max_vertex_attribs)
{
	OP_NEW_DBG("CanvasWebGLProgram::link()", "webgl.shaders");

	m_linked = FALSE;
	++m_linkId;
	int shader_index;

	RETURN_IF_ERROR(validateLink(extensions_enabled));

	RETURN_IF_ERROR(setUpActiveAttribBindings(max_vertex_attribs));

	OP_STATUS status = OpStatus::OK;

	for (shader_index = 0;
	     shader_index < (int)m_attachedShaders.GetCount();
	     ++shader_index)
	{
		VEGA3dShader *shader = m_attachedShaders.Get(shader_index)->getShader();
		status = shader ? m_program->addShader(shader) : OpStatus::ERR;
		if (OpStatus::IsError(status))
			break;
	}

	if (OpStatus::IsSuccess(status))
	{
		status = m_program->link(&m_info_log);

#ifdef _DEBUG
		if (!m_info_log.IsEmpty())
			OP_DBG((UNI_L("\nMessages from shader linking\n============================\n%s\n"),
			        m_info_log.CStr()));
#endif // _DEBUG
	}

	// Only try to remove the shaders that were actually added above
	while (shader_index-- > 0)
	{
		VEGA3dShader *shader = m_attachedShaders.Get(shader_index)->getShader();
		OP_ASSERT(shader);
		m_program->removeShader(shader);
	}

	RETURN_IF_ERROR(status);
	RETURN_IF_ERROR(setUpSamplerBindings());

	m_linked = TRUE;
	return status;
}

OP_STATUS CanvasWebGLProgram::setUpActiveAttribBindings(unsigned int max_vertex_attribs)
{
	OpINT32Set bound_locations;
	m_active_attrib_bindings.clear();

	OpString8 uni_conv;

	if (!m_shaderVariables)
	{
		OP_ASSERT(!"Did a shader fail to compile? If so, we should have bailed out earlier.");
		return OpStatus::ERR;
	}

	/* Pass 1: Assign locations to attributes that have been bound via
	   bindAttribLocation(). */

	for (WGL_ShaderVariable *attrib = m_shaderVariables->attributes.First();
	     attrib;
	     attrib = attrib->Suc())
	{
		unsigned location;
		uni_conv.Set(attrib->name);
		if (m_next_attrib_bindings.get(uni_conv.CStr(), location))
		{
			OP_ASSERT(location < max_vertex_attribs);
			// Keep track of used locations
			bound_locations.Add(static_cast<INT32>(location));
			RETURN_IF_ERROR(m_active_attrib_bindings.set(uni_conv.CStr(), location));
		}
	}

	/* Pass 2: Assign unused locations to attributes with no specified binding. */

	unsigned unbound_location = 0;

	for (WGL_ShaderVariable *attrib = m_shaderVariables->attributes.First();
	     attrib;
	     attrib = attrib->Suc())
	{
		uni_conv.Set(attrib->name);
		if (!m_next_attrib_bindings.contains(uni_conv.CStr()))
		{
			// Find free location
			while (bound_locations.Contains(static_cast<INT32>(unbound_location)))
				++unbound_location;
			if (unbound_location >= max_vertex_attribs)
				return OpStatus::ERR;
			RETURN_IF_ERROR(m_active_attrib_bindings.set(uni_conv.CStr(), unbound_location));
			++unbound_location;
		}
	}

	/* In the GL attribute model, we need to go out to the backend and bind
	   attributes to their locations prior to linking (which is when attribute
	   bindings become effectual) */

	if (g_vegaGlobals.vega3dDevice->getAttribModel() == VEGA3dDevice::GLAttribModel)
	{
		const char *name;
		unsigned location;
		StringHash<unsigned>::Iterator iter = m_active_attrib_bindings.get_iterator();
		while (iter.get_next(name, location))
			RETURN_IF_ERROR(getProgram()->setInputLocation(name, location));
	}

#ifdef _DEBUG
	{
		OP_NEW_DBG("CanvasWebGLProgram::link()", "webgl.attributes");
		const char *name;
		unsigned binding;

		OP_DBG(("Attribute bindings assigned via bindAttribLocation():"));
		StringHash<unsigned>::Iterator bound_iter = m_next_attrib_bindings.get_iterator();
		while (bound_iter.get_next(name, binding))
			OP_DBG(("%s : %u", name, binding));

		OP_DBG(("Attribute bindings put into effect:"));
		StringHash<unsigned>::Iterator active_iter = m_active_attrib_bindings.get_iterator();
		while (active_iter.get_next(name, binding))
			OP_DBG(("%s : %u", name, binding));
	}
#endif // _DEBUG

	return OpStatus::OK;
}

OP_STATUS CanvasWebGLProgram::setUpSamplerBindings()
{
	/* Populate the m_sampler_values hash table, which maps sampler
	   ID's to sampler values (WebGL texture unit indices). This is used to
	   postpone the setting of the actual back-end uniform samplers and/or
	   texture units until draw time, which makes the implementation
	   simpler for back-ends where samplers are hardwired to particular
	   texture units, such as D3D. */

	/* Remove any mappings from a previous link.

	   TODO: Does this correctly handle all combinations of
	   successful/failed links and installed/not installed programs? */
	m_sampler_values.RemoveAll();

	for (WGL_ShaderVariable *uniform = m_shaderVariables->uniforms.First();
	     uniform;
	     uniform = uniform->Suc())
	{
		if (uniform->type->GetType() == WGL_Type::Sampler)
		{
			// TODO: Failing below means we might leave the instance in a
			// "half-initialized" state w.r.t. m_sampler_values. Might be
			// able to clean up this method a bit later.

			OpString8 name_8;
			RETURN_IF_ERROR(name_8.Set(uniform->name));

			// All samplers are initially bound to texture unit 0
			UINT32 loc = getProgram()->getConstantLocation(name_8);
			if (static_cast<WGL_SamplerType*>(uniform->type)->type == WGL_SamplerType::SamplerCube)
				loc |= IS_SAMPLER_CUBE_BIT;
			RETURN_IF_ERROR(m_sampler_values.Add(loc, 0));
		}
	}

	return OpStatus::OK;
}

void CanvasWebGLProgram::destroyInternal()
{
	VEGARefCount::DecRef(m_program);
	m_program = NULL;
	while (m_attachedShaders.GetCount() > 0)
	{
		CanvasWebGLShader* shader = m_attachedShaders.Remove(0);
		shader->removeRef();
	}
}

BOOL CanvasWebGLProgram::hasVertexShader() const
{
	for (unsigned int i = 0; i < m_attachedShaders.GetCount(); ++i)
		if (!m_attachedShaders.Get(i)->isFragmentShader())
			return TRUE;
	return FALSE;
}

BOOL CanvasWebGLProgram::hasFragmentShader() const
{
	for (unsigned int i = 0; i < m_attachedShaders.GetCount(); ++i)
		if (m_attachedShaders.Get(i)->isFragmentShader())
			return TRUE;
	return FALSE;
}

OP_STATUS CanvasWebGLProgram::validateLink(unsigned int extensions_enabled)
{
	OP_ASSERT(!m_linked);
	OP_DELETE(m_shaderVariables);
	m_shaderVariables = NULL;

	unsigned count = m_attachedShaders.GetCount();
	if (count == 0)
		return OpStatus::OK;

	WGL_Validator::Configuration config;
	config.console_logging = TRUE;
	/* The info log is cleared regardless. */
	m_info_log.Set("");
	config.output_log = &m_info_log;
	config.support_oes_derivatives = (extensions_enabled & WEBGL_EXT_OES_STANDARD_DERIVATIVES) != 0;
	BOOL succeeded = TRUE;

	for (unsigned i = 0; i < count; i++)
	{
		CanvasWebGLShader* shader = m_attachedShaders.Get(i);
		if (!shader->isCompiled())
			return OpStatus::ERR;

		WGL_ShaderVariables *shader_variables = shader->getShaderVariables();
		if (!shader_variables)
		{
			OP_ASSERT(!"Successfully compiled shaders should always have a set of shader variables");
			continue;
		}

		if (!m_shaderVariables)
			RETURN_IF_LEAVE(m_shaderVariables = WGL_ShaderVariables::MakeL(shader_variables));
		else
		{
			WGL_ShaderVariables *extra_vars = shader_variables;
			OP_BOOLEAN result = OpBoolean::IS_TRUE;
#if 0
			/* Disabled until local tests verify that it is ready. */
			RETURN_IF_LEAVE(result = WGL_Validator::ValidateLinkageL(config, m_shaderVariables, extra_vars));
#endif
			succeeded = succeeded && (result == OpBoolean::IS_TRUE);
			RETURN_IF_LEAVE(m_shaderVariables->MergeWithL(extra_vars));
		}
	}
	return (succeeded ? OpStatus::OK : OpStatus::ERR);
}

OP_BOOLEAN CanvasWebGLProgram::lookupShaderAttribute(const char* name, VEGA3dShaderProgram::ConstantType &type, unsigned int &length, const uni_char** original_name)
{
	if (m_shaderVariables)
		return m_shaderVariables->LookupAttribute(name, type, length, original_name);
	else
		return OpStatus::ERR;
}

OP_BOOLEAN CanvasWebGLProgram::lookupShaderUniform(const char* name, VEGA3dShaderProgram::ConstantType &type, unsigned int &length, BOOL &is_array, const uni_char** original_name)
{
	if (!m_shaderVariables)
		return OpStatus::ERR;

	return m_shaderVariables->LookupUniform(name, type, length, is_array, original_name);
}

#endif // CANVAS3D_SUPPORT

