/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/pluginscript.h"
#include "modules/ns4plugins/src/plugin.h"
#include "modules/ns4plugins/opns4pluginhandler.h"
#include "modules/ns4plugins/src/pluginmemoryhandler.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/essyncif.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/domutils.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/util/str.h"
#include "modules/pi/OpThreadTools.h"

#include "modules/hardcore/cpuusagetracker/cpuusagetrackeractivator.h"

#ifdef MEMTOOLS_ENABLE_STACKGUARD
#include "modules/memtools/stacktraceguard.h"
#endif

#include "modules/layout/layout_workplace.h"
#include "modules/ns4plugins/src/pluginexceptions.h"

static ES_ThreadScheduler *
GetSchedulerFromRuntime(ES_Runtime *runtime)
{
	return runtime->GetESScheduler();
}

static ES_AsyncInterface *
GetAsyncIFFromRuntime(ES_Runtime *runtime)
{
	ES_AsyncInterface *asyncif = runtime->GetESAsyncInterface();
	asyncif->SetIsPluginRequested();

	return asyncif;
}

OpNPIdentifier::OpNPIdentifier()
	: is_string(TRUE),
	  name_int(-1)
{
}

void
OpNPIdentifier::CheckInteger()
{
	int length = name.Length();

	if (length > 10)
		return;

	for (int index = 0; index < length; ++index)
		if (name.CStr()[index] < '0' || name.CStr()[index] > '9')
			return;

	if (length == 10 && uni_strcmp(UNI_L("2147483647"), name.CStr()) < 0)
		return;

	is_string = FALSE;
	name_int = uni_atoi(name.CStr());
}

OpNPIdentifier::~OpNPIdentifier()
{
}

/* static */ OpNPIdentifier *
OpNPIdentifier::Make(const char *name)
{
	OpNPIdentifier *identifier = OP_NEW(OpNPIdentifier, ());
	if (!identifier)
		return NULL;

	if (OpStatus::IsError(identifier->name.SetFromUTF8(name)) ||
	    OpStatus::IsError(identifier->name_utf8.Set(name)))
	{
		OP_DELETE(identifier);
		return NULL;
	}

	identifier->CheckInteger();

	return identifier;
}

/* static */ OpNPIdentifier *
OpNPIdentifier::Make(const uni_char *name)
{
	OpNPIdentifier *identifier = OP_NEW(OpNPIdentifier, ());
	if (!identifier)
		return NULL;

	if (OpStatus::IsError(identifier->name.Set(name)) ||
	    OpStatus::IsError(identifier->name_utf8.SetUTF8FromUTF16(name)))
	{
		OP_DELETE(identifier);
		return NULL;
	}

	identifier->CheckInteger();

	return identifier;
}

/* static */ OpNPIdentifier *
OpNPIdentifier::Make(int name)
{
	char buffer[16]; /* ARRAY OK 2009-04-21 hela */
	op_snprintf(buffer, 16, "%d", name);

	OpNPIdentifier *identifier = Make(buffer);
	if (!identifier)
		return NULL;

	identifier->is_string = FALSE;
	identifier->name_int = name;

	return identifier;
}

BOOL
OpNPObject::Protect(ES_Runtime *runtime, ES_Object *object)
{
    return OpStatus::IsSuccess(reference.Protect(runtime, object));
}

void
OpNPObject::Unprotect()
{
    reference.Unprotect();
}

OpNPObject::OpNPObject(Plugin* plugin, ES_Object* internal, NPObject* external)
	: internal(internal),
	  external(external),
	  is_pure_internal(!!internal),
	  has_exception(FALSE),
	  plugin(plugin)
{
	OP_ASSERT(plugin || !"An OpNPObject must be owned by a plug-in instance.");
	OP_ASSERT(external);

	plugin->AddObject(this);
}

OpNPObject::~OpNPObject()
{
	Out(); /* Disentangle from plugin->m_objects. */

	if (internal)
		plugin->RemoveBinding(internal);

	plugin->InvalidateObject(this);
	g_pluginscriptdata->RemoveObject(this);
	g_pluginscriptdata->ReleaseObjectFromRestartObjects(this);

	if (is_pure_internal)
	{
		/* Remove our layer of protection from the internal ES object. */
		Unprotect();
	}
	else
	{
		/* Mark the internal ES object as deleted. */
		if (internal)
		{
			EcmaScript_Object* es_object = ES_Runtime::GetHostObject(internal);
			OP_ASSERT(es_object && es_object->IsA(NS4_TYPE_EXTERNAL_OBJECT));

			OpNPExternalObject* external_object = static_cast<OpNPExternalObject *>(es_object);
			external_object->MarkNPObjectAsDeleted();
		}

		BOOL deallocate_object = TRUE;
		if (!plugin->IsFailure() && external && external->_class)
		{
			/* Inform plug-in library of object invalidation if object is still referenced. */
			if (external->referenceCount > 0 && external->_class->invalidate)
			{
				TRY
					external->_class->invalidate(external);
				CATCH_PLUGIN_EXCEPTION(plugin->SetFailure(FALSE););
			}

			/* Request that plug-in library deallocate the memory.
			   Plugin and/or class can not be assumed to be valid after invalidate(). */
			if (!plugin->IsFailure() && external->_class && external->_class->deallocate)
			{
				deallocate_object = FALSE;

				TRY
					external->_class->deallocate(external);
				CATCH_PLUGIN_EXCEPTION(plugin->SetFailure(FALSE););
			}
		}

		if (deallocate_object)
			PluginMemoryHandler::GetHandler()->Free(external);
	}
}

/* static */ OpNPObject *
OpNPObject::Make(Plugin* plugin, ES_Runtime *runtime, ES_Object* internal)
{
	NPObject* external = (NPObject *) PluginMemoryHandler::GetHandler()->Malloc(sizeof(NPObject));
	if (!external)
		return NULL;

	OpNPObject* object = OP_NEW(OpNPObject, (plugin, internal, external));
	if (!object)
	{
		PluginMemoryHandler::GetHandler()->Free(external);
		return NULL;
	}

	if (!object->Protect(runtime, internal) ||
		OpStatus::IsError(plugin->AddBinding(internal, object)) ||
		OpStatus::IsError(g_pluginscriptdata->AddObject(object)))
	{
		OP_DELETE(object);
		return NULL;
	}

	object->external->_class = g_plugin_internal_object_class;
	object->external->referenceCount = 1;

	return object;
}

static OP_STATUS
OpNPSetRuntime(ES_Runtime *runtime, OpNPExternalObject *internal, NPObject *external)
{
	OP_STATUS status;

	if (NP_CLASS_STRUCT_VERSION_HAS_CTOR(external->_class) && external->_class->construct ||
		external->_class->invokeDefault)
		status = internal->SetFunctionRuntime(runtime, runtime->GetObjectPrototype(), "PluginObject", NULL);
	else
		status = internal->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "PluginObject");

	return status;
}

/* static */ OpNPObject *
OpNPObject::Make(Plugin* plugin, NPObject *external)
{
	if (OpNPObject* object = OP_NEW(OpNPObject, (plugin, NULL, external)))
	{
		if (OpStatus::IsSuccess(g_pluginscriptdata->AddObject(object)))
			return object;

		OP_DELETE(object);
	}

	return NULL;
}

void
OpNPObject::Retain()
{
	external->referenceCount++;
}

void
OpNPObject::Release(BOOL soft /* = FALSE */)
{
	/* We perform a 'soft release' of values returned from functions implemented by plug-ins. This
	 * is because we know we have just imported the value into the ES engine, so if this object had
	 * only one reference outside the engine before, the reference count should remain one and wait
	 * for the ES GC to reduce it to zero.
	 *
	 * This is however only valid for external objects. Internal objects are always referenced by
	 * ES, and remain protected from the GC while the external reference count is positive. */
	if (!is_pure_internal && soft && external->referenceCount == 1)
		return;

	if (--external->referenceCount == 0)
		OP_DELETE(this);
}

void
OpNPObject::UnsetInternal()
{
	OP_ASSERT(!is_pure_internal);
	if (internal)
	{
		plugin->RemoveBinding(internal);
		internal = NULL;
	}
}

ES_Object*
OpNPObject::Import(ES_Runtime* runtime)
{
	if (!internal)
		if (OpNPExternalObject* new_internal = OP_NEW(OpNPExternalObject, (this)))
		{
			if (OpStatus::IsSuccess(OpNPSetRuntime(runtime, new_internal, external)) &&
				OpStatus::IsSuccess(plugin->AddBinding(*new_internal, this)))
			{
				internal = *new_internal;
				Retain();
			}
			else
				OP_DELETE(new_internal);
		}

	return internal;
}

OP_BOOLEAN
OpNPObject::HasMethod(NPIdentifier method_name)
{
	if (external && external->_class && external->_class->hasMethod && !IsPureInternal())
	{
		bool ret = false;

		if (!GetPlugin()->IsFailure())
		{
			TRY
				ret = external->_class->hasMethod(external, method_name);
			CATCH_PLUGIN_EXCEPTION(ret = false; GetPlugin()->SetFailure(FALSE););
		}

		return ret ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	}

	return OpStatus::ERR;
}

OP_BOOLEAN
OpNPObject::Invoke(NPIdentifier method_name, const NPVariant* args, uint32_t arg_count, NPVariant* result)
{
	if (external && external->_class && external->_class->invoke && !IsPureInternal())
	{
		bool ret = false;

		if (!GetPlugin()->IsFailure())
		{
			TRY
				ret = external->_class->invoke(external, method_name, args, arg_count, result);
			CATCH_PLUGIN_EXCEPTION(ret = false; GetPlugin()->SetFailure(FALSE););
		}

		return ret ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	}

	return OpStatus::ERR;
}

OP_BOOLEAN
OpNPObject::InvokeDefault(const NPVariant* args, uint32_t arg_count, NPVariant* result)
{
	if (external && external->_class && external->_class->invokeDefault && !IsPureInternal())
	{
		bool ret = false;

		if (!GetPlugin()->IsFailure())
		{
			TRY
				ret = external->_class->invokeDefault(external, args, arg_count, result);
			CATCH_PLUGIN_EXCEPTION(ret = false; GetPlugin()->SetFailure(FALSE););
		}

		return ret ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	}

	return OpStatus::ERR;
}

OP_BOOLEAN
OpNPObject::HasProperty(NPIdentifier property_name)
{
	if (external && external->_class && external->_class->hasProperty && !IsPureInternal())
	{
		bool ret = false;

		if (!GetPlugin()->IsFailure())
		{
			TRY
				ret = external->_class->hasProperty(external, property_name);
			CATCH_PLUGIN_EXCEPTION(ret = false; GetPlugin()->SetFailure(FALSE););
		}

		return ret ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	}

	return OpStatus::ERR;
}

OP_BOOLEAN
OpNPObject::GetProperty(NPIdentifier property_name, NPVariant* result)
{
	if (external && external->_class && external->_class->getProperty && !IsPureInternal())
	{
		bool ret = false;

		if (!GetPlugin()->IsFailure())
		{
			TRY
				ret = external->_class->getProperty(external, property_name, result);
			CATCH_PLUGIN_EXCEPTION(ret = false; GetPlugin()->SetFailure(FALSE););
		}

		return ret ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	}

	return OpStatus::ERR;
}

OP_BOOLEAN
OpNPObject::SetProperty(NPIdentifier property_name, const NPVariant* value)
{
	if (external && external->_class && external->_class->setProperty && !IsPureInternal())
	{
		bool ret = false;

		if (!GetPlugin()->IsFailure())
		{
			TRY
				ret = external->_class->setProperty(external, property_name, value);
			CATCH_PLUGIN_EXCEPTION(ret = false; GetPlugin()->SetFailure(FALSE););
		}

		return ret ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	}

	return OpStatus::ERR;
}

