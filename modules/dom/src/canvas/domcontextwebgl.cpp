/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT

#include "modules/dom/src/canvas/domcontextwebgl.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domhtml/htmlelem.h"

#include "modules/ecmascript_utils/esasyncif.h"

#include "modules/libvega/src/canvas/canvas.h"
#include "modules/libvega/src/canvas/canvascontextwebgl.h"
#include "modules/libvega/src/canvas/canvaswebglprogram.h"
#include "modules/libvega/src/canvas/canvaswebglshader.h"
#include "modules/libvega/vega3ddevice.h"

#define WEBGL_CONSTANTS_CREATE_FUNCTION
#include "modules/dom/src/canvas/webglconstants.h"
#undef WEBGL_CONSTANTS_CREATE_FUNCTION

#include "modules/dom/src/canvas/domcontext2d.h"
#ifdef SVG_SUPPORT
#include "modules/svg/svg_image.h"
#include "modules/svg/SVGManager.h"
#include "modules/dom/src/domsvg/domsvgelement.h"
#endif // SVG_SUPPORT

#ifdef MEDIA_SUPPORT
#include "modules/media/mediaelement.h"
#include "modules/media/mediaplayer.h"
#endif // MEDIA_SUPPORT

#include "modules/logdoc/urlimgcontprov.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

#include "modules/security_manager/include/security_manager.h"

static BOOL
CanvasOriginCheckWebGL(DOM_Object *this_object, const URL& url1, const URL& url2, ES_Runtime* origining_runtime)
{
#ifdef GADGET_SUPPORT
	if (origining_runtime && origining_runtime->GetFramesDocument()->GetWindow()->GetType() == WIN_TYPE_GADGET)
	{
		BOOL allowed = FALSE;
		OpSecurityState secstate;
		OpStatus::Ignore(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_LOADSAVE,
															OpSecurityContext(static_cast<DOM_Runtime *>(origining_runtime)),
															OpSecurityContext(url1, url2),
															allowed,
															secstate));
		OP_ASSERT(!secstate.suspended);	// We're assuming that the host already has been resolved.

		return allowed;
	}
	else
#endif // GADGET_SUPPORT
		return this_object->OriginCheck(url1, url2);
}

static OP_STATUS
SetValueFromBOOLVec(const CanvasWebGLParameter& param, unsigned vecLen, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(vecLen <= ARRAY_SIZE(param.value.int_param));
	ES_Object *arrayObj;
	RETURN_IF_ERROR(origining_runtime->CreateNativeArrayObject(&arrayObj, vecLen));
	for (unsigned i = 0; i < vecLen; ++i)
	{
		ES_Value val;
		DOM_Object::DOMSetBoolean(&val, param.value.int_param[i] != 0);
		RETURN_IF_ERROR(origining_runtime->PutIndex(arrayObj, i, val));
	}

	DOM_Object::DOMSetObject(return_value, arrayObj);
	return OpStatus::OK;
}

static OP_STATUS
SetValueFromUBYTEVec(const CanvasWebGLParameter& param, unsigned vecLen, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(vecLen <= ARRAY_SIZE(param.value.ubyte_param));
	unsigned length = sizeof(UINT8)*vecLen;
	ES_Object* values;
	RETURN_IF_ERROR(origining_runtime->CreateNativeArrayBufferObject(&values, length));
	op_memcpy(origining_runtime->GetByteArrayStorage(values), param.value.ubyte_param, length);
	ES_Object* ta;
	RETURN_IF_ERROR(origining_runtime->CreateNativeTypedArrayObject(&ta, ES_Runtime::TYPED_ARRAY_UINT8, values));
	DOM_Object::DOMSetObject(return_value, ta);
	return OpStatus::OK;
}

static OP_STATUS
SetValueFromIntVec(const CanvasWebGLParameter& param, unsigned vecLen, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(vecLen <= ARRAY_SIZE(param.value.int_param));
	unsigned length = sizeof(INT32)*vecLen;
	ES_Object* values;
	RETURN_IF_ERROR(origining_runtime->CreateNativeArrayBufferObject(&values, length));
	op_memcpy(origining_runtime->GetByteArrayStorage(values), param.value.int_param, length);
	ES_Object* ta;
	RETURN_IF_ERROR(origining_runtime->CreateNativeTypedArrayObject(&ta, ES_Runtime::TYPED_ARRAY_INT32, values));
	DOM_Object::DOMSetObject(return_value, ta);
	return OpStatus::OK;
}

static OP_STATUS
SetValueFromFloatVec(const CanvasWebGLParameter& param, unsigned vecLen, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(vecLen <= ARRAY_SIZE(param.value.float_param));
	unsigned length = sizeof(float)*vecLen;
	ES_Object* values;
	RETURN_IF_ERROR(origining_runtime->CreateNativeArrayBufferObject(&values, length));
	op_memcpy(origining_runtime->GetByteArrayStorage(values), param.value.float_param, length);
	ES_Object* ta;
	RETURN_IF_ERROR(origining_runtime->CreateNativeTypedArrayObject(&ta, ES_Runtime::TYPED_ARRAY_FLOAT32, values));
	DOM_Object::DOMSetObject(return_value, ta);
	return OpStatus::OK;
}


