/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_OPENGL
#include "platforms/vega_backends/opengl/vegaglapi.h"

#include "platforms/vega_backends/opengl/vegagldevice.h"
#include "platforms/vega_backends/opengl/vegaglshader.h"

#ifdef CANVAS3D_SUPPORT
VEGAGlShader::~VEGAGlShader()
{
	if (shader_object)
		glDeleteShader(shader_object);
}

OP_STATUS VEGAGlShader::Construct(const char* source, OpString *info_log)
{
	if (shader_object)
		glDeleteShader(shader_object);
	shader_object = glCreateShader(isVertexShader()?GL_VERTEX_SHADER:GL_FRAGMENT_SHADER);
	glShaderSource(shader_object, 1, &source, NULL);
	glCompileShader(shader_object);
	if (info_log)
	{
		GLint log_length;
		glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &log_length);
		if (log_length > 0)
		{
			char* info_log_str = OP_NEWA(char, log_length);
			if (!info_log_str)
				return OpStatus::ERR_NO_MEMORY;

			glGetShaderInfoLog(shader_object, log_length, NULL, info_log_str);
			OP_STATUS s = info_log->Append(info_log_str);
			OP_DELETEA(info_log_str);
			if (OpStatus::IsMemoryError(s))
				return OpStatus::ERR_NO_MEMORY;
		}
	}
	GLint compile_status;
	glGetShaderiv(shader_object, GL_COMPILE_STATUS, &compile_status);
	if (compile_status == 0)
		return OpStatus::ERR;
	return OpStatus::OK;
}
#endif // CANVAS3D_SUPPORT

VEGAGlShaderProgram::~VEGAGlShaderProgram()
{
	clearDevice();
	OP_DELETEA(m_activeUniformSizes);
	OP_DELETEA(m_activeInputs);
}

