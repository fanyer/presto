/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMCONTEXTWEBGL_H
#define DOM_DOMCONTEXTWEBGL_H
#ifdef CANVAS3D_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/canvas/domcontext2d.h"

class CanvasContextWebGL;
class DOM_HTMLCanvasElement;
class DOMCanvasContextWebGL;

class CanvasWebGLProgram;
class CanvasWebGLShader;
class CanvasWebGLTexture;
class CanvasWebGLFramebuffer;
class CanvasWebGLRenderbuffer;
class CanvasWebGLBuffer;

class DOMWebGLContextAttributes
	: public DOM_Object
{
public:
	DOMWebGLContextAttributes() : m_alpha(TRUE), m_depth(TRUE), m_stencil(FALSE), m_antialias(TRUE), m_premultipliedAlpha(TRUE), m_preserveDrawingBuffer(FALSE) {}
	virtual ~DOMWebGLContextAttributes() {}
	static OP_STATUS Make(DOMWebGLContextAttributes*& attr, DOM_Runtime* runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLCONTEXTATTRIBUTES || DOM_Object::IsA(type); }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	BOOL m_alpha;
	BOOL m_depth;
	BOOL m_stencil;
	BOOL m_antialias;
	BOOL m_premultipliedAlpha;
	BOOL m_preserveDrawingBuffer;
};

class DOMWebGLObject
	: public DOM_Object
{
public:
	virtual ~DOMWebGLObject() {}
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLOBJECT || DOM_Object::IsA(type); }
};

class DOMWebGLExtension
	: public DOM_Object
{
public:
	DOMWebGLExtension(const char *name)
		: name(name)
	{
	}

	virtual ~DOMWebGLExtension()
	{
	}

	/** Create a new extension object. The created object will keep a reference to the
	 * name string passed in without copying it, so the caller must make sure the
	 * string is not deleted or modified before the object is deleted. */
	static OP_STATUS Make(DOMWebGLExtension*& extension, DOM_Runtime* runtime, const char* name);
	static OP_STATUS Initialize(DOMWebGLExtension* extension, DOM_Runtime* runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLEXTENSION || DOM_Object::IsA(type); }

	const char* GetExtensionName() { return name; }

protected:
	const char* name;
};

class DOMWebGLFramebuffer
	: public DOMWebGLObject
{
public:
	DOMWebGLFramebuffer(CanvasWebGLFramebuffer* buf, DOMCanvasContextWebGL* owner);
	virtual ~DOMWebGLFramebuffer();

	static OP_STATUS Make(DOMWebGLFramebuffer*& buf, DOMCanvasContextWebGL* owner);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLFRAMEBUFFER || DOMWebGLObject::IsA(type); }
	virtual void GCTrace();

	CanvasWebGLFramebuffer* m_framebuffer;
	DOMCanvasContextWebGL* m_owner;
};

class DOMWebGLRenderbuffer
	: public DOMWebGLObject
{
public:
	DOMWebGLRenderbuffer(CanvasWebGLRenderbuffer* buf, DOMCanvasContextWebGL* owner);
	virtual ~DOMWebGLRenderbuffer();

	static OP_STATUS Make(DOMWebGLRenderbuffer*& buf, DOMCanvasContextWebGL* owner);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLRENDERBUFFER || DOMWebGLObject::IsA(type); }
	virtual void GCTrace();

	CanvasWebGLRenderbuffer* m_renderbuffer;
	DOMCanvasContextWebGL* m_owner;
};

class DOMWebGLBuffer
	: public DOMWebGLObject
{
public:
	DOMWebGLBuffer(CanvasWebGLBuffer* buf, DOMCanvasContextWebGL* owner);
	virtual ~DOMWebGLBuffer();

	static OP_STATUS Make(DOMWebGLBuffer*& buf, DOMCanvasContextWebGL* owner);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLBUFFER || DOMWebGLObject::IsA(type); }
	virtual void GCTrace();

	CanvasWebGLBuffer* m_buffer;
	DOMCanvasContextWebGL* m_owner;
};

class DOMWebGLProgram
	: public DOMWebGLObject
{
public:
	DOMWebGLProgram(CanvasWebGLProgram* prog, DOMCanvasContextWebGL* owner);
	virtual ~DOMWebGLProgram();

	static OP_STATUS Make(DOMWebGLProgram*& prog, DOMCanvasContextWebGL* owner);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLPROGRAM || DOMWebGLObject::IsA(type); }
	virtual void GCTrace();

	CanvasWebGLProgram* m_program;
	DOMCanvasContextWebGL* m_owner;
};