OP_BOOLEAN
OpNPObject::RemoveProperty(NPIdentifier property_name)
{
	if (external && external->_class && external->_class->removeProperty && !IsPureInternal())
	{
		bool ret = false;

		if (!GetPlugin()->IsFailure())
		{
			TRY
				ret = external->_class->removeProperty(external, property_name);
			CATCH_PLUGIN_EXCEPTION(ret = false; GetPlugin()->SetFailure(FALSE););
		}

		return ret ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	}

	return OpStatus::ERR;
}

OP_BOOLEAN
OpNPObject::Construct(const NPVariant* args, uint32_t arg_count, NPVariant* result)
{
	if (external && external->_class && NP_CLASS_STRUCT_VERSION_HAS_CTOR(external->_class)
		&& external->_class->construct && !IsPureInternal())
	{
		bool ret = false;

		if (!GetPlugin()->IsFailure())
		{
			TRY
				ret = external->_class->construct(external, args, arg_count, result);
			CATCH_PLUGIN_EXCEPTION(ret = false; GetPlugin()->SetFailure(FALSE););
		}

		return ret ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	}

	return OpStatus::ERR;
}

static uni_char *
PluginConvertFromNPString(const NPString &npstring)
{
	UTF8toUTF16Converter converter;

	uni_char *dest = OP_NEWA(uni_char, npstring.UTF8Length + 1);
	if (!dest)
		return NULL;

	int read, written = converter.Convert(npstring.UTF8Characters, npstring.UTF8Length, dest, UNICODE_SIZE(npstring.UTF8Length), &read);
	dest[written / 2] = 0;

	return dest;
}

static OP_BOOLEAN
PluginImportValue(Plugin* plugin, ES_Runtime* runtime, ES_Value &internal, const NPVariant &external)
{
	switch (external.type)
	{
	case NPVariantType_Void:
		internal.type = VALUE_UNDEFINED;
		break;

	case NPVariantType_Null:
		internal.type = VALUE_NULL;
		break;

	case NPVariantType_Bool:
		internal.value.boolean = external.value.boolValue;
		internal.type = VALUE_BOOLEAN;
		break;

	case NPVariantType_Int32:
		internal.value.number = external.value.intValue;
		internal.type = VALUE_NUMBER;
		break;

	case NPVariantType_Double:
		internal.value.number = external.value.doubleValue;
		internal.type = VALUE_NUMBER;
		break;

	case NPVariantType_String:
		internal.value.string = PluginConvertFromNPString(external.value.stringValue);
		if (!internal.value.string)
			return OpStatus::ERR_NO_MEMORY;
		internal.type = VALUE_STRING;
		break;

	case NPVariantType_Object:
		if (!external.value.objectValue)
			internal.type = VALUE_NULL;
		else if (OpNPObject *object = g_pluginscriptdata->FindObject(external.value.objectValue))
		{
			internal.value.object = object->Import(runtime);

			if (internal.value.object)
				internal.type = VALUE_OBJECT;
			else
				return OpBoolean::IS_FALSE;
		}
		else
			return OpBoolean::IS_FALSE;
		break;

	default:
		return OpBoolean::IS_FALSE;
	}

	return OpBoolean::IS_TRUE;
}

static void
PluginReleaseInternalValue(ES_Value &value)
{
	if (value.type == VALUE_STRING)
		OP_DELETEA((uni_char *) value.value.string);
	value.type = VALUE_UNDEFINED;
}

static BOOL
PluginConvertToNPString(NPString &npstring, const uni_char *string)
{
	// First check if the input parameter string is a url and if so, use the 8 bit (Name) version.
	// Workaround fix for javascript/dom returning unicode representation for e.g. "window.location".
	URL check_domain = g_url_api->GetURL(string);
	if (!check_domain.IsEmpty())
	{
		OpString8 un8;
		OpString s;
		if (OpStatus::IsSuccess(check_domain.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, s, FALSE)) &&
			OpStatus::IsSuccess(check_domain.GetAttribute(URL::KName_Username_Password_Hidden_Escaped, un8, FALSE)) &&
			s.Compare(un8.CStr(), un8.Length()))
			{ // differs because url name contains (valid) unicode characters. Use the 8 bit version of the url name.
				npstring.UTF8Length = un8.Length();
				npstring.UTF8Characters = static_cast<NPUTF8 *>(PluginMemoryHandler::GetHandler()->Malloc(sizeof(NPUTF8) * npstring.UTF8Length));
				if (!npstring.UTF8Characters)
					return FALSE;
				op_strncpy(static_cast<char *>((NPUTF8*)npstring.UTF8Characters), un8.CStr(), npstring.UTF8Length);
				return TRUE;
			}
	}

	UTF16toUTF8Converter converter;

	unsigned length = uni_strlen(string);

	npstring.UTF8Length = converter.BytesNeeded(string, UNICODE_SIZE(length));
	npstring.UTF8Characters = (NPUTF8 *) PluginMemoryHandler::GetHandler()->Malloc(sizeof(NPUTF8) * npstring.UTF8Length);
	if (!npstring.UTF8Characters)
		return FALSE;

	int read; converter.Convert(string, UNICODE_SIZE(length), (NPUTF8 *) npstring.UTF8Characters, npstring.UTF8Length, &read);

	return TRUE;
}

static BOOL
PluginExportValue(Plugin* plugin, NPVariant &external, const ES_Value &internal, ES_Runtime *runtime)
{
	OpNPObject* object = 0;
	external.type = NPVariantType_Void;

	switch (internal.type)
	{
	case VALUE_UNDEFINED:
		break;

	case VALUE_NULL:
		external.type = NPVariantType_Null;
		break;

	case VALUE_BOOLEAN:
		external.value.boolValue = internal.value.boolean == TRUE;
		external.type = NPVariantType_Bool;
		break;

	case VALUE_NUMBER:
		if (op_isintegral(internal.value.number) && internal.value.number > INT_MIN && internal.value.number <= INT_MAX)
		{
			external.value.intValue = op_double2int32(internal.value.number);
			external.type = NPVariantType_Int32;
		}
		else
		{
			external.value.doubleValue = internal.value.number;
			external.type = NPVariantType_Double;
		}
		break;

	case VALUE_STRING:
		if (!PluginConvertToNPString(external.value.stringValue, internal.value.string))
			return FALSE;
		external.type = NPVariantType_String;
		break;

	case VALUE_OBJECT:
		if ((object = plugin->FindObject(internal.value.object)))
			object->Retain();
		else
			object = OpNPObject::Make(plugin, runtime, internal.value.object);

		if (object)
		{
			external.value.objectValue = object->GetExternal();
			external.type = NPVariantType_Object;
		}
		else
			return FALSE;
		break;
	}

	return TRUE;
}

void PluginReleaseExternalValue(NPVariant &value, BOOL soft)
{
	if (value.type == NPVariantType_String)
		PluginMemoryHandler::GetHandler()->Free((NPUTF8 *) value.value.stringValue.UTF8Characters);
	else if (value.type == NPVariantType_Object && value.value.objectValue)
	{
		if (OpNPObject* object = g_pluginscriptdata->FindObject(value.value.objectValue))
			object->Release(soft);
	}

	value.type = NPVariantType_Void;
}

static OP_STATUS
HandleNPObjectException(ES_Value *return_value, ES_Runtime *origining_runtime, OpNPObject *npobject)
{
	OP_STATUS stat = OpStatus::OK;
	EcmaScript_Object *exception = OP_NEW(EcmaScript_Object, ());
	if (!exception)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsSuccess(stat = exception->SetObjectRuntime(origining_runtime, origining_runtime->GetErrorPrototype(), "NPException")))
	{
		ES_Value property_value;
		const uni_char *message = npobject->GetException().CStr();
		if (!message)
		{
			Plugin* plugin = npobject->GetPlugin();
			if (plugin && plugin->IsScriptException())
			{
				message = plugin->GetExceptionMessage();
				plugin->SetScriptException(FALSE);
			}
		}
		if (message)
		{
			property_value.type = VALUE_STRING;
			property_value.value.string = message;
			stat = exception->Put(UNI_L("message"), property_value);
		}
		if (OpStatus::IsSuccess(stat))
		{
			property_value.type = VALUE_NUMBER;
			property_value.value.number = 9; // "DOM_Object::NOT_SUPPORTED_ERR"
			stat = exception->Put(UNI_L("code"), property_value);
			if (OpStatus::IsSuccess(stat))
			{
				return_value->type = VALUE_OBJECT;
				return_value->value.object = *exception;
			}
		}
	}
	if (OpStatus::IsError(stat))
		OP_DELETE(exception);
	return stat;
}

/**
 * ConvertLocalhost checks if the string result is a local file url and removes the "localhost" part of it.
 * Workaround required by the Java plugin.
 *
 * @param[in,out] result    The resulting local file url name, untouched if not a string result or not a local file url.
 *
 * @returns ERR if local file url name was not available or the conversion to NPString went wrong.
 *			OK if not a string result, not a local file url, or the local file url was successfully converted.
 */

OP_STATUS ConvertLocalhost(NPVariant* result)
{
	if (result->type == NPVariantType_String && result->value.stringValue.UTF8Length > 0)
	{
		OpString converted_url;
		RETURN_IF_ERROR(converted_url.SetFromUTF8(result->value.stringValue.UTF8Characters, result->value.stringValue.UTF8Length));
		URL url = g_url_api->GetURL(converted_url);
		if (url.Type() == URL_FILE && url.GetAttribute(URL::KHostName).Compare("localhost") == 0)
		{
			RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, converted_url));
			converted_url.Delete(STRINGLENGTH("file://"), STRINGLENGTH("localhost"));
			PluginMemoryHandler::GetHandler()->Free((NPUTF8 *) result->value.stringValue.UTF8Characters);
			if (!PluginConvertToNPString(result->value.stringValue, converted_url.CStr()))
				return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

PluginRestartObject::PluginRestartObject()
	: type(PLUGIN_UNINITIALIZED),
	  thread(NULL),
	  mh(NULL),
	  object(NULL),
	  identifier(NULL),
	  argv(NULL),
	  argc(0),
	  success(FALSE),
	  oom(FALSE),
	  called(FALSE),
	  next_restart_object(NULL)
{
	if (g_pluginscriptdata)
		g_pluginscriptdata->AddPluginRestartObject(this);
}

PluginRestartObject::~PluginRestartObject()
{
	if (g_pluginscriptdata)
		g_pluginscriptdata->RemovePluginRestartObject(this);

	if (type != PLUGIN_UNINITIALIZED)
		if (type != PLUGIN_RESTART && type != PLUGIN_RESTART_AFTER_INIT)
		{
			PluginReleaseInternalValue(value);
			if (!called)
				mh->UnsetCallBacks(this);
			OP_DELETEA(argv);
		}
		else if (g_pluginhandler)
			g_pluginhandler->DestroyPluginRestartObject(this);
}

#ifdef _DEBUG
class ThreadPointerGuardian
	: public ES_ThreadListener
{
public:
	ThreadPointerGuardian(ES_Thread*& thread)
	{
		thread->AddListener(this);
	}

	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal)
	{
		switch (signal)
		{
		case ES_SIGNAL_FINISHED:
		case ES_SIGNAL_FAILED:
		case ES_SIGNAL_CANCELLED:
			ES_ThreadListener::Remove();
			OP_ASSERT(!"Someone deleting our thread. We are not pleased.");
		}
		return OpStatus::OK;
	}
};
#endif // _DEBUG