// this provides a workaround for GPUs without full npot support
enum
{
	GL_SHADER_WRAP_FUNC_START,
	GL_SHADER_WRAP_REPEAT_S,
	GL_SHADER_WRAP_MIRROR_S,
	GL_SHADER_WRAP_REPEAT_T,
	GL_SHADER_WRAP_MIRROR_T,
	GL_SHADER_WRAP_FUNC_END,
	GL_SHADER_WRAP_DEFINE_OVERRIDE,
	GL_SHADER_WRAP_DEFINE_NO_OVERRIDE,
	GL_SHADER_WRAP_GLES_PRECISION,
	GL_SHADER_WRAP_STRING_COUNT, // last!
};
static const char* const GLShaderWrapStrings[GL_SHADER_WRAP_STRING_COUNT] =
{
	// function start
	"vec2 unwrapCoord(in vec2 texCoord)\n"
	"{\n"
	"\tvec2 coord = texCoord;\n",

	// repeat s
	"\tcoord.s = fract(coord.s);\n",

	// mirror s
	"\tif (mod(floor(coord.s), 2.0) == 0.0)\n"
	"\t\tcoord.s = fract(coord.s);\n"
	"\telse\n"
	"\t\tcoord.s = 1.0 - fract(coord.s);\n",

	// repeat t
	"\tcoord.t = fract(coord.t);\n",

	// mirror t
	"\tif (mod(floor(coord.t), 2.0) == 0.0)\n"
	"\t\tcoord.t = fract(coord.t);\n"
	"\telse\n"
	"\t\tcoord.t = 1.0 - fract(coord.t);\n",

	// function end
	"\treturn coord;\n"
	"}\n",

	// define override
	"#define texture2D_wrap(samp, coord) texture2D(samp, unwrapCoord(coord))\n",

	// define no override
	"#define texture2D_wrap(samp, coord) texture2D(samp, coord)\n",

	"#ifdef GL_ES\nprecision mediump float;\n#endif\n",
};
#ifndef VEGA_OPENGL_SHADERDATA_DEFINED
extern const char* g_GLSL_ShaderData[];
#endif
#define VEGA_OPENGL_MAX_SHADER_STRING_COUNT 8 /// max number of strings used
static void get_shader_source(VEGA3dShaderProgram::ShaderType shdtype, VEGA3dShaderProgram::WrapMode shdmode,
                                GLsizei& num_strings, const char** strings)
{
	strings[0] = "";
	num_strings = 1;

	//Always use the GLES precision declaration
	strings[num_strings ++] = GLShaderWrapStrings[GL_SHADER_WRAP_GLES_PRECISION];

	// no overrides needed for _CLAMP_CLAMP
	if (shdmode == VEGA3dShaderProgram::WRAP_CLAMP_CLAMP)
		strings[num_strings ++] = GLShaderWrapStrings[GL_SHADER_WRAP_DEFINE_NO_OVERRIDE];
	else
	{
		// open function
		strings[num_strings ++] = GLShaderWrapStrings[GL_SHADER_WRAP_FUNC_START];

		// s wrap mode
		switch (shdmode)
		{
		case VEGA3dShaderProgram::WRAP_REPEAT_REPEAT:
		case VEGA3dShaderProgram::WRAP_REPEAT_MIRROR:
		case VEGA3dShaderProgram::WRAP_REPEAT_CLAMP:
			strings[num_strings ++] = GLShaderWrapStrings[GL_SHADER_WRAP_REPEAT_S];
			break;
		case VEGA3dShaderProgram::WRAP_MIRROR_REPEAT:
		case VEGA3dShaderProgram::WRAP_MIRROR_MIRROR:
		case VEGA3dShaderProgram::WRAP_MIRROR_CLAMP:
			strings[num_strings ++] = GLShaderWrapStrings[GL_SHADER_WRAP_MIRROR_S];
			break;
		}

		// t wrap mode
		switch (shdmode)
		{
		case VEGA3dShaderProgram::WRAP_REPEAT_REPEAT:
		case VEGA3dShaderProgram::WRAP_MIRROR_REPEAT:
		case VEGA3dShaderProgram::WRAP_CLAMP_REPEAT:
			strings[num_strings ++] = GLShaderWrapStrings[GL_SHADER_WRAP_REPEAT_T];
			break;
		case VEGA3dShaderProgram::WRAP_REPEAT_MIRROR:
		case VEGA3dShaderProgram::WRAP_MIRROR_MIRROR:
		case VEGA3dShaderProgram::WRAP_CLAMP_MIRROR:
			strings[num_strings ++] = GLShaderWrapStrings[GL_SHADER_WRAP_MIRROR_T];
			break;
		}

		// close function
		strings[num_strings ++] = GLShaderWrapStrings[GL_SHADER_WRAP_FUNC_END];

		// define override calling function
		strings[num_strings ++] = GLShaderWrapStrings[GL_SHADER_WRAP_DEFINE_OVERRIDE];
	}

	// actual shader source
	// but first, move the #version setting to the top
	const char * shader = g_GLSL_ShaderData[shdtype];
	if (op_strncmp("#version 130\n", shader, 13) == 0)
	{
		strings[0] = "#version 130\n";
		shader += 13;
	}
	OP_ASSERT(op_strncmp("#version", shader, 8) != 0);
	strings[num_strings ++] = shader;
}

