/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CANVASWEBGLPROGRAM_H
#define CANVASWEBGLPROGRAM_H

#ifdef CANVAS3D_SUPPORT

#include "modules/libvega/src/canvas/webglutils.h"
#include "modules/util/stringhash.h"

class CanvasWebGLShader;
class WGL_ShaderVariables;

class CanvasWebGLProgram : public WebGLRefCounted
{
public:
	CanvasWebGLProgram();
	~CanvasWebGLProgram();

	OP_STATUS Construct();

	VEGA3dShaderProgram* getProgram(){return m_program;}

	BOOL hasShader(CanvasWebGLShader* shader);
	OP_STATUS addShader(CanvasWebGLShader* shader);
	OP_STATUS removeShader(CanvasWebGLShader* shader);

	unsigned int getNumAttachedShaders();
	ES_Object* getAttachedShaderESObject(unsigned int i);

	OP_STATUS link(unsigned int extensions_enabled, unsigned int max_vertex_attribs);
	BOOL isLinked(){return m_linked;}
	unsigned int getLinkId() { return m_linkId; }

	const uni_char* getInfoLog(){return m_info_log.CStr();}
	unsigned int getInfoLogLength(){return m_info_log.Length();}

	virtual void destroyInternal();

	BOOL hasVertexShader() const;
	BOOL hasFragmentShader() const;

	OP_BOOLEAN lookupShaderAttribute(const char* name, VEGA3dShaderProgram::ConstantType &type, unsigned int &length, const uni_char** original_name);
	OP_BOOLEAN lookupShaderUniform(const char* name, VEGA3dShaderProgram::ConstantType &type, unsigned int &length, BOOL &is_array, const uni_char **original_name);

	OP_STATUS validateLink(unsigned int extensions_enabled);

	/* Holds the current mapping from samplers to (WebGL) texture units.
	   The key is the sampler location, as reported by the backend. */
	OpUINT32ToUINT32HashTable m_sampler_values;

	/** Attribute bindings (name->index) specified via bindAttribLocation().
	    These take effect on the subsequent link. */
	StringHash<unsigned> m_next_attrib_bindings;

	/** The attribute bindings currently in effect. Basically the values
	    from 'm_next_attrib_bindings' as of the previous link plus values for
	    attributes with no specified binding. */
	StringHash<unsigned> m_active_attrib_bindings;

private:
	/** Helper function to link(): records the attribute bindings that
	    will be in effect for the shader until the next link. */
	OP_STATUS setUpActiveAttribBindings(unsigned int max_vertex_attribs);

	/** Helper function to link(): sets up an initial map from samplers
	    to texture units. */
	OP_STATUS setUpSamplerBindings();

	VEGA3dShaderProgram* m_program;

	OpVector<CanvasWebGLShader> m_attachedShaders;
	WGL_ShaderVariables *m_shaderVariables;

	BOOL m_linked;
	unsigned int m_linkId;

	OpString m_info_log;
};

#endif //CANVAS3D_SUPPORT
#endif //CANVASWEBGLPROGRAM_H