static OP_STATUS
SetValueFromParameter(const CanvasWebGLParameter& param, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (param.type)
	{
	case CanvasWebGLParameter::PARAM_BOOL:
		DOM_Object::DOMSetBoolean(return_value, param.value.bool_param);
		break;
	case CanvasWebGLParameter::PARAM_BOOL2:
		RETURN_IF_ERROR(SetValueFromBOOLVec(param, 2, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_BOOL3:
		RETURN_IF_ERROR(SetValueFromBOOLVec(param, 3, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_BOOL4:
		RETURN_IF_ERROR(SetValueFromBOOLVec(param, 4, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_UBYTE4:
		RETURN_IF_ERROR(SetValueFromUBYTEVec(param, 4, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_UINT:
		DOM_Object::DOMSetNumber(return_value, param.value.uint_param[0]);
		break;
	case CanvasWebGLParameter::PARAM_INT:
		DOM_Object::DOMSetNumber(return_value, param.value.int_param[0]);
		break;
	case CanvasWebGLParameter::PARAM_INT2:
		RETURN_IF_ERROR(SetValueFromIntVec(param, 2, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_INT3:
		RETURN_IF_ERROR(SetValueFromIntVec(param, 3, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_INT4:
		RETURN_IF_ERROR(SetValueFromIntVec(param, 4, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_FLOAT:
		DOM_Object::DOMSetNumber(return_value, param.value.float_param[0]);
		break;
	case CanvasWebGLParameter::PARAM_FLOAT2:
		RETURN_IF_ERROR(SetValueFromFloatVec(param, 2, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_FLOAT3:
		RETURN_IF_ERROR(SetValueFromFloatVec(param, 3, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_FLOAT4:
		RETURN_IF_ERROR(SetValueFromFloatVec(param, 4, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_OBJECT:
		if (param.value.object_param)
			DOM_Object::DOMSetObject(return_value, param.value.object_param);
		else
			DOM_Object::DOMSetNull(return_value);
		break;
	case CanvasWebGLParameter::PARAM_STRING:
		DOM_Object::DOMSetString(return_value, param.value.string_param);
		break;
	case CanvasWebGLParameter::PARAM_MAT2:
		RETURN_IF_ERROR(SetValueFromFloatVec(param, 4, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_MAT3:
		RETURN_IF_ERROR(SetValueFromFloatVec(param, 9, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_MAT4:
		RETURN_IF_ERROR(SetValueFromFloatVec(param, 16, return_value, origining_runtime));
		break;
	case CanvasWebGLParameter::PARAM_UNKNOWN:
		DOM_Object::DOMSetNull(return_value);
		break;
	default:
		DOM_Object::DOMSetNumber(return_value, 0);
		break;
	}
	return OpStatus::OK;
}


////////////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
DOMWebGLContextAttributes::Make(DOMWebGLContextAttributes*& attr, DOM_Runtime* runtime)
{
	return DOMSetObjectRuntime(attr = OP_NEW(DOMWebGLContextAttributes, ()), runtime, runtime->GetPrototype(DOM_Runtime::WEBGLCONTEXTATTRIBUTES_PROTOTYPE), "WebGLContextAttributes");
}

/* virtual */ ES_GetState
DOMWebGLContextAttributes::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_alpha:
		DOMSetBoolean(value, m_alpha);
		return GET_SUCCESS;
	case OP_ATOM_depth:
		DOMSetBoolean(value, m_depth);
		return GET_SUCCESS;
	case OP_ATOM_stencil:
		DOMSetBoolean(value, m_stencil);
		return GET_SUCCESS;
	case OP_ATOM_antialias:
		DOMSetBoolean(value, m_antialias);
		return GET_SUCCESS;
	case OP_ATOM_premultipliedAlpha:
		DOMSetBoolean(value, m_premultipliedAlpha);
		return GET_SUCCESS;
	case OP_ATOM_preserveDrawingBuffer:
		DOMSetBoolean(value, m_preserveDrawingBuffer);
		return GET_SUCCESS;
	}
	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOMWebGLContextAttributes::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_alpha:
		if (value->type == VALUE_BOOLEAN)
			m_alpha = value->value.boolean;
		else if (value->type == VALUE_NUMBER)
			m_alpha = (value->value.number != 0);
		else
			return PUT_NEEDS_BOOLEAN;
		return PUT_SUCCESS;
	case OP_ATOM_depth:
		if (value->type == VALUE_BOOLEAN)
			m_depth = value->value.boolean;
		else if (value->type == VALUE_NUMBER)
			m_depth = (value->value.number != 0);
		else
			return PUT_NEEDS_BOOLEAN;
		return PUT_SUCCESS;
	case OP_ATOM_stencil:
		if (value->type == VALUE_BOOLEAN)
			m_stencil = value->value.boolean;
		else if (value->type == VALUE_NUMBER)
			m_stencil = (value->value.number != 0);
		else
			return PUT_NEEDS_BOOLEAN;
		return PUT_SUCCESS;
	case OP_ATOM_antialias:
		if (value->type == VALUE_BOOLEAN)
			m_antialias = value->value.boolean;
		else if (value->type == VALUE_NUMBER)
			m_antialias = (value->value.number != 0);
		else
			return PUT_NEEDS_BOOLEAN;
		return PUT_SUCCESS;
	case OP_ATOM_premultipliedAlpha:
		if (value->type == VALUE_BOOLEAN)
			m_premultipliedAlpha = value->value.boolean;
		else if (value->type == VALUE_NUMBER)
			m_premultipliedAlpha = (value->value.number != 0);
		else
			return PUT_NEEDS_BOOLEAN;
		return PUT_SUCCESS;
	case OP_ATOM_preserveDrawingBuffer:
		if (value->type == VALUE_BOOLEAN)
			m_preserveDrawingBuffer = value->value.boolean;
		else if (value->type == VALUE_NUMBER)
			m_preserveDrawingBuffer = (value->value.number != 0);
		else
			return PUT_NEEDS_BOOLEAN;
		return PUT_SUCCESS;
	}
	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/*static */ OP_STATUS
DOMWebGLBuffer::Make(DOMWebGLBuffer*& buf, DOMCanvasContextWebGL* owner)
{
	DOM_Runtime *runtime = owner->GetRuntime();
	CanvasWebGLBuffer* buffer = OP_NEW(CanvasWebGLBuffer, ());
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;

	buf = OP_NEW(DOMWebGLBuffer, (buffer, owner));
	if (!buf)
		OP_DELETE(buffer);

	RETURN_IF_ERROR(DOMSetObjectRuntime(buf, runtime, runtime->GetPrototype(DOM_Runtime::WEBGLBUFFER_PROTOTYPE), "WebGLBuffer"));
	buffer->setESObject(*buf);
	return OpStatus::OK;
}

DOMWebGLBuffer::DOMWebGLBuffer(CanvasWebGLBuffer* buf, DOMCanvasContextWebGL* owner)
	: m_buffer(buf),
	  m_owner(owner)
{
}

DOMWebGLBuffer::~DOMWebGLBuffer()
{
	m_buffer->esObjectDestroyed();
}

/* virtual */ void
DOMWebGLBuffer::GCTrace()
{
	GCMark(m_owner);
}

/* static */ OP_STATUS
DOMWebGLFramebuffer::Make(DOMWebGLFramebuffer*& buf, DOMCanvasContextWebGL* owner)
{
	DOM_Runtime *runtime = owner->GetRuntime();
	CanvasWebGLFramebuffer* buffer = OP_NEW(CanvasWebGLFramebuffer, ());
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;

	buf = OP_NEW(DOMWebGLFramebuffer, (buffer, owner));
	if (!buf)
		OP_DELETE(buffer);

	RETURN_IF_ERROR(DOMSetObjectRuntime(buf, runtime, runtime->GetPrototype(DOM_Runtime::WEBGLFRAMEBUFFER_PROTOTYPE), "WebGLFramebuffer"));
	buffer->setESObject(*buf);
	return buffer->Construct();
}

DOMWebGLFramebuffer::DOMWebGLFramebuffer(CanvasWebGLFramebuffer* buf, DOMCanvasContextWebGL* owner)
	: m_framebuffer(buf),
	  m_owner(owner)
{
}

DOMWebGLFramebuffer::~DOMWebGLFramebuffer()
{
	m_framebuffer->esObjectDestroyed();
}

/* virtual */ void
DOMWebGLFramebuffer::GCTrace()
{
	if (m_framebuffer->getColorBuffer())
		GCMark(m_framebuffer->getColorBuffer()->getESObject());
	if (m_framebuffer->getDepthBuffer())
		GCMark(m_framebuffer->getDepthBuffer()->getESObject());
	if (m_framebuffer->getStencilBuffer())
		GCMark(m_framebuffer->getStencilBuffer()->getESObject());

	GCMark(m_owner);
}

/*static */ OP_STATUS
DOMWebGLProgram::Make(DOMWebGLProgram*& prog, DOMCanvasContextWebGL* owner)
{
	DOM_Runtime *runtime = owner->GetRuntime();
	CanvasWebGLProgram* sprog = OP_NEW(CanvasWebGLProgram, ());
	if (!sprog)
		return OpStatus::ERR_NO_MEMORY;

	prog = OP_NEW(DOMWebGLProgram, (sprog, owner));
	if (!prog)
		OP_DELETE(sprog);

	RETURN_IF_ERROR(DOMSetObjectRuntime(prog, runtime, runtime->GetPrototype(DOM_Runtime::WEBGLPROGRAM_PROTOTYPE), "WebGLProgram"));
	sprog->setESObject(*prog);
	return sprog->Construct();
}

DOMWebGLProgram::DOMWebGLProgram(CanvasWebGLProgram* prog, DOMCanvasContextWebGL* owner) : m_program(prog), m_owner(owner)
{
}

DOMWebGLProgram::~DOMWebGLProgram()
{
	m_program->esObjectDestroyed();
}

/* virtual */ void
DOMWebGLProgram::GCTrace()
{
	for (unsigned int i = 0; i < m_program->getNumAttachedShaders(); ++i)
		GCMark(m_program->getAttachedShaderESObject(i));
	GCMark(m_owner);
}

/* static */ OP_STATUS
DOMWebGLRenderbuffer::Make(DOMWebGLRenderbuffer*& buf, DOMCanvasContextWebGL* owner)
{
	DOM_Runtime *runtime = owner->GetRuntime();

	CanvasWebGLRenderbuffer* buffer = OP_NEW(CanvasWebGLRenderbuffer, ());
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;

	buf = OP_NEW(DOMWebGLRenderbuffer, (buffer, owner));
	if (!buf)
		OP_DELETE(buffer);

	RETURN_IF_ERROR(DOMSetObjectRuntime(buf, runtime, runtime->GetPrototype(DOM_Runtime::WEBGLRENDERBUFFER_PROTOTYPE), "WebGLRenderbuffer"));
	buffer->setESObject(*buf);
	return OpStatus::OK;
}

DOMWebGLRenderbuffer::DOMWebGLRenderbuffer(CanvasWebGLRenderbuffer* buf, DOMCanvasContextWebGL* owner) : m_renderbuffer(buf), m_owner(owner)
{
}

DOMWebGLRenderbuffer::~DOMWebGLRenderbuffer()
{
	m_renderbuffer->esObjectDestroyed();
}

void DOMWebGLRenderbuffer::GCTrace()
{
	GCMark(m_owner);
}

/*static */ OP_STATUS
DOMWebGLShader::Make(DOMWebGLShader*& shd, DOMCanvasContextWebGL* owner, BOOL fs)
{
	DOM_Runtime *runtime = owner->GetRuntime();

	CanvasWebGLShader* shader = OP_NEW(CanvasWebGLShader, (fs));
	if (!shader)
		return OpStatus::ERR_NO_MEMORY;

	shd = OP_NEW(DOMWebGLShader, (shader, owner));
	if (!shd)
		OP_DELETE(shader);

	RETURN_IF_ERROR(DOMSetObjectRuntime(shd, runtime, runtime->GetPrototype(DOM_Runtime::WEBGLSHADER_PROTOTYPE), "WebGLShader"));
	shader->setESObject(*shd);
	return OpStatus::OK;
}

DOMWebGLShader::DOMWebGLShader(CanvasWebGLShader* shader, DOMCanvasContextWebGL* owner) : m_shader(shader), m_owner(owner)
{
}

DOMWebGLShader::~DOMWebGLShader()
{
	m_shader->esObjectDestroyed();
}

/* virtual */ void
DOMWebGLShader::GCTrace()
{
	GCMark(m_owner);
}

/*static */ OP_STATUS
DOMWebGLTexture::Make(DOMWebGLTexture*& tex, DOMCanvasContextWebGL* owner)
{
	DOM_Runtime *runtime = owner->GetRuntime();

	CanvasWebGLTexture* texture = OP_NEW(CanvasWebGLTexture, ());
	if (!texture)
		return OpStatus::ERR_NO_MEMORY;

	tex = OP_NEW(DOMWebGLTexture, (texture, owner));
	if (!tex)
		OP_DELETE(texture);
	RETURN_IF_ERROR(DOMSetObjectRuntime(tex, runtime, runtime->GetPrototype(DOM_Runtime::WEBGLTEXTURE_PROTOTYPE), "WebGLTexture"));
	texture->setESObject(*tex);
	return OpStatus::OK;
}

DOMWebGLTexture::DOMWebGLTexture(CanvasWebGLTexture* tex, DOMCanvasContextWebGL* owner) : m_texture(tex), m_owner(owner)
{
}

DOMWebGLTexture::~DOMWebGLTexture()
{
	m_texture->esObjectDestroyed();
}

/* virtual */ void
DOMWebGLTexture::GCTrace()
{
	GCMark(m_owner);
}

/*static */ OP_STATUS
DOMWebGLObjectArray::Make(DOMWebGLObjectArray*& oa, DOM_Runtime* runtime, unsigned int len)
{
	DOMWebGLObject** objs = NULL;
	if (len > 0)
	{
		objs = OP_NEWA(DOMWebGLObject*, len);
		if (!objs)
			return OpStatus::ERR_NO_MEMORY;
	}

	oa = OP_NEW(DOMWebGLObjectArray, (len, objs));
	if (!oa)
		OP_DELETEA(objs);
	return DOMSetObjectRuntime(oa, runtime, runtime->GetArrayPrototype(), "WebGLObjectArray");
}

/* virtual */ void
DOMWebGLObjectArray::GCTrace()
{
	for (unsigned int i = 0; i < m_length; ++i)
		GCMark(m_objects[i]);
}

ES_GetState
DOMWebGLObjectArray::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		DOMSetNumber(value, m_length);
		return GET_SUCCESS;
	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

ES_PutState
DOMWebGLObjectArray::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		return PUT_SUCCESS;
	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

ES_GetState
DOMWebGLObjectArray::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	unsigned int idx = static_cast<unsigned int>(property_index);
	if (idx >= m_length)
		return GET_FAILED;
	DOMSetObject(value, m_objects[idx]);
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOMWebGLObjectArray::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = m_length;
	return GET_SUCCESS;
}

/* static */ OP_STATUS
DOMWebGLUniformLocation::Make(DOMWebGLUniformLocation*& loc, DOM_Runtime* runtime, int idx, const uni_char* name, CanvasWebGLProgram *prog)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(loc = OP_NEW(DOMWebGLUniformLocation, (idx, prog)), runtime, runtime->GetPrototype(DOM_Runtime::WEBGLUNIFORMLOCATION_PROTOTYPE), "WebGLUniformLocation"));
	return loc->m_name.Set(name);
}

DOMWebGLUniformLocation::DOMWebGLUniformLocation(int idx, CanvasWebGLProgram *program)
	: m_index(idx)
	, m_program(program)
{
	m_linkId = m_program->getLinkId();
}

void
DOMWebGLUniformLocation::GCTrace()
{
	GCMark(m_program->getESObject());
}


/* static */ OP_STATUS
DOMWebGLActiveInfo::Make(DOMWebGLActiveInfo*& ai, DOM_Runtime* runtime, OpString& name, unsigned int type, unsigned int size)
{
	ai = OP_NEW(DOMWebGLActiveInfo, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(ai, runtime, runtime->GetPrototype(DOM_Runtime::WEBGLACTIVEINFO_PROTOTYPE), "WebGLActiveInfo"));
	ai->m_name.TakeOver(name);
	ai->m_type = type;
	ai->m_size = size;
	return OpStatus::OK;
}

ES_GetState
DOMWebGLActiveInfo::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_size:
		DOMSetNumber(value, m_size);
		return GET_SUCCESS;
	case OP_ATOM_type:
		DOMSetNumber(value, m_type);
		return GET_SUCCESS;
	case OP_ATOM_name:
		DOMSetString(value, m_name.CStr());
		return GET_SUCCESS;
	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

ES_PutState
DOMWebGLActiveInfo::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_size:
	case OP_ATOM_type:
	case OP_ATOM_name:
		return PUT_SUCCESS;
	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}
////////////////////////////////////////////////////////////////////////////////
/* static */ OP_STATUS
DOMWebGLExtension::Make(DOMWebGLExtension*& extension, DOM_Runtime* runtime, const char* name)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(extension = OP_NEW(DOMWebGLExtension, (name)), runtime, runtime->GetPrototype(DOM_Runtime::WEBGLEXTENSION_PROTOTYPE), "WebGLExtension"));

	return Initialize(extension, runtime);
}

/* static */ OP_STATUS
DOMWebGLExtension::Initialize(DOMWebGLExtension* extension, DOM_Runtime* runtime)
{
	if (op_strcmp(extension->GetExtensionName(), "OES_texture_float") == 0)
		return OpStatus::OK;
	else if (op_strcmp(extension->GetExtensionName(), "OES_standard_derivatives") == 0)
	{
		return PutNumericConstant(*extension, UNI_L("FRAGMENT_SHADER_DERIVATIVE_HINT_OES"), WEBGL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES, runtime);
	}

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////
DOMCanvasContextWebGL::DOMCanvasContextWebGL(DOM_HTMLCanvasElement *domcanvas, CanvasContextWebGL *ctx)
	: m_domcanvas(domcanvas),
	  m_context(ctx)
{
	VEGARefCount::IncRef(m_context);
}

DOMCanvasContextWebGL::~DOMCanvasContextWebGL()
{
	VEGARefCount::DecRef(m_context);
}

/*static */ void
DOMCanvasContextWebGL::ConstructCanvasContextWebGLObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	AddWebGLConstantsL(object, runtime);
}

OP_STATUS
DOMCanvasContextWebGL::Initialize()
{
	return m_context->Construct();
}

/* static */ OP_STATUS
DOMCanvasContextWebGL::Make(DOMCanvasContextWebGL*& ctx, DOM_HTMLCanvasElement *domcanvas, DOMWebGLContextAttributes* attributes)
{
	DOM_Runtime *runtime = domcanvas->GetRuntime();
	HTML_Element *htm_elem = domcanvas->GetThisElement();

	Canvas* canvas = static_cast<Canvas*>(htm_elem->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP));
	if (!canvas)
		return OpStatus::ERR;

	FramesDocument* frm_doc = domcanvas->GetFramesDocument();
	if (!frm_doc)
		return OpStatus::ERR;

	switch (g_pccore->GetIntegerPref(PrefsCollectionCore::EnableWebGL, frm_doc->GetURL()))
	{
	case PrefsCollectionCore::Disable:
		return OpStatus::ERR;
	case PrefsCollectionCore::Force:
		canvas->SetUseCase(VEGA3dDevice::Force);
		break;
	case PrefsCollectionCore::Auto:
	default:
		canvas->SetUseCase(VEGA3dDevice::For3D);
		break;
	}
	
	CanvasContextWebGL *cgl = NULL;
	if (attributes)
		cgl = canvas->GetContextWebGL(attributes->m_alpha, attributes->m_depth, attributes->m_stencil, attributes->m_antialias, attributes->m_premultipliedAlpha, attributes->m_preserveDrawingBuffer);
	else
		cgl = canvas->GetContextWebGL(TRUE, TRUE, FALSE, TRUE, TRUE, FALSE);
	if (!cgl)
		return OpStatus::ERR;

	ctx = OP_NEW(DOMCanvasContextWebGL, (domcanvas, cgl));
	RETURN_IF_ERROR(DOMSetObjectRuntime(ctx, runtime, runtime->GetPrototype(DOM_Runtime::CANVASCONTEXTWEBGL_PROTOTYPE), "WebGLRenderingContext"));
	RETURN_IF_ERROR(ctx->Initialize());
	return OpStatus::OK;
}

ES_GetState
DOMCanvasContextWebGL::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_canvas:
		DOMSetObject(value, m_domcanvas);
		return GET_SUCCESS;
	case OP_ATOM_drawingBufferHeight:
		DOMSetNumber(value, m_context->getDrawingBufferHeight());
		return GET_SUCCESS;
	case OP_ATOM_drawingBufferWidth:
		DOMSetNumber(value, m_context->getDrawingBufferWidth());
		return GET_SUCCESS;
	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

ES_PutState
DOMCanvasContextWebGL::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_canvas:
	case OP_ATOM_drawingBufferHeight:
	case OP_ATOM_drawingBufferWidth:
		return PUT_SUCCESS;
	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOMCanvasContextWebGL::GCTrace()
{
	GCMark(m_domcanvas);
	m_context->GCTrace(GetRuntime());
	unsigned int i;
	for (i = 0; i < m_enabled_extensions.GetCount(); ++i)
		GCMark(m_enabled_extensions.Get(i));
}

/* static */ BOOL
DOMCanvasContextWebGL::IsExtensionSupported(DOMCanvasContextWebGL* domctx, const char* name)
{
	if (op_strcmp(name, "OES_texture_float") == 0)
		return domctx->m_context->supportsExtension(WEBGL_EXT_OES_TEXTURE_FLOAT);
	else if (op_strcmp(name, "OES_standard_derivatives") == 0)
		return domctx->m_context->supportsExtension(WEBGL_EXT_OES_STANDARD_DERIVATIVES);
	else
		return FALSE;
}

/* As a first approximation, keep the list of all possibly supported extensions hardcoded.
 * The list _must_ match that of the WEBGL_OEX_* constants.
 */

/* static */ const char*
DOMCanvasContextWebGL::SupportedExtensions(unsigned int i)
{
	OP_ASSERT(i < supportedExtensionsCount);
	if (i == 0)
		return "OES_texture_float";
	else
		return "OES_standard_derivatives";
}

/* static */ unsigned
DOMCanvasContextWebGL::supportedExtensionsCount = 2;

//////////////////////////////////////////////////////////////////////////

/** Custom function which truncates to a float based on the IDL rules and
 * then applies the clamping rules from opengl to make it a GLclampf.
 * Would not be needed if we did a thin wrapper over opengl, but we
 * are not doing that so the rule has to be applied here. */
static inline float TruncateDoubleToGLclampf(double d)
{
	float f = DOM_Object::TruncateDoubleToFloat(d);
	if (op_isnan(f))
		return 0.f;
	return f < 0 ? 0 : (f > 1.f ? 1.f : f);
}

/* static */ int
DOMCanvasContextWebGL::getContextAttributes(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLContextAttributes* attr;
	CALL_FAILED_IF_ERROR(DOMWebGLContextAttributes::Make(attr, domctx->GetRuntime()));
	attr->m_alpha = domctx->m_context->HasAlpha();
	attr->m_depth = domctx->m_context->HasDepth();
	attr->m_stencil = domctx->m_context->HasStencil();
	attr->m_antialias = domctx->m_context->HasAntialias();
	attr->m_premultipliedAlpha = domctx->m_context->HasPremultipliedAlpha();
	attr->m_preserveDrawingBuffer = domctx->m_context->HasPreserveDrawingBuffer();
	DOMSetObject(return_value, attr);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::isContextLost(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_CANVASCONTEXTWEBGL);
	// Current context lost is not implemented so this function always returns FALSE.
	DOMSetBoolean(return_value, FALSE);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getSupportedExtensions(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);

	ES_Object *arrayObj;
	CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&arrayObj, 0));

	unsigned idx = 0;
	for (unsigned i = 0; i < supportedExtensionsCount; i++)
		if (IsExtensionSupported(domctx, SupportedExtensions(i)))
		{
			ES_Value val;
			OpString str;
			CALL_FAILED_IF_ERROR(str.Set(SupportedExtensions(i)));
			DOMSetString(&val, str);
			CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(arrayObj, idx++, val));
		}

	DOMSetObject(return_value, arrayObj);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getExtension(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("s");

	const uni_char *name = argv[0].value.string;

	for (unsigned i = 0; i < domctx->m_enabled_extensions.GetCount(); i++)
		if (uni_strcmp(domctx->m_enabled_extensions.Get(i)->GetExtensionName(), name) == 0)
		{
			DOMSetObject(return_value, domctx->m_enabled_extensions.Get(i));
			return ES_VALUE;
		}

	for (unsigned i = 0; i < supportedExtensionsCount; i++)
		if (uni_strcmp(SupportedExtensions(i), name) == 0 && IsExtensionSupported(domctx, SupportedExtensions(i)))
		{
			DOMWebGLExtension* new_extension = NULL;
			CALL_FAILED_IF_ERROR(DOMWebGLExtension::Make(new_extension, domctx->GetRuntime(), SupportedExtensions(i)));
			CALL_FAILED_IF_ERROR(domctx->m_enabled_extensions.Add(new_extension));
			domctx->m_context->enableExtension(0x1 << i);
			DOMSetObject(return_value, new_extension);
			return ES_VALUE;
		}

	DOMSetNull(return_value);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::activeTexture(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->activeTexture(TruncateDoubleToUInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::attachShader(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("OO");
	DOMWebGLProgram* prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	DOMWebGLShader* shd = NULL;
	if (argv[1].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[1].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLSHADER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		shd = static_cast<DOMWebGLShader*>(domo);
	}
	if ((prog && prog->m_owner != domctx) || (shd && shd->m_owner != domctx))
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	CALL_FAILED_IF_ERROR(domctx->m_context->attachShader(prog ? prog->m_program : NULL, shd ? shd->m_shader : NULL));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::bindAttribLocation(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ONs");
	DOMWebGLProgram* prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	unsigned int idx = TruncateDoubleToUInt(argv[1].value.number);
	if (prog && prog->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	CALL_FAILED_IF_ERROR(domctx->m_context->bindAttribLocation(prog ? prog->m_program : NULL, argv[2].value.string, idx));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::bindBuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NO");
	DOMWebGLBuffer* buffer = NULL;
	if (argv[1].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[1].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLBUFFER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		buffer = static_cast<DOMWebGLBuffer*>(domo);
	}
	if (buffer && buffer->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	domctx->m_context->bindBuffer(target, buffer ? buffer->m_buffer : NULL);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::bindFramebuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NO");
	DOMWebGLFramebuffer* buffer = NULL;
	if (argv[1].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[1].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLFRAMEBUFFER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		buffer = static_cast<DOMWebGLFramebuffer*>(domo);
	}
	if (buffer && buffer->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	domctx->m_context->bindFramebuffer(target, buffer ? buffer->m_framebuffer : NULL);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::bindRenderbuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NO");
	DOMWebGLRenderbuffer* buffer = NULL;
	if (argv[1].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[1].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLRENDERBUFFER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		buffer = static_cast<DOMWebGLRenderbuffer*>(domo);
	}
	if (buffer && buffer->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	domctx->m_context->bindRenderbuffer(target, buffer ? buffer->m_renderbuffer : NULL);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::bindTexture(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NO");
	DOMWebGLTexture* texture = NULL;
	if (argv[1].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[1].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLTEXTURE))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		texture = static_cast<DOMWebGLTexture*>(domo);
	}
	if (texture && texture->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	domctx->m_context->bindTexture(target, texture ? texture->m_texture : NULL);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::blendColor(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNN");
	domctx->m_context->blendColor(TruncateDoubleToGLclampf(argv[0].value.number), TruncateDoubleToGLclampf(argv[1].value.number), TruncateDoubleToGLclampf(argv[2].value.number), TruncateDoubleToGLclampf(argv[3].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::blendEquation(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->blendEquation(TruncateDoubleToUInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::blendEquationSeparate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	domctx->m_context->blendEquationSeparate(TruncateDoubleToUInt(argv[0].value.number), TruncateDoubleToUInt(argv[1].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::blendFunc(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	unsigned int src = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int dst = TruncateDoubleToUInt(argv[1].value.number);
	domctx->m_context->blendFunc(src, dst);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::blendFuncSeparate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNN");
	unsigned int src = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int dst = TruncateDoubleToUInt(argv[1].value.number);
	unsigned int srcA = TruncateDoubleToUInt(argv[2].value.number);
	unsigned int dstA = TruncateDoubleToUInt(argv[3].value.number);
	domctx->m_context->blendFuncSeparate(src, dst, srcA, dstA);
	return ES_FAILED;
}

static unsigned int
WebGLLongLongToInt(const ES_Value& param, int &value, unsigned int negErr, unsigned int maxErr)
{
	OP_ASSERT(param.type == VALUE_NUMBER);
	double d;
	if (!DOM_Object::ConvertDoubleToLongLong(param.value.number, DOM_Object::Clamp, d))
		return WEBGL_INVALID_VALUE; // NaN
	if (d < 0)
		return negErr;
	if (d > INT_MAX)
		return maxErr;
	value = DOM_Object::TruncateDoubleToInt(d);
	return WEBGL_NO_ERROR;
}

/* static */ int
DOMCanvasContextWebGL::bufferData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N-N");
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int usage = TruncateDoubleToUInt(argv[2].value.number);

	int size = 0;
	void* data = NULL;
	if (argv[1].type == VALUE_NULL || argv[1].type == VALUE_UNDEFINED)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
		return ES_FAILED;
	}
	else if (argv[1].type == VALUE_NUMBER)
	{
		// This parameter is a GLsizeiptr which is defined to be a web idl long long
		// so we do some extra checks in the dom layer as we only use 32-bit sizes
		// internally.
		unsigned int webglErr = WebGLLongLongToInt(argv[1], size, WEBGL_INVALID_VALUE, WEBGL_OUT_OF_MEMORY);
		if (webglErr != WEBGL_NO_ERROR)
		{
			domctx->m_context->raiseError(webglErr);
			return ES_FAILED;
		}
	}
	else if (argv[1].type == VALUE_OBJECT)
	{
		ES_Object* obj = argv[1].value.object;
		if (origining_runtime->IsNativeTypedArrayObject(obj))
		{
			size = static_cast<int>(origining_runtime->GetStaticTypedArraySize(obj));
			data = origining_runtime->GetStaticTypedArrayStorage(obj);
		}
		else if (origining_runtime->IsNativeArrayBufferObject(obj))
		{
			size = static_cast<int>(origining_runtime->GetByteArrayLength(obj));
			data = origining_runtime->GetByteArrayStorage(obj);
		}
		else
		{
			DOMSetNumber(return_value, ES_CONVERT_ARGUMENT(ES_CALL_NEEDS_NUMBER, 1));
			return ES_NEEDS_CONVERSION;
		}
	}
	else
	{
		DOMSetNumber(return_value, ES_CONVERT_ARGUMENT(ES_CALL_NEEDS_NUMBER, 1));
		return ES_NEEDS_CONVERSION;
	}
	CALL_FAILED_IF_ERROR(domctx->m_context->bufferData(target, usage, size, data));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::bufferSubData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNO");

	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	int offset = 0;

	// This parameter is a GLintptr which is defined to be a web idl long long
	// so we do some extra checks in the dom layer as we only use 32-bit offsets
	// internally.
	unsigned int webglErr = WebGLLongLongToInt(argv[1], offset, WEBGL_INVALID_VALUE, WEBGL_INVALID_VALUE);
	if (webglErr != WEBGL_NO_ERROR)
	{
		domctx->m_context->raiseError(webglErr);
		return ES_FAILED;
	}

	int size = 0;
	void* data = NULL;

	if (argv[1].type == VALUE_NULL || argv[1].type == VALUE_UNDEFINED)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
		return ES_FAILED;
	}
	else if (argv[2].type == VALUE_OBJECT)
	{
		ES_Object* obj = argv[2].value.object;
		if (origining_runtime->IsNativeTypedArrayObject(obj))
		{
			size = static_cast<int>(origining_runtime->GetStaticTypedArraySize(obj));
			data = origining_runtime->GetStaticTypedArrayStorage(obj);
		}
		else if (origining_runtime->IsNativeArrayBufferObject(obj))
		{
			size = static_cast<int>(origining_runtime->GetByteArrayLength(obj));
			data = origining_runtime->GetByteArrayStorage(obj);
		}
		else
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		CALL_FAILED_IF_ERROR(domctx->m_context->bufferSubData(target, offset, size, data));
	}
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::checkFramebufferStatus(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	DOMSetNumber(return_value, domctx->m_context->checkFramebufferStatus(TruncateDoubleToUInt(argv[0].value.number)));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::clear(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	unsigned int mask = TruncateDoubleToUInt(argv[0].value.number);
	domctx->m_context->clear(mask);
	if (domctx->m_context->isRenderingToCanvas())
		CALL_FAILED_IF_ERROR(domctx->m_domcanvas->ScheduleInvalidation(origining_runtime));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::clearColor(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNN");
	domctx->m_context->clearColor(TruncateDoubleToGLclampf(argv[0].value.number), TruncateDoubleToGLclampf(argv[1].value.number), TruncateDoubleToGLclampf(argv[2].value.number), TruncateDoubleToGLclampf(argv[3].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::clearDepth(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->clearDepth(TruncateDoubleToGLclampf(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::clearStencil(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->clearStencil(TruncateDoubleToInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::colorMask(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("bbbb");
	domctx->m_context->colorMask(argv[0].value.boolean, argv[1].value.boolean, argv[2].value.boolean, argv[3].value.boolean);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::compileShader(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLShader* shader = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLSHADER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		shader = static_cast<DOMWebGLShader*>(domo);
	}
	if (shader && shader->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	if (shader)
	{
		BOOL validate_source = g_pcjs->GetIntegerPref(PrefsCollectionJS::ShaderValidation, DOM_EnvironmentImpl::GetHostName(origining_runtime->GetFramesDocument()));

		OpString url;
		CALL_FAILED_IF_ERROR(origining_runtime->GetOriginURL().GetAttribute(URL::KUniName_Username_Password_Hidden, url));
		CALL_FAILED_IF_ERROR(domctx->m_context->compileShader(shader->m_shader, url.CStr(), validate_source));
	}
	else
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::compressedTexImage2D(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNNNNo");

	// Until we have compressed texture extensions we should just return INVALID_ENUM.
	domctx->m_context->raiseError(WEBGL_INVALID_ENUM);
	return ES_FAILED;

	/*unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	int level = TruncateDoubleToInt(argv[1].value.number);
	unsigned int ifmt = TruncateDoubleToUInt(argv[2].value.number);
	int w = TruncateDoubleToInt(argv[3].value.number);
	int h = TruncateDoubleToInt(argv[4].value.number);
	int border = TruncateDoubleToInt(argv[5].value.number);*/
}

/* static */ int
DOMCanvasContextWebGL::compressedTexSubImage2D(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNNNNNo");

	// Until we have compressed texture extensions we should just return INVALID_ENUM.
	domctx->m_context->raiseError(WEBGL_INVALID_ENUM);
	return ES_FAILED;

	/*unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	int level = TruncateDoubleToInt(argv[1].value.number);
	int x = TruncateDoubleToInt(argv[2].value.number);
	int y = TruncateDoubleToInt(argv[3].value.number);
	int w = TruncateDoubleToInt(argv[4].value.number);
	int h = TruncateDoubleToInt(argv[5].value.number);
	int format = TruncateDoubleToInt(argv[6].value.number);
	*/
}

/* static */ int
DOMCanvasContextWebGL::copyTexImage2D(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNNNNNN");
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	int level = TruncateDoubleToInt(argv[1].value.number);
	unsigned int ifmt = TruncateDoubleToUInt(argv[2].value.number);
	int x = TruncateDoubleToInt(argv[3].value.number);
	int y = TruncateDoubleToInt(argv[4].value.number);
	int w = TruncateDoubleToInt(argv[5].value.number);
	int h = TruncateDoubleToInt(argv[6].value.number);
	int border = TruncateDoubleToInt(argv[7].value.number);
	CALL_FAILED_IF_ERROR(domctx->m_context->copyTexImage2D(target, level, ifmt, x, y, w, h, border));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::copyTexSubImage2D(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNNNNNN");
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	int level = TruncateDoubleToInt(argv[1].value.number);
	int xofs = TruncateDoubleToInt(argv[2].value.number);
	int yofs = TruncateDoubleToInt(argv[3].value.number);
	int x = TruncateDoubleToInt(argv[4].value.number);
	int y = TruncateDoubleToInt(argv[5].value.number);
	int w = TruncateDoubleToInt(argv[6].value.number);
	int h = TruncateDoubleToInt(argv[7].value.number);
	CALL_FAILED_IF_ERROR(domctx->m_context->copyTexSubImage2D(target, level, xofs, yofs, x, y, w, h));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::createBuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLBuffer* buf;
	CALL_FAILED_IF_ERROR(DOMWebGLBuffer::Make(buf, domctx));
	DOMSetObject(return_value, buf);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::createFramebuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLFramebuffer* buf;
	CALL_FAILED_IF_ERROR(DOMWebGLFramebuffer::Make(buf, domctx));
	DOMSetObject(return_value, buf);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::createProgram(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLProgram* prog;
	CALL_FAILED_IF_ERROR(DOMWebGLProgram::Make(prog, domctx));
	DOMSetObject(return_value, prog);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::createRenderbuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLRenderbuffer* buf;
	CALL_FAILED_IF_ERROR(DOMWebGLRenderbuffer::Make(buf, domctx));
	DOMSetObject(return_value, buf);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::createShader(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	unsigned int type = TruncateDoubleToUInt(argv[0].value.number);
	BOOL fragmentShader = FALSE;
	if (type == WEBGL_FRAGMENT_SHADER)
		fragmentShader = TRUE;
	else if (type != WEBGL_VERTEX_SHADER)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_ENUM);
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	DOMWebGLShader* shad;
	CALL_FAILED_IF_ERROR(DOMWebGLShader::Make(shad, domctx, fragmentShader));
	DOMSetObject(return_value, shad);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::createTexture(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLTexture* tex;
	CALL_FAILED_IF_ERROR(DOMWebGLTexture::Make(tex, domctx));
	DOMSetObject(return_value, tex);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::cullFace(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->cullFace(TruncateDoubleToUInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::deleteBuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLBuffer* buffer = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLBUFFER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		buffer = static_cast<DOMWebGLBuffer*>(domo);
	}
	if (buffer && buffer->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	if (buffer)
		domctx->m_context->deleteBuffer(buffer->m_buffer);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::deleteFramebuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLFramebuffer* buffer = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLFRAMEBUFFER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		buffer = static_cast<DOMWebGLFramebuffer*>(domo);
	}
	if (buffer && buffer->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	if (buffer)
		domctx->m_context->deleteFramebuffer(buffer->m_framebuffer);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::deleteProgram(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLProgram* program = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		program = static_cast<DOMWebGLProgram*>(domo);
	}
	if (program && program->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	if (program)
		domctx->m_context->deleteProgram(program->m_program);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::deleteRenderbuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLRenderbuffer* buffer = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLRENDERBUFFER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		buffer = static_cast<DOMWebGLRenderbuffer*>(domo);
	}
	if (buffer && buffer->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	if (buffer)
		domctx->m_context->deleteRenderbuffer(buffer->m_renderbuffer);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::deleteShader(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLShader* shader = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLSHADER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		shader = static_cast<DOMWebGLShader*>(domo);
	}
	if (shader && shader->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	if (shader)
		domctx->m_context->deleteShader(shader->m_shader);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::deleteTexture(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLTexture* tex = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLTEXTURE))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		tex = static_cast<DOMWebGLTexture*>(domo);
	}
	if (tex && tex->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	if (tex)
		domctx->m_context->deleteTexture(tex->m_texture);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::depthFunc(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->depthFunc(TruncateDoubleToUInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::depthMask(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("b");
	domctx->m_context->depthMask(argv[0].value.boolean);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::depthRange(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	// the depth range function needs to do its own clamping since it has to check
	// for near > far before clamping
	float znear = TruncateDoubleToFloat(argv[0].value.number);
	float zfar = TruncateDoubleToFloat(argv[1].value.number);
	if (op_isnan(znear))
		znear = 0;
	if (op_isnan(zfar))
		zfar = 0;
	domctx->m_context->depthRange(znear, zfar);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::detachShader(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("OO");
	DOMWebGLProgram* prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	DOMWebGLShader* shd = NULL;
	if (argv[1].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[1].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLSHADER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		shd = static_cast<DOMWebGLShader*>(domo);
	}
	if ((prog && prog->m_owner != domctx) || (shd && shd->m_owner != domctx))
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	CALL_FAILED_IF_ERROR(domctx->m_context->detachShader(prog ? prog->m_program : NULL, shd ? shd->m_shader : NULL));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::disable(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->disable(TruncateDoubleToUInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::disableVertexAttribArray(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	unsigned int idx = TruncateDoubleToUInt(argv[0].value.number);
	domctx->m_context->disableVertexAttribArray(idx);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::drawArrays(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNN");

	unsigned int mode = TruncateDoubleToUInt(argv[0].value.number);
	int first = TruncateDoubleToInt(argv[1].value.number);
	int count = TruncateDoubleToInt(argv[2].value.number);
	CALL_FAILED_IF_ERROR(domctx->m_context->drawArrays(mode, first, count));
	if (domctx->m_context->isRenderingToCanvas())
		CALL_FAILED_IF_ERROR(domctx->m_domcanvas->ScheduleInvalidation(origining_runtime));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::drawElements(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNN");

	unsigned int mode = TruncateDoubleToUInt(argv[0].value.number);
	int count = TruncateDoubleToInt(argv[1].value.number);
	unsigned int type = TruncateDoubleToUInt(argv[2].value.number);
	int offset = 0;

	// This parameter is a GLintptr which is defined to be a web idl long long
	// so we do some extra checks in the dom layer as we only use 32-bit offsets
	// internally.
	unsigned int webglErr = WebGLLongLongToInt(argv[3], offset, WEBGL_INVALID_VALUE, WEBGL_INVALID_VALUE);
	if (webglErr != WEBGL_NO_ERROR)
	{
		domctx->m_context->raiseError(webglErr);
		return ES_FAILED;
	}
	CALL_FAILED_IF_ERROR(domctx->m_context->drawElements(mode, count, type, offset));
	if (domctx->m_context->isRenderingToCanvas())
		CALL_FAILED_IF_ERROR(domctx->m_domcanvas->ScheduleInvalidation(origining_runtime));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::enable(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->enable(TruncateDoubleToUInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::enableVertexAttribArray(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	unsigned int idx = TruncateDoubleToUInt(argv[0].value.number);
	domctx->m_context->enableVertexAttribArray(idx);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::finish(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	domctx->m_context->finish();
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::flush(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	domctx->m_context->flush();
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::framebufferRenderbuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNO");
	DOMWebGLRenderbuffer* buffer = NULL;
	if (argv[3].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[3].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLRENDERBUFFER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		buffer = static_cast<DOMWebGLRenderbuffer*>(domo);
	}
	if (buffer && buffer->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	domctx->m_context->framebufferRenderbuffer(TruncateDoubleToUInt(argv[0].value.number), TruncateDoubleToUInt(argv[1].value.number), TruncateDoubleToUInt(argv[2].value.number), buffer ? buffer->m_renderbuffer : NULL);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::framebufferTexture2D(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNON");
	DOMWebGLTexture* tex = NULL;
	if (argv[3].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[3].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLTEXTURE))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		tex = static_cast<DOMWebGLTexture*>(domo);
	}
	if (tex && tex->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	domctx->m_context->framebufferTexture2D(TruncateDoubleToUInt(argv[0].value.number), TruncateDoubleToUInt(argv[1].value.number), TruncateDoubleToUInt(argv[2].value.number), tex ? tex->m_texture : NULL, TruncateDoubleToInt(argv[4].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::frontFace(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->frontFace(TruncateDoubleToUInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::generateMipmap(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->generateMipmap(TruncateDoubleToUInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::getActiveAttrib(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ON");
	DOMWebGLProgram* prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	if (prog && prog->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	unsigned int type;
	unsigned int size;
	OpString name;
	CALL_FAILED_IF_ERROR(domctx->m_context->getActiveAttrib(prog ? prog->m_program : NULL, TruncateDoubleToUInt(argv[1].value.number), &type, &size, &name));
	if (type == 0)
		DOMSetNull(return_value);
	else
	{
		DOMWebGLActiveInfo* info;
		CALL_FAILED_IF_ERROR(DOMWebGLActiveInfo::Make(info, domctx->GetRuntime(), name, type, size));
		DOMSetObject(return_value, info);
	}
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getActiveUniform(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ON");
	DOMWebGLProgram* prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	if (prog && prog->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	unsigned int type;
	unsigned int size;
	OpString name;
	OP_STATUS err = domctx->m_context->getActiveUniform(prog ? prog->m_program : NULL, TruncateDoubleToUInt(argv[1].value.number), &type, &size, &name);
	if (OpStatus::IsError(err) || !type)
		DOMSetNull(return_value);
	else
	{
		DOMWebGLActiveInfo* info;
		CALL_FAILED_IF_ERROR(DOMWebGLActiveInfo::Make(info, domctx->GetRuntime(), name, type, size));
		DOMSetObject(return_value, info);
	}
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getAttachedShaders(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLProgram* prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	else
	{
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	if (prog->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	DOMWebGLObjectArray* buf;
	CALL_FAILED_IF_ERROR(DOMWebGLObjectArray::Make(buf, domctx->GetRuntime(), prog->m_program->getNumAttachedShaders()));
	for (unsigned int i = 0; i < prog->m_program->getNumAttachedShaders(); ++i)
	{
		DOM_Object *domo = DOM_GetHostObject(prog->m_program->getAttachedShaderESObject(i));
		OP_ASSERT(domo && domo->IsA(DOM_TYPE_WEBGLOBJECT));
		buf->m_objects[i] = static_cast<DOMWebGLObject*>(domo);
	}
	DOMSetObject(return_value, buf);
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getAttribLocation(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("Os");
	DOMWebGLProgram* prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object *domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	if (prog && prog->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetNumber(return_value, -1.);
		return ES_VALUE;
	}
	DOMSetNumber(return_value, domctx->m_context->getAttribLocation(prog ? prog->m_program : NULL, argv[1].value.string));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getParameter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	unsigned int param = TruncateDoubleToUInt(argv[0].value.number);

	// Special case compressed texture formats as it's a variable length array.
	if (param == WEBGL_COMPRESSED_TEXTURE_FORMATS)
	{
		unsigned int numFormats = domctx->m_context->getNumCompressedTextureFormats();
		unsigned int byteLength = sizeof(UINT32) * numFormats;
		ES_Object* values;
		RETURN_IF_ERROR(origining_runtime->CreateNativeArrayBufferObject(&values, byteLength));
		unsigned int *uintPtr = reinterpret_cast<unsigned int *>(origining_runtime->GetByteArrayStorage(values));
		for (unsigned int i = 0; i < numFormats; ++i)
			*uintPtr++ = domctx->m_context->getCompressedTextureFormat(i);
		ES_Object* ta;
		RETURN_IF_ERROR(origining_runtime->CreateNativeTypedArrayObject(&ta, ES_Runtime::TYPED_ARRAY_UINT32, values));
		DOM_Object::DOMSetObject(return_value, ta);
	}
	else
		CALL_FAILED_IF_ERROR(SetValueFromParameter(domctx->m_context->getParameter(param), return_value, origining_runtime));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getBufferParameter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int param = TruncateDoubleToUInt(argv[1].value.number);
	CALL_FAILED_IF_ERROR(SetValueFromParameter(domctx->m_context->getBufferParameter(target, param), return_value, origining_runtime));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getError(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMSetNumber(return_value, domctx->m_context->getError());
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getFramebufferAttachmentParameter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNN");
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int attachment = TruncateDoubleToUInt(argv[1].value.number);
	unsigned int param = TruncateDoubleToUInt(argv[2].value.number);
	CALL_FAILED_IF_ERROR(SetValueFromParameter(domctx->m_context->getFramebufferAttachmentParameter(target, attachment, param), return_value, origining_runtime));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getProgramParameter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ON");
	DOMWebGLProgram* prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	if (prog && prog->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	unsigned int param = TruncateDoubleToUInt(argv[1].value.number);
	CALL_FAILED_IF_ERROR(SetValueFromParameter(domctx->m_context->getProgramParameter(prog ? prog->m_program : NULL, param), return_value, origining_runtime));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getProgramInfoLog(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLProgram* prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	if (prog && prog->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetString(return_value, UNI_L(""));
		return ES_VALUE;
	}
	DOMSetString(return_value, domctx->m_context->getProgramInfoLog(prog ? prog->m_program : NULL));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getRenderbufferParameter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int param = TruncateDoubleToUInt(argv[1].value.number);
	CALL_FAILED_IF_ERROR(SetValueFromParameter(domctx->m_context->getRenderbufferParameter(target, param), return_value, origining_runtime));
	return ES_VALUE;
}


/* static */ int
DOMCanvasContextWebGL::getShaderParameter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ON");
	DOMWebGLShader* shader = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLSHADER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		shader = static_cast<DOMWebGLShader*>(domo);
	}
	if (shader && shader->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	unsigned int param = TruncateDoubleToUInt(argv[1].value.number);
	CALL_FAILED_IF_ERROR(SetValueFromParameter(domctx->m_context->getShaderParameter(shader ? shader->m_shader : NULL, param), return_value, origining_runtime));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getShaderInfoLog(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLShader* shader = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLSHADER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		shader = static_cast<DOMWebGLShader*>(domo);
	}
	if (shader && shader->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetString(return_value, UNI_L(""));
		return ES_VALUE;
	}
	DOMSetString(return_value, domctx->m_context->getShaderInfoLog(shader ? shader->m_shader : NULL));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getShaderSource(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLShader* shader = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLSHADER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		shader = static_cast<DOMWebGLShader*>(domo);
	}
	if (shader && shader->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetString(return_value, UNI_L(""));
		return ES_VALUE;
	}
	DOMSetString(return_value, domctx->m_context->getShaderSource(shader ? shader->m_shader : NULL));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getTexParameter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");

	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	if (target != WEBGL_TEXTURE_2D && target != WEBGL_TEXTURE_CUBE_MAP)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_ENUM);
		DOMSetNull(return_value);
		return ES_VALUE;
	}

	unsigned int param = TruncateDoubleToUInt(argv[1].value.number);
	if (param != WEBGL_TEXTURE_MAG_FILTER && param != WEBGL_TEXTURE_MIN_FILTER &&
		param != WEBGL_TEXTURE_WRAP_S && param != WEBGL_TEXTURE_WRAP_T)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_ENUM);
		DOMSetNull(return_value);
		return ES_VALUE;
	}

	CALL_FAILED_IF_ERROR(SetValueFromParameter(domctx->m_context->getTexParameter(target, param), return_value, origining_runtime));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getUniform(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("OO");

	DOMWebGLProgram *domProgram = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		domProgram = static_cast<DOMWebGLProgram*>(domo);
	}
	else if (argv[0].type == VALUE_UNDEFINED)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
		DOMSetNull(return_value);
		return ES_VALUE;
	}

	DOMWebGLUniformLocation *domLocation = NULL;
	if (argv[1].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[1].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		domLocation = static_cast<DOMWebGLUniformLocation*>(domo);
	}

	int location = domLocation ? domLocation->m_index : -1;
	const char* name = domLocation ? domLocation->m_name.CStr() : "";
	CanvasWebGLProgram *program = domProgram ? domProgram->m_program : NULL;

	if (!program)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	if (domProgram->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	if (domLocation && (program != domLocation->m_program || program->getLinkId() != domLocation->m_linkId))
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetNull(return_value);
		return ES_VALUE;
	}

	CanvasWebGLParameter param = domctx->m_context->getUniform(program, location, name);
	SetValueFromParameter(param, return_value, origining_runtime);

	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getUniformLocation(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("Os");
	DOMWebGLProgram *program = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		program = static_cast<DOMWebGLProgram*>(domo);
	}
	if (program && program->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	const uni_char *str = argv[1].value.string;
	int idx = domctx->m_context->getUniformLocation(program ? program->m_program : NULL, str);

	if (idx == -1)
		DOMSetNull(return_value);
	else
	{
		DOMWebGLUniformLocation* loc;
		CALL_FAILED_IF_ERROR(DOMWebGLUniformLocation::Make(loc, domctx->GetRuntime(), idx, str, program ? program->m_program : NULL));
		DOMSetObject(return_value, loc);
	}
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getVertexAttrib(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	unsigned int idx = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int param = TruncateDoubleToUInt(argv[1].value.number);
	CALL_FAILED_IF_ERROR(SetValueFromParameter(domctx->m_context->getVertexAttrib(idx, param), return_value, origining_runtime));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::getVertexAttribOffset(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	unsigned int idx = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int param = TruncateDoubleToUInt(argv[1].value.number);
	DOMSetNumber(return_value, domctx->m_context->getVertexAttribOffset(idx, param));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::hint(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int mode = TruncateDoubleToUInt(argv[1].value.number);
	domctx->m_context->hint(target, mode);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::isBuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_CANVASCONTEXTWEBGL);
	DOM_CHECK_ARGUMENTS("O");

	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object *domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLBUFFER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		DOMWebGLBuffer *buffer = static_cast<DOMWebGLBuffer*>(domo);
		DOMSetBoolean(return_value, buffer->m_buffer && buffer->m_buffer->hasBeenBound() && !buffer->m_buffer->isDestroyed());
	}
	else
		DOMSetBoolean(return_value, FALSE);

	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::isEnabled(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	DOMSetBoolean(return_value, domctx->m_context->isEnabled(TruncateDoubleToUInt(argv[0].value.number)));
	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::isFramebuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_CANVASCONTEXTWEBGL);
	DOM_CHECK_ARGUMENTS("O");

	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object *domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLFRAMEBUFFER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		DOMWebGLFramebuffer *frameBuffer = static_cast<DOMWebGLFramebuffer*>(domo);
		DOMSetBoolean(return_value, frameBuffer->m_framebuffer && frameBuffer->m_framebuffer->hasBeenBound() && !frameBuffer->m_framebuffer->isDestroyed());
	}
	else
		DOMSetBoolean(return_value, FALSE);

	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::isProgram(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_CANVASCONTEXTWEBGL);
	DOM_CHECK_ARGUMENTS("O");

	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object *domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		DOMWebGLProgram *program = static_cast<DOMWebGLProgram*>(domo);
		DOMSetBoolean(return_value, program->m_program && !program->m_program->isDestroyed());
	}
	else
		DOMSetBoolean(return_value, FALSE);

	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::isRenderbuffer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_CANVASCONTEXTWEBGL);
	DOM_CHECK_ARGUMENTS("O");

	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object *domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLRENDERBUFFER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		DOMWebGLRenderbuffer *renderBuffer = static_cast<DOMWebGLRenderbuffer*>(domo);
		DOMSetBoolean(return_value, renderBuffer->m_renderbuffer && renderBuffer->m_renderbuffer->hasBeenBound() && !renderBuffer->m_renderbuffer->isDestroyed());
	}
	else
		DOMSetBoolean(return_value, FALSE);

	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::isShader(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_CANVASCONTEXTWEBGL);
	DOM_CHECK_ARGUMENTS("O");

	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object *domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLSHADER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		DOMWebGLShader *shader = static_cast<DOMWebGLShader*>(domo);
		DOMSetBoolean(return_value, shader->m_shader && !shader->m_shader->isDestroyed());
	} else
		DOMSetBoolean(return_value, FALSE);

	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::isTexture(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_CANVASCONTEXTWEBGL);
	DOM_CHECK_ARGUMENTS("O");

	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object *domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLTEXTURE))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		DOMWebGLTexture *texture = static_cast<DOMWebGLTexture*>(domo);
		DOMSetBoolean(return_value, texture->m_texture && texture->m_texture->hasBeenBound() && !texture->m_texture->isDestroyed());
	}
	else
		DOMSetBoolean(return_value, FALSE);

	return ES_VALUE;
}

/* static */ int
DOMCanvasContextWebGL::lineWidth(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->lineWidth(TruncateDoubleToFloat(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::linkProgram(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLProgram* program = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		program = static_cast<DOMWebGLProgram*>(domo);
	}
	if (program && program->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}

	CALL_FAILED_IF_ERROR(domctx->m_context->linkProgram(program ? program->m_program : NULL));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::pixelStorei(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	domctx->m_context->pixelStorei(TruncateDoubleToUInt(argv[0].value.number), TruncateDoubleToInt(argv[1].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::polygonOffset(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	domctx->m_context->polygonOffset(TruncateDoubleToFloat(argv[0].value.number), TruncateDoubleToFloat(argv[1].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::readPixels(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNNNNO");
	if (domctx->m_context->GetCanvas() && !domctx->m_context->GetCanvas()->IsSecure())
	{
		// Raise security exception
		return ES_EXCEPT_SECURITY;
	}
	int x = TruncateDoubleToInt(argv[0].value.number);
	int y = TruncateDoubleToInt(argv[1].value.number);
	int width = TruncateDoubleToInt(argv[2].value.number);
	int height = TruncateDoubleToInt(argv[3].value.number);
	unsigned int format = TruncateDoubleToUInt(argv[4].value.number);
	unsigned int type = TruncateDoubleToUInt(argv[5].value.number);

	// Check that type and format are valid enums.
	if ((type != WEBGL_UNSIGNED_BYTE && type != WEBGL_UNSIGNED_SHORT_5_6_5 &&
		type != WEBGL_UNSIGNED_SHORT_4_4_4_4 && type != WEBGL_UNSIGNED_SHORT_5_5_5_1) ||
		(format != WEBGL_ALPHA && format != WEBGL_RGB && format != WEBGL_RGBA))
	{
		domctx->m_context->raiseError(WEBGL_INVALID_ENUM);
		return ES_FAILED;
	}

	UINT8* arr = NULL;
	unsigned arrlen = 0;
	if (argv[6].type == VALUE_OBJECT)
	{
		ES_Object* obj = argv[6].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		if (!(origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_UINT8) ||
		      origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_UINT8CLAMPED)) ||
		      type != WEBGL_UNSIGNED_BYTE || format != WEBGL_RGBA)
		{
			domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
			return ES_FAILED;
		}
		arr = static_cast<UINT8*>(origining_runtime->GetStaticTypedArrayStorage(obj));
		arrlen = origining_runtime->GetStaticTypedArrayLength(obj);
	}
	else
	{
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
		return ES_FAILED;
	}
	CALL_FAILED_IF_ERROR(domctx->m_context->readPixels(x, y, width, height, format, type, arr, arrlen));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::renderbufferStorage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNN");
	CALL_FAILED_IF_ERROR(domctx->m_context->renderbufferStorage(TruncateDoubleToUInt(argv[0].value.number), TruncateDoubleToUInt(argv[1].value.number), TruncateDoubleToInt(argv[2].value.number), TruncateDoubleToInt(argv[3].value.number)));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::sampleCoverage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("Nb");
	domctx->m_context->sampleCoverage(TruncateDoubleToGLclampf(argv[0].value.number), argv[1].value.boolean);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::scissor(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNN");
	domctx->m_context->scissor(TruncateDoubleToInt(argv[0].value.number), TruncateDoubleToInt(argv[1].value.number), TruncateDoubleToInt(argv[2].value.number), TruncateDoubleToInt(argv[3].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::shaderSource(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("Os");
	DOMWebGLShader* shader = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLSHADER))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		shader = static_cast<DOMWebGLShader*>(domo);
	}
	if (shader && shader->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}
	CALL_FAILED_IF_ERROR(domctx->m_context->shaderSource(shader ? shader->m_shader : NULL, argv[1].value.string));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::stencilFunc(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNN");
	domctx->m_context->stencilFunc(TruncateDoubleToUInt(argv[0].value.number), TruncateDoubleToInt(argv[1].value.number), TruncateDoubleToUInt(argv[2].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::stencilFuncSeparate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNN");
	domctx->m_context->stencilFuncSeparate(TruncateDoubleToUInt(argv[0].value.number), TruncateDoubleToUInt(argv[1].value.number), TruncateDoubleToInt(argv[2].value.number), TruncateDoubleToUInt(argv[3].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::stencilMask(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("N");
	domctx->m_context->stencilMask(TruncateDoubleToUInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::stencilMaskSeparate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NN");
	domctx->m_context->stencilMaskSeparate(TruncateDoubleToUInt(argv[0].value.number), TruncateDoubleToUInt(argv[1].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::stencilOp(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNN");
	domctx->m_context->stencilOp(TruncateDoubleToUInt(argv[0].value.number), TruncateDoubleToUInt(argv[1].value.number), TruncateDoubleToUInt(argv[2].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::stencilOpSeparate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNN");
	domctx->m_context->stencilOpSeparate(TruncateDoubleToUInt(argv[0].value.number), TruncateDoubleToUInt(argv[1].value.number), TruncateDoubleToUInt(argv[2].value.number), TruncateDoubleToUInt(argv[3].value.number));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::texImage2D(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// Make sure the image is loaded with width and height available
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);

	DOM_Object* dh;
	unsigned int target = 0;
	int level = 0;
	unsigned int internalFormat = 0;
	unsigned int format = 0;
	unsigned int type = 0;

	DOM_CHECK_ARGUMENTS("NNNNN");
	target = TruncateDoubleToUInt(argv[0].value.number);
	level = TruncateDoubleToInt(argv[1].value.number);
	internalFormat = TruncateDoubleToUInt(argv[2].value.number);

	if (argc < 6)
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

	if (argv[5].type == VALUE_NULL || argv[5].type == VALUE_UNDEFINED)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
		return ES_FAILED;
	}
	else if (argv[5].type == VALUE_OBJECT)
	{
		format = TruncateDoubleToUInt(argv[3].value.number);
		type = TruncateDoubleToUInt(argv[4].value.number);
		dh = DOM_GetHostObject(argv[5].value.object);
		if (!dh)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	}
	else
	{
		DOM_CHECK_ARGUMENTS("NNNNNNNNO");
		int width = TruncateDoubleToInt(argv[3].value.number);
		int height = TruncateDoubleToInt(argv[4].value.number);
		int border = TruncateDoubleToInt(argv[5].value.number);
		format = TruncateDoubleToUInt(argv[6].value.number);
		type = TruncateDoubleToUInt(argv[7].value.number);
		if (argv[8].type == VALUE_OBJECT)
		{
			ES_Object* obj = argv[8].value.object;
			if (!origining_runtime->IsNativeTypedArrayObject(obj))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

			if (origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_UINT8) ||
			    origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_UINT8CLAMPED))
				CALL_FAILED_IF_ERROR(domctx->m_context->texImage2D(target, level, internalFormat, width, height, border, format, type,
				        static_cast<UINT8*>(origining_runtime->GetStaticTypedArrayStorage(obj)), origining_runtime->GetStaticTypedArrayLength(obj)*sizeof(UINT8)));
			else if (origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_UINT16))
				CALL_FAILED_IF_ERROR(domctx->m_context->texImage2D(target, level, internalFormat, width, height, border, format, type,
				        static_cast<UINT16*>(origining_runtime->GetStaticTypedArrayStorage(obj)), origining_runtime->GetStaticTypedArrayLength(obj)*sizeof(UINT16)));
			else if (origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
				CALL_FAILED_IF_ERROR(domctx->m_context->texImage2D(target, level, internalFormat, width, height, border, format, type,
				        static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj)), origining_runtime->GetStaticTypedArrayLength(obj)*sizeof(float)));
			else
				domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
			return ES_FAILED;
		}
		// null or undefined.
		CALL_FAILED_IF_ERROR(domctx->m_context->texImage2D(target, level, internalFormat, width, height, border, format, type));
		return ES_FAILED;
	}

	if (dh && dh->IsA(DOM_TYPE_CANVASIMAGEDATA))
	{
		DOMCanvasImageData *imgData = static_cast<DOMCanvasImageData*>(dh);
		VEGASWBuffer non_premultiplied;

		// Possible optimization: Extra allocation and copy done to convert between RGBA and internal pixel format.
		if (OpStatus::IsError(non_premultiplied.Create(imgData->GetWidth(), imgData->GetHeight())))
			return ES_NO_MEMORY;
		VEGA_PIXEL_FORMAT_CLASS::Accessor dst = non_premultiplied.GetAccessor(0,0);
		UINT8 *end = imgData->GetPixels() + 4 * imgData->GetWidth() * imgData->GetHeight();
		for (UINT8 *raw = imgData->GetPixels(); raw < end; raw += 4, ++dst)
			dst.StoreARGB(raw[3], raw[0], raw[1], raw[2]);

		domctx->m_context->texImage2D(target, level, internalFormat, format, type, NULL, &non_premultiplied);

		non_premultiplied.Destroy();

		return ES_FAILED;
	}

	URL src_url;
	OpBitmap *bitmap = NULL;
	VEGAFill* fill = NULL;
	Image img;
	VEGASWBuffer *non_premultiplied = NULL;
	VEGASWBuffer **non_premultiplied_ptr = NULL;
#ifdef USE_PREMULTIPLIED_ALPHA
	if (!domctx->m_context->UnpackPremultipliedAlpha())
		non_premultiplied_ptr = &non_premultiplied;
#endif
	BOOL releaseImage = FALSE;
	BOOL releaseBitmap = FALSE;
	BOOL tainted = FALSE;
	unsigned int width, height;
	BOOL ignoreColorSpaceConversions = domctx->m_context->UnpackColorSpaceConversion() == WEBGL_NONE;

	CanvasResourceDecoder::DecodeStatus status = CanvasResourceDecoder::DecodeResource(dh, domctx->m_domcanvas, origining_runtime, -1, -1, FALSE, src_url, bitmap, fill, non_premultiplied_ptr, ignoreColorSpaceConversions, img, releaseImage, releaseBitmap, width, height, tainted);

	if (status == CanvasResourceDecoder::DECODE_OOM)
		return ES_NO_MEMORY;
	else if (status == CanvasResourceDecoder::DECODE_EXCEPTION_TYPE_MISMATCH)
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	else if (status == CanvasResourceDecoder::DECODE_EXCEPTION_NOT_SUPPORTED)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	else if (status != CanvasResourceDecoder::DECODE_OK)
		return ES_FAILED;

	int retval = ES_FAILED;
	if (tainted || !CanvasOriginCheckWebGL(domctx, src_url, domctx->GetRuntime()->GetOriginURL(), origining_runtime))
	{
		// Raise security exception
		retval = ES_EXCEPT_SECURITY;
	}
	else if (bitmap || non_premultiplied)
		retval = OpStatus::IsMemoryError(domctx->m_context->texImage2D(target, level, internalFormat, format, type, bitmap, non_premultiplied)) ? ES_NO_MEMORY : ES_FAILED;

	if (releaseImage)
	{
		if (bitmap)
			img.ReleaseBitmap();
		img.DecVisible(null_image_listener);
	}
	if (releaseBitmap)
		OP_DELETE(bitmap);
	if (non_premultiplied)
	{
		non_premultiplied->Destroy();
		OP_DELETE(non_premultiplied);
	}

	return retval;
}

/* static */ int
DOMCanvasContextWebGL::texParameterf(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNn");
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int param = TruncateDoubleToUInt(argv[1].value.number);
	float val = TruncateDoubleToFloat(argv[2].value.number);
	domctx->m_context->texParameterf(target, param, val);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::texParameteri(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNN");
	unsigned int target = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int param = TruncateDoubleToUInt(argv[1].value.number);
	int val = TruncateDoubleToInt(argv[2].value.number);
	domctx->m_context->texParameteri(target, param, val);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::texSubImage2D(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// Make sure the image is loaded with width and height available
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);

	DOM_Object* dh = NULL;
	unsigned int target = 0;
	int level = 0;
	int xoffset = 0;
	int yoffset = 0;
	unsigned int format = 0;
	unsigned int type = 0;

	DOM_CHECK_ARGUMENTS("NNNNNN");
	target = TruncateDoubleToUInt(argv[0].value.number);
	level = TruncateDoubleToInt(argv[1].value.number);
	xoffset = TruncateDoubleToInt(argv[2].value.number);
	yoffset = TruncateDoubleToInt(argv[3].value.number);

	if (argc < 7)
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

	if (argv[6].type == VALUE_NULL || argv[6].type == VALUE_UNDEFINED)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
		return ES_FAILED;
	}
	else if (argv[6].type == VALUE_OBJECT)
	{
		DOM_CHECK_ARGUMENTS("NNNNNNO");
		format = TruncateDoubleToUInt(argv[4].value.number);
		type = TruncateDoubleToUInt(argv[5].value.number);
		if (argv[6].type == VALUE_OBJECT)
		{
			dh = DOM_GetHostObject(argv[6].value.object);
			if (!dh)
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
	}
	else
	{
		DOM_CHECK_ARGUMENTS("NNNNNNNNO");
		if (argv[8].type == VALUE_NULL || argv[8].type == VALUE_UNDEFINED)
		{
			domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
			return ES_FAILED;
		}
		int width = TruncateDoubleToInt(argv[4].value.number);
		int height = TruncateDoubleToInt(argv[5].value.number);
		format = TruncateDoubleToUInt(argv[6].value.number);
		type = TruncateDoubleToUInt(argv[7].value.number);

		ES_Object* obj = argv[8].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		if (origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_UINT8) ||
		    origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_UINT8CLAMPED))
			CALL_FAILED_IF_ERROR(domctx->m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
				    static_cast<UINT8*>(origining_runtime->GetStaticTypedArrayStorage(obj)), origining_runtime->GetStaticTypedArrayLength(obj)*sizeof(UINT8)));
		else if (origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_UINT16))
			CALL_FAILED_IF_ERROR(domctx->m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
				    static_cast<UINT16*>(origining_runtime->GetStaticTypedArrayStorage(obj)), origining_runtime->GetStaticTypedArrayLength(obj)*sizeof(UINT16)));
		else
			domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}

	if (dh && dh->IsA(DOM_TYPE_CANVASIMAGEDATA))
	{
		DOMCanvasImageData *imgData = static_cast<DOMCanvasImageData*>(dh);
		VEGASWBuffer non_premultiplied;

		// OPTIMIZE: Extra allocation and copy done to convert between RGBA and internal pixel format.
		if (OpStatus::IsError(non_premultiplied.Create(imgData->GetWidth(), imgData->GetHeight())))
			return ES_NO_MEMORY;
		VEGA_PIXEL_FORMAT_CLASS::Accessor dst = non_premultiplied.GetAccessor(0,0);
		UINT8 *end = imgData->GetPixels() + 4 * imgData->GetWidth() * imgData->GetHeight();
		for (UINT8 *raw = imgData->GetPixels(); raw < end; raw += 4, ++dst)
			dst.StoreARGB(raw[3], raw[0], raw[1], raw[2]);

		domctx->m_context->texSubImage2D(target, level, xoffset, yoffset, format, type, NULL, &non_premultiplied);

		non_premultiplied.Destroy();

		return ES_FAILED;
	}

	URL src_url;
	OpBitmap *bitmap = NULL;
	VEGAFill* fill = NULL;
	Image img;
	VEGASWBuffer *non_premultiplied = NULL;
	VEGASWBuffer **non_premultiplied_ptr = NULL;
#ifdef USE_PREMULTIPLIED_ALPHA
	if (!domctx->m_context->UnpackPremultipliedAlpha())
		non_premultiplied_ptr = &non_premultiplied;
#endif
	BOOL releaseImage = FALSE;
	BOOL releaseBitmap = FALSE;
	BOOL tainted = FALSE;
	unsigned int width, height;
	BOOL ignoreColorSpaceConversions = domctx->m_context->UnpackColorSpaceConversion() == WEBGL_NONE;

	CanvasResourceDecoder::DecodeStatus status = CanvasResourceDecoder::DecodeResource(dh, domctx->m_domcanvas, origining_runtime, -1, -1, FALSE, src_url, bitmap, fill, non_premultiplied_ptr, ignoreColorSpaceConversions, img, releaseImage, releaseBitmap, width, height, tainted);

	if (status == CanvasResourceDecoder::DECODE_OOM)
		return ES_NO_MEMORY;
	else if (status == CanvasResourceDecoder::DECODE_EXCEPTION_TYPE_MISMATCH)
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	else if (status == CanvasResourceDecoder::DECODE_EXCEPTION_NOT_SUPPORTED)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	else if (status != CanvasResourceDecoder::DECODE_OK)
		return ES_FAILED;

	int retval = ES_FAILED;
	if (tainted || !CanvasOriginCheckWebGL(domctx, src_url, domctx->GetRuntime()->GetOriginURL(), origining_runtime))
	{
		// Raise security exception
		retval = ES_EXCEPT_SECURITY;
	}
	else if (bitmap || non_premultiplied)
		retval = OpStatus::IsMemoryError(domctx->m_context->texSubImage2D(target, level, xoffset, yoffset, format, type, bitmap, non_premultiplied)) ? ES_NO_MEMORY : ES_FAILED;

	if (releaseImage)
	{
		if (bitmap)
			img.ReleaseBitmap();
		img.DecVisible(null_image_listener);
	}
	if (releaseBitmap)
		OP_DELETE(bitmap);
	if (non_premultiplied)
	{
		non_premultiplied->Destroy();
		OP_DELETE(non_premultiplied);
	}

	return retval;
}

/* static */ int
DOMCanvasContextWebGL::uniform1f(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ON");
	DOMWebGLUniformLocation* location = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = static_cast<DOMWebGLUniformLocation*>(domo);
	}
	else
		return ES_FAILED;

	float val = TruncateDoubleToFloat(argv[1].value.number);
	domctx->m_context->uniform1(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), &val, 1);
	return ES_FAILED;
}

class DOMTypedArrayConverter
	: public ES_AsyncCallback,
	  public DOM_Object
{
public:
	DOMTypedArrayConverter()
		: m_typedArrayObject(NULL), m_location(NULL)
	{}
	virtual ~DOMTypedArrayConverter()
	{}

	virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
	{
		if (status == ES_ASYNC_SUCCESS && result.type == VALUE_OBJECT)
			m_typedArrayObject = result.value.object;
		return OpStatus::OK;
	}

	virtual void GCTrace()
	{
		GCMark(m_typedArrayObject);
		GCMark(m_location);
	}

	static int Start(const ES_Value& val, ES_Value* return_value, DOM_Runtime *origining_runtime, ES_Runtime::NativeTypedArrayKind kind, DOMWebGLUniformLocation* loc, unsigned int idx)
	{
		DOMTypedArrayConverter* aconv = OP_NEW(DOMTypedArrayConverter, ());
		CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(aconv, origining_runtime));
		CALL_FAILED_IF_ERROR(aconv->Put(UNI_L("dataobj"), val));

		aconv->m_location = loc;
		aconv->m_index = idx;

		ES_Object *esobject = *aconv;

		// Attempt to convert using ES_AyncInterface->Eval if it contains the correct parameters
		ES_Thread* thread = GetCurrentThread(origining_runtime);
		ES_AsyncInterface* asyncif = origining_runtime->GetEnvironment()->GetAsyncInterface();
		asyncif->SetWantExceptions();
		const uni_char* script;
		switch (kind)
		{
		case ES_Runtime::TYPED_ARRAY_INT32:
			script = UNI_L("return new Int32Array(dataobj);");
			break;
		case ES_Runtime::TYPED_ARRAY_FLOAT32:
			script = UNI_L("return new Float32Array(dataobj);");
			break;
		default:
			OP_ASSERT(!"Unimplemented typed array conversion");
			return ES_FAILED;
		}
		CALL_FAILED_IF_ERROR(asyncif->Eval(script, &esobject, 1, aconv, thread));
		DOM_Object::DOMSetObject(return_value, aconv);
		return ES_RESTART|ES_SUSPEND;
	}
	ES_Object* GetTypedArrayObject(){return m_typedArrayObject;}
	DOMWebGLUniformLocation* GetLocation(){return m_location;}
	unsigned int GetIndexArgument(){return m_index;}
private:
	ES_Object* m_typedArrayObject;
	DOMWebGLUniformLocation* m_location;
	unsigned int m_index;
};

/* static */ int
DOMCanvasContextWebGL::uniform1fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Oo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length == 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniform1(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform1i(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ON");
	DOMWebGLUniformLocation* location = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = static_cast<DOMWebGLUniformLocation*>(domo);
	}
	else
		return ES_FAILED;

	int val = TruncateDoubleToInt(argv[1].value.number);
	domctx->m_context->uniform1(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), &val, 1);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform1iv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Oo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_INT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_INT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length == 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniform1(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<int*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform2f(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ONN");
	DOMWebGLUniformLocation* location = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = static_cast<DOMWebGLUniformLocation*>(domo);
	}
	else
		return ES_FAILED;

	float val[2];
	val[0] = TruncateDoubleToFloat(argv[1].value.number);
	val[1] = TruncateDoubleToFloat(argv[2].value.number);
	domctx->m_context->uniform2(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), val, 1);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform2fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Oo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 2 || length % 2 != 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniform2(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length/2);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform2i(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ONN");
	DOMWebGLUniformLocation* location = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = static_cast<DOMWebGLUniformLocation*>(domo);
	}
	else
		return ES_FAILED;

	int val[2];
	val[0] = TruncateDoubleToInt(argv[1].value.number);
	val[1] = TruncateDoubleToInt(argv[2].value.number);
	domctx->m_context->uniform2(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), val, 1);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform2iv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Oo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_INT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_INT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 2 || length % 2 != 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniform2(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<int*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length/2);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform3f(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ONNN");
	DOMWebGLUniformLocation* location = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = static_cast<DOMWebGLUniformLocation*>(domo);
	}
	else
		return ES_FAILED;

	float val[3];
	val[0] = TruncateDoubleToFloat(argv[1].value.number);
	val[1] = TruncateDoubleToFloat(argv[2].value.number);
	val[2] = TruncateDoubleToFloat(argv[3].value.number);
	domctx->m_context->uniform3(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), val, 1);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform3fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Oo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 3 || length % 3 != 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniform3(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length/3);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform3i(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ONNN");
	DOMWebGLUniformLocation* location = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = static_cast<DOMWebGLUniformLocation*>(domo);
	}
	else
		return ES_FAILED;

	int val[3];
	val[0] = TruncateDoubleToInt(argv[1].value.number);
	val[1] = TruncateDoubleToInt(argv[2].value.number);
	val[2] = TruncateDoubleToInt(argv[3].value.number);
	domctx->m_context->uniform3(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), val, 1);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform3iv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Oo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_INT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_INT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 3 || length % 3 != 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniform3(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<int*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length/3);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform4f(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ONNNN");
	DOMWebGLUniformLocation* location = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = static_cast<DOMWebGLUniformLocation*>(domo);
	}
	else
		return ES_FAILED;

	float val[4];
	val[0] = TruncateDoubleToFloat(argv[1].value.number);
	val[1] = TruncateDoubleToFloat(argv[2].value.number);
	val[2] = TruncateDoubleToFloat(argv[3].value.number);
	val[3] = TruncateDoubleToFloat(argv[4].value.number);
	domctx->m_context->uniform4(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), val, 1);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform4fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Oo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 4 || length % 4 != 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniform4(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length/4);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform4i(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("ONNNN");
	DOMWebGLUniformLocation* location = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = static_cast<DOMWebGLUniformLocation*>(domo);
	}
	else
		return ES_FAILED;

	int val[4];
	val[0] = TruncateDoubleToInt(argv[1].value.number);
	val[1] = TruncateDoubleToInt(argv[2].value.number);
	val[2] = TruncateDoubleToInt(argv[3].value.number);
	val[3] = TruncateDoubleToInt(argv[4].value.number);
	domctx->m_context->uniform4(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), val, 1);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniform4iv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Oo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_INT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_INT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 4 || length % 4 != 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniform4(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<int*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length/4);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniformMatrix2fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Obo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		if (argv[1].value.boolean)
		{
			domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
			return ES_FAILED;
		}

		obj = argv[2].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[2], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 4 || length % 4 != 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniformMatrix2(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length/4, FALSE);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniformMatrix3fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Obo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		if (argv[1].value.boolean)
		{
			domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
			return ES_FAILED;
		}

		obj = argv[2].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[2], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 9 || length % 9 != 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniformMatrix3(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length/9, FALSE);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::uniformMatrix4fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOMWebGLUniformLocation *location = NULL;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		location = ac->GetLocation();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Obo");
		if (argv[0].type == VALUE_OBJECT)
		{
			DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
			if (!domo || !domo->IsA(DOM_TYPE_WEBGLUNIFORMLOCATION))
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			location = static_cast<DOMWebGLUniformLocation*>(domo);
		}
		else
			return ES_FAILED;

		if (argv[1].value.boolean)
		{
			domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
			return ES_FAILED;
		}

		obj = argv[2].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[2], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, location, 0);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 16 || length % 16 != 0)
		domctx->m_context->raiseError(WEBGL_INVALID_VALUE);
	else
		domctx->m_context->uniformMatrix4(location->m_program, location->m_linkId, location->m_index, location->m_name.CStr(), static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj)), length/16, FALSE);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::useProgram(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLProgram *prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (domo && !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	if (prog && prog->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}

	domctx->m_context->useProgram(prog ? prog->m_program : NULL);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::validateProgram(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("O");
	DOMWebGLProgram *prog = NULL;
	if (argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* domo = DOM_GetHostObject(argv[0].value.object);
		if (domo && !domo->IsA(DOM_TYPE_WEBGLPROGRAM))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		prog = static_cast<DOMWebGLProgram*>(domo);
	}
	if (prog && prog->m_owner != domctx)
	{
		domctx->m_context->raiseError(WEBGL_INVALID_OPERATION);
		return ES_FAILED;
	}

	CALL_FAILED_IF_ERROR(domctx->m_context->validateProgram(prog ? prog->m_program : NULL));
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::vertexAttrib1f(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("Nn");
	unsigned int index = TruncateDoubleToUInt(argv[0].value.number);
	float val[4] = { 0, 0, 0, 1.f };
	val[0] = TruncateDoubleToFloat(argv[1].value.number);
	domctx->m_context->vertexAttrib(index, val);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::vertexAttrib1fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	unsigned int index;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		index = ac->GetIndexArgument();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("No");
		index = TruncateDoubleToUInt(argv[0].value.number);

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, NULL, index);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 1)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	float val[4] = { 0, 0, 0, 1.f };
	val[0] = static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj))[0];
	domctx->m_context->vertexAttrib(index, val);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::vertexAttrib2f(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("Nnn");
	unsigned int index = TruncateDoubleToUInt(argv[0].value.number);
	float val[4] = { 0, 0, 0, 1.f };
	val[0] = TruncateDoubleToFloat(argv[1].value.number);
	val[1] = TruncateDoubleToFloat(argv[2].value.number);
	domctx->m_context->vertexAttrib(index, val);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::vertexAttrib2fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	unsigned int index;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		index = ac->GetIndexArgument();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("No");
		index = TruncateDoubleToUInt(argv[0].value.number);

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, NULL, index);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 2)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	float val[4] = { 0, 0, 0, 1.f };
	val[0] = static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj))[0];
	val[1] = static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj))[1];
	domctx->m_context->vertexAttrib(index, val);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::vertexAttrib3f(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("Nnnn");
	unsigned int index = TruncateDoubleToUInt(argv[0].value.number);
	float val[4] = { 0, 0, 0, 1.f };
	val[0] = TruncateDoubleToFloat(argv[1].value.number);
	val[1] = TruncateDoubleToFloat(argv[2].value.number);
	val[2] = TruncateDoubleToFloat(argv[3].value.number);
	domctx->m_context->vertexAttrib(index, val);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::vertexAttrib3fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	unsigned int index;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		index = ac->GetIndexArgument();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("No");
		index = TruncateDoubleToUInt(argv[0].value.number);

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, NULL, index);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 3)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	float val[4] = { 0, 0, 0, 1.f };
	val[0] = static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj))[0];
	val[1] = static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj))[1];
	val[2] = static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj))[2];
	domctx->m_context->vertexAttrib(index, val);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::vertexAttrib4f(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("Nnnnn");
	unsigned int index = TruncateDoubleToUInt(argv[0].value.number);
	float val[4] = { 0, 0, 0, 1.f };
	val[0] = TruncateDoubleToFloat(argv[1].value.number);
	val[1] = TruncateDoubleToFloat(argv[2].value.number);
	val[2] = TruncateDoubleToFloat(argv[3].value.number);
	val[3] = TruncateDoubleToFloat(argv[4].value.number);
	domctx->m_context->vertexAttrib(index, val);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::vertexAttrib4fv(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	unsigned int index;

	ES_Object* obj = NULL;
	if (argc < 0)
	{
		DOMTypedArrayConverter* ac = DOM_VALUE2OBJECT(*return_value, DOMTypedArrayConverter);
		obj = ac->GetTypedArrayObject();
		if (!obj)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		index = ac->GetIndexArgument();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("No");
		index = TruncateDoubleToUInt(argv[0].value.number);

		obj = argv[1].value.object;
		if (!origining_runtime->IsNativeTypedArrayObject(obj, ES_Runtime::TYPED_ARRAY_FLOAT32))
			return DOMTypedArrayConverter::Start(argv[1], return_value, origining_runtime, ES_Runtime::TYPED_ARRAY_FLOAT32, NULL, index);
	}

	unsigned length = origining_runtime->GetStaticTypedArrayLength(obj);
	if (length < 4)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	float val[4] = { 0, 0, 0, 1.f };
	val[0] = static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj))[0];
	val[1] = static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj))[1];
	val[2] = static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj))[2];
	val[3] = static_cast<float*>(origining_runtime->GetStaticTypedArrayStorage(obj))[3];
	domctx->m_context->vertexAttrib(index, val);
	return ES_FAILED;
}


/* static */ int
DOMCanvasContextWebGL::vertexAttribPointer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNbNN");
	unsigned int idx = TruncateDoubleToUInt(argv[0].value.number);
	int size = TruncateDoubleToInt(argv[1].value.number);
	unsigned int type = TruncateDoubleToUInt(argv[2].value.number);
	BOOL norm = argv[3].value.boolean;
	int stride = TruncateDoubleToInt(argv[4].value.number);
	int offset = 0;

	// This parameter is a GLintptr which is defined to be a web idl long long
	// so we do some extra checks in the dom layer as we only use 32-bit offsets
	// internally.
	unsigned int webglErr = WebGLLongLongToInt(argv[5], offset, WEBGL_INVALID_VALUE, WEBGL_INVALID_VALUE);
	if (webglErr != WEBGL_NO_ERROR)
	{
		domctx->m_context->raiseError(webglErr);
		return ES_FAILED;
	}

	domctx->m_context->vertexAttribPointer(idx, size, type, norm, stride, offset);
	return ES_FAILED;
}

/* static */ int
DOMCanvasContextWebGL::viewport(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domctx, DOM_TYPE_CANVASCONTEXTWEBGL, DOMCanvasContextWebGL);
	DOM_CHECK_ARGUMENTS("NNNN");
	domctx->m_context->viewport(TruncateDoubleToInt(argv[0].value.number), TruncateDoubleToInt(argv[1].value.number), TruncateDoubleToInt(argv[2].value.number), TruncateDoubleToInt(argv[3].value.number));
	return ES_FAILED;
}

//////////////////////////////////////////////////////////////////////////

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOMCanvasContextWebGL)
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getContextAttributes, "getContextAttributes", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::isContextLost, "isContextLost", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getSupportedExtensions, "getSupportedExtensions", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getExtension, "getExtension", "s-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::activeTexture, "activeTexture", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::attachShader, "attachShader", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::bindAttribLocation, "bindAttribLocation", "-ns-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::bindBuffer, "bindBuffer", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::bindFramebuffer, "bindFramebuffer", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::bindRenderbuffer, "bindRenderbuffer", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::bindTexture, "bindTexture", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::blendColor, "blendColor", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::blendEquation, "blendEquation", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::blendEquationSeparate, "blendEquationSeparate", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::blendFunc, "blendFunc", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::blendFuncSeparate, "blendFuncSeparate", "nnnn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::bufferData, "bufferData", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::bufferSubData, "bufferSubData", "nn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::checkFramebufferStatus, "checkFramebufferStatus", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::clear, "clear", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::clearColor, "clearColor", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::clearDepth, "clearDepth", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::clearStencil, "clearStencil", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::colorMask, "colorMask", "bbbb-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::compileShader, "compileShader", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::compressedTexImage2D, "compressedTexImage2D", "nnnnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::compressedTexSubImage2D, "compressedTexSubImage2D", "nnnnnnn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::copyTexImage2D, "copyTexImage2D", "nnnnnnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::copyTexSubImage2D, "copyTexSubImage2D", "nnnnnnnn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::createBuffer, "createBuffer", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::createFramebuffer, "createFramebuffer", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::createProgram, "createProgram", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::createRenderbuffer, "createRenderbuffer", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::createShader, "createShader", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::createTexture, "createTexture", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::cullFace, "cullFace", "n-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::deleteBuffer, "deleteBuffer", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::deleteFramebuffer, "deleteFramebuffer", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::deleteProgram, "deleteProgram", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::deleteRenderbuffer, "deleteRenderbuffer", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::deleteShader, "deleteShader", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::deleteTexture, "deleteTexture", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::depthFunc, "depthFunc", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::depthMask, "depthMask", "b-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::depthRange, "depthRange", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::detachShader, "detachShader", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::disable, "disable", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::disableVertexAttribArray, "disableVertexAttribArray", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::drawArrays, "drawArrays", "nnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::drawElements, "drawElements", "nnnn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::enable, "enable", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::enableVertexAttribArray, "enableVertexAttribArray", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::finish, "finish", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::flush, "flush", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::framebufferRenderbuffer, "framebufferRenderbuffer", "nnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::framebufferTexture2D, "framebufferTexture2D", "nnn-n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::frontFace, "frontFace", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::generateMipmap, "generateMipmap", "n-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getActiveAttrib, "getActiveAttrib", "-n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getActiveUniform, "getActiveUniform", "-n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getAttachedShaders, "getAttachedShaders", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getAttribLocation, "getAttribLocation", "-s-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getParameter, "getParameter", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getBufferParameter, "getBufferParameter", "nn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getError, "getError", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getFramebufferAttachmentParameter, "getFramebufferAttachmentParameter", "nnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getProgramParameter, "getProgramParameter", "-n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getProgramInfoLog, "getProgramInfoLog", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getRenderbufferParameter, "getRenderbufferParameter", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getShaderParameter, "getShaderParameter", "-n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getShaderInfoLog, "getShaderInfoLog", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getShaderSource, "getShaderSource", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getTexParameter, "getTexParameter", "nn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getUniform, "getUniform", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getUniformLocation, "getUniformLocation", "-s-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getVertexAttrib, "getVertexAttrib", "nn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::getVertexAttribOffset, "getVertexAttribOffset", "nn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::hint, "hint", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::isBuffer, "isBuffer", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::isEnabled, "isEnabled", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::isFramebuffer, "isFramebuffer", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::isProgram, "isProgram", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::isRenderbuffer, "isRenderbuffer", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::isShader, "isShader", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::isTexture, "isTexture", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::lineWidth, "lineWidth", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::linkProgram, "linkProgram", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::pixelStorei, "pixelStorei", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::polygonOffset, "polygonOffset", "nn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::readPixels, "readPixels", "nnnnnn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::renderbufferStorage, "renderbufferStorage", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::sampleCoverage, "sampleCoverage", "nb-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::scissor, "scissor", "nnnn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::shaderSource, "shaderSource", "-s-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::stencilFunc, "stencilFunc", "nnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::stencilFuncSeparate, "stencilFuncSeparate", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::stencilMask, "stencilMask", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::stencilMaskSeparate, "stencilMaskSeparate", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::stencilOp, "stencilOp", "nnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::stencilOpSeparate, "stencilOpSeparate", "nnnn-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::texImage2D, "texImage2D", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::texParameterf, "texParameterf", "nnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::texParameteri, "texParameteri", "nnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::texSubImage2D, "texSubImage2D", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform1f, "uniform1f", "-n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform1fv, "uniform1fv", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform1i, "uniform1i", "-n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform1iv, "uniform1iv", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform2f, "uniform2f", "-nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform2fv, "uniform2fv", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform2i, "uniform2i", "-nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform2iv, "uniform2iv", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform3f, "uniform3f", "-nnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform3fv, "uniform3fv", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform3i, "uniform3i", "-nnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform3iv, "uniform3iv", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform4f, "uniform4f", "-nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform4fv, "uniform4fv", "")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform4i, "uniform4i", "-nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniform4iv, "uniform4iv", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniformMatrix2fv, "uniformMatrix2fv", "-b-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniformMatrix3fv, "uniformMatrix3fv", "-b-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::uniformMatrix4fv, "uniformMatrix4fv", "-b-")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::useProgram, "useProgram", "--")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::validateProgram, "validateProgram", "")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::vertexAttrib1f, "vertexAttrib1f", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::vertexAttrib1fv, "vertexAttrib1fv", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::vertexAttrib2f, "vertexAttrib2f", "nnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::vertexAttrib2fv, "vertexAttrib2fv", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::vertexAttrib3f, "vertexAttrib3f", "nnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::vertexAttrib3fv, "vertexAttrib3fv", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::vertexAttrib4f, "vertexAttrib4f", "nnnnn-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::vertexAttrib4fv, "vertexAttrib4fv", "n-")
	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::vertexAttribPointer, "vertexAttribPointer", "nnnbnn")

	DOM_FUNCTIONS_FUNCTION(DOMCanvasContextWebGL, DOMCanvasContextWebGL::viewport, "viewport", "nnnn-")
DOM_FUNCTIONS_END(DOMCanvasContextWebGL)

#endif // CANVAS3D_SUPPORT