OP_STATUS VEGAGlShaderProgram::loadProgram(VEGA3dShaderProgram::ShaderType shdtype, VEGA3dShaderProgram::WrapMode shdmode)
{
	m_type = shdtype;
	const char* vertex_text = g_GLSL_ShaderData[VEGA3dShaderProgram::NUM_SHADERS];

	const char* fragment_strings[VEGA_OPENGL_MAX_SHADER_STRING_COUNT]; // ARRAY OK 2011-07-14 wonko
	GLsizei num_fragment_strings;
	get_shader_source(shdtype, shdmode, num_fragment_strings, fragment_strings);
	OP_ASSERT(num_fragment_strings && num_fragment_strings <= VEGA_OPENGL_MAX_SHADER_STRING_COUNT);

	OP_ASSERT(fragment_strings[num_fragment_strings-1] && vertex_text);

	program_object = glCreateProgram();
	GLuint shader_object = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(shader_object, num_fragment_strings, fragment_strings, NULL);
	glCompileShader(shader_object);

	GLint compile_status;
	glGetShaderiv(shader_object, GL_COMPILE_STATUS, &compile_status);
	if (compile_status == 0)
	{
#ifdef _DEBUG
		OP_ASSERT(!"Fragment shader compilation failed!");

		GLint log_length;
		glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &log_length);
		char* error_log = OP_NEWA(char, log_length);
		glGetShaderInfoLog(shader_object, log_length, NULL, error_log);
		OP_DELETEA(error_log);
#endif // _DEBUG
		glDeleteShader(shader_object);
		shader_object = 0;
		glDeleteProgram(program_object);
		program_object = 0;
		return OpStatus::ERR;
	}

	glAttachShader(program_object, shader_object);
	glDeleteShader(shader_object);

	shader_object = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(shader_object, 1, &vertex_text, NULL);
	glCompileShader(shader_object);

	glGetShaderiv(shader_object, GL_COMPILE_STATUS, &compile_status);
	if (compile_status == 0)
	{
#ifdef _DEBUG
		OP_ASSERT(!"Vertex shader compilation failed!");

		GLint log_length;
		glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &log_length);
		char* error_log = OP_NEWA(char, log_length);
		glGetShaderInfoLog(shader_object, log_length, NULL, error_log);
		OP_DELETEA(error_log);
#endif // _DEBUG
		glDeleteShader(shader_object);
		shader_object = 0;
		glDeleteProgram(program_object);
		program_object = 0;
		return OpStatus::ERR;
	}

	glAttachShader(program_object, shader_object);
	glDeleteShader(shader_object);
	shader_object = 0;

	glBindAttribLocation(program_object, 0, "inPosition");
	glBindAttribLocation(program_object, 1, "inTex");
	glBindAttribLocation(program_object, 2, "inColor");
	glBindAttribLocation(program_object, 3, "inTex2");

#ifndef VEGA_OPENGLES
	if (shdtype == VEGA3dShaderProgram::SHADER_TEXT2DEXTBLEND)
	{
		glBindFragDataLocationIndexed(program_object, 0, 0, "fragColor0");
		glBindFragDataLocationIndexed(program_object, 0, 1, "fragColor1");
	}
#endif // !VEGA_OPENGLES

	glLinkProgram(program_object);

	GLint link_status;
	glGetProgramiv(program_object, GL_LINK_STATUS, &link_status);
	if (link_status == 0)
	{
#ifdef _DEBUG
		OP_ASSERT(!"Shader linking failed!");

		GLint log_length;
		glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &log_length);
		char* error_log = OP_NEWA(char, log_length);
		glGetProgramInfoLog(program_object, log_length, NULL, error_log);
		OP_DELETEA(error_log);