class DOMWebGLShader
	: public DOMWebGLObject
{
public:
	DOMWebGLShader(CanvasWebGLShader* shd, DOMCanvasContextWebGL* owner);
	virtual ~DOMWebGLShader();

	static OP_STATUS Make(DOMWebGLShader*& shd, DOMCanvasContextWebGL* owner, BOOL fs);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLSHADER || DOMWebGLObject::IsA(type); }
	virtual void GCTrace();

	CanvasWebGLShader* m_shader;
	DOMCanvasContextWebGL* m_owner;
};

class DOMWebGLTexture
	: public DOMWebGLObject
{
public:
	DOMWebGLTexture(CanvasWebGLTexture* tex, DOMCanvasContextWebGL* owner);
	virtual ~DOMWebGLTexture();

	static OP_STATUS Make(DOMWebGLTexture*& tex, DOMCanvasContextWebGL* owner);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLTEXTURE || DOMWebGLObject::IsA(type); }
	virtual void GCTrace();

	CanvasWebGLTexture* m_texture;
	DOMCanvasContextWebGL* m_owner;
};

class DOMWebGLObjectArray
	: public DOM_Object
{
public:
	DOMWebGLObjectArray(unsigned int len, DOMWebGLObject** objs) : m_length(len), m_objects(objs) {}
	virtual ~DOMWebGLObjectArray()
	{
		OP_DELETEA(m_objects);
	}
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLOBJECTARRAY || DOM_Object::IsA(type); }
	virtual void GCTrace();

	static OP_STATUS Make(DOMWebGLObjectArray*& oa, DOM_Runtime* runtime, unsigned int len);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	unsigned int m_length;
	DOMWebGLObject** m_objects;
};

class DOMWebGLUniformLocation
	: public DOM_Object
{
public:
	DOMWebGLUniformLocation(int idx, CanvasWebGLProgram *program);
	virtual ~DOMWebGLUniformLocation() {}
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLUNIFORMLOCATION || DOM_Object::IsA(type); }
	virtual void GCTrace();

	static OP_STATUS Make(DOMWebGLUniformLocation*& loc, DOM_Runtime* runtime, int idx, const uni_char* name, CanvasWebGLProgram *prog);

	int m_index;
	OpString8 m_name;
	CanvasWebGLProgram *m_program;
	unsigned int m_linkId;         // The link id for the program when this location was extracted.
};

class DOMWebGLActiveInfo
	: public DOM_Object
{
public:
	virtual ~DOMWebGLActiveInfo()
	{}
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBGLACTIVEINFO || DOM_Object::IsA(type); }

	static OP_STATUS Make(DOMWebGLActiveInfo*& ai, DOM_Runtime* runtime, OpString& name, unsigned int type, unsigned int size);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
private:
	OpString m_name;
	unsigned int m_type;
	unsigned int m_size;
};