void
PluginRestartObject::HandleCallback(OpMessage msg, MH_PARAM_1 p1, MH_PARAM_2 p2)
{
	OP_NEW_DBG("PluginRestartObject::HandleCallback", "ns4p");
	if (called || !thread || !object)
		return;

	called = TRUE;
	Plugin* plugin = object->GetPlugin();

	OP_ASSERT(plugin);
	OP_ASSERT(!object->IsPureInternal());

	if (!plugin->GetDocument()) // plugin has been destroyed
	{
		success = FALSE;
	}
	else
	{
		OP_DBG((UNI_L("Plugin [%d] running command %d in thread %p"), plugin ? plugin->GetID() : 0, type, thread));
#ifdef _DEBUG
		ThreadPointerGuardian thread_guardian(thread);
#endif // _DEBUG

		OP_BOOLEAN status;
		NPVariant result;

		/*	Some plugins (WMP) claim to set result but don't and then
			we need to be able to discover that case by using a value
			of type that definately isn't legal. Discovered at
			http://t/core/plugins/wmp/np-mswmp/008.html where randomly
			an undefined valueOf/toString appeared. */
		result.type = static_cast<NPVariantType>(-1);

		plugin->SetScriptException(FALSE);

		switch (type)
		{
		case PLUGIN_GET:
			if (object->HasProperty(identifier) == OpBoolean::IS_TRUE)
			{
				if (!object) /* Object may have been released by plug-in call. */
					break;

				if (object->GetProperty(identifier, &result) == OpBoolean::IS_TRUE)
				{
					if (!object) /* Object may have been released by plug-in call. */
						break;

					OP_BOOLEAN import_status = PluginImportValue(plugin, GetRuntime(), value, result);
					if (OpStatus::IsMemoryError(import_status))
						oom = TRUE;
					else if (import_status == OpBoolean::IS_TRUE)
						success = TRUE;

					PluginReleaseExternalValue(result, TRUE); /* The caller releases return values. */
					break;
				}
			}

			if (object && object->HasMethod(identifier) == OpBoolean::IS_TRUE)
			{
				if (!object) /* Object may have been released by plug-in call. */
					break;

				if (ES_Object* es_obj = object->Import(GetRuntime()))
				{
					OpNPExternalObject *internal = (OpNPExternalObject *) ES_Runtime::GetHostObject(es_obj);
					OpNPExternalMethod *method = internal->GetMethod(identifier);

					if (method)
					{
						value.type = VALUE_OBJECT;
						value.value.object = *method;
						success = TRUE;
					}
				}

				oom = !success;
				break;
			}

			break;

		case PLUGIN_PUT:
			if (PluginExportValue(plugin, result, value, GetRuntime()))
			{
				if (object->HasProperty(identifier) == OpBoolean::IS_TRUE)
				{
					if (!object) /* Object may have been released by plug-in call. */
						break;

					if (object->SetProperty(identifier, &result) == OpBoolean::IS_TRUE)
						success = TRUE;
				}
				else if (object->HasMethod(identifier) == OpBoolean::IS_TRUE)
				{
					plugin->SetScriptException(TRUE);
					plugin->SetExceptionMessage("Couldn't overwrite method of NPObject.");

					break;
				}
			}
			else
				oom = TRUE;

			PluginReleaseInternalValue(value);
			PluginReleaseExternalValue(result); /* Release resources allocated by PluginExportValue above. */
			break;

		case PLUGIN_CALL:
			if (identifier)
				status = object->Invoke(identifier, argv, argc, &result);
			else
				status = object->InvokeDefault(argv, argc, &result);

			if (status == OpBoolean::IS_TRUE)
			{
				success = TRUE;

				if (OpStatus::IsMemoryError(PluginImportValue(plugin, GetRuntime(), value, result)))
					oom = TRUE;
			}

			PluginReleaseExternalValue(result, TRUE); /* The caller releases return values. */
			break;

		case PLUGIN_CONSTRUCT:
			if (object->Construct(argv, argc, &result) == OpBoolean::IS_TRUE)
			{
				success = TRUE;

				if (OpStatus::IsMemoryError(PluginImportValue(plugin, GetRuntime(), value, result)))
					oom = TRUE;

				PluginReleaseExternalValue(result, TRUE); /* The caller releases return values. */
			}

			break;
		}
	}

	if (object && !success && plugin->IsScriptException())
	{
		// a call to one of the plugin's NPObjects generated an exception
		object->SetHasException(TRUE);
	}

	/* In OpNPSetupCallOrConstruct(), we call PluginExportValue on all arguments. For
	 * objects and strings, this may involve memory allocation, and for objects,
	 * temporarily incrementing their reference counts to ensure they stay alive
	 * during the call. The following two lines balance these preparations by
	 * deallocating memory and decrementing reference counts. */
	if (type == PLUGIN_CALL || type == PLUGIN_CONSTRUCT)
		for (int i = 0; i < argc; i++)
			PluginReleaseExternalValue(argv[i]);

	mh->UnsetCallBacks(this);

	if (thread)
	{
		thread->Unblock();
		thread->SetIsPluginActive(FALSE);
		thread = NULL;

		Remove();
	}
}

OP_STATUS
PluginRestartObject::Signal(ES_Thread *, ES_ThreadSignal signal)
{
	OP_ASSERT(signal != ES_SIGNAL_FINISHED && signal != ES_SIGNAL_FAILED);
	if (signal == ES_SIGNAL_CANCELLED)
	{
		thread = NULL;
		Remove();
	}
	return OpStatus::OK;
}

void
PluginRestartObject::GCTrace()
{
	if (value.type == VALUE_OBJECT)
		GetRuntime()->GCMark(value.value.object);
}

void
PluginRestartObject::Resume()
{
	if (thread)
	{
		thread->Unblock();
		thread->SetIsPluginActive(FALSE);
		thread = NULL;

		Remove();
	}
}

/* static */ OP_STATUS
PluginRestartObject::Suspend(PluginRestartObject *&restart_object, ES_Runtime *runtime)
{
	ES_Thread *current_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	if (!current_thread)
		return OpStatus::ERR;

	restart_object = OP_NEW(PluginRestartObject, ());

	if (!restart_object || OpStatus::IsMemoryError(restart_object->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "Object")))
	{
		OP_DELETE(restart_object);
		return OpStatus::ERR_NO_MEMORY;
	}

	restart_object->thread = current_thread;
	restart_object->thread->Block();
	restart_object->thread->SetIsPluginActive(TRUE);
	restart_object->thread->AddListener(restart_object);

	return OpStatus::OK;
}


OpNPExternalObject::OpNPExternalObject(OpNPObject *npobject)
	: npobject(npobject), methods(0), m_enum_array(NULL), m_enum_count(0)
{
}

OpNPExternalObject::~OpNPExternalObject()
{
	// Triggered by ES engine's GC
	if (npobject) // if npobject is NULL it has already been deleted by ns4plugins in OpNPObject::~OpNPObject()
	{
		npobject->UnsetInternal();
		npobject->Release();
	}

	if (m_enum_array)
	{
		for (UINT32 i=0; i < m_enum_count; ++i)
			OP_DELETEA(m_enum_array[i]);

		OP_DELETEA(m_enum_array);
	}
}

/* static */ OP_STATUS
PluginRestartObject::Make(PluginRestartObject *&restart_object, ES_Runtime *runtime, OpNPObject *object, BOOL for_direct_use)
{
	ES_Thread *current_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	if (!current_thread)
		return OpStatus::ERR;

	restart_object = OP_NEW(PluginRestartObject, ());

	if (!restart_object || OpStatus::IsMemoryError(restart_object->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "Object")))
	{
		OP_DELETE(restart_object);
		return OpStatus::ERR_NO_MEMORY;
	}

	restart_object->thread = current_thread;
	restart_object->mh = g_main_message_handler;
	restart_object->object = object;

	restart_object->thread->Block();
	restart_object->thread->SetIsPluginActive(TRUE);

	if (for_direct_use)
		return OpStatus::OK;
	else
	{
		restart_object->thread->AddListener(restart_object);
		RETURN_IF_ERROR(restart_object->mh->SetCallBack(restart_object, MSG_PLUGIN_ECMASCRIPT_RESTART, reinterpret_cast<MH_PARAM_1>(restart_object)));
		return restart_object->mh->PostMessage(MSG_PLUGIN_ECMASCRIPT_RESTART, reinterpret_cast<MH_PARAM_1>(restart_object), 0) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}
}


static BOOL ShouldExecuteDirectly(OpNPObject* npobject)
{
	OP_NEW_DBG("ShouldExecuteDirectly", "ns4p");

	if (!g_pluginscriptdata->AllowNestedMessageLoop())
	{
		OP_DBG(("Nested message loops not allowed so must execute directly"));
		return TRUE;
	}

	// If a plugin calls back into itself, we have to process it directly
	// because if we return to the message loop (nested message loop)
	// then the plugin might see other messages before it gets the
	// answer from the script execution and that is known to confuse
	// and break plugins and plugin scripts
	if (npobject)
		if (Plugin* plugin = npobject->GetPlugin())
			if (plugin->IsInSynchronousLoop())
			{
				OP_DBG(("Plugin calling itself so must execute directly"));
				return TRUE;
			}

	return FALSE;
}

/* virtual */ ES_GetState
OpNPExternalObject::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	OP_NEW_DBG("OpNPExternalObject::GetName", "ns4p");

	if (!npobject)
		return GET_FAILED;

	if (!value)
	{
		OpNPIdentifier *identifier = g_pluginscriptdata->GetStringIdentifier(property_name);

		if (!identifier)
			return GET_NO_MEMORY;

		BOOL has_it = FALSE;
		TRY
			has_it = npobject->GetExternal()->_class->hasMethod && npobject->GetExternal()->_class->hasMethod(npobject->GetExternal(), identifier) ||
			(npobject && npobject->GetExternal()->_class->hasProperty && npobject->GetExternal()->_class->hasProperty(npobject->GetExternal(), identifier));
		CATCH_PLUGIN_EXCEPTION(has_it = FALSE; npobject->GetPlugin()->SetFailure(FALSE););
		if (has_it)
			return GET_SUCCESS;

		return GET_FAILED;
	}

	BOOL execute_directly = ShouldExecuteDirectly(npobject);

	PluginRestartObject *restart_object;

	GET_FAILED_IF_ERROR(PluginRestartObject::Make(restart_object, origining_runtime, npobject, execute_directly));

	restart_object->type = PluginRestartObject::PLUGIN_GET;
	restart_object->identifier = g_pluginscriptdata->GetStringIdentifier(property_name);

	if (!restart_object->identifier)
		return GET_NO_MEMORY;

	if (execute_directly)
	{
		if (!origining_runtime->Protect(*restart_object))
			return GET_NO_MEMORY;
		restart_object->HandleCallback(MSG_PLUGIN_ECMASCRIPT_RESTART, reinterpret_cast<MH_PARAM_1>(restart_object), 0);
		ES_GetState ret = GetNameRestart(property_name, property_code, value, origining_runtime, *restart_object);
		origining_runtime->Unprotect(*restart_object);
		return ret;
	}
	else
	{
		// Will be restarted from a message sent by PluginRestartObject::Make()
		value->type = VALUE_OBJECT;
		value->value.object = *restart_object;

		return GET_SUSPEND;
	}
}

/* virtual */ ES_GetState
OpNPExternalObject::GetNameRestart(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	if (!npobject)
		return GET_FAILED;

	PluginRestartObject *restart = (PluginRestartObject *) ES_Runtime::GetHostObject(restart_object);

	if (npobject->HasException())
	{
		npobject->SetHasException(FALSE);
		GET_FAILED_IF_ERROR(HandleNPObjectException(value, origining_runtime, npobject));
		return GET_EXCEPTION;
	}
	else if (restart->success)
	{
		*value = restart->value;
		return GET_SUCCESS;
	}
	else if (restart->oom)
		return GET_NO_MEMORY;
	else
		return GET_FAILED;
}

