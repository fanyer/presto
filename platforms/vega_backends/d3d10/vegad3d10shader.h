/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAD3D10SHADER_H
#define VEGAD3D10SHADER_H

#ifdef VEGA_BACKEND_DIRECT3D10
#include "modules/libvega/vega3ddevice.h"
#include "modules/util/stringhash.h"
#ifdef CANVAS3D_SUPPORT
#include <D3Dcompiler.h>
#include "platforms/vega_backends/d3d10/vegad3d10device.h"
#ifndef NO_DXSDK

class VEGAD3d10Shader
	: public VEGA3dShader
{
public:
	VEGAD3d10Shader()
		: m_isPixelShader(FALSE),
		  m_attribute_count(0),
		  m_attributes(NULL)
	{
	}

	~VEGAD3d10Shader();

	OP_STATUS Construct(const char* src, BOOL pixelShader, unsigned int attribute_count, VEGA3dDevice::AttributeData *attributes);

	const char* getSource(){return m_source.CStr();}
	unsigned int getSourceLength(){return m_source.Length();}

	virtual BOOL isFragmentShader(){return m_isPixelShader;}
	virtual BOOL isVertexShader(){return !m_isPixelShader;}

	VEGA3dDevice::AttributeData* getAttributes(){return m_attributes;}
	unsigned int getAttributeCount(){return m_attribute_count;}
private:
	OpString8 m_source;
	BOOL m_isPixelShader;
	unsigned int m_attribute_count;
	VEGA3dDevice::AttributeData* m_attributes;
};
#endif //NO_DXSDK
#endif // CANVAS3D_SUPPORT

#ifndef NO_DXSDK

class VEGAD3d10ShaderConstants
{
public:
	VEGAD3d10ShaderConstants()
	    : m_names(NULL),
	      m_values(NULL),
	      m_count(0),
	      m_slotCount(0),
#ifdef CANVAS3D_SUPPORT
	      m_shader_reflection(NULL),
#endif // CANVAS3D_SUPPORT
	      m_ramBuffer(NULL),
	      m_cbuffer(NULL),
	      m_dirty(true) {}

	~VEGAD3d10ShaderConstants()
	{
		Release();
	}

	void Release();

	int getConstantOffset(int idx);
#ifdef CANVAS3D_SUPPORT
	OP_STATUS getConstantValue(int idx, float* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex);
	OP_STATUS getConstantValue(int idx, int* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex);
#endif // CANVAS3D_SUPPORT
	int lookupConstName(const char* name);
#ifdef CANVAS3D_SUPPORT
	unsigned int getInputCount() { return m_attrib_to_semantic_index.count(); }
	unsigned int getInputMaxLength();

	int attribToSemanticIndex(const char *name);

	unsigned int getConstantCount() { return m_count; }
	unsigned int getConstantMaxLength();

	/// NOTE: may overwrite global 2k temp-buffer
	OP_STATUS indexToAttributeName(unsigned idx, const char **name);

	OP_STATUS getConstantDescription(unsigned int idx, VEGA3dShaderProgram::ConstantType* type, unsigned int* size, const char** name);

	int getSemanticIndex(unsigned int idx);
	OP_STATUS initializeForCustomShader(ID3D10Device1 *dev,
	                                    ID3D10ShaderReflection *shader_reflection);
	OP_STATUS addAttributes(unsigned int attr_count, VEGA3dDevice::AttributeData* attributes);
#endif // CANVAS3D_SUPPORT

	// The variable with name names[i] has the offset values[i] within the
	// cbuffer
	const char *const *m_names;
	const unsigned int *m_values;
	unsigned int m_count;
	unsigned int m_slotCount;

#ifdef CANVAS3D_SUPPORT
	ID3D10ShaderReflection *m_shader_reflection;
#endif // CANVAS3D_SUPPORT

	void* m_ramBuffer;
	ID3D10Buffer* m_cbuffer;

	bool m_dirty;

#ifdef CANVAS3D_SUPPORT
	StringHash<unsigned> m_attrib_to_semantic_index;
#endif // CANVAS3D_SUPPORT

};

class VEGAD3d10ShaderProgram : public VEGA3dShaderProgram
{
public:
	VEGAD3d10ShaderProgram(ID3D10Device1* dev, bool level9);
	~VEGAD3d10ShaderProgram();
#ifdef CANVAS3D_SUPPORT
	OP_STATUS loadShader(VEGA3dShaderProgram::ShaderType shdtype, WrapMode mode, pD3DCompile D3DCompileProc, VEGAD3D10_pD3DReflectShader D3DReflectShaderProc);
#else
	OP_STATUS loadShader(VEGA3dShaderProgram::ShaderType shdtype, WrapMode mode);
#endif

