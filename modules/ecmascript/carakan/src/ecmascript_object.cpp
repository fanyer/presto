/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995-2011
 */

#include "core/pch.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript/carakan/src/es_pch.h"

/* virtual */
EcmaScript_Object::~EcmaScript_Object()
{
	if (native_object)
	{
		// FIXME  Memory management.  Why is this Unprotect() needed?
		// Try uncommenting it when core-2 is more stable, or test this module in core-1
		// with the Unprotect removed.
		//GetRuntime()->Unprotect(native_object);
		ES_Runtime::DecoupleHostObject(native_object);
	}

	native_object=NULL;
}

/* virtual */ BOOL
EcmaScript_Object::IsA(int tag)
{
    return FALSE;
}

OP_STATUS
EcmaScript_Object::Put(const uni_char* property_name, ES_Object* object, BOOL read_only)
{
	if (!native_object)
		return OpStatus::OK;

	ES_Value value;

	value.type = VALUE_OBJECT;
	value.value.object = object;

	const int roflag = PROP_READ_ONLY;
	return GetRuntime()->PutName(native_object, const_cast<uni_char*>(property_name), value, read_only ? roflag : 0);
}

OP_STATUS
EcmaScript_Object::Put(const uni_char* property_name, const ES_Value &value, BOOL read_only)
{
	if (!native_object)
		return OpStatus::OK;

	const int roflag = PROP_READ_ONLY;
	return GetRuntime()->PutName(native_object, const_cast<uni_char*>(property_name), value, read_only ? roflag : 0);
}

void
EcmaScript_Object::PutL(const uni_char* property_name, ES_Object* object, BOOL read_only)
{
	LEAVE_IF_ERROR(Put(property_name,object,read_only));
}

OP_BOOLEAN
EcmaScript_Object::Get(const uni_char* property_name, ES_Value* value)
{
	if (native_object)
	{
#ifndef _STANDALONE
		return GetRuntime()->GetName(native_object, const_cast<uni_char*>(property_name), value);
#endif
	}
	else if (value)
		value->type = VALUE_UNDEFINED;
	return OpBoolean::IS_FALSE;
}

OP_STATUS
EcmaScript_Object::Delete(const uni_char* property_name)
{
	if (!native_object)
		return OpStatus::OK;

	return GetRuntime()->DeleteName(native_object, const_cast<uni_char*>(property_name));
}

void
EcmaScript_Object::ConnectObject(ES_Runtime* runtime0, ES_Object* native_object0)
{
	ES_Host_Object *host_object = static_cast<ES_Host_Object *>(native_object0);

	OP_ASSERT(host_object && host_object->GetHostObject() == NULL);

	runtime = runtime0;
	native_object = native_object0;

	host_object->SetHostObject(this);
}

OP_STATUS
EcmaScript_Object::SetObjectRuntime(ES_Runtime* es_runtime, ES_Object* prototype, const char* object_class, BOOL dummy)
{
	runtime = es_runtime;

	if (native_object)
		OP_ASSERT(!"Not implemented");
	else
		native_object = es_runtime->CreateHostObjectWrapper(this, prototype, object_class);

	return native_object != NULL ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
EcmaScript_Object::SetFunctionRuntime(ES_Runtime* es_runtime, ES_Object* prototype, const char* object_class8, const char* parameter_types)
{
	runtime = es_runtime;

	if (native_object)
		OP_ASSERT(!"Not implemented");
	else
	{
#ifndef _STANDALONE
		const uni_char *object_class16 = make_doublebyte_in_tempbuffer(object_class8, op_strlen(object_class8));
#else // _STANDALONE
		uni_char object_class16[256]; // ARRAY OK 2011-01-11 eddy
		if (op_strlen(object_class8) < 256)
		{
			uni_char *dst = object_class16;
			const char *src = object_class8;
			while (*dst = *src);
		}
		else
			return OpStatus::ERR;
#endif // _STANDALONE

		native_object = es_runtime->CreateHostFunctionWrapper(this, prototype, object_class16, object_class8, parameter_types);
	}

	return native_object != NULL ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
EcmaScript_Object::SetFunctionRuntime(ES_Runtime* es_runtime, ES_Object* prototype, const uni_char *function_name, const char* object_class, const char* parameter_types)
{
    runtime = es_runtime;

    if (native_object)
        OP_ASSERT(!"Not implemented");
    else
        native_object = es_runtime->CreateHostFunctionWrapper(this, prototype, function_name, object_class, parameter_types);

    return native_object != NULL ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
EcmaScript_Object::SetFunctionRuntime(ES_Runtime* es_runtime, const uni_char* function_name, const char* object_class, const char* parameter_types)
{
	return SetFunctionRuntime(es_runtime, es_runtime->GetFunctionPrototype(), function_name, object_class, parameter_types);
}

void
EcmaScript_Object::SetFunctionRuntimeL(ES_Runtime* es_runtime, const uni_char* function_name, const char *object_class, const char* parameter_types)
{
	LEAVE_IF_ERROR(SetFunctionRuntime(es_runtime, function_name, object_class, parameter_types));
}

void
EcmaScript_Object::ChangeRuntime(ES_Runtime* es_runtime)
{
    OP_ASSERT(GetRuntime() != NULL);

    runtime = es_runtime;

    // Since there was already a runtime, the property set by RegisterRuntimeOn
    // already exists, and this call should not need to allocate anything.
    // Anyway, using this function to safely move a cluster of objects from one
    // runtime to another depends on this not failing half-way.

    OP_STATUS status = es_runtime->RegisterRuntimeOn(this);
    OpStatus::Ignore(status);

    OP_ASSERT(status == OpStatus::OK);
}

void
EcmaScript_Object::SetHasMultipleIdentities()
{
	GCLOCK(context, (runtime));
	native_object->SetHasMultipleIdentities(&context);
}

void
EcmaScript_Object::SignalIdentityChange(ES_Object *previous_identity)
{
	static_cast<ES_Host_Object *>(native_object)->SignalIdentityChange(previous_identity);
}

void
EcmaScript_Object::SignalDisableCaches(ES_Object *current_identity)
{
	static_cast<ES_Host_Object *>(native_object)->SignalDisableCaches(current_identity);
}

void
EcmaScript_Object::SetHasVolatilePropertySet()
{
	native_object->SetHasVolatilePropertySet();
}

BOOL
EcmaScript_Object::HasVolatilePropertySet()
{
	return native_object->HasVolatilePropertySet();
}

void
EcmaScript_Object::SignalPropertySetChanged()
{
    native_object->SignalPropertySetChanged();
}

/* virtual */ OP_STATUS
EcmaScript_Object::Identity(ES_Object **loc)
{
	*loc = native_object;
	return OpStatus::OK;
}

/* virtual */ void
EcmaScript_Object::GCTrace()
{
	// nothing
}

/* virtual */ BOOL
EcmaScript_Object::SecurityCheck(ES_Runtime* origining_runtime)
{
	// Very strict: only access to same runtime allowed, domains are ignored.
	// Subclasses can loosen this, and DOM_Object normally will.
	return GetRuntime() == origining_runtime;
}

/*-- Default {Get,Put}{Index,Name} functions that does nothing: --*/

/* virtual */ ES_GetState
EcmaScript_Object::GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	return GET_FAILED;
}

/* virtual */ ES_GetState
EcmaScript_Object::GetIndexRestart(int property_index, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	return GET_FAILED;
}

/* virtual */ ES_GetState
EcmaScript_Object::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	return GET_FAILED;
}

/* virtual */ ES_GetState
EcmaScript_Object::GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	return GET_FAILED;
}