#endif // _DEBUG
		glDeleteProgram(program_object);
		program_object = 0;
		return OpStatus::ERR;
	}
	VEGA3dShaderProgram* oldprog = device->getCurrentShaderProgram();
	switch (shdtype)
	{
	case SHADER_VECTOR2D:
	case SHADER_TEXT2D:
	case SHADER_TEXT2DEXTBLEND:
	case SHADER_COLORMATRIX:
	case SHADER_LUMINANCE_TO_ALPHA:
	case SHADER_SRGB_TO_LINEARRGB:
	case SHADER_LINEARRGB_TO_SRGB:
	case SHADER_LIGHTING_DISTANTLIGHT:
	case SHADER_LIGHTING_POINTLIGHT:
	case SHADER_LIGHTING_SPOTLIGHT:
	case SHADER_LIGHTING_MAKE_BUMP:
	case SHADER_CONVOLVE_GEN_16:
	case SHADER_CONVOLVE_GEN_16_BIAS:
	case SHADER_CONVOLVE_GEN_25_BIAS:
	case SHADER_MORPHOLOGY_DILATE_15:
	case SHADER_MORPHOLOGY_ERODE_15:
		device->setShaderProgram(this);
		setScalar(getConstantLocation("src"), 0);
		device->setShaderProgram(oldprog);
		break;
	case SHADER_TEXT2D_INTERPOLATE:
	case SHADER_TEXT2DEXTBLEND_INTERPOLATE:
		device->setShaderProgram(this);
		setScalar(getConstantLocation("src"), 0);
		setScalar(getConstantLocation("src2"), 1);
		device->setShaderProgram(oldprog);
		break;
	case SHADER_TEXT2DTEXGEN:
	case SHADER_TEXT2DEXTBLENDTEXGEN:
		device->setShaderProgram(this);
		setScalar(getConstantLocation("src"), 0);
		setScalar(getConstantLocation("stencilSrc"), 1);
		device->setShaderProgram(oldprog);
		break;
	case SHADER_TEXT2DTEXGEN_INTERPOLATE:
	case SHADER_TEXT2DEXTBLENDTEXGEN_INTERPOLATE:
		device->setShaderProgram(this);
		setScalar(getConstantLocation("src"), 0);
		setScalar(getConstantLocation("src2"), 1);
		setScalar(getConstantLocation("stencilSrc"), 2);
		device->setShaderProgram(oldprog);
		break;
	case SHADER_VECTOR2DTEXGEN:
		device->setShaderProgram(this);
		setScalar(getConstantLocation("src"), 0);
		setScalar(getConstantLocation("stencilSrc"), 1);
		setScalar(getConstantLocation("maskSrc"), 2);
		device->setShaderProgram(oldprog);
		break;
	case SHADER_VECTOR2DTEXGENRADIAL:
	case SHADER_VECTOR2DTEXGENRADIALSIMPLE:
		device->setShaderProgram(this);
		setScalar(getConstantLocation("src"), 0);
		setScalar(getConstantLocation("stencilSrc"), 1);
		setScalar(getConstantLocation("maskSrc"), 2);
		device->setShaderProgram(oldprog);
		break;
	case SHADER_COMPONENTTRANSFER:
		device->setShaderProgram(this);
		setScalar(getConstantLocation("src"), 0);
		setScalar(getConstantLocation("map"), 1);
		device->setShaderProgram(oldprog);
		break;
	case SHADER_DISPLACEMENT:
		device->setShaderProgram(this);
		setScalar(getConstantLocation("src"), 0);
		setScalar(getConstantLocation("displace_map"), 1);
		device->setShaderProgram(oldprog);
		break;
	case SHADER_MERGE_ARITHMETIC:
	case SHADER_MERGE_MULTIPLY:
	case SHADER_MERGE_SCREEN:
	case SHADER_MERGE_DARKEN:
	case SHADER_MERGE_LIGHTEN:
		device->setShaderProgram(this);
		setScalar(getConstantLocation("src1"), 0);
		setScalar(getConstantLocation("src2"), 1);
		device->setShaderProgram(oldprog);
		break;
	case SHADER_CUSTOM:
	default:
		break;
	}
	return OpStatus::OK;
}

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGAGlShaderProgram::addShader(VEGA3dShader* shader)
{
	glAttachShader(program_object, ((VEGAGlShader*)shader)->shader_object);
	return OpStatus::OK;
}