/* virtual */ ES_GetState
OpNPExternalObject::GetIndex(int property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (!npobject)
		return GET_FAILED;

	if (!value)
	{
		OpNPIdentifier *identifier = g_pluginscriptdata->GetIntIdentifier(property_name);

		if (!identifier)
			return GET_NO_MEMORY;
		BOOL has_it = FALSE;
		TRY
			has_it = npobject->GetExternal()->_class->hasMethod && npobject->GetExternal()->_class->hasMethod(npobject->GetExternal(), identifier) ||
			(npobject && npobject->GetExternal()->_class->hasProperty && npobject->GetExternal()->_class->hasProperty(npobject->GetExternal(), identifier));
		CATCH_PLUGIN_EXCEPTION(has_it = FALSE; npobject->GetPlugin()->SetFailure(FALSE););

		return has_it ? GET_SUCCESS : GET_FAILED;
	}

	BOOL execute_directly = ShouldExecuteDirectly(npobject);

	PluginRestartObject *restart_object;

	GET_FAILED_IF_ERROR(PluginRestartObject::Make(restart_object, origining_runtime, npobject, execute_directly));

	restart_object->type = PluginRestartObject::PLUGIN_GET;
	restart_object->identifier = g_pluginscriptdata->GetIntIdentifier(property_name);

	if (!restart_object->identifier)
		return GET_NO_MEMORY;

	if (execute_directly)
	{
		if (!origining_runtime->Protect(*restart_object))
			return GET_NO_MEMORY;
		restart_object->HandleCallback(MSG_PLUGIN_ECMASCRIPT_RESTART, reinterpret_cast<MH_PARAM_1>(restart_object), 0);
		ES_GetState ret = GetIndexRestart(property_name, value, origining_runtime, *restart_object);
		origining_runtime->Unprotect(*restart_object);
		return ret;
	}
	else
	{
		// Will be restarted from a message sent by PluginRestartObject::Make()
		value->type = VALUE_OBJECT;
		value->value.object = *restart_object;

		return GET_SUSPEND;
	}
}

/* virtual */ ES_GetState
OpNPExternalObject::GetIndexRestart(int property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	if (!npobject)
		return GET_FAILED;

	PluginRestartObject *restart = (PluginRestartObject *) ES_Runtime::GetHostObject(restart_object);

	if (npobject->HasException())
	{
		npobject->SetHasException(FALSE);
		GET_FAILED_IF_ERROR(HandleNPObjectException(value, origining_runtime, npobject));
		return GET_EXCEPTION;
	}
	else if (restart->success)
	{
		*value = restart->value;
		return GET_SUCCESS;
	}
	else if (restart->oom)
		return GET_NO_MEMORY;
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
OpNPExternalObject::PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (!npobject)
		return PUT_FAILED;

	PluginRestartObject *restart_object;

	BOOL execute_directly = ShouldExecuteDirectly(npobject);

	PUT_FAILED_IF_ERROR(PluginRestartObject::Make(restart_object, origining_runtime, npobject, execute_directly));

	restart_object->type = PluginRestartObject::PLUGIN_PUT;
	restart_object->identifier = g_pluginscriptdata->GetStringIdentifier(property_name);

	restart_object->value = *value;
	if (restart_object->value.type == VALUE_STRING)
	{
		restart_object->value.value.string = UniSetNewStr(restart_object->value.value.string);
		if (!restart_object->value.value.string)
			return PUT_NO_MEMORY;
	}

	if (execute_directly)
	{
		if (!origining_runtime->Protect(*restart_object))
			return PUT_NO_MEMORY;
		restart_object->HandleCallback(MSG_PLUGIN_ECMASCRIPT_RESTART, reinterpret_cast<MH_PARAM_1>(restart_object), 0);
		ES_PutState ret = PutNameRestart(property_name, property_code, value, origining_runtime, *restart_object);
		origining_runtime->Unprotect(*restart_object);
		return ret;
	}
	else
	{
		// Will be restarted from a message sent by PluginRestartObject::Make()
		value->type = VALUE_OBJECT;
		value->value.object = *restart_object;

		return PUT_SUSPEND;
	}
}

/* virtual */ ES_PutState
OpNPExternalObject::PutNameRestart(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	if (!npobject)
		return PUT_FAILED;

	if (npobject->HasException())
	{
		npobject->SetHasException(FALSE);
		PUT_FAILED_IF_ERROR(HandleNPObjectException(value, origining_runtime, npobject));
		return PUT_EXCEPTION;
	}
	else
	{
		PluginRestartObject *restart = (PluginRestartObject *) ES_Runtime::GetHostObject(restart_object);
		return restart->success ? PUT_SUCCESS : PUT_FAILED;
	}
}

/* virtual */ ES_PutState
OpNPExternalObject::PutIndex(int property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (!npobject)
		return PUT_FAILED;

	PluginRestartObject *restart_object;

	BOOL execute_directly = ShouldExecuteDirectly(npobject);

	PUT_FAILED_IF_ERROR(PluginRestartObject::Make(restart_object, origining_runtime, npobject, execute_directly));

	restart_object->type = PluginRestartObject::PLUGIN_PUT;
	restart_object->identifier = g_pluginscriptdata->GetIntIdentifier(property_name);

	restart_object->value = *value;
	if (restart_object->value.type == VALUE_STRING)
	{
		restart_object->value.value.string = UniSetNewStr(restart_object->value.value.string);
		if (!restart_object->value.value.string)
			return PUT_NO_MEMORY;
	}

	if (execute_directly)
	{
		if (!origining_runtime->Protect(*restart_object))
			return PUT_NO_MEMORY;
		restart_object->HandleCallback(MSG_PLUGIN_ECMASCRIPT_RESTART, reinterpret_cast<MH_PARAM_1>(restart_object), 0);
		ES_PutState ret = PutIndexRestart(property_name, value, origining_runtime, *restart_object);
		origining_runtime->Unprotect(*restart_object);
		return ret;
	}
	else
	{
		// Will be restarted from a message sent by PluginRestartObject::Make()
		value->type = VALUE_OBJECT;
		value->value.object = *restart_object;

		return PUT_SUSPEND;
	}
}

/* virtual */ ES_PutState
OpNPExternalObject::PutIndexRestart(int property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	if (!npobject)
		return PUT_FAILED;

	if (npobject->HasException())
	{
		npobject->SetHasException(FALSE);
		PUT_FAILED_IF_ERROR(HandleNPObjectException(value, origining_runtime, npobject));
		return PUT_EXCEPTION;
	}
	else
		return PUT_SUCCESS;
}

/* virtual */ ES_DeleteStatus
OpNPExternalObject::DeleteName(const uni_char* property_name, ES_Runtime *origining_runtime)
{
	if (!npobject)
		return DELETE_OK;

	OpNPIdentifier *identifier = g_pluginscriptdata->GetStringIdentifier(property_name);
	BOOL ret = TRUE;
	if (identifier && npobject->GetExternal()->_class->removeProperty)
	{
		TRY
			ret = npobject->GetExternal()->_class->removeProperty(npobject->GetExternal(), identifier);
		CATCH_PLUGIN_EXCEPTION(ret = FALSE; npobject->GetPlugin()->SetFailure(FALSE););
	}
	return ret ? DELETE_OK : DELETE_REJECT;
}

/* virtual */ ES_DeleteStatus
OpNPExternalObject::DeleteIndex(int property_index, ES_Runtime* origining_runtime)
{
	if (!npobject)
		return DELETE_OK;

	OpNPIdentifier *identifier = g_pluginscriptdata->GetIntIdentifier(property_index);
	BOOL ret = TRUE;
	if (identifier && npobject->GetExternal()->_class->removeProperty)
	{
		TRY
			ret = npobject->GetExternal()->_class->removeProperty(npobject->GetExternal(), identifier);
		CATCH_PLUGIN_EXCEPTION(ret = FALSE; npobject->GetPlugin()->SetFailure(FALSE););
	}
	return ret ? DELETE_OK : DELETE_REJECT;
}

/* virtual */ BOOL
OpNPExternalObject::TypeofYieldsObject()
{
	return TRUE;
}

OP_STATUS
OpNPExternalObject::InitEnum()
{
	OP_STATUS stat = OpStatus::OK;

	if (npobject)
	{
		if (NP_CLASS_STRUCT_VERSION_HAS_ENUM(npobject->GetExternal()->_class) &&
			npobject->GetExternal()->_class->enumerate && !m_enum_array)
		{
			NPIdentifier* value = NULL;
			TRY
				npobject->GetExternal()->_class->enumerate(npobject->GetExternal(), &value, &m_enum_count);
			CATCH_PLUGIN_EXCEPTION(m_enum_count = 0; npobject->GetPlugin()->SetFailure(FALSE););
			if (m_enum_count > 0 && (m_enum_array = OP_NEWA(uni_char*, m_enum_count)))
			{
				for (UINT32 i=0; i<m_enum_count && OpStatus::IsSuccess(stat); ++i)
				{
					OpNPIdentifier* identifier = static_cast<OpNPIdentifier *>(NPIdentifier(value[i]));
					if (identifier && identifier->IsString() && identifier->GetNameUTF8())
					{
						OpString buffer;
						if (OpStatus::IsSuccess(stat = buffer.SetFromUTF8(static_cast<OpNPIdentifier *>(identifier)->GetNameUTF8())))
						{
							int buffer_length = buffer.Length();
							m_enum_array[i] = OP_NEWA(uni_char, buffer_length + 1);
							if (m_enum_array[i])
							{
								uni_strncpy(m_enum_array[i], buffer.CStr(), buffer_length);
								m_enum_array[i][buffer_length] = '\0';
							}
							else
								stat = OpStatus::ERR_NO_MEMORY;
						}
					}
					else
						m_enum_count = 0;
				}
			}
			else if (m_enum_count > 0)
				stat = OpStatus::ERR_NO_MEMORY;

			PluginMemoryHandler::GetHandler()->Free(value);
		}
	}
	else
		m_enum_count = 0;

	return stat;
}

/* virtual */ ES_GetState
OpNPExternalObject::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	GET_FAILED_IF_ERROR(InitEnum());
	for (UINT32 i = 0; i < m_enum_count; i++)
		enumerator->AddPropertyL(m_enum_array[i]);

	return GET_SUCCESS;
}

/* virtual */ ES_GetState
OpNPExternalObject::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	if (!npobject)
		return GET_FAILED;

	// Default to 0 when length property does not exist.
	count = 0;

	OpNPIdentifier* name = g_pluginscriptdata->GetStringIdentifier(UNI_L("length"));
	if (!name)
		return GET_NO_MEMORY;

	if (npobject->HasProperty(name) == OpBoolean::IS_TRUE)
	{
		NPVariant result;
		if (npobject->GetProperty(name, &result) == OpBoolean::IS_TRUE)
		{
			if (NPVARIANT_IS_INT32(result))
				count = result.value.intValue;
			else if (NPVARIANT_IS_DOUBLE(result))
				count = static_cast<int>(result.value.doubleValue);

			PluginReleaseExternalValue(result);
		}
	}

	return GET_SUCCESS;
}

static int
OpNPFinishCall(ES_Value *return_value, ES_Runtime *origining_runtime)
{
	PluginRestartObject *restart_object = (PluginRestartObject *) ES_Runtime::GetHostObject(return_value->value.object);
	if (restart_object->object && restart_object->object->HasException())
	{
		restart_object->object->SetHasException(FALSE);
		CALL_FAILED_IF_ERROR(HandleNPObjectException(return_value, origining_runtime, restart_object->object));
		return ES_EXCEPTION;
	}
	else if (restart_object->success)
	{
		*return_value = restart_object->value;
		return ES_VALUE;
	}
	else if (restart_object->oom)
		return ES_NO_MEMORY;
	else
		return ES_FAILED;
}

