/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CANVASWEBGLSHADER_H
#define CANVASWEBGLSHADER_H

#ifdef CANVAS3D_SUPPORT

#include "modules/libvega/src/canvas/webglutils.h"

class WGL_ShaderVariables;
class CanvasWebGLShader : public WebGLRefCounted
{
public:
	CanvasWebGLShader(BOOL fragmentShader);
	~CanvasWebGLShader();

	VEGA3dShader* getShader(){return m_shader;}
	BOOL isFragmentShader(){return m_fragmentShader;}

	const uni_char* getSource(){return m_source.CStr();}
	unsigned int getSourceLength(){return m_source.Length();}
	OP_STATUS setSource(const uni_char* src){return m_source.Set(src);}

	OP_STATUS compile(const uni_char *url, BOOL validate, unsigned int extensions_enabled);
	BOOL isCompiled(){return m_compiled;}

	const uni_char* getInfoLog(){return m_info_log.CStr();}
	unsigned int getInfoLogLength(){return m_info_log.Length();}

	WGL_ShaderVariables* getShaderVariables(){return m_variables;}
	void setShaderVariables(WGL_ShaderVariables* vs){m_variables = vs;}

	virtual void destroyInternal();
private:
	VEGA3dShader* m_shader;

	BOOL m_fragmentShader;
	OpString m_source;

	BOOL m_compiled;

	OpString m_info_log;
	WGL_ShaderVariables* m_variables;
};


#endif //CANVAS3D_SUPPORT
#endif //CANVASWEBGLSHADER_H