OP_STATUS VEGAGlShaderProgram::removeShader(VEGA3dShader* shader)
{
#ifdef VEGA_BACKENDS_GL_QUIRKS_DONT_DETACH_SHADERS_WORKAROUND
	if (!device->isQuirkEnabled(VEGAGlDevice::QUIRK_DONT_DETACH_SHADERS))
#endif // VEGA_BACKENDS_GL_QUIRKS_DONT_DETACH_SHADERS_WORKAROUND
		glDetachShader(program_object, ((VEGAGlShader*)shader)->shader_object);
	return OpStatus::OK;
}

OP_STATUS VEGAGlShaderProgram::link(OpString *info_log)
{
	glLinkProgram(program_object);

	if (info_log)
	{
		GLint log_length;
		glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &log_length);
		if (log_length > 0)
		{
			char* info_log_str = OP_NEWA(char, log_length);
			if (!info_log_str)
				return OpStatus::ERR_NO_MEMORY;

			glGetProgramInfoLog(program_object, log_length, NULL, info_log_str);
			OP_STATUS s = info_log->Append(info_log_str);
			OP_DELETEA(info_log_str);
			if (OpStatus::IsMemoryError(s))
				return OpStatus::ERR_NO_MEMORY;
		}
	}

	GLint link_status;
	glGetProgramiv(program_object, GL_LINK_STATUS, &link_status);
	if (link_status == 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(updateActiveUniformSizes());
	RETURN_IF_ERROR(updateAttributeStatus());
	return OpStatus::OK;
}

OP_STATUS VEGAGlShaderProgram::setAsRenderTarget(VEGA3dRenderTarget *target)
{
	return OpStatus::OK;
}

OP_STATUS VEGAGlShaderProgram::validate()
{
	glValidateProgram(program_object);
	GLint validate_status;
	glGetProgramiv(program_object, GL_VALIDATE_STATUS, &validate_status);
	return validate_status != 0 ? OpStatus::OK : OpStatus::ERR;
}

bool VEGAGlShaderProgram::isValid()
{
	GLint validate_status;
	glGetProgramiv(program_object, GL_VALIDATE_STATUS, &validate_status);
	return validate_status != 0;
}

unsigned int VEGAGlShaderProgram::getNumInputs()
{
	GLint count = 0;
	glGetProgramiv(program_object, GL_ACTIVE_ATTRIBUTES, &count);
	return count;
}

unsigned int VEGAGlShaderProgram::getNumConstants()
{
	return m_numActiveUniforms;
}

unsigned int VEGAGlShaderProgram::getInputMaxLength()
{
	GLint count = 0;
	glGetProgramiv(program_object, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &count);
	return count;
}

unsigned int VEGAGlShaderProgram::getConstantMaxLength()
{
	GLint count = 0;
	glGetProgramiv(program_object, GL_ACTIVE_UNIFORM_MAX_LENGTH, &count);
	return count;
}

