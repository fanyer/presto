/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_ECMASCRIPT

#include "modules/scope/src/scope_transport.h"
#include "modules/scope/src/scope_ecmascript.h"
#include "modules/scope/src/scope_utils.h"
#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esdebugger.h"
#include "modules/forms/piforms.h"

OpScopeEcmascript::EvalCallback::EvalCallback(OpScopeEcmascript *parent, ES_Runtime *rt, unsigned tag)
	: parent(parent), rt(rt), tag(tag)
{
}

/* virtual */
OpScopeEcmascript::EvalCallback::~EvalCallback()
{
}

/* virtual */ OP_STATUS
OpScopeEcmascript::EvalCallback::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	OP_ASSERT(operation == ES_ASYNC_EVAL); // Intended only for evals.
	OP_STATUS op_status = OpStatus::OK;

	if (parent && operation == ES_ASYNC_EVAL)
		op_status = parent->EvalReply(tag, rt, status, result);

	Detach();
	OP_DELETE(this);
	return op_status;
}

/* virtual */ void
OpScopeEcmascript::EvalCallback::Detach()
{
	Out();
	parent = NULL;
}

OpScopeEcmascript::ObjectHandler::ObjectHandler(ESU_ObjectManager &objman, ES_Runtime *runtime, ES_Object *es_obj, OpScopeEcmascript_SI::Object *out_object)
	: objman(objman),
	  runtime(runtime),
	  es_object(es_obj),
	  es_prototype(NULL),
	  out_object(out_object),
	  result(NULL)
{
}

OpScopeEcmascript::ObjectHandler::~ObjectHandler()
{
	for (unsigned i = 0; i < m_getslotshandlers.GetCount(); i++)
	{
		ES_GetSlotHandler* handler = m_getslotshandlers.Get(i);
		handler->OnListenerDied();
	}
}