class DOMCanvasContextWebGL
	: public DOM_Object
{
protected:
	DOMCanvasContextWebGL(DOM_HTMLCanvasElement *domcanvas, CanvasContextWebGL *ctx);
	OP_STATUS Initialize();

	DOM_HTMLCanvasElement* m_domcanvas;
	CanvasContextWebGL* m_context;

	OpVector<DOMWebGLExtension> m_enabled_extensions;

public:
	~DOMCanvasContextWebGL();

	static OP_STATUS Make(DOMCanvasContextWebGL*& ctx, DOM_HTMLCanvasElement *domcanvas, DOMWebGLContextAttributes* attributes);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CANVASCONTEXTWEBGL || DOM_Object::IsA(type); }
	virtual void GCTrace();

	static BOOL IsExtensionSupported(DOMCanvasContextWebGL* domctx, const char* name);
	/**< TRUE if this extension is supported for the given context. */

	static const char* SupportedExtensions(unsigned int i);
	/* Hardcoded list of possible supported extensions */

	static unsigned supportedExtensionsCount;

	static void ConstructCanvasContextWebGLObjectL(ES_Object *object, DOM_Runtime *runtime);

	DOM_DECLARE_FUNCTION(getContextAttributes);
	DOM_DECLARE_FUNCTION(isContextLost);

	DOM_DECLARE_FUNCTION(getSupportedExtensions);
	DOM_DECLARE_FUNCTION(getExtension);

	DOM_DECLARE_FUNCTION(activeTexture);
	DOM_DECLARE_FUNCTION(attachShader);
	DOM_DECLARE_FUNCTION(bindAttribLocation);
	DOM_DECLARE_FUNCTION(bindBuffer);
	DOM_DECLARE_FUNCTION(bindFramebuffer);
	DOM_DECLARE_FUNCTION(bindRenderbuffer);
	DOM_DECLARE_FUNCTION(bindTexture);
	DOM_DECLARE_FUNCTION(blendColor);
	DOM_DECLARE_FUNCTION(blendEquation);
	DOM_DECLARE_FUNCTION(blendEquationSeparate);
	DOM_DECLARE_FUNCTION(blendFunc);
	DOM_DECLARE_FUNCTION(blendFuncSeparate);

	DOM_DECLARE_FUNCTION(bufferData);
	DOM_DECLARE_FUNCTION(bufferSubData);

	DOM_DECLARE_FUNCTION(checkFramebufferStatus);
	DOM_DECLARE_FUNCTION(clear);
	DOM_DECLARE_FUNCTION(clearColor);
	DOM_DECLARE_FUNCTION(clearDepth);
	DOM_DECLARE_FUNCTION(clearStencil);
	DOM_DECLARE_FUNCTION(colorMask);
	DOM_DECLARE_FUNCTION(compileShader);

	DOM_DECLARE_FUNCTION(compressedTexImage2D);
	DOM_DECLARE_FUNCTION(compressedTexSubImage2D);

	DOM_DECLARE_FUNCTION(copyTexImage2D);
	DOM_DECLARE_FUNCTION(copyTexSubImage2D);

	DOM_DECLARE_FUNCTION(createBuffer);
	DOM_DECLARE_FUNCTION(createFramebuffer);
	DOM_DECLARE_FUNCTION(createProgram);
	DOM_DECLARE_FUNCTION(createRenderbuffer);
	DOM_DECLARE_FUNCTION(createShader);
	DOM_DECLARE_FUNCTION(createTexture);

	DOM_DECLARE_FUNCTION(cullFace);

	DOM_DECLARE_FUNCTION(deleteBuffer);
	DOM_DECLARE_FUNCTION(deleteFramebuffer);
	DOM_DECLARE_FUNCTION(deleteProgram);
	DOM_DECLARE_FUNCTION(deleteRenderbuffer);
	DOM_DECLARE_FUNCTION(deleteShader);
	DOM_DECLARE_FUNCTION(deleteTexture);

	DOM_DECLARE_FUNCTION(depthFunc);
	DOM_DECLARE_FUNCTION(depthMask);
	DOM_DECLARE_FUNCTION(depthRange);
	DOM_DECLARE_FUNCTION(detachShader);
	DOM_DECLARE_FUNCTION(disable);
	DOM_DECLARE_FUNCTION(disableVertexAttribArray);
	DOM_DECLARE_FUNCTION(drawArrays);
	DOM_DECLARE_FUNCTION(drawElements);

	DOM_DECLARE_FUNCTION(enable);
	DOM_DECLARE_FUNCTION(enableVertexAttribArray);
	DOM_DECLARE_FUNCTION(finish);
	DOM_DECLARE_FUNCTION(flush);
	DOM_DECLARE_FUNCTION(framebufferRenderbuffer);
	DOM_DECLARE_FUNCTION(framebufferTexture2D);
	DOM_DECLARE_FUNCTION(frontFace);
	DOM_DECLARE_FUNCTION(generateMipmap);

	DOM_DECLARE_FUNCTION(getActiveAttrib);
	DOM_DECLARE_FUNCTION(getActiveUniform);
	DOM_DECLARE_FUNCTION(getAttachedShaders);

	DOM_DECLARE_FUNCTION(getAttribLocation);

	DOM_DECLARE_FUNCTION(getParameter);
	DOM_DECLARE_FUNCTION(getBufferParameter);

	DOM_DECLARE_FUNCTION(getError);

	DOM_DECLARE_FUNCTION(getFramebufferAttachmentParameter);
	DOM_DECLARE_FUNCTION(getProgramParameter);
	DOM_DECLARE_FUNCTION(getProgramInfoLog);
	DOM_DECLARE_FUNCTION(getRenderbufferParameter);
	DOM_DECLARE_FUNCTION(getShaderParameter);
	DOM_DECLARE_FUNCTION(getShaderInfoLog);

	DOM_DECLARE_FUNCTION(getShaderSource);

	DOM_DECLARE_FUNCTION(getTexParameter);

	DOM_DECLARE_FUNCTION(getUniform);

	DOM_DECLARE_FUNCTION(getUniformLocation);

	DOM_DECLARE_FUNCTION(getVertexAttrib);

	DOM_DECLARE_FUNCTION(getVertexAttribOffset);

	DOM_DECLARE_FUNCTION(hint);
	DOM_DECLARE_FUNCTION(isBuffer);
	DOM_DECLARE_FUNCTION(isEnabled);
	DOM_DECLARE_FUNCTION(isFramebuffer);
	DOM_DECLARE_FUNCTION(isProgram);
	DOM_DECLARE_FUNCTION(isRenderbuffer);
	DOM_DECLARE_FUNCTION(isShader);
	DOM_DECLARE_FUNCTION(isTexture);
	DOM_DECLARE_FUNCTION(lineWidth);
	DOM_DECLARE_FUNCTION(linkProgram);
	DOM_DECLARE_FUNCTION(pixelStorei);
	DOM_DECLARE_FUNCTION(polygonOffset);

	DOM_DECLARE_FUNCTION(readPixels);

	DOM_DECLARE_FUNCTION(renderbufferStorage);
	DOM_DECLARE_FUNCTION(sampleCoverage);
	DOM_DECLARE_FUNCTION(scissor);

	DOM_DECLARE_FUNCTION(shaderSource);

	DOM_DECLARE_FUNCTION(stencilFunc);
	DOM_DECLARE_FUNCTION(stencilFuncSeparate);
	DOM_DECLARE_FUNCTION(stencilMask);
	DOM_DECLARE_FUNCTION(stencilMaskSeparate);
	DOM_DECLARE_FUNCTION(stencilOp);
	DOM_DECLARE_FUNCTION(stencilOpSeparate);

	DOM_DECLARE_FUNCTION(texImage2D);
	DOM_DECLARE_FUNCTION(texParameterf);
	DOM_DECLARE_FUNCTION(texParameteri);
	DOM_DECLARE_FUNCTION(texSubImage2D);

	DOM_DECLARE_FUNCTION(uniform1f);
	DOM_DECLARE_FUNCTION(uniform1fv);
	DOM_DECLARE_FUNCTION(uniform1i);
	DOM_DECLARE_FUNCTION(uniform1iv);
	DOM_DECLARE_FUNCTION(uniform2f);
	DOM_DECLARE_FUNCTION(uniform2fv);
	DOM_DECLARE_FUNCTION(uniform2i);
	DOM_DECLARE_FUNCTION(uniform2iv);
	DOM_DECLARE_FUNCTION(uniform3f);
	DOM_DECLARE_FUNCTION(uniform3fv);
	DOM_DECLARE_FUNCTION(uniform3i);
	DOM_DECLARE_FUNCTION(uniform3iv);
	DOM_DECLARE_FUNCTION(uniform4f);
	DOM_DECLARE_FUNCTION(uniform4fv);
	DOM_DECLARE_FUNCTION(uniform4i);
	DOM_DECLARE_FUNCTION(uniform4iv);

	DOM_DECLARE_FUNCTION(uniformMatrix2fv);
	DOM_DECLARE_FUNCTION(uniformMatrix3fv);
	DOM_DECLARE_FUNCTION(uniformMatrix4fv);

	DOM_DECLARE_FUNCTION(useProgram);
	DOM_DECLARE_FUNCTION(validateProgram);

	DOM_DECLARE_FUNCTION(vertexAttrib1f);
	DOM_DECLARE_FUNCTION(vertexAttrib1fv);
	DOM_DECLARE_FUNCTION(vertexAttrib2f);
	DOM_DECLARE_FUNCTION(vertexAttrib2fv);
	DOM_DECLARE_FUNCTION(vertexAttrib3f);
	DOM_DECLARE_FUNCTION(vertexAttrib3fv);
	DOM_DECLARE_FUNCTION(vertexAttrib4f);
	DOM_DECLARE_FUNCTION(vertexAttrib4fv);
	DOM_DECLARE_FUNCTION(vertexAttribPointer);

	DOM_DECLARE_FUNCTION(viewport);

	enum { FUNCTIONS_ARRAY_SIZE = 136 };
};

#endif // CANVAS3D_SUPPORT
#endif // !DOM_DOMCONTEXTWEBGL_H