static VEGAGlShaderProgram::ConstantType GetConstantType(GLenum gltype)
{
	VEGAGlShaderProgram::ConstantType type;
	switch (gltype)
	{
	case GL_FLOAT:
		type = VEGAGlShaderProgram::SHDCONST_FLOAT;
		break;
	case GL_FLOAT_VEC2:
		type = VEGAGlShaderProgram::SHDCONST_FLOAT2;
		break;
	case GL_FLOAT_VEC3:
		type = VEGAGlShaderProgram::SHDCONST_FLOAT3;
		break;
	case GL_FLOAT_VEC4:
		type = VEGAGlShaderProgram::SHDCONST_FLOAT4;
		break;
	case GL_INT:
		type = VEGAGlShaderProgram::SHDCONST_INT;
		break;
	case GL_INT_VEC2:
		type = VEGAGlShaderProgram::SHDCONST_INT2;
		break;
	case GL_INT_VEC3:
		type = VEGAGlShaderProgram::SHDCONST_INT3;
		break;
	case GL_INT_VEC4:
		type = VEGAGlShaderProgram::SHDCONST_INT4;
		break;
	case GL_BOOL:
		type = VEGAGlShaderProgram::SHDCONST_BOOL;
		break;
	case GL_BOOL_VEC2:
		type = VEGAGlShaderProgram::SHDCONST_BOOL2;
		break;
	case GL_BOOL_VEC3:
		type = VEGAGlShaderProgram::SHDCONST_BOOL3;
		break;
	case GL_BOOL_VEC4:
		type = VEGAGlShaderProgram::SHDCONST_BOOL4;
		break;
	case GL_FLOAT_MAT2:
		type = VEGAGlShaderProgram::SHDCONST_MAT2;
		break;
	case GL_FLOAT_MAT3:
		type = VEGAGlShaderProgram::SHDCONST_MAT3;
		break;
	case GL_FLOAT_MAT4:
		type = VEGAGlShaderProgram::SHDCONST_MAT4;
		break;
	case GL_SAMPLER_2D:
		type = VEGAGlShaderProgram::SHDCONST_SAMP2D;
		break;
	case GL_SAMPLER_CUBE:
		type = VEGAGlShaderProgram::SHDCONST_SAMPCUBE;
		break;
	default:
		type = VEGAGlShaderProgram::SHDCONST_NONE;
		break;
	}
	return type;
}

OP_STATUS VEGAGlShaderProgram::indexToAttributeName(unsigned int idx, const char** name)
{
	GLenum gltype_dummy;
	GLint glsize_dummy;
	GLsizei gllength = 0;
	char* buf = (char*)g_memory_manager->GetTempBuf2k();
	glGetActiveAttrib(program_object, idx, g_memory_manager->GetTempBuf2kLen(), &gllength, &glsize_dummy, &gltype_dummy, buf);
	if (gllength == 0)
		return OpStatus::ERR;
	buf[gllength] = '\0';
	*name = buf;
	return OpStatus::OK;
}

OP_STATUS VEGAGlShaderProgram::getConstantDescription(unsigned int idx, ConstantType* type, unsigned int* size, const char** name)
{
	if (idx >= static_cast<unsigned int>(m_numActiveUniforms))
		return OpStatus::ERR;

	GLenum gltype = 0;
	GLint glsize = 0;
	GLsizei gllength = 0;
	char* buf = (char*)g_memory_manager->GetTempBuf2k();
	glGetActiveUniform(program_object, idx, g_memory_manager->GetTempBuf2kLen(), &gllength, &glsize, &gltype, buf);

	// The OpenGL ES spec requires that array uniform names
	// be returned with "[0]" at the end of the string. That
	// is not what callers of this function expects so remove
	// that part from the returned name.
	if (gllength > 3 && op_strncmp(&buf[gllength-3], "[0]", 3) == 0)
		gllength -= 3;

	buf[gllength] = '\0';
	*type = GetConstantType(gltype);
	*size = glsize;
	*name = buf;
	return OpStatus::OK;
}

OP_STATUS VEGAGlShaderProgram::setInputLocation(const char* name, int idx)
{
	glBindAttribLocation(program_object, idx, name);
	return OpStatus::OK;
}
#endif // CANVAS3D_SUPPORT

int VEGAGlShaderProgram::getConstantLocation(const char* name)
{
	return glGetUniformLocation(program_object, name);
}

OP_STATUS VEGAGlShaderProgram::setScalar(int idx, float val)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniform1f(idx, val);
	return OpStatus::OK;
}
OP_STATUS VEGAGlShaderProgram::setScalar(int idx, int val)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniform1i(idx, val);
	return OpStatus::OK;
}