static int
OpNPSetupCallOrConstruct(OpNPObject *npobject, OpNPIdentifier *methodname, int argc, ES_Value *argv, ES_Value *return_value, ES_Runtime *origining_runtime, BOOL construct)
{
	if (!npobject)
		return ES_FAILED;

	BOOL execute_directly = ShouldExecuteDirectly(npobject);

	PluginRestartObject *restart_object;

	CALL_FAILED_IF_ERROR(PluginRestartObject::Make(restart_object, origining_runtime, npobject, execute_directly));

	restart_object->type = construct ? PluginRestartObject::PLUGIN_CONSTRUCT : PluginRestartObject::PLUGIN_CALL;
	restart_object->identifier = methodname;
	restart_object->argc = argc;

	NPVariant *args = restart_object->argv = OP_NEWA(NPVariant, argc);

	if (!args)
		return ES_NO_MEMORY;

	for (int index = 0; index < argc; ++index)
		if (!PluginExportValue(npobject->GetPlugin(), args[index], argv[index], origining_runtime))
		{
			while (--index >= 0)
				PluginReleaseExternalValue(args[index]);

			OP_DELETEA(args);
			return ES_NO_MEMORY;
		}

	return_value->type = VALUE_OBJECT;
	return_value->value.object = *restart_object;
	if (execute_directly)
	{
		if (!origining_runtime->Protect(*restart_object))
			return ES_NO_MEMORY;
		restart_object->HandleCallback(MSG_PLUGIN_ECMASCRIPT_RESTART, reinterpret_cast<MH_PARAM_1>(restart_object), 0);
		int ret = OpNPFinishCall(return_value, origining_runtime);
		origining_runtime->Unprotect(*restart_object);
		return ret;
	}
	else
		// Will be restarted from a message sent by PluginRestartObject::Make()
		return ES_SUSPEND | ES_RESTART;
}

/* virtual */ int
OpNPExternalObject::Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	if (argc >= 0)
		return OpNPSetupCallOrConstruct(npobject, NULL, argc, argv, return_value, origining_runtime, FALSE);
	else
		return OpNPFinishCall(return_value, origining_runtime);
}

/* virtual */ int
OpNPExternalObject::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	if (argc >= 0)
		return OpNPSetupCallOrConstruct(npobject, NULL, argc, argv, return_value, origining_runtime, TRUE);
	else
		return OpNPFinishCall(return_value, origining_runtime);
}

/* virtual */ void
OpNPExternalObject::GCTrace()
{
	OpNPExternalMethod *method = methods;

	while (method)
	{
		GetRuntime()->GCMark(method);
		method = method->GetNext();
	}
}

OpNPExternalMethod *
OpNPExternalObject::GetMethod(OpNPIdentifier *name)
{
	OpNPExternalMethod *method = methods;

	while (method)
	{
		if (name == method->GetMethodName())
			return method;

		method = method->GetNext();
	}

	method = OpNPExternalMethod::Make(GetRuntime(), npobject, name, methods);

	if (method)
		methods = method;

	return method;
}

void
OpNPExternalObject::MarkNPObjectAsDeleted()
{
	npobject = NULL;

	for (OpNPExternalMethod* method = methods; method; method = method->GetNext())
		method->MarkNPObjectAsDeleted();
}

OpNPExternalMethod::OpNPExternalMethod()
	: npobject(NULL),
	  name(NULL),
	  next(NULL)
{
}

/* static */ OpNPExternalMethod *
OpNPExternalMethod::Make(ES_Runtime *runtime, OpNPObject *this_object, OpNPIdentifier *name, OpNPExternalMethod *next)
{
	OpNPExternalMethod *method = OP_NEW(OpNPExternalMethod, ());

	if (!method || OpStatus::IsMemoryError(method->SetFunctionRuntime(runtime, runtime->GetFunctionPrototype(), "Function", 0)))
	{
		OP_DELETE(method);
		return NULL;
	}

	method->npobject = this_object;
	method->name = name;
	method->next = next;

	return method;
}

/* virtual */ int
OpNPExternalMethod::Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	if (argc >= 0)
		return OpNPSetupCallOrConstruct(npobject, name, argc, argv, return_value, origining_runtime, FALSE);
	else
		return OpNPFinishCall(return_value, origining_runtime);
}

/* virtual */ void
OpNPExternalMethod::GCTrace()
{
	/* When an OpNPObject is deleted, it calls MarkNPObjectAsDeleted() on the OpNPExternalObject, which
	 * propagates the call to all its OpNPExternalMethod children, ensuring that no ES objects remain
	 * with a reference the dead OpNPObject.
	 *
	 * In this manner the OpNPExternalObject functions as a bridge between the OpNPObject and the
	 * OpNPExternalMethods, and we need to ensure that all methods are collected before it is. */
	if (npobject)
		GetRuntime()->GCMark(npobject->GetInternal());
}

PluginScriptData::PluginScriptData()
	: string_identifiers(TRUE),
	  m_allow_nested_message_loop(TRUE)
{
}

PluginScriptData::~PluginScriptData()
{
}

OpNPIdentifier *
PluginScriptData::GetStringIdentifier(const char *name8)
{
	OpString name;
	OpNPIdentifier *data;

	if (!name8 || OpStatus::IsError(name.SetFromUTF8(name8)))
		return NULL;

	if (string_identifiers.GetData(name.CStr(), &data) == OpStatus::OK)
		return data;

	OpNPIdentifier *identifier = OpNPIdentifier::Make(name.CStr());

	if (!identifier || OpStatus::IsMemoryError(string_identifiers.Add(identifier->GetName(), identifier)))
	{
		OP_DELETE(identifier);
		return NULL;
	}

	return identifier;
}

OpNPIdentifier *
PluginScriptData::GetStringIdentifier(const uni_char *name)
{
	OpNPIdentifier *data;

	if (string_identifiers.GetData(name, &data) == OpStatus::OK)
		return data;

	OpNPIdentifier *identifier = OpNPIdentifier::Make(name);

	if (!identifier || OpStatus::IsMemoryError(string_identifiers.Add(identifier->GetName(), identifier)))
	{
		OP_DELETE(identifier);
		return NULL;
	}

	return identifier;
}

OpNPIdentifier *
PluginScriptData::GetIntIdentifier(int name)
{
	OpNPIdentifier *data;

	if (int_identifiers.GetData(name, &data) == OpStatus::OK)
		return data;

	OpNPIdentifier *identifier = OpNPIdentifier::Make(name);

	if (!identifier || OpStatus::IsMemoryError(int_identifiers.Add(name, identifier)))
	{
		OP_DELETE(identifier);
		return NULL;
	}

	return identifier;
}

OP_STATUS
PluginScriptData::AddPluginRestartObject(PluginRestartObject* restart_object)
{
	return restart_objects.Add(restart_object);
}

void
PluginScriptData::RemovePluginRestartObject(PluginRestartObject* restart_object)
{
	restart_objects.RemoveByItem(restart_object);
}

void
PluginScriptData::ReleaseObjectFromRestartObjects(OpNPObject* object)
{
	for (UINT32 i=0; i<restart_objects.GetCount(); i++)
	{
		PluginRestartObject* restart_object = restart_objects.Get(i);
		if (restart_object->object == object)
		{
			restart_object->object = NULL;
		}
	}
}

OP_STATUS
PluginScriptData::AddObject(OpNPObject* object)
{
	OP_ASSERT(!external_objects.Contains(object->GetExternal()));
	return external_objects.Add(object->GetExternal(), object);
}

void
PluginScriptData::RemoveObject(OpNPObject* object)
{
	OpNPObject* ignored;
	OpStatus::Ignore(external_objects.Remove(object->GetExternal(), &ignored));
}

OpNPObject*
PluginScriptData::FindObject(const NPObject* external)
{
	OpNPObject* object;
	if (external_objects.GetData(external, &object) == OpStatus::OK)
		return object;

	return NULL;
}