	virtual ShaderType getShaderType(){return m_type;}

#ifdef CANVAS3D_SUPPORT
	/** For custom shaders. */
	virtual OP_STATUS addShader(VEGA3dShader* shader);
	virtual OP_STATUS removeShader(VEGA3dShader* shader);
	virtual OP_STATUS link(OpString *info_log);
	virtual OP_STATUS setAsRenderTarget(VEGA3dRenderTarget *target);

	virtual OP_STATUS validate();
	virtual bool isValid();

	virtual unsigned int getNumInputs();
	virtual unsigned int getNumConstants();

	virtual unsigned int getInputMaxLength() { return m_vertexConstants.getInputMaxLength(); }
	virtual unsigned int getConstantMaxLength() { return MAX(m_pixelConstants.getConstantMaxLength(), m_vertexConstants.getConstantMaxLength()); }

	/// NOTE: may overwrite global 2k temp-buffer
	virtual OP_STATUS indexToAttributeName(unsigned int idx, const char **name);

	virtual OP_STATUS getConstantDescription(unsigned int idx, ConstantType* type, unsigned int* size, const char** name);

	int getSemanticIndex(unsigned int idx);

	/** Maps an attribute name to the corresponding semantic
	    index for the shader. For example, if we have 'float4 foo : TEXCOORD3'
	    in the shader, then attribToSemanticIndex("foo") = 3.

	    Returns -1 if the shader does not have an attribute with the
	    specified name. */
	virtual int attribToSemanticIndex(const char *name);

#endif // CANVAS3D_SUPPORT
	virtual int getConstantLocation(const char* name);

	int getConstantOffset(int idx);

	virtual OP_STATUS setScalar(int idx, float val);
	virtual OP_STATUS setScalar(int idx, int val);
	virtual OP_STATUS setScalar(int idx, float* val, int count);
	virtual OP_STATUS setScalar(int idx, int* val, int count);
	virtual OP_STATUS setVector2(int idx, float* val, int count);
	virtual OP_STATUS setVector2(int idx, int* val, int count);
	virtual OP_STATUS setVector3(int idx, float* val, int count);
	virtual OP_STATUS setVector3(int idx, int* val, int count);
	virtual OP_STATUS setVector4(int idx, float* val, int count);
	virtual OP_STATUS setVector4(int idx, int* val, int count);
	virtual OP_STATUS setMatrix2(int idx, float* val, int count, bool transpose);
	virtual OP_STATUS setMatrix3(int idx, float* val, int count, bool transpose);
	virtual OP_STATUS setMatrix4(int idx, float* val, int count, bool transpose);

#ifdef CANVAS3D_SUPPORT
	virtual OP_STATUS getConstantValue(int idx, float* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex);
	virtual OP_STATUS getConstantValue(int idx, int* val, VEGA3dShaderProgram::ConstantType type, int arrayIndex);
#endif // CANVAS3D_SUPPORT

	ID3D10PixelShader* getPixelShader(){return m_pshader;}
	ID3D10VertexShader* getVertexShader(){return m_vshader;}
	const BYTE* getShaderData(){return m_vsSource;}
	unsigned int getShaderDataSize(){return m_vsSourceSize;}
	ID3D10Buffer* getVertConstBuffer();
	ID3D10Buffer* getPixConstBuffer();
	bool isVertConstantDataModified(){return m_vertexConstants.m_dirty;}
	bool isPixConstantDataModified(){return m_pixelConstants.m_dirty;}
	unsigned int getLinkId() const { return m_linkId; }
private:
	/**
	 * Helper function. If 'error_blob' is non-null, appends its contents to
	 * 'error_log' (if it is non-null), and then Release()s 'error_blob'. Does
	 * nothing if 'error_blob' is null.
	 */
	void registerErrorsFromBlob(ID3D10Blob *error_blob, OpString *error_log);

	ID3D10Device1* m_device;

	ID3D10PixelShader* m_pshader;
	ID3D10VertexShader* m_vshader;
	ID3D10Blob* m_vshaderBlob;

	VEGAD3d10ShaderConstants m_pixelConstants;
	VEGAD3d10ShaderConstants m_vertexConstants;

#ifdef CANVAS3D_SUPPORT
	VEGAD3d10Shader *m_pshaderObject;
	VEGAD3d10Shader *m_vshaderObject;
#endif // CANVAS3D_SUPPORT

	const BYTE* m_vsSource;
	unsigned int m_vsSourceSize;

	bool m_featureLevel9;

	ShaderType m_type;
#ifdef CANVAS3D_SUPPORT
	pD3DCompile m_D3DCompileProc;
	VEGAD3D10_pD3DReflectShader m_D3DReflectShaderProc;
#endif
	unsigned int m_linkId;
};

#endif //NO_DXSDK
#endif // VEGA_BACKEND_DIRECT3D10
#endif // !VEGAD3D10SHADER_H