/* virtual */ ES_PutState
EcmaScript_Object::PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	return PUT_FAILED;
}

/* virtual */ ES_PutState
EcmaScript_Object::PutIndexRestart(int property_index, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	return PUT_FAILED;
}

/* virtual */ ES_PutState
EcmaScript_Object::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	return PUT_FAILED;
}

/* virtual */ ES_PutState
EcmaScript_Object::PutNameRestart(const uni_char*, int property_code, ES_Value*, ES_Runtime*, ES_Object*)
{
	return PUT_FAILED;
}

/* virtual */ ES_DeleteStatus
EcmaScript_Object::DeleteIndex(int, ES_Runtime*)
{
	return DELETE_OK;
}

/* virtual */ ES_DeleteStatus
EcmaScript_Object::DeleteName(const uni_char*, ES_Runtime*)
{
	return DELETE_OK;
}

/* virtual */ BOOL
EcmaScript_Object::AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op)
{
	return TRUE;
}

/* virtual */ int
EcmaScript_Object::Construct(ES_Value*, int, ES_Value*, ES_Runtime*)
{
	return 0;
}

/* virtual */ int
EcmaScript_Object::Call(ES_Object*, ES_Value*, int, ES_Value*, ES_Runtime*)
{
	return 0;
}

/* virtual */ ES_GetState
EcmaScript_Object::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
EcmaScript_Object::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = 0;
	return GET_SUCCESS;
}

/* virtual */ BOOL
EcmaScript_Object::TypeofYieldsObject()
{
    return FALSE;
}

ES_GetState
EcmaScript_Object::HasPropertyIndex(unsigned property_index, BOOL is_restarted, ES_Runtime* origining_runtime)
{
	ES_GetState result = !is_restarted ? GetIndex(property_index, NULL, origining_runtime) : GetIndexRestart(property_index, NULL, origining_runtime, NULL);
	OP_ASSERT(result != GET_EXCEPTION || !"Returning non-security exceptions from [[HasProperty]] not supported.");
	return result;
}

ES_GetState
EcmaScript_Object::HasPropertyName(const uni_char* property_name, int property_code, BOOL is_restarted, ES_Runtime* origining_runtime)
{
	ES_GetState result = !is_restarted ? GetName(property_name, property_code, NULL, origining_runtime) : GetNameRestart(property_name, property_code, NULL, origining_runtime, NULL);
	OP_ASSERT(result != GET_EXCEPTION || !"Returning non-security exceptions from [[HasProperty]] not supported.");
	return result;
}

OP_BOOLEAN
EcmaScript_Object::GetPrivate(int private_name, ES_Value* value)
{
	if (native_object)
		return GetRuntime()->GetPrivate(native_object,private_name,value);
	else
		return OpBoolean::IS_FALSE;
}

OP_STATUS
EcmaScript_Object::PutPrivate(int private_name, ES_Value& value)
{
	if (native_object)
		return GetRuntime()->PutPrivate(native_object,private_name,value);
	else
		return OpStatus::OK;
}

OP_STATUS
EcmaScript_Object::PutPrivate(int private_name, ES_Object* object)
{
	if (native_object)
	{
		ES_Value val;

		val.type = VALUE_OBJECT;
		val.value.object = object;
		return GetRuntime()->PutPrivate(native_object,private_name,val);
	}
	else
		return OpStatus::OK;
}

OP_STATUS
EcmaScript_Object::DeletePrivate(int private_name)
{
	if (native_object)
		return GetRuntime()->DeletePrivate(native_object,private_name);
	else
		return OpStatus::OK;
}