OP_STATUS VEGAGlShaderProgram::setScalar(int idx, float* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniform1fv(idx, count, val);
	return OpStatus::OK;
}
OP_STATUS VEGAGlShaderProgram::setScalar(int idx, int* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniform1iv(idx, count, val);
	return OpStatus::OK;
}
OP_STATUS VEGAGlShaderProgram::setVector2(int idx, float* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniform2fv(idx, count, val);
	return OpStatus::OK;
}
OP_STATUS VEGAGlShaderProgram::setVector2(int idx, int* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniform2iv(idx, count, val);
	return OpStatus::OK;
}
OP_STATUS VEGAGlShaderProgram::setVector3(int idx, float* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniform3fv(idx, count, val);
	return OpStatus::OK;
}
OP_STATUS VEGAGlShaderProgram::setVector3(int idx, int* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniform3iv(idx, count, val);
	return OpStatus::OK;
}
OP_STATUS VEGAGlShaderProgram::setVector4(int idx, float* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniform4fv(idx, count, val);
	return OpStatus::OK;
}
OP_STATUS VEGAGlShaderProgram::setVector4(int idx, int* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniform4iv(idx, count, val);
	return OpStatus::OK;
}

#ifdef VEGA_OPENGLES
static void transposeMatrix(int dimensions, const float* inVal, float* outVal)
{
	// Transposes the incoming matrix, the matrix must be square!
	for (int i=0; i<dimensions; i++)
		for (int u=0; u<dimensions; u++)
			outVal[i*dimensions+u] = inVal[u*dimensions+i];
}
#endif // VEGA_OPENGLES

OP_STATUS VEGAGlShaderProgram::setMatrix2(int idx, float* val, int count, bool transpose)
{
	if (idx < 0)
		return OpStatus::ERR;

#ifdef VEGA_OPENGLES
	// OpenGL ES does not support transpose so we need to do it manually.
	float* transMat = NULL;
	if (transpose)
	{
		transMat = OP_NEWA(float, count*4);
		if (!transMat)
			return OpStatus::ERR_NO_MEMORY;

		for (int i=0; i<count; i++)
			transposeMatrix(2,  &val[i*4], &transMat[i*4]);
		val = transMat;
		transpose = false;
	}
#endif // VEGA_OPENGLES

	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniformMatrix2fv(idx, count, transpose?GL_TRUE:GL_FALSE, val);

#ifdef VEGA_OPENGLES
	OP_DELETEA(transMat);
#endif // VEGA_OPENGLES

	return OpStatus::OK;
}

OP_STATUS VEGAGlShaderProgram::setMatrix3(int idx, float* val, int count, bool transpose)
{
	if (idx < 0)
		return OpStatus::ERR;

#ifdef VEGA_OPENGLES
	// OpenGL ES does not support transpose so we need to do it manually.
	float* transMat = NULL;
	if (transpose)
	{
		transMat = OP_NEWA(float, count*9);
		if (!transMat)
			return OpStatus::ERR_NO_MEMORY;

		for (int i=0; i<count; i++)
			transposeMatrix(3, &val[i*9], &transMat[i*9]);
		val = transMat;
		transpose = false;
	}
#endif // VEGA_OPENGLES

	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniformMatrix3fv(idx, count, transpose?GL_TRUE:GL_FALSE, val);

#ifdef VEGA_OPENGLES
	OP_DELETEA(transMat);
#endif // VEGA_OPENGLES

	return OpStatus::OK;
}

OP_STATUS VEGAGlShaderProgram::setMatrix4(int idx, float* val, int count, bool transpose)
{
	if (idx < 0)
		return OpStatus::ERR;

#ifdef VEGA_OPENGLES
	// OpenGL ES does not support transpose so we need to do it manually.
	float* transMat = NULL;
	if (transpose)
	{
		transMat = OP_NEWA(float, count*16);
		if (!transMat)
			return OpStatus::ERR_NO_MEMORY;

		for (int i=0; i<count; i++)
			transposeMatrix(4, &val[i*16], &transMat[i*16]);
		val = transMat;
		transpose = false;
	}
#endif // VEGA_OPENGLES

	OP_ASSERT(device->getCurrentShaderProgram() == this);
	glUniformMatrix4fv(idx, count, transpose?GL_TRUE:GL_FALSE, val);

#ifdef VEGA_OPENGLES
	OP_DELETEA(transMat);
#endif // VEGA_OPENGLES

	return OpStatus::OK;
}

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGAGlShaderProgram::getConstantValue(int idx, float* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex)
{
	glGetUniformfv(program_object, idx, val);
	return OpStatus::OK;
}
OP_STATUS VEGAGlShaderProgram::getConstantValue(int idx, int* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex)
{
	glGetUniformiv(program_object, idx, val);
	return OpStatus::OK;
}
#endif // CANVAS3D_SUPPORT