void
NPN_ReleaseVariantValue(NPVariant *variant)
{
	OP_NEW_DBG("NPN_ReleaseVariantValue", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	PluginReleaseExternalValue(*variant);
}

NPIdentifier
NPN_GetStringIdentifier(const NPUTF8 *name)
{
	OP_NEW_DBG("NPN_GetStringIdentifier", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	return g_pluginscriptdata->GetStringIdentifier(name);
}

void
NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers)
{
	OP_NEW_DBG("NPN_GetStringIdentifiers", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	for (int32_t index = 0; index < nameCount; ++index)
		identifiers[index] = g_pluginscriptdata->GetStringIdentifier(names[index]);
}

NPIdentifier
NPN_GetIntIdentifier(int32_t intid)
{
	OP_NEW_DBG("NPN_GetIntIdentifier", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	return g_pluginscriptdata->GetIntIdentifier(intid);
}

bool
NPN_IdentifierIsString(NPIdentifier identifier)
{
	OP_NEW_DBG("NPN_IdentifierIsString", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	return ((OpNPIdentifier *) identifier)->IsString() == TRUE;
}

NPUTF8 *
NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
	OP_NEW_DBG("NPN_UTF8FromIdentifier", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	NPUTF8* utf8 = NULL;
	if (identifier != NULL)
	{
		unsigned length = op_strlen(((OpNPIdentifier *) identifier)->GetNameUTF8());
		utf8 = (NPUTF8 *) PluginMemoryHandler::GetHandler()->Malloc(length + 1);
		if (utf8)
		{
			op_memcpy(utf8, ((OpNPIdentifier *) identifier)->GetNameUTF8(), length);
			utf8[length] = 0;
		}
	}
	return utf8;
}

int32_t
NPN_IntFromIdentifier(NPIdentifier identifier)
{
	OP_NEW_DBG("NPN_IntFromIdentifier", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	return ((OpNPIdentifier *) identifier)->GetNameInt();
}

NPObject *
NPN_CreateObject(NPP npp, NPClass *aClass)
{
	OP_NEW_DBG("NPN_CreateObject", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NULL;
	}

	OP_ASSERT(g_op_system_info->IsInMainThread());
	if (OpNS4PluginHandler::GetHandler())
	{
		Plugin *plugin = NULL;
		FramesDocument *frames_doc = static_cast<PluginHandler*>(OpNS4PluginHandler::GetHandler())->GetScriptablePluginDocument(npp, plugin);

		if (plugin)
		{
			if (frames_doc)
				OpStatus::Ignore(frames_doc->ConstructDOMEnvironment());

			NPObject *external;

			if (aClass->allocate)
				external = aClass->allocate(npp, aClass);
			else
				external = static_cast<NPObject*>(PluginMemoryHandler::GetHandler()->Malloc(sizeof(NPObject)));

			if (external)
			{
				external->_class = aClass;
				external->referenceCount = 1;

				OpNPObject *npobject = OpNPObject::Make(plugin, external);
				if (npobject)
					return external;

				if (aClass->allocate && aClass->deallocate)
					aClass->deallocate(external);
				else
					PluginMemoryHandler::GetHandler()->Free(external);
			}
		}
	}
	return NULL;
}

NPObject *
NPN_RetainObject(NPObject *npobj)
{
	OP_NEW_DBG("NPN_RetainObject", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	if (OpNPObject* obj = g_pluginscriptdata->FindObject(npobj))
		obj->Retain();

	return npobj;
}

void
NPN_ReleaseObject(NPObject *npobj)
{
	OP_NEW_DBG("NPN_ReleaseObject", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	if (OpNPObject* obj = g_pluginscriptdata->FindObject(npobj))
		obj->Release();
}

class OpNPSyncCallback
	: public ES_SyncInterface::Callback
{
protected:
	Plugin* plugin;
	ES_Runtime *runtime;
	bool success, *ismethod;
	NPVariant *result;

public:
	OpNPSyncCallback(Plugin* plugin, ES_Runtime *runtime)
		: plugin(plugin),
		  runtime(runtime),
		  success(false),
		  ismethod(NULL),
		  result(NULL)
	{
	}

	OpNPSyncCallback(Plugin* plugin, ES_Runtime *runtime, NPVariant *result)
		: plugin(plugin),
		  runtime(runtime),
		  success(false),
		  ismethod(NULL),
		  result(result)
	{
	}

	OpNPSyncCallback(Plugin* plugin, ES_Runtime *runtime, bool *ismethod)
		: plugin(plugin),
		  runtime(runtime),
		  success(false),
		  ismethod(ismethod),
		  result(NULL)
	{
	}

	virtual OP_STATUS HandleCallback(ES_SyncInterface::Callback::Status status, const ES_Value &value)
	{
		if (status == ES_SyncInterface::Callback::ESSYNC_STATUS_SUCCESS)
		{
			success = TRUE;

			if (result && !PluginExportValue(plugin, *result, value, runtime))
				success = FALSE;
			else if (ismethod && value.type == VALUE_OBJECT)
			{
				const char* tmp = ES_Runtime::GetClass(value.value.object);
				*ismethod = op_strcmp(tmp, "Function") == 0 ||
					op_strcmp(tmp, "HTMLCollection") == 0 ||
					op_strcmp(tmp, "NodeList") == 0;
			}
		}
		else
			success = FALSE;

		return OpStatus::OK;
	}

	bool IsSuccess() { return success; }
};

static bool
OpNPStartCall(NPP npp, NPObject *npobj, ES_Runtime *&runtime, ES_Object *&object, Plugin *&plugin)
{
	if (OpNS4PluginHandler::GetHandler())
	{
		FramesDocument *frames_doc = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->GetScriptablePluginDocument(npp, plugin);
		if (plugin && frames_doc && OpStatus::IsSuccess(frames_doc->ConstructDOMEnvironment()))
		{
			runtime = frames_doc->GetESRuntime();

			if (npobj)
			{
				if (OpNPObject *opnpobject = g_pluginscriptdata->FindObject(npobj))
				{
					object = opnpobject->Import(runtime);
					if (object)
						return true;
				}
			}
			else
			{
				object = DOM_Utils::GetES_Object(frames_doc->GetDOMEnvironment()->GetWindow());
				return true;
			}
		}
	}
	return false;
}

OP_STATUS GetLocation(FramesDocument* doc, NPVariant*& result)
{
	OpString8 url_name;
	OP_STATUS stat = doc->GetURL().GetAttribute(URL::KName_With_Fragment_Username_Password_Hidden, url_name, FALSE);
	if (OpStatus::IsSuccess(stat))
	{
		unsigned ret_value_length = url_name.Length();
		const char* ret_value = (char*)PluginMemoryHandler::GetHandler()->Malloc(ret_value_length + 1);
		if (ret_value)
		{
			op_memcpy((void*)ret_value, url_name.CStr(), ret_value_length);
			((char*)ret_value)[ret_value_length] = '\0';
			STRINGN_TO_NPVARIANT(ret_value, ret_value_length, *result);
			stat = OpStatus::OK;
		}
		else
			stat = OpStatus::ERR_NO_MEMORY;
	}
	return stat;
}

bool
NPN_Invoke(NPP npp, NPObject *npobj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	OP_NEW_DBG("NPN_Invoke", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return false;
	}

	OpNPObject* opnpobj = g_pluginscriptdata->FindObject(npobj);

	if (!opnpobj || !methodName)
		return false;

	/* If the object we are to invoke a method on was created by the calling plug-in library,
	 * then we skip the detour through ES and call the plug-in library method directly. */
	if (opnpobj->HasMethod(methodName) == OpBoolean::IS_TRUE)
	{
		OP_BOOLEAN status = opnpobj->Invoke(methodName, args, argCount, result);
		if (OpStatus::IsSuccess(status))
			return status == OpBoolean::IS_TRUE ? true : false;
	}

	ES_Runtime *runtime;
	ES_Object *object;
	Plugin* plugin;

	if (!OpNPStartCall(npp, npobj, runtime, object, plugin))
		return false;

	DOM_Object* dom_object = DOM_Utils::GetDOM_Object(object);
	if (result && dom_object && DOM_Utils::IsA(dom_object, DOM_TYPE_LOCATION) &&
		static_cast<OpNPIdentifier*>(methodName)->GetName() &&
		uni_str_eq(static_cast<OpNPIdentifier*>(methodName)->GetName(), "toString"))
	{
		// Common case in Flash's security checks. Optimize
		// to avoid complexity that might bring Opera down
		// due to too many nested message loops
		if (FramesDocument* doc = DOM_Utils::GetDocument(DOM_Utils::GetDOM_Runtime(runtime)))
		{
			OP_DBG(("Plugin [%d]: returning shortcut value for window.location.toString()", plugin->GetID()));
			return OpStatus::IsSuccess(GetLocation(doc, result)) ? true : false;
		}
	}

	ES_AsyncInterface* asyncif = GetAsyncIFFromRuntime(runtime);

	if (plugin->GetPopupEnabledState())
		asyncif->SetIsUserRequested();

	ES_SyncInterface syncif(runtime, asyncif);
	ES_SyncInterface::CallData data;
	OpNPSyncCallback callback(plugin, runtime, result);

	data.arguments = OP_NEWA(ES_Value, argCount);
	data.arguments_count = 0;
	if (!data.arguments)
		return false;

	uint32_t index;
	bool success = false;
	BOOL saved_allow_nested_message_loop;
	FramesDocument* frames_doc;
	LayoutWorkplace* wp;

	for (index = 0; index < argCount; ++index)
		if (PluginImportValue(plugin, runtime, data.arguments[index], args[index]) == OpBoolean::IS_TRUE)
			++data.arguments_count;
		else
			goto failed;

	saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	frames_doc = plugin->GetDocument();
	wp = frames_doc ? (frames_doc->GetLogicalDocument() ? frames_doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL) : NULL;
	if ((frames_doc && frames_doc->IsReflowing()) ||
		(wp && wp->IsTraversing()) ||
		plugin->GetContextMenuActive() ||
		g_input_manager->IsLockMouseInputContext())
	{
		data.allow_nested_message_loop = FALSE;
		g_pluginscriptdata->SetAllowNestedMessageLoop(data.allow_nested_message_loop);
	}
	else
		data.allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	data.this_object = object;
	data.method = ((OpNPIdentifier *) methodName)->GetName();
	data.interrupt_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	g_in_synchronous_loop++;
	plugin->EnterSynchronousLoop();
	OP_ASSERT(data.allow_nested_message_loop == g_pluginscriptdata->AllowNestedMessageLoop());
	if (data.allow_nested_message_loop)
		OP_DBG((UNI_L("Plugin [%d] possibly entering nested message loop to run method '%s'"), plugin->GetID(), data.method));

	if (OpStatus::IsSuccess(syncif.Call(data, &callback)))
		success = callback.IsSuccess();

	plugin->LeaveSynchronousLoop();
	g_in_synchronous_loop--;

	g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

	if (result && result->type == NPVariantType_Object) // make sure the result will be GC'ed later when the plugin is deleted
	{
		if (!g_pluginscriptdata->FindObject(result->value.objectValue))
			success = false;
	}

failed:
	for (index = 0; index < data.arguments_count; ++index)
		PluginReleaseInternalValue(data.arguments[index]);

	OP_DELETEA(data.arguments);
	return success;
}

bool
NPN_InvokeDefault(NPP npp, NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	OP_NEW_DBG("NPN_InvokeDefault", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return false;
	}

	OpNPObject* opnpobj = g_pluginscriptdata->FindObject(npobj);

	if (!opnpobj)
		return false;

	/* If the object we are to run was created by the calling plug-in library, then
	 * we skip the detour through ES and call the plug-in library method directly. */
	OP_BOOLEAN status = opnpobj->InvokeDefault(args, argCount, result);
	if (OpStatus::IsSuccess(status))
		return status == OpBoolean::IS_TRUE ? true : false;

	ES_Runtime *runtime;
	ES_Object *object;
	Plugin* plugin;

	if (!OpNPStartCall(npp, npobj, runtime, object, plugin))
		return false;

	ES_AsyncInterface* asyncif = GetAsyncIFFromRuntime(runtime);

	if (plugin->GetPopupEnabledState())
		asyncif->SetIsUserRequested();

	ES_SyncInterface syncif(runtime, asyncif);
	ES_SyncInterface::CallData data;
	OpNPSyncCallback callback(plugin, runtime, result);

	data.arguments = OP_NEWA(ES_Value, argCount);
	data.arguments_count = 0;
	data.interrupt_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	if (!data.arguments)
		return false;

	uint32_t index;
	bool success = false;
	BOOL saved_allow_nested_message_loop;

	for (index = 0; index < argCount; ++index)
		if (PluginImportValue(plugin, runtime, data.arguments[index], args[index]) == OpBoolean::IS_TRUE)
			++data.arguments_count;
		else
			goto failed;

	data.allow_nested_message_loop = !g_input_manager->IsLockMouseInputContext() && g_pluginscriptdata->AllowNestedMessageLoop();
	saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	g_pluginscriptdata->SetAllowNestedMessageLoop(data.allow_nested_message_loop);
	data.function = object;

	g_in_synchronous_loop++;
	plugin->EnterSynchronousLoop();
	OP_ASSERT(data.allow_nested_message_loop == g_pluginscriptdata->AllowNestedMessageLoop());
	if (data.allow_nested_message_loop)
		OP_DBG((UNI_L("Plugin [%d] possibly entering nested message loop to use object as function"), plugin->GetID()));

	if (OpStatus::IsSuccess(syncif.Call(data, &callback)))
		success = callback.IsSuccess();

	plugin->LeaveSynchronousLoop();
	g_in_synchronous_loop--;

	g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

	if (result && result->type == NPVariantType_Object) // make sure the result will be GC'ed later when the plugin is deleted
	{
		if (!g_pluginscriptdata->FindObject(result->value.objectValue))
			success = false;
	}

failed:
	for (index = 0; index < data.arguments_count; ++index)
		PluginReleaseInternalValue(data.arguments[index]);

	OP_DELETEA(data.arguments);
	return success;
}

bool
NPN_Evaluate(NPP npp, NPObject *npobj, NPString *script, NPVariant *result)
{
	OP_NEW_DBG("NPN_Evaluate", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return false;
	}

	ES_Runtime *runtime;
	ES_Object *object;
	Plugin* plugin;

	if (!script || !OpNPStartCall(npp, npobj, runtime, object, plugin))
		return false;

	TRACK_CPU_PER_TAB(runtime->GetFramesDocument() ? runtime->GetFramesDocument()->GetWindow()->GetCPUUsageTracker() : NULL);
	ES_AsyncInterface* asyncif = GetAsyncIFFromRuntime(runtime);

	if (plugin->GetPopupEnabledState())
		asyncif->SetIsUserRequested();

	ES_SyncInterface syncif(runtime, asyncif);
	ES_SyncInterface::EvalData data;

	NPVariant external_value;
	external_value.type = NPVariantType_String;
	external_value.value.stringValue = *script;

	ES_Value internal_value;
	if (PluginImportValue(plugin, runtime, internal_value, external_value) != OpBoolean::IS_TRUE)
		return false;

	FramesDocument* frames_doc = plugin->GetDocument();
	LayoutWorkplace* wp = frames_doc ? (frames_doc->GetLogicalDocument() ? frames_doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL) : NULL;
	if ((frames_doc && frames_doc->IsReflowing()) || (wp && wp->IsTraversing()) || plugin->GetContextMenuActive() || g_input_manager->IsLockMouseInputContext())
		data.allow_nested_message_loop = FALSE;
	else
		data.allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	g_pluginscriptdata->SetAllowNestedMessageLoop(data.allow_nested_message_loop);
	data.this_object = object;
	data.program = internal_value.value.string;
	data.interrupt_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	OpNPSyncCallback callback(plugin, runtime, result);
	bool success = false;

	g_in_synchronous_loop++;
	plugin->EnterSynchronousLoop();
	OP_ASSERT(data.allow_nested_message_loop == g_pluginscriptdata->AllowNestedMessageLoop());
	if (data.allow_nested_message_loop)
		OP_DBG((UNI_L("Plugin [%d] possibly entering nested message loop to eval script '%.30s%s'"), plugin->GetID(), data.program, uni_strlen(data.program) > 30 ? UNI_L("...") : UNI_L("")));

	if (OpStatus::IsSuccess(syncif.Eval(data, &callback)))
		success = callback.IsSuccess();

	plugin->LeaveSynchronousLoop();
	g_in_synchronous_loop--;

	g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

	if (result && result->type == NPVariantType_Object) // make sure the result will be GC'ed later when the plugin is deleted
	{
		if (!g_pluginscriptdata->FindObject(result->value.objectValue))
			success = false;
	}

	PluginReleaseInternalValue(internal_value);
	return success;
}

bool
NPN_GetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName, NPVariant *result)
{
	OP_NEW_DBG("NPN_GetProperty", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return false;
	}

	OpNPObject* opnpobj = g_pluginscriptdata->FindObject(npobj);

	if (!opnpobj || !propertyName)
		return false;

	/* If the object we are to retrieve a property from was created by the calling plug-in
	 * library, then we skip the detour through ES and call the plug-in library method directly. */
	OP_BOOLEAN status = opnpobj->GetProperty(propertyName, result);
	if (OpStatus::IsSuccess(status))
		return status == OpBoolean::IS_TRUE ? true : false;

	ES_Runtime *runtime;
	ES_Object *object;
	Plugin* plugin;

	if (!OpNPStartCall(npp, npobj, runtime, object, plugin))
		return false;

	// The flash plugin loves to ask for the document.location all the time for its
	// internal security check. That becomes expensive and caused complexity that
	// easily triggers other bugs. By shortcircuiting things here we get
	// both a performance optimization, can allow the location property
	// in DOM to change and avoid unnecessary nested message loops
	const uni_char* property_name = static_cast<OpNPIdentifier*>(propertyName)->GetName();
	OP_DBG((UNI_L("Plugin [%d] reading property '%s'"), plugin->GetID(), property_name ? property_name : UNI_L("<null>")));
	if (property_name &&
		(uni_str_eq(property_name, "location") || uni_str_eq(property_name, "top")))
	{
		DOM_Object* dom_object = DOM_Utils::GetDOM_Object(object);
		if (dom_object && DOM_Utils::IsA(dom_object, DOM_TYPE_WINDOW))
		{
			FramesDocument* doc = DOM_Utils::GetDocument(DOM_Utils::GetDOM_Runtime(runtime));
			if (doc && result)
			{
				DOM_Object* return_dom_object;
				if (*property_name == 'l') // location
				{
					DOM_Object* location;
					if (OpStatus::IsError(DOM_Utils::GetWindowLocationObject(dom_object, location)))
						return false;
					OP_ASSERT(location);
					return_dom_object = location;
				}
				else
				{
					OP_ASSERT(*property_name == 't'); // top
					DocumentManager* top_docman = doc->GetWindow()->DocManager();
					DOM_Object* top_js_window;

					if (OpStatus::IsError(top_docman->GetJSWindow(top_js_window, runtime)))
						return false;

					OP_ASSERT(top_js_window);
					return_dom_object = top_js_window;
					runtime->MergeHeapWith(DOM_Utils::GetDOM_Environment(return_dom_object)->GetRuntime());
				}

				ES_Object* es_obj = DOM_Utils::GetES_Object(return_dom_object);
				OpNPObject* return_np_object = plugin->FindObject(es_obj);

				if (!return_np_object)
					return_np_object = OpNPObject::Make(plugin, runtime, es_obj);

				if (return_np_object)
				{
					result->value.objectValue = return_np_object->GetExternal();
					result->type = NPVariantType_Object;
				}
			}
			return true;
		}
	}

	ES_AsyncInterface *asyncif = GetAsyncIFFromRuntime(runtime);

	ES_SyncInterface syncif(runtime, asyncif);
	ES_SyncInterface::SlotData data;

	FramesDocument* frames_doc = plugin->GetDocument();
	LayoutWorkplace* wp = frames_doc ? (frames_doc->GetLogicalDocument() ? frames_doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL) : NULL;
	if ((frames_doc && frames_doc->IsReflowing()) || (wp && wp->IsTraversing()) || plugin->GetContextMenuActive() || g_input_manager->IsLockMouseInputContext())
		data.allow_nested_message_loop = FALSE;
	else
		data.allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	g_pluginscriptdata->SetAllowNestedMessageLoop(data.allow_nested_message_loop);
	data.object = object;
	data.name = ((OpNPIdentifier *) propertyName)->GetName();
	data.interrupt_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	OpNPSyncCallback callback(plugin, runtime, result);

	g_in_synchronous_loop++;
	plugin->EnterSynchronousLoop();
	OP_ASSERT(data.allow_nested_message_loop == g_pluginscriptdata->AllowNestedMessageLoop());

	if (data.allow_nested_message_loop)
		OP_DBG((UNI_L("Plugin [%d] possibly entering nested message loop to fetch property '%s'"), plugin->GetID(), data.name));
	bool retval = OpStatus::IsSuccess(syncif.GetSlot(data, &callback)) && callback.IsSuccess();

	plugin->LeaveSynchronousLoop();
	g_in_synchronous_loop--;

	g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

	if (result && result->type == NPVariantType_Object) // make sure the result will be GC'ed later when the plugin is deleted
	{
		if (!g_pluginscriptdata->FindObject(result->value.objectValue))
			retval = false;
	}
	else if (result && property_name && uni_str_eq(property_name, "URL") && OpStatus::IsError(ConvertLocalhost(result)))
		retval = false;

	return retval;
}

bool
NPN_SetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName, const NPVariant *value)
{
	OP_NEW_DBG("NPN_SetProperty", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return false;
	}

	OpNPObject* opnpobj = g_pluginscriptdata->FindObject(npobj);

	if (!opnpobj || !propertyName)
		return false;

	/* If the object we are to set a property on was created by the calling plug-in library,
	 * then we skip the detour through ES and call the plug-in library method directly. */
	OP_BOOLEAN status = opnpobj->SetProperty(propertyName, value);
	if (OpStatus::IsSuccess(status))
		return status == OpBoolean::IS_TRUE ? true : false;

	ES_Runtime *runtime;
	ES_Object *object;
	Plugin* plugin;

	if (!OpNPStartCall(npp, npobj, runtime, object, plugin))
		return false;

	ES_AsyncInterface *asyncif = GetAsyncIFFromRuntime(runtime);

	ES_SyncInterface syncif(runtime, asyncif);
	ES_SyncInterface::SlotData data;

	ES_Value internal_value;

	if (PluginImportValue(plugin, runtime, internal_value, *value) != OpBoolean::IS_TRUE)
		return false;

	data.allow_nested_message_loop = !g_input_manager->IsLockMouseInputContext() && g_pluginscriptdata->AllowNestedMessageLoop();
	BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	g_pluginscriptdata->SetAllowNestedMessageLoop(data.allow_nested_message_loop);

	data.object = object;
	data.name = ((OpNPIdentifier *) propertyName)->GetName();
	data.interrupt_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	OpNPSyncCallback callback(plugin, runtime);

	g_in_synchronous_loop++;
	plugin->EnterSynchronousLoop();
	OP_ASSERT(data.allow_nested_message_loop == g_pluginscriptdata->AllowNestedMessageLoop());
	if (data.allow_nested_message_loop)
		OP_DBG((UNI_L("Plugin [%d] possibly entering nested message loop to set property '%s'"), plugin->GetID(), data.name));

	bool success = OpStatus::IsSuccess(syncif.SetSlot(data, internal_value, &callback)) && callback.IsSuccess();

	plugin->LeaveSynchronousLoop();
	g_in_synchronous_loop--;

	g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

	PluginReleaseInternalValue(internal_value);
	return success;
}

bool
NPN_RemoveProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName)
{
	OP_NEW_DBG("NPN_RemoveProperty", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return false;
	}

	OpNPObject* opnpobj = g_pluginscriptdata->FindObject(npobj);

	if (!opnpobj || !propertyName)
		return false;

	/* If the object we are to remove a property from was created by the calling plug-in
	 * library, then we skip the detour through ES and call the plug-in library method directly. */
	OP_BOOLEAN status = opnpobj->RemoveProperty(propertyName);
	if (OpStatus::IsSuccess(status))
		return status == OpBoolean::IS_TRUE ? true : false;

	ES_Runtime *runtime;
	ES_Object *object;
	Plugin *plugin;

	if (!OpNPStartCall(npp, npobj, runtime, object, plugin))
		return false;

	ES_AsyncInterface *asyncif = GetAsyncIFFromRuntime(runtime);

	ES_SyncInterface syncif(runtime, asyncif);
	ES_SyncInterface::SlotData data;

	data.allow_nested_message_loop = !g_input_manager->IsLockMouseInputContext() && g_pluginscriptdata->AllowNestedMessageLoop();
	BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	g_pluginscriptdata->SetAllowNestedMessageLoop(data.allow_nested_message_loop);
	data.object = object;
	data.name = ((OpNPIdentifier *) propertyName)->GetName();
	data.interrupt_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	OpNPSyncCallback callback(plugin, runtime);

	g_in_synchronous_loop++;
	plugin->EnterSynchronousLoop();
	OP_ASSERT(data.allow_nested_message_loop == g_pluginscriptdata->AllowNestedMessageLoop());
	if (data.allow_nested_message_loop)
		OP_DBG((UNI_L("Plugin [%d] possibly entering nested message loop to delete property '%s'"), plugin->GetID(), data.name));

	bool retval = OpStatus::IsSuccess(syncif.RemoveSlot(data, &callback)) && callback.IsSuccess();

	plugin->LeaveSynchronousLoop();
	g_in_synchronous_loop--;

	g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

	return retval;
}

bool
NPN_HasProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName)
{
	OP_NEW_DBG("NPN_HasProperty", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return false;
	}

	OpNPObject* opnpobj = g_pluginscriptdata->FindObject(npobj);

	if (!opnpobj || !propertyName)
		return false;

	/* If the object we are to check a property of was created by the calling plug-in library,
	 * then we skip the detour through ES and call the plug-in library method directly. */
	OP_BOOLEAN status = opnpobj->HasProperty(propertyName);
	if (OpStatus::IsSuccess(status))
		return status == OpBoolean::IS_TRUE ? true : false;

	ES_Runtime *runtime;
	ES_Object *object;
	Plugin* plugin;

	if (!OpNPStartCall(npp, npobj, runtime, object, plugin))
		return false;

	/* workaround for bug #235193, Acrobat Reader expects plugin's dom element not to have "onfocus" property */
	const uni_char* property_name = ((OpNPIdentifier *) propertyName)->GetName();
	if (property_name && uni_strcmp(property_name, "onfocus") == 0 && npobj)
	{
		OpNPObject *opnpobject = g_pluginscriptdata->FindObject(npobj);
		if (opnpobject && opnpobject->GetInternal() == ((PluginHandler*)OpNS4PluginHandler::GetHandler())->GetScriptablePluginDOMElement(npp))
			return false;
	}

	ES_SyncInterface syncif(runtime, GetAsyncIFFromRuntime(runtime));
	ES_SyncInterface::SlotData data;

	data.allow_nested_message_loop = !g_input_manager->IsLockMouseInputContext() && g_pluginscriptdata->AllowNestedMessageLoop();
	BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	g_pluginscriptdata->SetAllowNestedMessageLoop(data.allow_nested_message_loop);
	data.object = object;
	data.name = property_name;
	data.interrupt_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	NPVariant result;
	OpNPSyncCallback callback(plugin, runtime, &result);

	g_in_synchronous_loop++;
	plugin->EnterSynchronousLoop();
	OP_ASSERT(data.allow_nested_message_loop == g_pluginscriptdata->AllowNestedMessageLoop());
	if (data.allow_nested_message_loop)
		OP_DBG((UNI_L("Plugin [%d] possibly entering nested message loop to check for property '%s'"), plugin->GetID(), data.name));

	bool retval = OpStatus::IsSuccess(syncif.HasSlot(data, &callback)) && callback.IsSuccess() && result.type == NPVariantType_Bool && result.value.boolValue;

	plugin->LeaveSynchronousLoop();
	g_in_synchronous_loop--;

	g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

	return retval;
}

bool
NPN_HasMethod(NPP npp, NPObject *npobj, NPIdentifier methodName)
{
	OP_NEW_DBG("NPN_HasMethod", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return false;
	}

	OpNPObject* opnpobj = g_pluginscriptdata->FindObject(npobj);

	if (!opnpobj || !methodName)
		return false;

	/* If the object we are to check a method of was created by the calling plug-in library,
	 * then we skip the detour through ES and call the plug-in library method directly. */
	OP_BOOLEAN status = opnpobj->HasMethod(methodName);
	if (OpStatus::IsSuccess(status))
		return status == OpBoolean::IS_TRUE ? true : false;

	ES_Runtime *runtime;
	ES_Object *object;
	Plugin *plugin;

	if (!OpNPStartCall(npp, npobj, runtime, object, plugin))
		return false;

	ES_AsyncInterface *asyncif = GetAsyncIFFromRuntime(runtime);

	ES_SyncInterface syncif(runtime, asyncif);
	ES_SyncInterface::SlotData data;

	data.allow_nested_message_loop = !g_input_manager->IsLockMouseInputContext() && g_pluginscriptdata->AllowNestedMessageLoop();
	BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	g_pluginscriptdata->SetAllowNestedMessageLoop(data.allow_nested_message_loop);
	data.object = object;
	data.name = ((OpNPIdentifier *) methodName)->GetName();
	data.interrupt_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	bool ismethod = false;

	OpNPSyncCallback callback(plugin, runtime, &ismethod);

	g_in_synchronous_loop++;
	plugin->EnterSynchronousLoop();
	OP_ASSERT(data.allow_nested_message_loop == g_pluginscriptdata->AllowNestedMessageLoop());

	if (data.allow_nested_message_loop)
		OP_DBG((UNI_L("Plugin [%d] possibly entering nested message loop to fetch property '%s'"), plugin->GetID(), data.name));
	bool retval = OpStatus::IsSuccess(syncif.GetSlot(data, &callback)) && callback.IsSuccess() && ismethod;

	plugin->LeaveSynchronousLoop();
	g_in_synchronous_loop--;

	g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

	return retval;
}

bool
NPN_Enumerate(NPP npp, NPObject *npobj, NPIdentifier **identifier, uint32_t *count)
{
	OP_NEW_DBG("NPN_Enumerate", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return false;
	}

	ES_Runtime *runtime;
	ES_Object *object;
	Plugin *plugin;

	if (!OpNPStartCall(npp, npobj, runtime, object, plugin))
		return false;

	OpAutoVector<OpString> names;

	if (OpStatus::IsError(runtime->GetPropertyNames(object, names)))
		return false;

	*count = names.GetCount();

	if (*count != 0)
	{
		*identifier = (NPIdentifier *) PluginMemoryHandler::GetHandler()->Malloc(sizeof(NPIdentifier) * *count);

		if (!*identifier)
		{
			*count = 0;
			return false;
		}

		for (unsigned index = 0; index < *count; ++index)
			if (!((*identifier)[index] = g_pluginscriptdata->GetStringIdentifier(names.Get(index)->CStr())))
			{
				PluginMemoryHandler::GetHandler()->Free(*identifier);
				*count = 0;
				*identifier = NULL;
				return false;
			}
	}

	return true;
}

extern "C" void
NPN_PluginThreadAsyncCall(NPP instance, void (*func)(void *), void *userData)
{
	// NOTE: This function must be threadsafe (it's called from any of a plugin's threads)
	// The only (?) method that is documented to work on the non main thread

	OP_NEW_DBG("NPN_PluginThreadAsyncCall", "ns4p");
	IN_CALL_FROM_PLUGIN  // This is not threadsafe but the consequences of an error are mild

	PluginAsyncCall* call = (PluginAsyncCall*)g_thread_tools->Allocate(sizeof(PluginAsyncCall));
	if (call)
	{
		call->function = func;
		call->userdata = userData;
		g_thread_tools->PostMessageToMainThread(MSG_PLUGIN_CALL_ASYNC, (MH_PARAM_1)instance, (MH_PARAM_2)call);
	}
	// If allocation fails we have to drop the message. The plugin will probably fail, but
	// there is nothing we can do (and the plugin would probably fail to OOM anyway).
	return;
}

bool
NPN_Construct(NPP npp, NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	OP_NEW_DBG("NPN_Construct", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return false;
	}

	ES_Runtime *runtime;
	ES_Object *object;
	Plugin *plugin;

	if (!OpNPStartCall(npp, npobj, runtime, object, plugin))
		return false;

	ES_AsyncInterface *asyncif = GetAsyncIFFromRuntime(runtime);

	ES_SyncInterface syncif(runtime, asyncif);
	ES_SyncInterface::EvalData data;
	OpNPSyncCallback callback(plugin, runtime, result);
	OpString constructor;
	BOOL saved_allow_nested_message_loop;
	bool success = false;

	// create temporary objects 'o' and 'a'
	EcmaScript_Object *o = OP_NEW(EcmaScript_Object, ());
	if (!o)
		return false;

	EcmaScript_Object *a = OP_NEW(EcmaScript_Object, ());
	if (!a)
		goto failed;

	if (OpStatus::IsError(o->SetObjectRuntime(runtime, NULL, "Object")))
		goto failed;
	if (OpStatus::IsError(o->Put(UNI_L("x"), object)))
		goto failed;

	if (OpStatus::IsError(a->SetObjectRuntime(runtime, NULL, "Object")))
		goto failed;
	if (OpStatus::IsError(o->Put(UNI_L("a"), *a)))
		goto failed;

	// create index values for 'a' and a temporary string representing the constructor statement
	uint32_t index;
	uni_char buf[16]; /* ARRAY OK 2009-04-21 hela */
	if (OpStatus::IsError(constructor.Set(UNI_L("new x("))))
		goto failed;
	for (index = 0; index < argCount; ++index)
	{
		ES_Value value;
		if (PluginImportValue(plugin, runtime, value, args[index]) != OpBoolean::IS_TRUE)
			goto failed;
		runtime->PutIndex(*a, index, value);
		PluginReleaseInternalValue(value);

		if (OpStatus::IsError(constructor.Append(UNI_L("a["))))
			goto failed;
		uni_itoa(index, buf, 10);
		if (OpStatus::IsError(constructor.Append(buf)))
			goto failed;
		if (OpStatus::IsError(constructor.Append(UNI_L("]"))))
			goto failed;
		if (index+1 < argCount)
		{
			if (OpStatus::IsError(constructor.Append(UNI_L(", "))))
				goto failed;
		}
	}
	if (OpStatus::IsError(constructor.Append(UNI_L(")"))))
		goto failed;

	// create the evaluation data with scope_chain containing 'o'
	data.allow_nested_message_loop = !g_input_manager->IsLockMouseInputContext() && g_pluginscriptdata->AllowNestedMessageLoop();
	saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	g_pluginscriptdata->SetAllowNestedMessageLoop(data.allow_nested_message_loop);
	ES_Object *scope_chain[1];
	scope_chain[0] = *o;
	data.scope_chain = scope_chain;
	data.scope_chain_length = 1;
	data.this_object = object;
	data.program = constructor.CStr();
	data.interrupt_thread = GetSchedulerFromRuntime(runtime)->GetCurrentThread();

	g_in_synchronous_loop++;
	plugin->EnterSynchronousLoop();
	OP_ASSERT(data.allow_nested_message_loop == g_pluginscriptdata->AllowNestedMessageLoop());

	if (data.allow_nested_message_loop)
		OP_DBG((UNI_L("Plugin [%d] possibly entering nested message loop to run Constructor"), plugin->GetID()));
	if (OpStatus::IsSuccess(syncif.Eval(data, &callback)))
		success = callback.IsSuccess();

	plugin->LeaveSynchronousLoop();
	g_in_synchronous_loop--;

	g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

	if (result && result->type == NPVariantType_Object) // make sure the result will be GC'ed later when the plugin is deleted
	{
		if (!g_pluginscriptdata->FindObject(result->value.objectValue))
			success = false;
	}
	else
		success = false;

failed:
	OP_DELETE(a);
	OP_DELETE(o);

	return success;
}

void
NPN_SetException(NPObject *npobj, const NPUTF8 *message)
{
	OP_NEW_DBG("NPN_SetException", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return;
	}

	if (OpNPObject *object = g_pluginscriptdata->FindObject(npobj))
	{
		OpString msg;
		msg.SetFromUTF8(message);
		if (OpStatus::IsSuccess(object->SetException(msg)))
			object->SetHasException(TRUE);

		object->GetPlugin()->SetScriptException(TRUE);
		OpStatus::Ignore(object->GetPlugin()->SetExceptionMessage(message));
	}
}

class PluginJSCallback : public ES_SyncInterface::Callback
{
public:
	Plugin* plugin;
	ES_Runtime *runtime;
	BOOL result;
	NPVariant res_value;

    OP_STATUS HandleCallback(ES_SyncInterface::Callback::Status status, const ES_Value &value)
	{
		if (status == ES_SyncInterface::Callback::ESSYNC_STATUS_SUCCESS)
		{
			result = TRUE;
			PluginExportValue(plugin, res_value, value, runtime);
		}
		else
			result = FALSE;
		return OpStatus::OK;
    }
};

BOOL SynchronouslyEvaluateJavascriptURL(Plugin *plugin, FramesDocument *frames_doc, const uni_char *script, NPVariant& result_value)
{
	OP_NEW_DBG("SynchronouslyEvaluateJavascriptURL", "ns4p");
	if (frames_doc && (frames_doc->GetESRuntime() || OpStatus::IsSuccess(frames_doc->ConstructDOMEnvironment())))
	{
		ES_AsyncInterface *asyncif = frames_doc->GetESAsyncInterface();

		asyncif->SetIsPluginRequested();

		ES_SyncInterface syncif(frames_doc->GetESRuntime(), asyncif);
		ES_SyncInterface::EvalData data;
		data.program = script;

		data.want_string_result = TRUE;

		PluginJSCallback cb;
		cb.plugin = plugin;

		g_in_synchronous_loop++;

		if (data.allow_nested_message_loop)
			OP_DBG((UNI_L("Possibly entering nested message loop to eval script '%.30s%s'"), data.program, uni_strlen(data.program) > 30 ? UNI_L("...") : UNI_L("")));
		OP_STATUS retval = syncif.Eval(data, &cb);

		g_in_synchronous_loop--;

		if (OpStatus::IsSuccess(retval))
		{
			if (cb.res_value.type == NPVariantType_String)
				result_value = cb.res_value;
			else
			{ // default value is set to an empty string if the scripting result is not a string
				PluginConvertToNPString(result_value.value.stringValue, UNI_L(""));
				result_value.type = NPVariantType_String;
			}
			return cb.result;
		}
	}
	return FALSE;
}

#endif // _PLUGIN_SUPPORT_
