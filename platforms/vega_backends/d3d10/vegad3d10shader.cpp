/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_DIRECT3D10

#include <d3d10_1.h>
// If WebGL is enabled we need both the DXSDK and the D3DCompiler header. To find out if we
// have both we include the D3DCompiler header here. If the header isn't installed we will
// fall back and include our dummy D3DCompiler header which will define NO_DXSDK. That will
// in turn disable compilation of the DX backend.
#ifdef CANVAS3D_SUPPORT
#include <D3DCompiler.h>
#endif //CANVAS3D_SUPPORT
#ifndef NO_DXSDK

#include "platforms/vega_backends/d3d10/vegad3d10shader.h"
#include "platforms/vega_backends/d3d10/vegad3d10device.h"
#ifdef CANVAS3D_SUPPORT
#  include "modules/webgl/src/wgl_cgbuilder.h"
#endif // CANVAS3D_SUPPORT

/* Uses the HLSL "Packing Rules for Constant Variables" to copy a compactly packed
   matrix from/to the memory layout with padding that DX expects and optionally
   transposing it.
   A matrix2x2 is 24 bytes, a matrix3x3 is 44 bytes and a matrix4x4 is 64 bytes.
   i.e. each row in the matrix starts on a new vec4 component.
*/
static void copy_matrix(float *dst, float *src, unsigned dim, bool transpose, bool toDX = true)
{
	// If we're copying to memory used by the shader and the dimension is not 4
	// We need to zero out the memory not getting set first. Fastest and easiest
	// done with a single memset.
	//
	// Calculation breakdown:
	// 4*(dim - 1) : Number of floats in dim - 1 padded rows. The last row
	//               has no padding and so does not need to be cleared.
	// - dim       : Skip non-padding floats on first row.
	// Floats to clear: 4*(dim - 1) - dim = 3d - 4
	if (toDX && dim != 4)
		op_memset(dst + dim, 0, (3*dim - 4)*sizeof(float));

	int dstride = toDX ? 4 : dim;
	int sstride = toDX ? dim: 4;
	if (transpose)
	{
		for (unsigned row = 0; row < dim; ++row)
			for (unsigned col = 0; col < dim; ++col)
				dst[row * dstride + col] = src[col * sstride + row];
	}
	else
	{
		for (unsigned row = 0; row < dim; ++row)
			for (unsigned col = 0; col < dim; ++col)
				dst[row * dstride + col] = src[row * sstride + col];
	}
}

VEGAD3d10ShaderProgram::VEGAD3d10ShaderProgram(ID3D10Device1* dev, bool level9)
	: m_device(dev),
#ifdef CANVAS3D_SUPPORT
	  m_pshaderObject(NULL),
	  m_vshaderObject(NULL),
#endif //CANVAS3D_SUPPORT
	  m_pshader(NULL),
	  m_vshader(NULL),
	  m_vshaderBlob(NULL),
	  m_featureLevel9(level9)
#ifdef CANVAS3D_SUPPORT
	, m_D3DCompileProc(NULL)
#endif
	, m_linkId(0)
{}

VEGAD3d10ShaderProgram::~VEGAD3d10ShaderProgram()
{
	if (m_pshader)
		m_pshader->Release();
	if (m_vshader)
		m_vshader->Release();
	if (m_vshaderBlob)
		m_vshaderBlob->Release();

	m_pixelConstants.Release();
	m_vertexConstants.Release();

#ifdef CANVAS3D_SUPPORT
	OP_DELETE(m_pshaderObject);
	OP_DELETE(m_vshaderObject);
#endif // CANVAS3D_SUPPORT
}

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGAD3d10Shader::Construct(const char *src, BOOL pixelShader, unsigned int attribute_count, VEGA3dDevice::AttributeData *attributes)
{
	// Assumed input: HLSL generated from validated GLSL.
	m_isPixelShader = pixelShader;
	RETURN_IF_ERROR(m_source.Set(src));
	m_attribute_count = attribute_count;

	if (attribute_count > 0)
	{
		VEGA3dDevice::AttributeData *block = OP_NEWA(VEGA3dDevice::AttributeData, attribute_count);
		RETURN_OOM_IF_NULL(block);
		op_memset(block, 0, attribute_count * sizeof(VEGA3dDevice::AttributeData));

		for (unsigned int i = 0; i < attribute_count; i++)
		{
			block[i].name = NULL;
			block[i].size = 0;
			if (attributes[i].name)
			{
				block[i].name = OP_NEWA(char, op_strlen(attributes[i].name) + 1);
				if (!block[i].name)
				{
					for (unsigned int j = 0; j < i; j++)
						OP_DELETEA(block[i].name);
					OP_DELETEA(block);
					return OpStatus::ERR_NO_MEMORY;
				}
				else
					op_strcpy(block[i].name, attributes[i].name);

				block[i].size = attributes[i].size;
			}
		}
		m_attributes = block;
	}
	else
		m_attributes = NULL;

	return OpStatus::OK;
}

VEGAD3d10Shader::~VEGAD3d10Shader()
{
	for (unsigned int i = 0; i < m_attribute_count; i++)
		OP_DELETEA(m_attributes[i].name);

	OP_DELETEA(m_attributes);
}
#endif // CANVAS3D_SUPPORT