#ifdef VEGA_BACKENDS_OPENGL_FLIPY
bool VEGAGlShaderProgram::orthogonalProjectionNeedsUpdate()
{
	VEGA3dRenderTarget* rt = device->getRenderTarget();
	if (!rt) 
		return false;

	int flipY = rt->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW;

	return m_flipY == -1 || m_flipY != flipY;
}
#endif // VEGA_BACKENDS_OPENGL_FLIPY

void VEGAGlShaderProgram::clearDevice()
{
	Out();

	if (program_object)
	{
		glDeleteProgram(program_object);
		program_object = 0;
	}
}

OP_STATUS VEGAGlShaderProgram::updateActiveUniformSizes()
{
	GLint count = 0;
	glGetProgramiv(program_object, GL_ACTIVE_UNIFORMS, &count);
	OP_ASSERT(m_numActiveUniforms >= 0);

	if (count != m_numActiveUniforms)
	{
		OP_DELETEA(m_activeUniformSizes);
		m_numActiveUniforms = count;
		m_activeUniformSizes = OP_NEWA(GLint, m_numActiveUniforms);
		if (!m_activeUniformSizes)
			return OpStatus::ERR_NO_MEMORY;
	}

	for (GLint i = 0; i < m_numActiveUniforms; ++i)
	{
		GLenum gltype = 0;
		GLsizei gllength = 0;
		glGetActiveUniform(program_object, i, g_memory_manager->GetTempBuf2kLen(), &gllength, m_activeUniformSizes + i, &gltype, (GLchar *)g_memory_manager->GetTempBuf2k());
		OP_ASSERT(m_activeUniformSizes[i] >= 0);
	}

	return OpStatus::OK;
}

#ifdef CANVAS3D_SUPPORT
bool VEGAGlShaderProgram::isAttributeActive(int idx)
{
	return m_activeInputs && m_activeInputs[idx];
}
#endif // CANVAS3D_SUPPORT

OP_STATUS VEGAGlShaderProgram::updateAttributeStatus()
{
	int maxVertexAttribs = device->getMaxVertexAttribs();
	GLint numInputs = 0;
	OP_DELETEA(m_activeInputs);
	m_activeInputs = NULL;
	glGetProgramiv(program_object, GL_ACTIVE_ATTRIBUTES, &numInputs);
	m_activeInputs = OP_NEWA(bool, maxVertexAttribs);
	if (!m_activeInputs)
		return OpStatus::ERR_NO_MEMORY;
	op_memset(m_activeInputs, 0, sizeof(bool) * maxVertexAttribs);
	for (int i = 0; i < numInputs; ++i)
	{
		GLenum gltype = 0;
		GLint glsize = 0;
		GLsizei gllength = 0;
		char* buf = (char*)g_memory_manager->GetTempBuf2k();
		glGetActiveAttrib(program_object, i, g_memory_manager->GetTempBuf2kLen(), &gllength, &glsize, &gltype, buf);
		buf[gllength] = '\0';
		int index = glGetAttribLocation(program_object, buf);
		OP_ASSERT(index >= 0 && index < maxVertexAttribs);
		m_activeInputs[index] = true;
	}
	return OpStatus::OK;
}

#endif // VEGA_BACKEND_OPENGL
