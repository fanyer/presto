/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAGLSHADER_H
#define VEGAGLSHADER_H

#ifdef VEGA_BACKEND_OPENGL

#include "modules/libvega/vega3ddevice.h"

class VEGAGlDevice;

#ifdef CANVAS3D_SUPPORT
class VEGAGlShader : public VEGA3dShader
{
public:
	VEGAGlShader(VEGAGlDevice* dev, BOOL frag) : device(dev), shader_object(0), fragmentShader(frag)
	{}
	~VEGAGlShader();

	OP_STATUS Construct(const char* source, OpString *info_log);

	BOOL isFragmentShader(){return fragmentShader;}
	BOOL isVertexShader(){return !fragmentShader;}

	VEGAGlDevice* device;
	GLuint shader_object;
	BOOL fragmentShader;
};
#endif // CANVAS3D_SUPPORT

class VEGAGlShaderProgram : public VEGA3dShaderProgram, public Link
{
public:
	VEGAGlShaderProgram(VEGAGlDevice* dev) : device(dev), program_object(0), m_numActiveUniforms(0), m_activeUniformSizes(NULL), m_type(VEGA3dShaderProgram::SHADER_CUSTOM), m_activeInputs(NULL)
#ifdef VEGA_BACKENDS_OPENGL_FLIPY
	 ,m_flipY(-1) 
#endif
	{}
	~VEGAGlShaderProgram();

	OP_STATUS loadProgram(VEGA3dShaderProgram::ShaderType shdtype, VEGA3dShaderProgram::WrapMode shdmode);

	virtual ShaderType getShaderType(){return m_type;}

#ifdef CANVAS3D_SUPPORT
	OP_STATUS addShader(VEGA3dShader* shader);
	OP_STATUS removeShader(VEGA3dShader* shader);
	OP_STATUS link(OpString *error_log);
	virtual OP_STATUS setAsRenderTarget(VEGA3dRenderTarget *target);

	virtual OP_STATUS validate();
	virtual bool isValid();

	virtual unsigned int getNumInputs();
	virtual unsigned int getNumConstants();

	virtual unsigned int getInputMaxLength();
	virtual unsigned int getConstantMaxLength();

	/// NOTE: may overwrite global 2k temp-buffer
	virtual OP_STATUS indexToAttributeName(unsigned int idx, const char **name);

	virtual OP_STATUS getConstantDescription(unsigned int idx, ConstantType* type, unsigned int* size, const char** name);

	virtual OP_STATUS setInputLocation(const char* name, int idx);
#endif // CANVAS3D_SUPPORT
	virtual int getConstantLocation(const char* name);

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

	virtual bool isAttributeActive(int idx);
#endif // CANVAS3D_SUPPORT

#ifdef VEGA_BACKENDS_OPENGL_FLIPY
	virtual bool orthogonalProjectionNeedsUpdate();
	void setFlipY(bool flipY) { m_flipY = flipY?1:0; }
#endif // VEGA_BACKENDS_OPENGL_FLIPY

	void clearDevice();

	VEGAGlDevice* device;
	GLuint program_object;
private:

	OP_STATUS updateActiveUniformSizes();
	OP_STATUS updateAttributeStatus();

	GLint m_maxUniformIndex;		// Num active uniforms, including each array index.
	GLint m_numActiveUniforms;		// Num active uniforms counting arrays as one uniform.
	GLint *m_activeUniformSizes;	// Size of each of the active uniforms.

	ShaderType m_type;
	bool *m_activeInputs;
#ifdef VEGA_BACKENDS_OPENGL_FLIPY
	int m_flipY;
#endif
};

#endif // VEGA_BACKEND_OPENGL
#endif // VEGAGLSHADER_H