/*virtual*/
OP_STATUS
OpScopeEcmascript::ObjectHandler::Object(BOOL is_callable, const char *class_name, const uni_char *function_name, ES_Object *prototype)
{
	OP_ASSERT(class_name || function_name);

	OP_ASSERT(out_object);
	RETURN_VALUE_IF_NULL(out_object, OpStatus::ERR_NULL_POINTER);

	unsigned object_id;
	RETURN_IF_ERROR(objman.GetId(runtime, es_object, object_id));
	out_object->SetObjectID(object_id);
	out_object->SetIsCallable(is_callable);
	out_object->SetType(function_name ? OpScopeEcmascript_SI::Object::OBJECTTYPE_FUNCTION : OpScopeEcmascript_SI::Object::OBJECTTYPE_OBJECT);
	if (function_name)
		RETURN_IF_ERROR(out_object->SetFunctionName(function_name));
	if (class_name)
		RETURN_IF_ERROR(out_object->SetClassName(class_name));

	if (prototype)
	{
		unsigned prototype_id;
		RETURN_IF_ERROR(objman.GetId(runtime, prototype, prototype_id));
		out_object->SetPrototypeID(prototype_id);
		es_prototype = prototype;
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpScopeEcmascript::ObjectHandler::ExportValue(ESU_ObjectManager &objman, ES_Runtime *runtime, const ES_Value &value, OpScopeEcmascript_SI::Value &out)
{
	switch(value.type)
	{
	case VALUE_UNDEFINED:
	case VALUE_NULL:
	case VALUE_BOOLEAN:
	case VALUE_NUMBER:
	case VALUE_STRING_WITH_LENGTH:
	case VALUE_STRING:
		return ExportPrimitiveValue(value, out);
	case VALUE_OBJECT:
		{
			OpAutoPtr<OpScopeEcmascript::Object> obj(OP_NEW(OpScopeEcmascript::Object, ()));
			RETURN_OOM_IF_NULL(obj.get());
			ObjectHandler object_handler(objman, runtime, value.value.object, obj.get());
			RETURN_IF_ERROR(ESU_ObjectExporter::ExportObject(runtime, value.value.object, &object_handler));
			out.SetObject(obj.release());
			out.SetType(OpScopeEcmascript_SI::Value::TYPE_OBJECT);
		}
		break;
	default:
		OP_ASSERT(!"Please add a case for the new type");
	}

	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpScopeEcmascript::ObjectHandler::ExportPrimitiveValue(const ES_Value &value, OpScopeEcmascript_SI::Value &out)
{
	switch(value.type)
	{
	case VALUE_UNDEFINED:
		out.SetType(OpScopeEcmascript_SI::Value::TYPE_UNDEFINED);
		break;
	case VALUE_NULL:
		out.SetType(OpScopeEcmascript_SI::Value::TYPE_NULL);
		break;
	case VALUE_BOOLEAN:
		out.SetType(value.value.boolean ? OpScopeEcmascript_SI::Value::TYPE_TRUE : OpScopeEcmascript_SI::Value::TYPE_FALSE);
		break;
	case VALUE_NUMBER:
		// NaN and +-Infinite needs to be set using enums as JSON does not support them
		if (op_isnan(value.value.number))
			out.SetType(OpScopeEcmascript_SI::Value::TYPE_NAN);
		else if (op_isinf(value.value.number))
			out.SetType(op_signbit(value.value.number) ? OpScopeEcmascript_SI::Value::TYPE_MINUS_INFINITY : OpScopeEcmascript_SI::Value::TYPE_PLUS_INFINITY);
		else
		{
			out.SetType(OpScopeEcmascript_SI::Value::TYPE_NUMBER);
			out.SetNumber(value.value.number);
		}
		break;
	case VALUE_STRING_WITH_LENGTH:
		out.SetType(OpScopeEcmascript_SI::Value::TYPE_STRING);
		RETURN_IF_ERROR(out.SetStr(value.value.string_with_length->string,
			value.value.string_with_length->length));
		break;
	case VALUE_STRING:
		out.SetType(OpScopeEcmascript_SI::Value::TYPE_STRING);
		RETURN_IF_ERROR(out.SetStr(value.value.string));
		break;
	case VALUE_OBJECT:
		OP_ASSERT(!"Cannot export objects with SetPrimitiveValue");
		return OpStatus::ERR;
	default:
		OP_ASSERT(!"Please add a case for the new type");
	}

	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeEcmascript::ObjectHandler::PropertyCount(unsigned count)
{
	remaining_properties = count;
	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeEcmascript::ObjectHandler::Property(const uni_char *name, const ES_Value &value)
{
	remaining_properties--;
	OP_ASSERT(out_object);
	RETURN_VALUE_IF_NULL(out_object, OpStatus::ERR_NULL_POINTER);

	OpScopeEcmascript_SI::Object::Property *property = out_object->AppendNewPropertyList();
	RETURN_OOM_IF_NULL(property);

	RETURN_IF_ERROR(property->SetName(name));

	OP_STATUS status = ObjectHandler::ExportValue(objman, runtime, value, property->GetValueRef());
	
	if (!remaining_properties)
		result->OnObjectFinished(this);
	
	return status;
}

/*virtual*/
void OpScopeEcmascript::ObjectHandler::OnError()
{
	result->SetHasError();
	if (!--remaining_properties)
		result->OnObjectFinished(this);
}

void OpScopeEcmascript::ObjectHandler::OnPropertyExcluded()
{
	if (!--remaining_properties)
		result->OnObjectFinished(this);
}

void
OpScopeEcmascript::ObjectHandler::SetExamineResult(ExamineResult *result)
{
	this->result = result;
}

BOOL
OpScopeEcmascript::ObjectHandler::IsFinished()
{
	return remaining_properties == 0;
}

OP_STATUS
OpScopeEcmascript::ObjectHandler::OnPropertyValue(ES_DebugObjectProperties *properties, const uni_char *propertyname, const ES_Value &result, unsigned propertyindex, BOOL exclude, GetNativeStatus getstatus)
{
	if (exclude)
	{
		OnPropertyExcluded();
		return OpStatus::OK;
	}
	return Property(propertyname, result);
}

/* OpScopeEcmascript */

OpScopeEcmascript::OpScopeEcmascript()
	: OpScopeEcmascript_SI()
{
}

/* virtual */
OpScopeEcmascript::~OpScopeEcmascript()
{
	// There might be ongoing evals which will try to report
	// to 'this' when complete. The callbacks must be notified
	// that we're about to be deleted.
	EvalCallback *c = callbacks.First();

	while(c)
	{
		c->Detach();
		c = c->Suc();
	}
}

// OpScopeService

/* virtual */
OP_STATUS
OpScopeEcmascript::OnServiceDisabled()
{
	objman.Reset();
	rtman.Reset();

	return OpStatus::OK;
}

void
OpScopeEcmascript::ReadyStateChanged(FramesDocument *doc, OpScopeReadyStateListener::ReadyState state)
{
	if (!IsEnabled() || !doc || !doc->GetESRuntime())
		return;

	unsigned rtid;
	RAISE_IF_MEMORY_ERROR(rtman.GetId(doc->GetESRuntime(), rtid));

	if (rtid == 0)
		return;

	ReadyStateChange::State ready_state;
	RETURN_VOID_IF_ERROR(Convert(state, ready_state));

	ReadyStateChange msg;
	msg.SetRuntimeID(rtid);
	msg.SetState(ready_state);

	RAISE_IF_MEMORY_ERROR(SendOnReadyStateChanged(msg));
}

OP_STATUS
OpScopeEcmascript::EvalReply(unsigned tag, ES_Runtime *rt, ES_AsyncStatus status, const ES_Value &result)
{
	EvalResult msg;

	msg.SetStatus(Convert(status));
	ObjectHandler::ExportValue(objman, rt, result, msg.GetValueRef());
	
	return SendEval(msg, tag);
}

OP_STATUS
OpScopeEcmascript::OnExamineResult(ExamineResult *result)
{
	OP_STATUS ret = OpStatus::OK;
	if (result->HasError())
	{
		OpScopeTPError error;
		error.SetStatus(OpScopeTPMessage::BadRequest);
		RETURN_IF_ERROR(error.SetDescription(UNI_L("ExamineObjects failed")));
		ret = SendAsyncError(result->GetAsyncTag(), error);
	}
	else
		ret = SendExamineObjects(*(result->GetObjects()), result->GetAsyncTag());

	OP_DELETE(result);
	return ret;
}

OpScopeEcmascript::EvalResult::Status
OpScopeEcmascript::Convert(ES_AsyncStatus status) const
{
	switch(status)
	{
	case ES_ASYNC_SUCCESS: return EvalResult::STATUS_SUCCESS;
	case ES_ASYNC_FAILURE: return EvalResult::STATUS_FAILURE;
	case ES_ASYNC_EXCEPTION: return EvalResult::STATUS_EXCEPTION;
	case ES_ASYNC_NO_MEMORY: return EvalResult::STATUS_OOM;
	case ES_ASYNC_CANCELLED: return EvalResult::STATUS_CANCELLED;
	default:
		OP_ASSERT(!"Please add a case for the new status");
		return EvalResult::STATUS_FAILURE;
	}
}

OP_STATUS
OpScopeEcmascript::Convert(OpScopeReadyStateListener::ReadyState state_in, ReadyStateChange::State &state_out) const
{
	switch(state_in)
	{
	case OpScopeReadyStateListener::READY_STATE_DOM_ENVIRONMENT_CREATED:
		state_out = ReadyStateChange::STATE_DOM_ENVIRONMENT_CREATED;
		break;
	case OpScopeReadyStateListener::READY_STATE_BEFORE_DOM_CONTENT_LOADED:
		state_out = ReadyStateChange::STATE_DOM_CONTENT_LOADED;
		break;
	case OpScopeReadyStateListener::READY_STATE_AFTER_ONLOAD:
		state_out = ReadyStateChange::STATE_AFTER_ONLOAD;
		break;
	case OpScopeReadyStateListener::READY_STATE_AFTER_DOM_CONTENT_LOADED:
	case OpScopeReadyStateListener::READY_STATE_BEFORE_ONLOAD:
		return OpStatus::ERR_NOT_SUPPORTED;
	default:
		OP_ASSERT(!"Explicitly ignore unknown state or convert to a proper state!");
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	return OpStatus::OK;
}

// Request/Response functions
OP_STATUS
OpScopeEcmascript::DoListRuntimes(const ListRuntimesArg &in, RuntimeList &out)
{
	OpINT32Set rtset;
	for (unsigned i = 0; i < in.GetRuntimeIDList().GetCount(); ++i)
		RETURN_IF_ERROR(rtset.Add(in.GetRuntimeIDList().Get(i)));

	OpVector<ES_Runtime> rts;
	RETURN_IF_ERROR(ESU_RuntimeManager::FindAllRuntimes(rts, in.GetCreate()));

	for (unsigned i = 0; i < rts.GetCount(); ++i)
	{
		ES_Runtime *rt = rts.Get(i);

		if (!rt)
		{
			OP_ASSERT(!"DoListRuntimes: ES_Runtime == NULL");
			continue; // Attempt to list other runtimes.
		}

		unsigned rtid;
		RETURN_IF_ERROR(rtman.GetId(rt, rtid));

		if (in.GetRuntimeIDList().GetCount() == 0 || rtset.Contains(rtid))
		{
			Runtime *runtime = out.GetRuntimeListRef().AddNew();
			RETURN_OOM_IF_NULL(runtime);
			runtime->SetRuntimeID(rtid);
			runtime->SetWindowID(ESU_RuntimeManager::GetWindowId(rt));

			unsigned id;
			RETURN_IF_ERROR(objman.GetId(rt, rt->GetGlobalObjectAsPlainObject(), id));
			runtime->SetObjectID(id);

			OpString htmlFramePath;
			OpString uri;
			RETURN_IF_ERROR(ESU_RuntimeManager::GetDocumentUri(rt, uri));
			RETURN_IF_ERROR(ESU_RuntimeManager::GetFramePath(rt, htmlFramePath));

			RETURN_IF_ERROR(runtime->SetHtmlFramePath(htmlFramePath.CStr()));
			RETURN_IF_ERROR(runtime->SetUri(uri.CStr()));
		}
	}
	
	return OpStatus::OK;
}

OP_STATUS
OpScopeEcmascript::DoExamineObjects(const ExamineObjectsArg &in, unsigned int async_tag)
{
	ES_Runtime *rt = rtman.GetRuntime(in.GetRuntimeID());
	if (!rt)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Specified runtime does not exist"));

	OpAutoPtr<ObjectList> objects(OP_NEW(ObjectList, ()));
	RETURN_OOM_IF_NULL(objects.get());

	UINT32 object_count = in.GetObjectIDList().GetCount();

	if (!object_count)
	{
		SendExamineObjects(*objects.get(), async_tag);
		return OpStatus::OK;
	}

	OP_STATUS status = OpStatus::OK;

	ExamineResult* examres = OP_NEW(ExamineResult, (async_tag, this, objects.get()));
	RETURN_OOM_IF_NULL(examres);
	objects.release();
	examres->Into(&examine_results);

	for (unsigned i = 0; i < object_count; ++i)
	{
		ES_Object *es_obj = objman.GetObject(in.GetObjectIDList().Get(i));
		if (!es_obj)
		{
			status = SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Specified object ID does not exist"));
			break;
		}

		PrototypeChain *chain = examres->GetObjects()->AppendNewPrototypeList();
		if (!chain)
		{
			status = OpStatus::ERR_NO_MEMORY;
			break;
		}
		while (es_obj)
		{
			Object *object = chain->AppendNewObjectList();
			if (!object)
			{
				status = OpStatus::ERR_NO_MEMORY;
				break;
			}

			ObjectHandler object_handler(objman, rt, es_obj, object);
			status = ESU_ObjectExporter::ExportObject(rt, es_obj, &object_handler);
			if (OpStatus::IsError(status))
				break;

			ObjectHandler *prop_handler = OP_NEW(ObjectHandler, (objman, rt, es_obj, object));
			if (!prop_handler)
			{
				status = OpStatus::ERR_NO_MEMORY;
				break;
			}

			prop_handler->SetExamineResult(examres);

			examres->AddPropertyHandler(prop_handler);

			es_obj = in.GetExaminePrototypes() ? object_handler.GetPrototype() : NULL;
		}

		if (OpStatus::IsError(status))
		{
			examres->SetHasError();
			break;
		}
	}

	if (OpStatus::IsSuccess(status))
		status = examres->ExportProperties(rt);

	if (OpStatus::IsError(status))
	{
		examres->Cancel();
		OP_DELETE(examres);
	}

	return status;
}

OP_STATUS
OpScopeEcmascript::DoReleaseObjects(const ReleaseObjectsArg &in)
{
	if (in.GetObjectIDList().GetCount() != 0)
	{
		for (unsigned i = 0; i < in.GetObjectIDList().GetCount(); ++i)
			objman.Release(in.GetObjectIDList().Get(i));
	}
	else
		objman.ReleaseAll();

	return OpStatus::OK;
}

OP_STATUS
OpScopeEcmascript::DoSetFormElementValue(const SetFormElementValueArg &in)
{
	ES_Object *es_obj = objman.GetObject(in.GetObjectID());
	if (!es_obj)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Specified object ID does not exist"));

	HTML_Element *he = NULL;
	RETURN_IF_ERROR(GetHTMLElement(&he, es_obj));

	FormObject *form_object = he->GetFormObject();
	RETURN_VALUE_IF_NULL(form_object, OpStatus::ERR_NO_SUCH_RESOURCE);

	return form_object->SetFormWidgetValue(in.GetValue());
}

OP_STATUS
OpScopeEcmascript::DoEval(const EvalArg &in, unsigned int tag)
{
	ES_Runtime *rt = rtman.GetRuntime(in.GetRuntimeID());
	if (!rt)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Specified runtime does not exist"));

	ES_AsyncInterface* asyncif = rt->GetESAsyncInterface();
	RETURN_VALUE_IF_NULL(asyncif, OpStatus::ERR_NO_SUCH_RESOURCE);

	OpAutoPtr<EcmaScript_Object> scope(NULL);
	unsigned var_count = in.GetVariableList().GetCount();

	if (var_count > 0)
	{
		scope.reset(OP_NEW(EcmaScript_Object, ()));
		RETURN_OOM_IF_NULL(scope.get());
		RETURN_IF_ERROR(scope->SetObjectRuntime(rt, rt->GetObjectPrototype(), "Object"));

		for (unsigned i = 0; i < var_count; ++i)
		{
			EvalArg::Variable *var = in.GetVariableList().Get(i);
			ES_Object *obj = objman.GetObject(var->GetObjectID());
			if (!obj)
				return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Specified object ID does not exist"));

			RETURN_IF_ERROR(scope->Put(var->GetName().CStr(), obj, TRUE));
		}
	}

	ES_Object *scope_chain = scope.get() ? scope->GetNativeObject() : NULL;
	OpAutoPtr<EvalCallback> eval_cb(OP_NEW(EvalCallback, (this, rt, tag)));
	RETURN_OOM_IF_NULL(eval_cb.get());
	RETURN_IF_ERROR(asyncif->Eval(in.GetScriptData().CStr(), scope_chain ? &scope_chain : NULL, scope_chain ? 1 : 0, eval_cb.get()));
	eval_cb.release()->Into(&callbacks);

	return OpStatus::OK;
}

ExamineResult::ExamineResult(unsigned tag, ExamineResultCallback* finishcallback, OpScopeEcmascript::ObjectList *objs)
	: objects(objs),
	  async_tag(tag),
	  callback(finishcallback),
	  has_error(FALSE)
{
}

void ExamineResult::AddPropertyHandler(OpScopeEcmascript::ObjectHandler* handler)
{
	handler->Into(&handlers);
}

ExamineResult::~ExamineResult()
{
	OP_DELETE(objects);
	Out();
}

OP_STATUS 
ExamineResult::OnObjectFinished(OpScopeEcmascript::ObjectHandler *handler)
{
	OP_ASSERT(handlers.HasLink(handler));
	handler->Out();
	OP_DELETE(handler);
	OP_STATUS ret = OpStatus::OK;
	if (handlers.Empty())
		ret = callback->OnExamineResult(this);
	return ret;
}

BOOL
ExamineResult::IsFinished() const
{
	return handlers.Empty();
}

OP_STATUS
ExamineResult::ExportProperties(ES_Runtime *rt)
{
	OP_STATUS status = OpStatus::OK;
	OpScopeEcmascript::ObjectHandler *handler = handlers.First();
	while (handler)
	{
		OpScopeEcmascript::ObjectHandler *nexthandler = handler->Suc();
		status = ESU_ObjectExporter::ExportProperties(rt, handler->GetObject(), handler);
		if (OpStatus::IsError(status))
			break;
		handler = nexthandler;
	}
	return status;
}

void
ExamineResult::Cancel()
{
	OpScopeEcmascript::ObjectHandler *handler = handlers.First();
	while (handler)
	{
		OpScopeEcmascript::ObjectHandler *nexthandler = handler->Suc();
		handler->SetExamineResult(NULL);
		OP_DELETE(handler);
		handler = nexthandler;
	}
}

#endif // SCOPE_ECMASCRIPT