void VEGAD3d10ShaderConstants::Release()
{
	OP_DELETEA(m_ramBuffer);
	m_ramBuffer = NULL;
	if (m_cbuffer)
	{
		m_cbuffer->Release();
		m_cbuffer = NULL;
	}
#ifdef CANVAS3D_SUPPORT
	m_attrib_to_semantic_index.clear();

	if (m_shader_reflection)
	{
		m_shader_reflection->Release();
		m_shader_reflection = NULL;

		for (unsigned int i = 0; i < m_count; ++i)
			op_free((void*)m_names[i]);
		OP_DELETEA(m_names);

		OP_DELETEA(m_values);
	}
#endif // CANVAS3D_SUPPORT

	m_names = NULL;
	m_values = NULL;
	m_count = 0;
	m_slotCount = 0;
	m_dirty = false;
}

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGAD3d10ShaderConstants::initializeForCustomShader(ID3D10Device1 *dev,
                                                              ID3D10ShaderReflection *shader_reflection)
{
	/* This method assumes VEGAD3d10ShaderConstants::Release() has been
	   called prior. */
	OP_ASSERT(!m_names);
	OP_ASSERT(!m_values);
	OP_ASSERT(m_count == 0);
	OP_ASSERT(m_slotCount == 0);
	OP_ASSERT(!m_ramBuffer);
	OP_ASSERT(!m_dirty);

	m_shader_reflection = shader_reflection;

	/* The reflection interfaces below are "helper interfaces" and do
	   not need to be Release()'d; see
	   http://msdn.microsoft.com/en-us/library/windows/desktop/bb205158%28v=vs.85%29.aspx */

	ID3D10ShaderReflectionConstantBuffer *cbuffer_reflection;
	D3D10_SHADER_BUFFER_DESC cbuffer_desc;

	/* !cbuffer_reflection never seems to happen, even if the shader lacks a
	   cbuffer. But just to be safe.

	   TODO: We assume that the failure to get a cbuffer here is due to there
	   being no constants. Perhaps we could do better. */
	cbuffer_reflection = shader_reflection->GetConstantBufferByIndex(0);
	if (!cbuffer_reflection || FAILED(cbuffer_reflection->GetDesc(&cbuffer_desc)))
		return OpStatus::OK;

	/* Create the cbuffer on the GPU and a corresponding buffer in RAM. The RAM
	   buffer is copied to the GPU buffer at draw time. */

	D3D10_BUFFER_DESC new_cbuffer_desc;
	new_cbuffer_desc.ByteWidth = cbuffer_desc.Size;
	new_cbuffer_desc.Usage = D3D10_USAGE_DYNAMIC;
	new_cbuffer_desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
	new_cbuffer_desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	new_cbuffer_desc.MiscFlags = 0;
	if (FAILED(dev->CreateBuffer(&new_cbuffer_desc, NULL, &m_cbuffer)))
		return OpStatus::ERR;

	BYTE *ramBuffer = OP_NEWA(BYTE, cbuffer_desc.Size);
	RETURN_OOM_IF_NULL(ramBuffer);
	OpAutoArray<BYTE> ramBufferAnchor((BYTE*)ramBuffer);
	op_memset(ramBuffer, 0, cbuffer_desc.Size);

	OP_ASSERT(cbuffer_desc.Size % (4*sizeof(float)) == 0);
	m_slotCount = cbuffer_desc.Size / (4*sizeof(float));

	/* Make sure the initial zero-initialized RAM buffer will be copied to the
	   GPU even if no constants are set before the first draw operation */
	m_dirty = true;

	// Create a map from constant names to byte offsets within the cbuffer

	ID3D10ShaderReflectionVariable *var_reflection;
	D3D10_SHADER_VARIABLE_DESC var_desc;

	m_count = cbuffer_desc.Variables;

	char **names = OP_NEWA(char*, m_count);
	RETURN_OOM_IF_NULL(names);
	OpAutoArray<char*> namesAnchor(names);

	unsigned *offsets = OP_NEWA(unsigned, m_count);
	RETURN_OOM_IF_NULL(offsets);
	OpAutoArray<unsigned> offsetsAnchor(offsets);

	for (unsigned i = 0; i < m_count; ++i)
	{
		var_reflection = cbuffer_reflection->GetVariableByIndex(i);
		if (FAILED(var_reflection->GetDesc(&var_desc)) || !(names[i] = op_strdup(var_desc.Name)))
		{
			while (i-- > 0)
				op_free((void*)names[i]);
			return OpStatus::ERR;
		}

		/* The cross-compiler does not bind the location of cbuffer constants
		   to slot boundaries, so we need to store the byte offset rather than
		   the slot number in m_values[idx]. This also produces more compact
		   cbuffers and gives the HLSL compiler more freedom to arrange cbuffer
		   data as it sees fit. */
		offsets[i] = var_desc.StartOffset;
	}

	m_names = const_cast<const char* const*>(names);
	m_values = offsets;
	m_ramBuffer = ramBuffer;

	ramBufferAnchor.release();
	namesAnchor.release();
	offsetsAnchor.release();

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10ShaderConstants::addAttributes(unsigned int count, VEGA3dDevice::AttributeData* attributes)
{
	/* Record attribute-name-to-semantic-index mappings.
	   m_attrib_to_semantic_index["foo"] = n means we have
	   'T foo : TEXCOORD<n>' in the vertex shader input signature. */

	for (unsigned int i = 0; i < count; i++)
		RETURN_IF_ERROR(m_attrib_to_semantic_index.set(attributes[i].name, i));

	return OpStatus::OK;
}
#endif // CANVAS3D_SUPPORT

int VEGAD3d10ShaderConstants::getConstantOffset(int idx)
{
	/* For custom (WebGL) shaders 'idx' is the byte offset within the
	   cbuffer. For builtin shaders it is the slot number, and so needs to be
	   multiplied by 4*sizeof(float) to derive the byte offset. */
#ifdef CANVAS3D_SUPPORT
	if (m_shader_reflection)
		return idx;
#endif // CANVAS3D_SUPPORT
	return idx*4*sizeof(float);
}

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGAD3d10ShaderConstants::getConstantValue(int idx, float* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex)
{
	OP_ASSERT(idx >= 0);

	const int offset = getConstantOffset(idx);
	if (offset < 0)
		return OpStatus::ERR;
	const unsigned n_components = VEGA3dShaderProgram::nComponentsForType[type];
	if (offset + n_components * sizeof(float) > m_slotCount * 4 * sizeof(float))
	{
		OP_ASSERT(!"Attempt to read outside of memory buffer for cbuffer");
		return OpStatus::ERR;
	}

	float *const src = (float*)(((char*)m_ramBuffer) + offset);
	switch (type)
	{
	case VEGA3dShaderProgram::SHDCONST_MAT2:
		copy_matrix(val, src, 2, true, false);
		break;
	case VEGA3dShaderProgram::SHDCONST_MAT3:
		copy_matrix(val, src, 3, true, false);
		break;
	case VEGA3dShaderProgram::SHDCONST_MAT4:
		copy_matrix(val, src, 4, true, false);
		break;
	default:
		for (unsigned int v = 0; v < n_components; ++v)
			val[v] = src[v];
		break;
	}

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10ShaderConstants::getConstantValue(int idx, int* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex)
{
	OP_ASSERT(idx >= 0);

	const int offset = getConstantOffset(idx);
	if (offset < 0)
		return OpStatus::ERR;
	const unsigned n_components = VEGA3dShaderProgram::nComponentsForType[type];
	if (offset + n_components * sizeof(INT32) > m_slotCount * 4 * sizeof(float))
	{
		OP_ASSERT(!"Attempt to read outside of memory buffer for cbuffer");
		return OpStatus::ERR;
	}

	INT32 *const src = (INT32*)(((char*)m_ramBuffer) + offset);
	for (unsigned int v = 0; v < n_components; ++v)
		val[v] = src[v];

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10ShaderConstants::getConstantDescription(unsigned int idx, VEGA3dShaderProgram::ConstantType* type, unsigned int* size, const char** name)
{
	if (!(idx >= 0 && idx < m_count && m_shader_reflection))
		return OpStatus::ERR;

	ID3D10ShaderReflectionConstantBuffer *const_buffer = m_shader_reflection->GetConstantBufferByIndex(0);
	if (!const_buffer)
		return OpStatus::ERR;

	ID3D10ShaderReflectionVariable *var = const_buffer->GetVariableByIndex(idx);
	if (!var)
		return OpStatus::ERR;

	ID3D10ShaderReflectionType *sh_type = var->GetType();
	if (!sh_type)
		return OpStatus::ERR;

	D3D10_SHADER_TYPE_DESC desc;
	D3D10_SHADER_VARIABLE_DESC var_desc;
	if (FAILED(sh_type->GetDesc(&desc)) || FAILED(var->GetDesc(&var_desc)))
		return OpStatus::ERR;

	if (size)
		*size = 1;
	if (name)
		*name = var_desc.Name;
	if (!type)
		return OpStatus::OK;

	*type = VEGA3dShaderProgram::SHDCONST_NONE;
	switch (desc.Class)
	{
	case D3D_SVC_SCALAR:
	{
		switch (desc.Type)
		{
		case D3D_SVT_INT:
			*type = VEGA3dShaderProgram::SHDCONST_INT;
			return OpStatus::OK;
		case D3D_SVT_BOOL:
			*type = VEGA3dShaderProgram::SHDCONST_BOOL;
			return OpStatus::OK;
		case D3D_SVT_FLOAT:
			*type = VEGA3dShaderProgram::SHDCONST_FLOAT;
			return OpStatus::OK;
		default:
			OP_ASSERT(!"Unexpected scalar type");
			return OpStatus::ERR;
		}
	}
	case D3D_SVC_VECTOR:
	{
		/* NOTE: column == 1 will map to the scalar type. */
		unsigned mag = desc.Columns;
		switch (desc.Type)
		{
		case D3D_SVT_INT:
			*type = static_cast<VEGA3dShaderProgram::ConstantType>(VEGA3dShaderProgram::SHDCONST_INT - 1 + mag);
			return OpStatus::OK;
		case D3D_SVT_BOOL:
			*type = static_cast<VEGA3dShaderProgram::ConstantType>(VEGA3dShaderProgram::SHDCONST_BOOL - 1 + mag);
			return OpStatus::OK;
		case D3D_SVT_FLOAT:
			*type = static_cast<VEGA3dShaderProgram::ConstantType>(VEGA3dShaderProgram::SHDCONST_FLOAT - 1 + mag);
			return OpStatus::OK;
		default:
			OP_ASSERT(!"Unexpected vector type");
			return OpStatus::ERR;
		}
	}
	case D3D_SVC_MATRIX_ROWS:
	case D3D_SVC_MATRIX_COLUMNS:
	{
		unsigned cols = desc.Columns;
		OP_ASSERT(cols == desc.Rows && desc.Type == D3D_SVT_FLOAT && cols >=2 && cols <= 4);
		if (cols == 2)
			*type = VEGA3dShaderProgram::SHDCONST_MAT2;
		else if (cols == 3)
			*type = VEGA3dShaderProgram::SHDCONST_MAT3;
		else if (cols == 4)
			*type = VEGA3dShaderProgram::SHDCONST_MAT4;

		return OpStatus::OK;
	}
	case D3D_SVC_OBJECT:
	{
		if (desc.Type == D3D_SVT_TEXTURE2D)
			*type = VEGA3dShaderProgram::SHDCONST_SAMP2D;
		else if (desc.Type == D3D_SVT_TEXTURECUBE)
			*type = VEGA3dShaderProgram::SHDCONST_SAMPCUBE;
		else
		{
			OP_ASSERT(!"Unexpected object type");
			return OpStatus::ERR;
		}

		return OpStatus::OK;
	}
	default:
	{
		OP_ASSERT(!"Unexpected class type");
		return OpStatus::ERR;
	}
	}

	// Unreachable

	// (An OP_ASSERT() here triggers a 4702 (Unreachable code) warning, even
	// with a local warning(disable: 4702). I think we would need to put the
	// suppressions around the macro definition itself and use e.g. a separate
	// UNREACHABLE_CODE macro.)
}

int VEGAD3d10ShaderConstants::attribToSemanticIndex(const char *name)
{
	unsigned index;
	return m_attrib_to_semantic_index.get(name, index) ? index : -1;
}
#endif // CANVAS3D_SUPPORT

int VEGAD3d10ShaderConstants::lookupConstName(const char* name)
{
#ifdef CANVAS3D_SUPPORT
	/* First, look for a texture with the name. m_shader_reflection is only
	   set for SHADER_CUSTOM (user) shaders.

	   TODO: Results could be cached here. Alternatively, all resources could
	   be looked up when the shader is loaded and put in a hash table. */
	if (m_shader_reflection)
	{
		D3D10_SHADER_INPUT_BIND_DESC desc;
		for (UINT i = 0;; ++i)
		{
			if (FAILED(m_shader_reflection->GetResourceBindingDesc(i, &desc)))
				break;
			if (desc.Type == D3D10_SIT_TEXTURE && strcmp(desc.Name, name) == 0)
				return desc.BindPoint;
		}
	}
#endif // CANVAS3D_SUPPORT

	for (unsigned i = 0; i < m_count; ++i)
		if (!op_strcmp(name, m_names[i]))
			return m_values[i];

	return -1;
}

#ifdef CANVAS3D_SUPPORT
unsigned int VEGAD3d10ShaderConstants::getConstantMaxLength()
{
	// FIXME.
	return g_memory_manager->GetTempBuf2kLen();
}

unsigned int VEGAD3d10ShaderConstants::getInputMaxLength()
{
	// FIXME.
	return g_memory_manager->GetTempBuf2kLen();
}

OP_STATUS VEGAD3d10ShaderConstants::indexToAttributeName(unsigned int idx, const char** name)
{
	StringHash<unsigned>::Iterator iter = m_attrib_to_semantic_index.get_iterator();
	const char *current_name;
	unsigned location;
	bool found = false;
	while (iter.get_next(current_name, location))
		if (location == idx)
		{
			found = true;
			break;
		}
	if (!found)
		return OpStatus::ERR;
	*name = current_name;
	return OpStatus::OK;
}

int VEGAD3d10ShaderConstants::getSemanticIndex(unsigned int idx)
{
	D3D10_SIGNATURE_PARAMETER_DESC desc;
	if (SUCCEEDED(m_shader_reflection->GetInputParameterDesc(idx, &desc)))
		return desc.SemanticIndex;
	return -1;
}

#endif // CANVAS3D_SUPPORT

#include "platforms/vega_backends/d3d10/shaders/colormatrix.h"
#include "platforms/vega_backends/d3d10/shaders/colormatrix_level9.h"
#include "platforms/vega_backends/d3d10/shaders/componenttransfer.h"
#include "platforms/vega_backends/d3d10/shaders/componenttransfer_level9.h"
#include "platforms/vega_backends/d3d10/shaders/convolve_gen_16.h"
#include "platforms/vega_backends/d3d10/shaders/convolve_gen_16_bias.h"
#include "platforms/vega_backends/d3d10/shaders/convolve_gen_25_bias.h"
#include "platforms/vega_backends/d3d10/shaders/displacement.h"
#include "platforms/vega_backends/d3d10/shaders/displacement_level9.h"
#include "platforms/vega_backends/d3d10/shaders/lighting_bump.h"
#include "platforms/vega_backends/d3d10/shaders/lighting_bump_level9.h"
#include "platforms/vega_backends/d3d10/shaders/lighting_distant.h"
#include "platforms/vega_backends/d3d10/shaders/lighting_distant_level9.h"
#include "platforms/vega_backends/d3d10/shaders/lighting_point.h"
#include "platforms/vega_backends/d3d10/shaders/lighting_point_level9.h"
#include "platforms/vega_backends/d3d10/shaders/lighting_spot.h"
#include "platforms/vega_backends/d3d10/shaders/lighting_spot_level9.h"
#include "platforms/vega_backends/d3d10/shaders/linearrgb_to_srgb.h"
#include "platforms/vega_backends/d3d10/shaders/linearrgb_to_srgb_level9.h"
#include "platforms/vega_backends/d3d10/shaders/luminance.h"
#include "platforms/vega_backends/d3d10/shaders/luminance_level9.h"
#include "platforms/vega_backends/d3d10/shaders/merge_arithmetic.h"
#include "platforms/vega_backends/d3d10/shaders/merge_arithmetic_level9.h"
#include "platforms/vega_backends/d3d10/shaders/merge_darken.h"
#include "platforms/vega_backends/d3d10/shaders/merge_darken_level9.h"
#include "platforms/vega_backends/d3d10/shaders/merge_lighten.h"
#include "platforms/vega_backends/d3d10/shaders/merge_lighten_level9.h"
#include "platforms/vega_backends/d3d10/shaders/merge_multiply.h"
#include "platforms/vega_backends/d3d10/shaders/merge_multiply_level9.h"
#include "platforms/vega_backends/d3d10/shaders/merge_screen.h"
#include "platforms/vega_backends/d3d10/shaders/merge_screen_level9.h"
#include "platforms/vega_backends/d3d10/shaders/morphology_dilate_15.h"
#include "platforms/vega_backends/d3d10/shaders/morphology_erode_15.h"
#include "platforms/vega_backends/d3d10/shaders/srgb_to_linearrgb.h"
#include "platforms/vega_backends/d3d10/shaders/srgb_to_linearrgb_level9.h"
#include "platforms/vega_backends/d3d10/shaders/text2d.h"
#include "platforms/vega_backends/d3d10/shaders/text2d_level9.h"
#include "platforms/vega_backends/d3d10/shaders/text2dtexgen.h"
#include "platforms/vega_backends/d3d10/shaders/text2dtexgen_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2d.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgen.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgenradial.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgenradial_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgenradialsimple.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgenradialsimple_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vert2d.h"
#include "platforms/vega_backends/d3d10/shaders/vert2d_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vert2dmultitex.h"
#include "platforms/vega_backends/d3d10/shaders/vert2dmultitex_level9.h"

#include "platforms/vega_backends/d3d10/shaders/vector2d_REPEAT_REPEAT_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2d_REPEAT_MIRROR_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2d_REPEAT_CLAMP_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2d_MIRROR_REPEAT_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2d_MIRROR_MIRROR_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2d_MIRROR_CLAMP_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2d_CLAMP_REPEAT_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2d_CLAMP_MIRROR_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2d_CLAMP_CLAMP_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgen_REPEAT_REPEAT_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgen_REPEAT_MIRROR_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgen_REPEAT_CLAMP_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgen_MIRROR_REPEAT_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgen_MIRROR_MIRROR_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgen_MIRROR_CLAMP_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgen_CLAMP_REPEAT_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgen_CLAMP_MIRROR_level9.h"
#include "platforms/vega_backends/d3d10/shaders/vector2dtexgen_CLAMP_CLAMP_level9.h"

#include "platforms/vega_backends/d3d10/shaders/FilterConvolve16_REPEAT_REPEAT_level9.h"
#include "platforms/vega_backends/d3d10/shaders/FilterConvolve16_CLAMP_CLAMP_level9.h"
#include "platforms/vega_backends/d3d10/shaders/FilterConvolve16Bias_REPEAT_REPEAT_level9.h"
#include "platforms/vega_backends/d3d10/shaders/FilterConvolve16Bias_CLAMP_CLAMP_level9.h"
#include "platforms/vega_backends/d3d10/shaders/FilterConvolve25Bias_CLAMP_CLAMP_level9.h"
#include "platforms/vega_backends/d3d10/shaders/FilterConvolve25Bias_REPEAT_REPEAT_level9_3.h"
#include "platforms/vega_backends/d3d10/shaders/FilterMorphologyDilate_CLAMP_CLAMP_level9.h"
#include "platforms/vega_backends/d3d10/shaders/FilterMorphologyDilate_REPEAT_REPEAT_level9.h"
#include "platforms/vega_backends/d3d10/shaders/FilterMorphologyErode_CLAMP_CLAMP_level9.h"
#include "platforms/vega_backends/d3d10/shaders/FilterMorphologyErode_REPEAT_REPEAT_level9.h"


static const BYTE* const s_compiledD3D10Shaders[] = {
	g_vector2d,
	g_vector2dTexGen,
	g_vector2dTexGenRadial,
	g_vector2dTexGenRadialSimple,
	g_text2d,
	g_text2dTexGen,
	NULL, // not supported
	NULL, // not supported
	NULL, // not supported
	NULL, // not supported
	NULL, // not supported
	NULL, // not supported
	g_colorMatrix,
	g_componentTransfer,
	g_displacement,
	g_luminance,
	g_srgbToLinearRgb,
	g_linearRgbToSrgb,
	g_mergeArithmetic,
	g_mergeMultiply,
	g_mergeScreen,
	g_mergeDarken,
	g_mergeLighten,
	g_lightingDistant,
	g_lightingPoint,
	g_lightingSpot,
	g_lightingBump,
	g_convolveGen16,
	g_convolveGen16Bias,
	g_convolveGen25Bias,
	g_morphologyDilate15,
	g_morphologyErode15,
	NULL
};

static const unsigned int s_compiledD3D10ShaderSizes[] = {
	sizeof(g_vector2d),
	sizeof(g_vector2dTexGen),
	sizeof(g_vector2dTexGenRadial),
	sizeof(g_vector2dTexGenRadialSimple),
	sizeof(g_text2d),
	sizeof(g_text2dTexGen),
	0,
	0,
	0,
	0,
	0,
	0,
	sizeof(g_colorMatrix),
	sizeof(g_componentTransfer),
	sizeof(g_displacement),
	sizeof(g_luminance),
	sizeof(g_srgbToLinearRgb),
	sizeof(g_linearRgbToSrgb),
	sizeof(g_mergeArithmetic),
	sizeof(g_mergeMultiply),
	sizeof(g_mergeScreen),
	sizeof(g_mergeDarken),
	sizeof(g_mergeLighten),
	sizeof(g_lightingDistant),
	sizeof(g_lightingPoint),
	sizeof(g_lightingSpot),
	sizeof(g_lightingBump),
	sizeof(g_convolveGen16),
	sizeof(g_convolveGen16Bias),
	sizeof(g_convolveGen25Bias),
	sizeof(g_morphologyDilate15),
	sizeof(g_morphologyErode15),
	0
};

static const BYTE* const s_compiledD3D10Level9Shaders[] = {
	g_ps20_Vector2d_REPEAT_REPEAT,
	g_ps20_Vector2d_REPEAT_MIRROR,
	g_ps20_Vector2d_REPEAT_CLAMP,
	g_ps20_Vector2d_MIRROR_REPEAT,
	g_ps20_Vector2d_MIRROR_MIRROR,
	g_ps20_Vector2d_MIRROR_CLAMP,
	g_ps20_Vector2d_CLAMP_REPEAT,
	g_ps20_Vector2d_CLAMP_MIRROR,
	g_ps20_Vector2d_CLAMP_CLAMP,
	g_ps20_Vector2dTexGen_REPEAT_REPEAT,
	g_ps20_Vector2dTexGen_REPEAT_MIRROR,
	g_ps20_Vector2dTexGen_REPEAT_CLAMP,
	g_ps20_Vector2dTexGen_MIRROR_REPEAT,
	g_ps20_Vector2dTexGen_MIRROR_MIRROR,
	g_ps20_Vector2dTexGen_MIRROR_CLAMP,
	g_ps20_Vector2dTexGen_CLAMP_REPEAT,
	g_ps20_Vector2dTexGen_CLAMP_MIRROR,
	g_ps20_Vector2dTexGen_CLAMP_CLAMP,
	g_vector2dTexGenRadialLevel9,
	g_vector2dTexGenRadialSimpleLevel9,
	g_text2dLevel9,
	g_text2dTexGenLevel9,
	NULL, // not supported
	NULL, // not supported
	NULL, // not supported
	NULL, // not supported
	NULL, // not supported
	NULL, // not supported
	g_colorMatrixLevel9,
	g_componentTransferLevel9,
	g_displacementLevel9,
	g_luminanceLevel9,
	g_srgbToLinearRgbLevel9,
	g_linearRgbToSrgbLevel9,
	g_mergeArithmeticLevel9,
	g_mergeMultiplyLevel9,
	g_mergeScreenLevel9,
	g_mergeDarkenLevel9,
	g_mergeLightenLevel9,
	g_lightingDistantLevel9,
	g_lightingPointLevel9,
	g_lightingSpotLevel9,
	g_lightingBumpLevel9,
	g_ps20_FilterConvolve16_REPEAT_REPEAT,
	g_ps20_FilterConvolve16_CLAMP_CLAMP,
	g_ps20_FilterConvolve16Bias_REPEAT_REPEAT,
	g_ps20_FilterConvolve16Bias_CLAMP_CLAMP,
	g_ps20_FilterConvolve25Bias_REPEAT_REPEAT,
	g_ps20_FilterConvolve25Bias_CLAMP_CLAMP,
	g_ps20_FilterMorphologyDilate_REPEAT_REPEAT,
	g_ps20_FilterMorphologyDilate_CLAMP_CLAMP,
	g_ps20_FilterMorphologyErode_REPEAT_REPEAT,
	g_ps20_FilterMorphologyErode_CLAMP_CLAMP,
	NULL
};

static const unsigned int s_compiledD3D10Level9ShaderSizes[] = {
	sizeof(g_ps20_Vector2d_REPEAT_REPEAT),
	sizeof(g_ps20_Vector2d_REPEAT_MIRROR),
	sizeof(g_ps20_Vector2d_REPEAT_CLAMP),
	sizeof(g_ps20_Vector2d_MIRROR_REPEAT),
	sizeof(g_ps20_Vector2d_MIRROR_MIRROR),
	sizeof(g_ps20_Vector2d_MIRROR_CLAMP),
	sizeof(g_ps20_Vector2d_CLAMP_REPEAT),
	sizeof(g_ps20_Vector2d_CLAMP_MIRROR),
	sizeof(g_ps20_Vector2d_CLAMP_CLAMP),
	sizeof(g_ps20_Vector2dTexGen_REPEAT_REPEAT),
	sizeof(g_ps20_Vector2dTexGen_REPEAT_MIRROR),
	sizeof(g_ps20_Vector2dTexGen_REPEAT_CLAMP),
	sizeof(g_ps20_Vector2dTexGen_MIRROR_REPEAT),
	sizeof(g_ps20_Vector2dTexGen_MIRROR_MIRROR),
	sizeof(g_ps20_Vector2dTexGen_MIRROR_CLAMP),
	sizeof(g_ps20_Vector2dTexGen_CLAMP_REPEAT),
	sizeof(g_ps20_Vector2dTexGen_CLAMP_MIRROR),
	sizeof(g_ps20_Vector2dTexGen_CLAMP_CLAMP),
	sizeof(g_vector2dTexGenRadialLevel9),
	sizeof(g_vector2dTexGenRadialSimpleLevel9),
	sizeof(g_text2dLevel9),
	sizeof(g_text2dTexGenLevel9),
	0,
	0,
	0,
	0,
	0,
	0,
	sizeof(g_colorMatrixLevel9),
	sizeof(g_componentTransferLevel9),
	sizeof(g_displacementLevel9),
	sizeof(g_luminanceLevel9),
	sizeof(g_srgbToLinearRgbLevel9),
	sizeof(g_linearRgbToSrgbLevel9),
	sizeof(g_mergeArithmeticLevel9),
	sizeof(g_mergeMultiplyLevel9),
	sizeof(g_mergeScreenLevel9),
	sizeof(g_mergeDarkenLevel9),
	sizeof(g_mergeLightenLevel9),
	sizeof(g_lightingDistantLevel9),
	sizeof(g_lightingPointLevel9),
	sizeof(g_lightingSpotLevel9),
	sizeof(g_lightingBumpLevel9),
	sizeof(g_ps20_FilterConvolve16_REPEAT_REPEAT),
	sizeof(g_ps20_FilterConvolve16_CLAMP_CLAMP),
	sizeof(g_ps20_FilterConvolve16Bias_REPEAT_REPEAT),
	sizeof(g_ps20_FilterConvolve16Bias_CLAMP_CLAMP),
	sizeof(g_ps20_FilterConvolve25Bias_REPEAT_REPEAT),
	sizeof(g_ps20_FilterConvolve25Bias_CLAMP_CLAMP),
	sizeof(g_ps20_FilterMorphologyDilate_REPEAT_REPEAT),
	sizeof(g_ps20_FilterMorphologyDilate_CLAMP_CLAMP),
	sizeof(g_ps20_FilterMorphologyErode_REPEAT_REPEAT),
	sizeof(g_ps20_FilterMorphologyErode_CLAMP_CLAMP),
	0
};


static const char* const vert2dConstantNamesD3D10[] =
{
	"worldProjMatrix",
	"texTransS",
	"texTransT"
};
static const unsigned int vert2dConstantNumsD3D10[] =
{
	0,
	4,
	6
};
static const unsigned int vert2dConstantSlotsD3D10 = 8;

static const char* const vert2dMultiTexConstantNamesD3D10[] =
{
	"worldProjMatrix"
};
static const unsigned int vert2dMultiTexConstantNumsD3D10[] =
{
	0
};
static const unsigned int vert2dMultiTexConstantSlotsD3D10 = 4;

static const char* const vector2dTexGenConstantNamesD3D10[] =
{
	"stencilComponentBased",
	"straightAlpha"
};
static const unsigned int vector2dTexGenConstantNumsD3D10[] =
{
	0,
	1
};
static const unsigned int vector2dTexGenConstantSlotsD3D10 = 2;

static const char* const vector2dTexGenRadialConstantNamesD3D10[] =
{
	"stencilComponentBased",
	"straightAlpha",
	"uSrcY",
	"uFocusPoint",
	"uCenterPoint"
};
static const unsigned int vector2dTexGenRadialConstantNumsD3D10[] =
{
	0,
	1,
	2,
	3,
	4
};
static const unsigned int vector2dTexGenRadialConstantSlotsD3D10 = 5;

static const char* const vector2dTexGenRadialSimpleConstantNamesD3D10[] =
{
	"stencilComponentBased",
	"straightAlpha",
	"uSrcY",
	"uFocusPoint",
	"uCenterPoint"
};
static const unsigned int vector2dTexGenRadialSimpleConstantNumsD3D10[] =
{
	0,
	1,
	2,
	3,
	4
};
static const unsigned int vector2dTexGenRadialSimpleConstantSlotsD3D10 = 5;

static const char* const text2dConstantNamesD3D10[] =
{
	"alphaComponent"
};
static const unsigned int text2dConstantNumsD3D10[] =
{
	0
};
static const unsigned int text2dConstantSlotsD3D10 = 1;
static const char* const text2dTexGenConstantNamesD3D10[] =
{
	"alphaComponent"
};
static const unsigned int text2dTexGenConstantNumsD3D10[] =
{
	0
};
static const unsigned int text2dTexGenConstantSlotsD3D10 = 1;
static const char* const colorMatrixConstantNamesD3D10[] =
{
	"colormat",
	"colorbias"
};
static const unsigned int colorMatrixConstantNumsD3D10[] =
{
	0,
	4
};
static const unsigned int colorMatrixConstantSlotsD3D10 = 5;
static const char* const displacementConstantNamesD3D10[] =
{
	"xselector",
	"yselector",
	"src_scale"
};
static const unsigned int displacementConstantNumsD3D10[] =
{
	0,
	1,
	2
};
static const unsigned int displacementConstantSlotsD3D10 = 3;
static const char* const arithmeticConstantNamesD3D10[] =
{
	"k1",
	"k2",
	"k3",
	"k4"
};
static const unsigned int arithmeticConstantNumsD3D10[] =
{
	0,
	1,
	2,
	3
};
static const unsigned int arithmeticConstantSlotsD3D10 = 4;
static const char* const lightingBumpConstantNamesD3D10[] =
{
	"pixel_size"
};
static const unsigned int lightingBumpConstantNumsD3D10[] =
{
	0
};
static const unsigned int lightingBumpConstantSlotsD3D10 = 1;
static const char* const lightingConstantNamesD3D10[] =
{
	"light_color",
	"light_position",
	"light_kd",
	"light_ks",
	"light_specexp",
	"surface_scale",
	"k1",
	"k2",
	"k3",
	"k4",
	"pixel_size"
};
static const unsigned int lightingConstantNumsD3D10[] =
{
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10
};
static const unsigned int lightingConstantSlotsD3D10 = 11;
static const char* const lightingSpotConstantNamesD3D10[] =
{
	"light_color",
	"light_position",
	"light_kd",
	"light_ks",
	"light_specexp",
	"surface_scale",
	"k1",
	"k2",
	"k3",
	"k4",
	"pixel_size",
	"spot_dir",
	"spot_falloff",
	"spot_coneangle",
	"spot_specexp",
	"spot_has_cone"
};
static const unsigned int lightingSpotConstantNumsD3D10[] =
{
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10,
	11,
	12,
	13,
	14,
	15
};
static const unsigned int lightingSpotConstantSlotsD3D10 = 16;
static const char* const convolve16ConstantNamesD3D10[] =
{
	"coeffs"
};
static const unsigned int convolve16ConstantNumsD3D10[] =
{
	0
};
static const unsigned int convolve16ConstantSlotsD3D10 = 16;
static const char* const convolve16BiasConstantNamesD3D10[] =
{
	"coeffs",
	"divisor",
	"bias",
	"preserve_alpha"
};
static const unsigned int convolve16BiasConstantNumsD3D10[] =
{
	0,
	16,
	17,
	18
};
static const unsigned int convolve16BiasConstantSlotsD3D10 = 19;
static const char* const convolve25BiasConstantNamesD3D10[] =
{
	"coeffs",
	"divisor",
	"bias",
	"preserve_alpha"
};
static const unsigned int convolve25BiasConstantNumsD3D10[] =
{
	0,
	25,
	26,
	27
};
static const unsigned int convolve25BiasConstantSlotsD3D10 = 28;
static const char* const morphologyConstantNamesD3D10[] =
{
	"offsets"
};
static const unsigned int morphologyConstantNumsD3D10[] =
{
	0
};
static const unsigned int morphologyConstantSlotsD3D10 = 3;

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGAD3d10ShaderProgram::loadShader(VEGAD3d10ShaderProgram::ShaderType shdtype, WrapMode shdmode, pD3DCompile D3DCompileProc, VEGAD3D10_pD3DReflectShader D3DReflectShaderProc)
#else
OP_STATUS VEGAD3d10ShaderProgram::loadShader(VEGAD3d10ShaderProgram::ShaderType shdtype, WrapMode shdmode)
#endif
{
#ifdef CANVAS3D_SUPPORT
	m_D3DCompileProc = D3DCompileProc;
	m_D3DReflectShaderProc = D3DReflectShaderProc;
#endif
	m_type = shdtype;

	bool multiTexVertexShader = false;

	m_pixelConstants.m_names = NULL;
	m_pixelConstants.m_values = 0;
	m_pixelConstants.m_count = 0;
	m_pixelConstants.m_slotCount = 0;

	const BYTE* psSource = NULL;
	unsigned int psSourceSize = 0;

	if (m_featureLevel9)
	{
		OP_ASSERT(ARRAY_SIZE(s_compiledD3D10Level9Shaders) == VEGA3dShaderProgram::NUM_ACTUAL_SHADERS);
		OP_ASSERT(ARRAY_SIZE(s_compiledD3D10Level9ShaderSizes) == VEGA3dShaderProgram::NUM_ACTUAL_SHADERS);

		size_t index = VEGA3dShaderProgram::GetShaderIndex(shdtype,shdmode);
		psSource = s_compiledD3D10Level9Shaders[index];
		psSourceSize = s_compiledD3D10Level9ShaderSizes[index];
	}
	else
	{
		psSource = s_compiledD3D10Shaders[shdtype];
		psSourceSize = s_compiledD3D10ShaderSizes[shdtype];
	}

	switch (shdtype)
	{
	case SHADER_VECTOR2D:
	case SHADER_LUMINANCE_TO_ALPHA:
	case SHADER_SRGB_TO_LINEARRGB:
	case SHADER_LINEARRGB_TO_SRGB:
	case SHADER_COMPONENTTRANSFER:
		break;
	case SHADER_VECTOR2DTEXGEN:
		m_pixelConstants.m_names = vector2dTexGenConstantNamesD3D10;
		m_pixelConstants.m_values = vector2dTexGenConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(vector2dTexGenConstantNamesD3D10);
		m_pixelConstants.m_slotCount = vector2dTexGenConstantSlotsD3D10;
		break;
	case SHADER_VECTOR2DTEXGENRADIAL:
		m_pixelConstants.m_names = vector2dTexGenRadialConstantNamesD3D10;
		m_pixelConstants.m_values = vector2dTexGenRadialConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(vector2dTexGenRadialConstantNamesD3D10);
		m_pixelConstants.m_slotCount = vector2dTexGenRadialConstantSlotsD3D10;
		break;
	case SHADER_VECTOR2DTEXGENRADIALSIMPLE:
		m_pixelConstants.m_names = vector2dTexGenRadialSimpleConstantNamesD3D10;
		m_pixelConstants.m_values = vector2dTexGenRadialSimpleConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(vector2dTexGenRadialSimpleConstantNamesD3D10);
		m_pixelConstants.m_slotCount = vector2dTexGenRadialSimpleConstantSlotsD3D10;
		break;
	case SHADER_MERGE_MULTIPLY:
	case SHADER_MERGE_SCREEN:
	case SHADER_MERGE_DARKEN:
	case SHADER_MERGE_LIGHTEN:
		multiTexVertexShader = true;
		break;
	case SHADER_TEXT2D:
		m_pixelConstants.m_names = text2dConstantNamesD3D10;
		m_pixelConstants.m_values = text2dConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(text2dConstantNamesD3D10);
		m_pixelConstants.m_slotCount = text2dConstantSlotsD3D10;
	case SHADER_TEXT2DTEXGEN:
		m_pixelConstants.m_names = text2dTexGenConstantNamesD3D10;
		m_pixelConstants.m_values = text2dTexGenConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(text2dTexGenConstantNamesD3D10);
		m_pixelConstants.m_slotCount = text2dTexGenConstantSlotsD3D10;
		break;
	case SHADER_COLORMATRIX:
		m_pixelConstants.m_names = colorMatrixConstantNamesD3D10;
		m_pixelConstants.m_values = colorMatrixConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(colorMatrixConstantNamesD3D10);
		m_pixelConstants.m_slotCount = colorMatrixConstantSlotsD3D10;
		break;
	case SHADER_DISPLACEMENT:
		m_pixelConstants.m_names = displacementConstantNamesD3D10;
		m_pixelConstants.m_values = displacementConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(displacementConstantNamesD3D10);
		m_pixelConstants.m_slotCount = displacementConstantSlotsD3D10;

		multiTexVertexShader = true;
		break;
	case SHADER_MERGE_ARITHMETIC:
		m_pixelConstants.m_names = arithmeticConstantNamesD3D10;
		m_pixelConstants.m_values = arithmeticConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(arithmeticConstantNamesD3D10);
		m_pixelConstants.m_slotCount = arithmeticConstantSlotsD3D10;

		multiTexVertexShader = true;
		break;
	case SHADER_LIGHTING_DISTANTLIGHT:
	case SHADER_LIGHTING_POINTLIGHT:
		m_pixelConstants.m_names = lightingConstantNamesD3D10;
		m_pixelConstants.m_values = lightingConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(lightingConstantNamesD3D10);
		m_pixelConstants.m_slotCount = lightingConstantSlotsD3D10;
		break;
	case SHADER_LIGHTING_MAKE_BUMP:
		m_pixelConstants.m_names = lightingBumpConstantNamesD3D10;
		m_pixelConstants.m_values = lightingBumpConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(lightingBumpConstantNamesD3D10);
		m_pixelConstants.m_slotCount = lightingBumpConstantSlotsD3D10;
		break;
	case SHADER_LIGHTING_SPOTLIGHT:
		m_pixelConstants.m_names = lightingSpotConstantNamesD3D10;
		m_pixelConstants.m_values = lightingSpotConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(lightingSpotConstantNamesD3D10);
		m_pixelConstants.m_slotCount = lightingSpotConstantSlotsD3D10;
		break;
	case SHADER_CONVOLVE_GEN_16:
		m_pixelConstants.m_names = convolve16ConstantNamesD3D10;
		m_pixelConstants.m_values = convolve16ConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(convolve16ConstantNamesD3D10);
		m_pixelConstants.m_slotCount = convolve16ConstantSlotsD3D10;
		break;
	case SHADER_CONVOLVE_GEN_16_BIAS:
		m_pixelConstants.m_names = convolve16BiasConstantNamesD3D10;
		m_pixelConstants.m_values = convolve16BiasConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(convolve16BiasConstantNamesD3D10);
		m_pixelConstants.m_slotCount = convolve16BiasConstantSlotsD3D10;
		break;
	case SHADER_CONVOLVE_GEN_25_BIAS:
		m_pixelConstants.m_names = convolve25BiasConstantNamesD3D10;
		m_pixelConstants.m_values = convolve25BiasConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(convolve25BiasConstantNamesD3D10);
		m_pixelConstants.m_slotCount = convolve25BiasConstantSlotsD3D10;
		break;
	case SHADER_MORPHOLOGY_DILATE_15:
	case SHADER_MORPHOLOGY_ERODE_15:
		m_pixelConstants.m_names = morphologyConstantNamesD3D10;
		m_pixelConstants.m_values = morphologyConstantNumsD3D10;
		m_pixelConstants.m_count = ARRAY_SIZE(morphologyConstantNamesD3D10);
		m_pixelConstants.m_slotCount = morphologyConstantSlotsD3D10;
		break;
	case SHADER_CUSTOM:
		m_vertexConstants.m_names = NULL;
		m_vertexConstants.m_values = NULL;
		m_vertexConstants.m_count = 0;
		m_vertexConstants.m_slotCount = 0;
		return OpStatus::OK;
	case SHADER_TEXT2D_INTERPOLATE:
	case SHADER_TEXT2DTEXGEN_INTERPOLATE:
	case SHADER_TEXT2DEXTBLEND:
	case SHADER_TEXT2DEXTBLENDTEXGEN:
	case SHADER_TEXT2DEXTBLEND_INTERPOLATE:
	case SHADER_TEXT2DEXTBLENDTEXGEN_INTERPOLATE:
		OP_ASSERT(!"d3d10 doesn't support certain text shaders yet");
	default:
		return OpStatus::ERR;
	}
	if (FAILED(m_device->CreatePixelShader(psSource, psSourceSize, &m_pshader)))
		return OpStatus::ERR;

	if (multiTexVertexShader)
	{
		m_vertexConstants.m_names = vert2dMultiTexConstantNamesD3D10;
		m_vertexConstants.m_values = vert2dMultiTexConstantNumsD3D10;
		m_vertexConstants.m_count = ARRAY_SIZE(vert2dMultiTexConstantNamesD3D10);
		m_vertexConstants.m_slotCount = vert2dMultiTexConstantSlotsD3D10;
		if (m_featureLevel9)
		{
			m_vsSource = g_vert2dMultiTexLevel9;
			m_vsSourceSize = sizeof(g_vert2dMultiTexLevel9);
		}
		else
		{
			m_vsSource = g_vert2dMultiTex;
			m_vsSourceSize = sizeof(g_vert2dMultiTex);
		}
	}
	else
	{
		m_vertexConstants.m_names = vert2dConstantNamesD3D10;
		m_vertexConstants.m_values = vert2dConstantNumsD3D10;
		m_vertexConstants.m_count = ARRAY_SIZE(vert2dConstantNamesD3D10);
		m_vertexConstants.m_slotCount = vert2dConstantSlotsD3D10;
		if (m_featureLevel9)
		{
			m_vsSource = g_vert2dLevel9;
			m_vsSourceSize = sizeof(g_vert2dLevel9);
		}
		else
		{
			m_vsSource = g_vert2d;
			m_vsSourceSize = sizeof(g_vert2d);
		}
	}
	if (FAILED(m_device->CreateVertexShader(m_vsSource, m_vsSourceSize, &m_vshader)))
		return OpStatus::ERR;

	if (m_vertexConstants.m_slotCount)
	{
		D3D10_BUFFER_DESC cbDesc;
		cbDesc.ByteWidth = m_vertexConstants.m_slotCount * 4 * sizeof(float);

		m_vertexConstants.m_ramBuffer = OP_NEWA(float, 4 * m_vertexConstants.m_slotCount);
		RETURN_OOM_IF_NULL(m_vertexConstants.m_ramBuffer);
		op_memset(m_vertexConstants.m_ramBuffer, 0, cbDesc.ByteWidth);

		cbDesc.Usage = D3D10_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
		cbDesc.MiscFlags = 0;
		if (FAILED(m_device->CreateBuffer(&cbDesc, NULL, &m_vertexConstants.m_cbuffer)))
			return OpStatus::ERR;
	}
	if (m_pixelConstants.m_slotCount)
	{
		D3D10_BUFFER_DESC cbDesc;
		cbDesc.ByteWidth = m_pixelConstants.m_slotCount * 4 * sizeof(float);

		m_pixelConstants.m_ramBuffer = OP_NEWA(float, 4 * m_pixelConstants.m_slotCount);
		RETURN_OOM_IF_NULL(m_pixelConstants.m_ramBuffer);
		op_memset(m_pixelConstants.m_ramBuffer, 0, cbDesc.ByteWidth);

		cbDesc.Usage = D3D10_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
		cbDesc.MiscFlags = 0;
		if (FAILED(m_device->CreateBuffer(&cbDesc, NULL, &m_pixelConstants.m_cbuffer)))
			return OpStatus::ERR;
	}
	return OpStatus::OK;
}

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGAD3d10ShaderProgram::addShader(VEGA3dShader* shader)
{
	VEGAD3d10Shader *const d3d_shader = static_cast<VEGAD3d10Shader *>(shader);

	if (d3d_shader->isFragmentShader())
	{
		OP_ASSERT(!m_pshaderObject);
		m_pshaderObject = d3d_shader;
	}
	else // Vertex shader
	{
		OP_ASSERT(!m_vshaderObject);
		m_vshaderObject = d3d_shader;
	}

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10ShaderProgram::removeShader(VEGA3dShader* shader)
{
	VEGAD3d10Shader *d3d_shader = static_cast<VEGAD3d10Shader *>(shader);

	// It's ok to compare pointers here since m_pshader/vshaderObject are
	// always "live" when non-null
	if (d3d_shader == m_pshaderObject)
		m_pshaderObject = NULL;
	else if (d3d_shader == m_vshaderObject)
		m_vshaderObject = NULL;
	// Shaders will be removed if the link-step fails. If so both pointers will be NULL,
	// and we shouldn't fail.
	else if (m_pshaderObject || m_vshaderObject)
	{
		OP_ASSERT(!"Attempt to remove non-existent shader");
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

void VEGAD3d10ShaderProgram::registerErrorsFromBlob(ID3D10Blob *error_blob, OpString *error_log)
{
	if (!error_blob)
		return;

	if (error_log)
		error_log->Append((LPCSTR)error_blob->GetBufferPointer());

	error_blob->Release();
}

OP_STATUS VEGAD3d10ShaderProgram::link(OpString *info_log)
{
	++m_linkId;
	if (m_pshader)
	{
		m_pshader->Release();
		m_pshader = NULL;
	}
	if (m_vshader)
	{
		m_vshader->Release();
		m_vshader = NULL;
	}
	if (m_vshaderBlob)
	{
		m_vshaderBlob->Release();
		m_vshaderBlob = NULL;
	}

	m_pixelConstants.Release();
	m_vertexConstants.Release();

	m_vsSource = NULL;
	m_vsSourceSize = 0;

	ID3D10ShaderReflection *vertex_reflection = NULL;
	ID3D10ShaderReflection *pixel_reflection = NULL;

	ID3D10Blob *shader_blob = NULL;
	ID3D10Blob *error_blob = NULL;

	if (!(m_pshaderObject && m_vshaderObject))
	{
		OP_ASSERT(!"Missing shader in link().");
		return OpStatus::ERR;
	}

	OP_STATUS status;

	//
	// Compile and reflect pixel shader
	//

	status = m_D3DCompileProc(m_pshaderObject->getSource(),
	                          m_pshaderObject->getSourceLength(),
	                          NULL, NULL, NULL, PS_MAIN_FN_NAME,
	                          m_featureLevel9 ? "ps_4_0_level_9_1" : "ps_4_0",
	                          0, 0, &shader_blob, &error_blob);
	registerErrorsFromBlob(error_blob, info_log);
	RETURN_IF_ERROR(status);

	if (FAILED(m_device->CreatePixelShader(shader_blob->GetBufferPointer(),
	                                       shader_blob->GetBufferSize(),
	                                       &m_pshader)))
	{
		shader_blob->Release();
		return OpStatus::ERR;
	}

	if (FAILED(m_D3DReflectShaderProc(shader_blob->GetBufferPointer(),
	                                  shader_blob->GetBufferSize(),
	                                  &pixel_reflection)))
	{
		shader_blob->Release();
		m_pshader->Release();
		m_pshader = NULL;
		return OpStatus::ERR;
	}

	shader_blob->Release();

	//
	// Compile and reflect vertex shader
	//

	status = m_D3DCompileProc(m_vshaderObject->getSource(),
	                          m_vshaderObject->getSourceLength(),
	                          NULL, NULL, NULL, VS_MAIN_FN_NAME,
	                          m_featureLevel9 ? "vs_4_0_level_9_1" : "vs_4_0",
	                          0, 0, &shader_blob, &error_blob);
	registerErrorsFromBlob(error_blob, info_log);
	RETURN_IF_ERROR(status);

	if (FAILED(m_device->CreateVertexShader(shader_blob->GetBufferPointer(),
	                                        shader_blob->GetBufferSize(),
	                                        &m_vshader)))
	{
		shader_blob->Release();
		return OpStatus::ERR;
	}

	if (FAILED(m_D3DReflectShaderProc(shader_blob->GetBufferPointer(),
	                                  shader_blob->GetBufferSize(),
	                                  &vertex_reflection)))
	{
		shader_blob->Release();
		m_vshader->Release();
		m_vshader = NULL;
		return OpStatus::ERR;
	}

	m_vshaderBlob = shader_blob;

	m_vsSource = (const BYTE*)m_vshaderBlob->GetBufferPointer();
	m_vsSourceSize = m_vshaderBlob->GetBufferSize();

	// We can't Release() the vertex shader blob here as that would render
	// m_vsSource invalid


	RETURN_IF_ERROR(m_pixelConstants.initializeForCustomShader(m_device, pixel_reflection));
	RETURN_IF_ERROR(m_vertexConstants.initializeForCustomShader(m_device, vertex_reflection));

	RETURN_IF_ERROR(m_vertexConstants.addAttributes(m_vshaderObject->getAttributeCount(),
	                                                m_vshaderObject->getAttributes()));

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10ShaderProgram::setAsRenderTarget(VEGA3dRenderTarget *target)
{
	return OpStatus::OK;
}

OP_STATUS VEGAD3d10ShaderProgram::validate()
{
	// FIXME
	return OpStatus::OK;
}

bool VEGAD3d10ShaderProgram::isValid()
{
	// FIXME
	return true;
}

unsigned int VEGAD3d10ShaderProgram::getNumInputs()
{
	// Inputs == number of attributes used by the vertex shaders.
	return m_vertexConstants.getInputCount();
}

unsigned int VEGAD3d10ShaderProgram::getNumConstants()
{
	// Constants (uniforms) for vertex + pixel shaders considered separate
	return (m_pixelConstants.getConstantCount() + m_vertexConstants.getConstantCount());
}

OP_STATUS VEGAD3d10ShaderProgram::indexToAttributeName(unsigned int idx, const char** name)
{
	return m_vertexConstants.indexToAttributeName(idx, name);
}

int VEGAD3d10ShaderProgram::getSemanticIndex(unsigned int idx)
{
	return m_vertexConstants.getSemanticIndex(idx);
}

OP_STATUS VEGAD3d10ShaderProgram::getConstantDescription(unsigned int idx, ConstantType* type, unsigned int* size, const char** name)
{
	const unsigned vc = m_vertexConstants.getConstantCount();
	if (idx < vc)
		return m_vertexConstants.getConstantDescription(idx, type, size, name);
	return m_pixelConstants.getConstantDescription(idx - vc, type, size, name);
}

int VEGAD3d10ShaderProgram::attribToSemanticIndex(const char *name)
{
	return m_vertexConstants.attribToSemanticIndex(name);
}
#endif // CANVAS3D_SUPPORT

int VEGAD3d10ShaderProgram::getConstantLocation(const char* name)
{
	const int loc = m_pixelConstants.lookupConstName(name);
	if (loc >= 0)
		return loc;
	return (m_vertexConstants.lookupConstName(name) | FROM_VSHADER_BIT);
}

int VEGAD3d10ShaderProgram::getConstantOffset(int idx)
{
	if (idx < 0)
		return idx;
	if (idx & FROM_VSHADER_BIT)
		return m_vertexConstants.getConstantOffset(idx & ~FROM_VSHADER_BIT);
	else
		return m_pixelConstants.getConstantOffset(idx);
}

OP_STATUS VEGAD3d10ShaderProgram::setScalar(int idx, float val)
{
	return setScalar(idx, &val, 1);
}
OP_STATUS VEGAD3d10ShaderProgram::setScalar(int idx, int val)
{
	return setScalar(idx, &val, 1);
}
OP_STATUS VEGAD3d10ShaderProgram::setScalar(int idx, float* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	float *const dst = (float*)(((char*)shader_constants.m_ramBuffer) + off);
	for (int i = 0; i < count; ++i)
		dst[i] = val[i];

	return OpStatus::OK;
}
OP_STATUS VEGAD3d10ShaderProgram::setScalar(int idx, int* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	INT32 *const dst = (INT32*)(((char*)shader_constants.m_ramBuffer) + off);
	for (int i = 0; i < count; ++i)
		dst[i] = val[i];

	return OpStatus::OK;
}
OP_STATUS VEGAD3d10ShaderProgram::setVector2(int idx, float* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	float *const dst = (float*)(((char*)shader_constants.m_ramBuffer) + off);
	for (int i = 0; i < count; ++i)
	{
		dst[2*i] = val[2*i];
		dst[2*i + 1] = val[2*i + 1];
	}

	return OpStatus::OK;
}
OP_STATUS VEGAD3d10ShaderProgram::setVector2(int idx, int* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	INT32 *const dst = (INT32*)(((char*)shader_constants.m_ramBuffer) + off);
	for (int i = 0; i < count; ++i)
	{
		dst[2*i] = val[2*i];
		dst[2*i + 1] = val[2*i + 1];
	}

	return OpStatus::OK;
}
OP_STATUS VEGAD3d10ShaderProgram::setVector3(int idx, float* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	float *const dst = (float*)(((char*)shader_constants.m_ramBuffer) + off);
	for (int i = 0; i < count; ++i)
	{
		dst[3*i] = val[3*i];
		dst[3*i + 1] = val[3*i + 1];
		dst[3*i + 2] = val[3*i + 2];
	}

	return OpStatus::OK;
}
OP_STATUS VEGAD3d10ShaderProgram::setVector3(int idx, int* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	INT32 *const dst = (INT32*)(((char*)shader_constants.m_ramBuffer) + off);
	for (int i = 0; i < count; ++i)
	{
		dst[3*i] = val[3*i];
		dst[3*i + 1] = val[3*i + 1];
		dst[3*i + 2] = val[3*i + 2];
	}

	return OpStatus::OK;
}
OP_STATUS VEGAD3d10ShaderProgram::setVector4(int idx, float* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	op_memcpy((float*)(((char*)shader_constants.m_ramBuffer) + off), val, count * 4 * sizeof(float));

	return OpStatus::OK;
}
OP_STATUS VEGAD3d10ShaderProgram::setVector4(int idx, int* val, int count)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	op_memcpy((int*)(((char*)shader_constants.m_ramBuffer) + off), val, count * 4 * sizeof(float));

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10ShaderProgram::setMatrix2(int idx, float* val, int count, bool transpose)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	float *const dst = (float*)(((char*)shader_constants.m_ramBuffer) + off);

	// HLSL Packing Rules for Constant Variables says a matrix2x2 will be 24 bytes (or 4 + 2 floats).
	// Since each struct causes the next variable to start on a vec4 boundary the stride will be 8 floats.
	int dstMatrixStride = 4 + 4;
	for (unsigned i = 0; i < (unsigned)count; ++i)
		copy_matrix(dst + dstMatrixStride * i, val + 2 * 2 * i, 2, !transpose);

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10ShaderProgram::setMatrix3(int idx, float* val, int count, bool transpose)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	float *const dst = (float*)(((char*)shader_constants.m_ramBuffer) + off);

	// HLSL Packing Rules for Constant Variables says a matrix3x3 will be 44 bytes (or 4 + 4 + 3 floats).
	// Since each struct causes the next variable to start on a vec4 boundary the stride will be 12 floats.
	int dstMatrixSize = 4 + 4 + 4;
	for (unsigned i = 0; i < (unsigned)count; ++i)
		copy_matrix(dst + dstMatrixSize * i, val + 3 * 3 * i, 3, !transpose);

	return OpStatus::OK;
}

OP_STATUS VEGAD3d10ShaderProgram::setMatrix4(int idx, float* val, int count, bool transpose)
{
	if (idx < 0)
		return OpStatus::ERR;
	OP_ASSERT(g_vegaGlobals.vega3dDevice->getCurrentShaderProgram() == this);

	int off = getConstantOffset(idx);
	if (off < 0)
		return OpStatus::ERR;

	VEGAD3d10ShaderConstants& shader_constants = (idx & FROM_VSHADER_BIT) != 0 ? m_vertexConstants : m_pixelConstants;
	shader_constants.m_dirty = true;
	float *const dst = (float*)(((char*)shader_constants.m_ramBuffer) + off);

	// HLSL Packing Rules for Constant Variables won't affect 4x4 matrices.
	if (transpose)
	{
		op_memcpy(dst, val, count * 16 * sizeof(float));
	}
	else
	{
		for (unsigned i = 0; i < (unsigned)count; ++i)
			copy_matrix(dst + 16 * i, val + 16 * i, 4, true);
	}

	return OpStatus::OK;
}

#ifdef CANVAS3D_SUPPORT
OP_STATUS VEGAD3d10ShaderProgram::getConstantValue(int idx, float* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex)
{
	if (idx < 0)
		return OpStatus::ERR;
	if (idx & FROM_VSHADER_BIT)
		return m_vertexConstants.getConstantValue(idx & ~FROM_VSHADER_BIT, val, type, arrayIndex);
	else
		return m_pixelConstants.getConstantValue(idx, val, type, arrayIndex);
}

OP_STATUS VEGAD3d10ShaderProgram::getConstantValue(int idx, int* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex)
{
	if (idx < 0)
		return OpStatus::ERR;
	if (idx & FROM_VSHADER_BIT)
		return m_vertexConstants.getConstantValue(idx & ~FROM_VSHADER_BIT, val, type, arrayIndex);
	else
		return m_pixelConstants.getConstantValue(idx, val, type, arrayIndex);
}
#endif // CANVAS3D_SUPPORT

ID3D10Buffer* VEGAD3d10ShaderProgram::getVertConstBuffer()
{
	if (m_vertexConstants.m_slotCount && m_vertexConstants.m_dirty)
	{
		void* data;
		m_vertexConstants.m_cbuffer->Map(D3D10_MAP_WRITE_DISCARD, NULL, &data);
		op_memcpy(data, m_vertexConstants.m_ramBuffer, m_vertexConstants.m_slotCount * 4 * sizeof(float));
		m_vertexConstants.m_cbuffer->Unmap();
	}
	m_vertexConstants.m_dirty = false;
	return m_vertexConstants.m_cbuffer;
}

ID3D10Buffer* VEGAD3d10ShaderProgram::getPixConstBuffer()
{
	if (m_pixelConstants.m_slotCount && m_pixelConstants.m_dirty)
	{
		void* data;
		m_pixelConstants.m_cbuffer->Map(D3D10_MAP_WRITE_DISCARD, NULL, &data);
		op_memcpy(data, m_pixelConstants.m_ramBuffer, m_pixelConstants.m_slotCount * 4 * sizeof(float));
		m_pixelConstants.m_cbuffer->Unmap();
	}
	m_pixelConstants.m_dirty = false;
	return m_pixelConstants.m_cbuffer;
}

#endif //NO_DXSDK
#endif // VEGA_BACKEND_DIRECT3D10
