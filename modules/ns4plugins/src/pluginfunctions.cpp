/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/ns4plugins/src/pluginfunctions.h"
#include "modules/ns4plugins/src/pluginlib.h"
#include "modules/ns4plugins/src/pluginhandler.h"
#include "modules/ns4plugins/src/plugininstance.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"


/* static */ NPError
PluginFunctions::NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved)
{
	PluginInstance* pi = OP_NEW(PluginInstance, (instance));
	RETURN_VALUE_IF_NULL(pi, NPERR_OUT_OF_MEMORY_ERROR);

	/* The call to New() below will call plug-in functions, which may again call browser functions
	 * resulting in a call to another of the NPP-wrappers defined in this file. Those wrappers use
	 * GetPluginInstance() and therefore require instance->pdata to be correctly initialized. */
	instance->pdata = pi;

	NPError err = pi->New(pluginType, mode, argc, argn, argv, saved);

	if (err != NPERR_NO_ERROR)
	{
		OP_DELETE(pi);
		return err;
	}

	return NPERR_NO_ERROR;
}

/* static */ NPError
PluginFunctions::NPP_Destroy(NPP instance, NPSavedData** save)
{
	PluginInstance* pi = GetPluginInstance(instance);

	NPError err = pi->Destroy(save);

	OP_DELETE(pi);

	return err;
}

/* static */ NPError
PluginFunctions::NPP_SetWindow(NPP instance, NPWindow* window)
{
	OP_ASSERT(!"This method should not be called. Use PluginFunctions::SetWindow.");
	return NPERR_GENERIC_ERROR;
}

/* static */ NPError
PluginFunctions::SetWindow(NPP instance, NPWindow* window, OpChannel* adapter_channel)
{
	return GetPluginInstance(instance)->SetWindow(window, adapter_channel);
}

/* static */ NPError
PluginFunctions::NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype)
{
	return GetPluginInstance(instance)->NewStream(type, stream, seekable, stype);
}

/* static */ NPError
PluginFunctions::NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
	return GetPluginInstance(instance)->DestroyStream(stream, reason);
}

/* static */ int32
PluginFunctions::NPP_WriteReady(NPP instance, NPStream* stream)
{
	return GetPluginInstance(instance)->WriteReady(stream);
}

/* static */ int32
PluginFunctions::NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer)
{
	return GetPluginInstance(instance)->Write(stream, offset, len, buffer);
}

/* static */ void
PluginFunctions::NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
	GetPluginInstance(instance)->StreamAsFile(stream, fname);
}

/* static */ void
PluginFunctions::NPP_Print(NPP instance, NPPrint* platformPrint)
{
	OP_ASSERT(!"Implement me.");
}

/* static */ int16
PluginFunctions::NPP_HandleEvent(NPP instance, void* event)
{
	OP_ASSERT(!"This function should never be called, see source comment.");
	/* If this function is called the calling code should be changed to call a event
	 * specific function called HandleBlahEvent() where 'Blah' is the event type.
	 * The reason why this function exists is because we wanted PluginFunctions
	 * to be a complete set of NPP functions. The reason why it cannot be implemented
	 * is because it's impossible for core to serialize and send the "void*" event
	 * over the IPC wire.
	 */
	return 0;
}

/* static */ void
PluginFunctions::NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
	return GetPluginInstance(instance)->URLNotify(url, reason, notifyData);
}

/* static */ NPError
PluginFunctions::NPP_GetValue(NPP instance, NPPVariable variable, void *ret_value)
{
	return GetPluginInstance(instance)->GetValue(variable, ret_value);
}

/* static */ NPError
PluginFunctions::NPP_SetValue(NPP instance, NPNVariable variable, void *ret_value)
{
	OP_ASSERT(!"Implement me.");
	return NPERR_GENERIC_ERROR;
}

/* static */ NPBool
PluginFunctions::NPP_GotFocus(NPP instance, NPFocusDirection direction)
{
	OP_ASSERT(!"Implement me.");
	return false;
}

/* static */ void
PluginFunctions::NPP_LostFocus(NPP instance)
{
	OP_ASSERT(!"Implement me.");
}

/* static */ void
PluginFunctions::NPP_URLRedirectNotify(NPP instance, const char* url, int32_t status, void* notifyData)
{
	OP_ASSERT(!"Implement me.");
}

/* static */ NPError
PluginFunctions::NPP_ClearSiteData(const char* site, uint64_t flags, uint64_t maxAge)
{
	OP_ASSERT(!"Implemented elsewhere, see PluginComponent::OnClearSiteData");
	return NPERR_GENERIC_ERROR;
}

/* static */ char**
PluginFunctions::NPP_GetSitesWithData()
{
	OP_ASSERT(!"Implemented elsewhere, see PluginComponent::OnGetSitesWithData");
	return NULL;
}

/**
 * Annotated version of NPObject.
 *
 * Facilitates simple library lookup.
 */
struct AnnotatedNPObject
	: public NPObject
{
	PluginLib* library;
};

/* static */ NPObject*
PluginFunctions::NPO_Allocate(NPP npp, NPClass* aClass)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(NPN_MemAlloc(sizeof(*object)));
	if (object)
		object->library = GetPluginInstance(npp)->GetLibrary();

	return object;
}

/* static */ void
PluginFunctions::NPO_Deallocate(NPObject* npobj)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	if (!object->library->GetComponent())
		return;

	OpPluginObjectDeallocateMessage* message = OpPluginObjectDeallocateMessage::Create();
	if (!message)
		return;
	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);

	object->library->GetComponent()->SendMessage(message);

	NPN_MemFree(object);
}

/* static */ void
PluginFunctions::NPO_Invalidate(NPObject* npobj)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	if (!object->library->GetComponent())
		return;

	OpPluginObjectInvalidateMessage* message = OpPluginObjectInvalidateMessage::Create();
	if (!message)
		return;
	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);

	object->library->GetComponent()->SendMessage(message);
}

/* static */ bool
PluginFunctions::NPO_HasMethod(NPObject* npobj, NPIdentifier name)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	RETURN_VALUE_IF_NULL(object->library->GetComponent(), false);

	OpPluginHasMethodMessage* message = OpPluginHasMethodMessage::Create();
	RETURN_VALUE_IF_NULL(message, false);

	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);
	SetPluginIdentifier(message->GetMethodRef(), name);

	PluginSyncMessenger sync(g_opera->CreateChannel(object->library->GetComponent()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	return !!response->GetSuccess();
}

/* static */ bool
PluginFunctions::NPO_Invoke(NPObject* npobj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	RETURN_VALUE_IF_NULL(object->library->GetComponent(), false);

	OpPluginInvokeMessage* message = OpPluginInvokeMessage::Create();
	RETURN_VALUE_IF_NULL(message, false);

	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);
	SetPluginIdentifier(message->GetMethodRef(), name);

	for (unsigned int i = 0; i < argCount; i++)
	{
		PluginInstance::PluginVariant* variant = OP_NEW(PluginInstance::PluginVariant, ());
		if (!variant
		    || OpStatus::IsError(message->GetArgumentsRef().Add(variant))
		    || OpStatus::IsError(PluginInstance::SetPluginVariant(*variant, args[i])))
		{
			OP_DELETE(variant);
			OP_DELETE(message);
			return false;
		}
	}

	PluginSyncMessenger sync(g_opera->CreateChannel(object->library->GetComponent()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	if (!response->GetSuccess())
		return false;

	if (result && (!response->HasResult() ||
				   OpStatus::IsError(PluginInstance::SetNPVariant(result, *response->GetResult()))))
		return false;

	return true;
}

/* static */ bool
PluginFunctions::NPO_InvokeDefault(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	RETURN_VALUE_IF_NULL(object->library->GetComponent(), false);

	OpPluginInvokeDefaultMessage* message = OpPluginInvokeDefaultMessage::Create();
	RETURN_VALUE_IF_NULL(message, false);

	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);

	for (unsigned int i = 0; i < argCount; i++)
	{
		PluginInstance::PluginVariant* variant = OP_NEW(PluginInstance::PluginVariant, ());
		if (!variant
		    || OpStatus::IsError(message->GetArgumentsRef().Add(variant))
		    || OpStatus::IsError(PluginInstance::SetPluginVariant(*variant, args[i])))
		{
			OP_DELETE(variant);
			OP_DELETE(message);
			return false;
		}
	}

	PluginSyncMessenger sync(g_opera->CreateChannel(object->library->GetComponent()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	if (!response->GetSuccess())
		return false;

	if (result && (!response->HasResult() ||
				   OpStatus::IsError(PluginInstance::SetNPVariant(result, *response->GetResult()))))
		return false;

	return true;
}

/* static */ bool
PluginFunctions::NPO_HasProperty(NPObject* npobj, NPIdentifier name)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	RETURN_VALUE_IF_NULL(object->library->GetComponent(), false);

	OpPluginHasPropertyMessage* message = OpPluginHasPropertyMessage::Create();
	RETURN_VALUE_IF_NULL(message, false);

	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);
	SetPluginIdentifier(message->GetPropertyRef(), name);

	PluginSyncMessenger sync(g_opera->CreateChannel(object->library->GetComponent()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	return !!response->GetSuccess();
}

/* static */ bool
PluginFunctions::NPO_GetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	RETURN_VALUE_IF_NULL(object->library->GetComponent(), false);

	OpPluginGetPropertyMessage* message = OpPluginGetPropertyMessage::Create();
	RETURN_VALUE_IF_NULL(message, false);

	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);
	SetPluginIdentifier(message->GetPropertyRef(), name);

	PluginSyncMessenger sync(g_opera->CreateChannel(object->library->GetComponent()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	if (!response->GetSuccess())
		return false;

	if (result && (!response->HasResult() ||
				   OpStatus::IsError(PluginInstance::SetNPVariant(result, *response->GetResult()))))
		return false;

	return true;
}

/* static */ bool
PluginFunctions::NPO_SetProperty(NPObject* npobj, NPIdentifier name, const NPVariant* value)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	RETURN_VALUE_IF_NULL(object->library->GetComponent(), false);

	OpAutoPtr<OpPluginSetPropertyMessage> message(OpPluginSetPropertyMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), false);

	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);
	SetPluginIdentifier(message->GetPropertyRef(), name);

	RETURN_VALUE_IF_ERROR(PluginInstance::SetPluginVariant(message->GetValueRef(), *value), false);

	PluginSyncMessenger sync(g_opera->CreateChannel(object->library->GetComponent()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	return !!response->GetSuccess();
}

/* static */ bool
PluginFunctions::NPO_RemoveProperty(NPObject* npobj, NPIdentifier name)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	RETURN_VALUE_IF_NULL(object->library->GetComponent(), false);

	OpPluginRemovePropertyMessage* message = OpPluginRemovePropertyMessage::Create();
	RETURN_VALUE_IF_NULL(message, false);

	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);
	SetPluginIdentifier(message->GetPropertyRef(), name);

	PluginSyncMessenger sync(g_opera->CreateChannel(object->library->GetComponent()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	return !!response->GetSuccess();
}

/* static */ bool
PluginFunctions::NPO_Enumerate(NPObject* npobj,  NPIdentifier** identifier, uint32_t* count)
{
	if (!identifier || !count)
		return false;

	*count = 0;

	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	RETURN_VALUE_IF_NULL(object->library->GetComponent(), false);

	OpPluginObjectEnumerateMessage* message = OpPluginObjectEnumerateMessage::Create();
	RETURN_VALUE_IF_NULL(message, false);

	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);

	PluginSyncMessenger sync(g_opera->CreateChannel(object->library->GetComponent()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), false);

	OpPluginObjectEnumerateResponseMessage* response = OpPluginObjectEnumerateResponseMessage::Cast(sync.GetResponse());
	if (!response->GetSuccess())
		return false;

	*identifier = static_cast<NPIdentifier*>(NPN_MemAlloc(sizeof(NPIdentifier) * response->GetProperties().GetCount()));
	RETURN_VALUE_IF_NULL(*identifier, false);
	*count = response->GetProperties().GetCount();

	for (unsigned int i = 0; i < *count; i++)
		(*identifier)[i] = reinterpret_cast<NPIdentifier*>(response->GetProperties().Get(i)->GetIdentifier());

	return true;
}

/* static */ bool
PluginFunctions::NPO_Construct(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
	AnnotatedNPObject* object = static_cast<AnnotatedNPObject*>(npobj);
	RETURN_VALUE_IF_NULL(object->library->GetComponent(), false);

	OpPluginObjectConstructMessage* message = OpPluginObjectConstructMessage::Create();
	RETURN_VALUE_IF_NULL(message, false);

	PluginInstance::SetPluginObject(message->GetObjectRef(), npobj);

	for (unsigned int i = 0; i < argCount; i++)
	{
		PluginInstance::PluginVariant* variant = OP_NEW(PluginInstance::PluginVariant, ());
		if (!variant
		    || OpStatus::IsError(message->GetArgumentsRef().Add(variant))
		    || OpStatus::IsError(PluginInstance::SetPluginVariant(*variant, args[i])))
		{
			OP_DELETE(variant);
			OP_DELETE(message);
			return false;
		}
	}

	PluginSyncMessenger sync(g_opera->CreateChannel(object->library->GetComponent()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	if (!response->GetSuccess())
		return false;

	if (result && (!response->HasResult() ||
				   OpStatus::IsError(PluginInstance::SetNPVariant(result, *response->GetResult()))))
		return false;

	return true;
}

/* static */ void
PluginFunctions::SetPluginIdentifier(PluginIdentifier& out_identifier, NPIdentifier identifier)
{
	out_identifier.SetIdentifier(reinterpret_cast<UINT64>(identifier));
	out_identifier.SetIsString(NPN_IdentifierIsString(identifier));

	if (!out_identifier.GetIsString())
		out_identifier.SetIntValue(NPN_IntFromIdentifier(identifier));
}

/* static */ void
PluginFunctions::InitializeFunctionTable(NPPluginFuncs* plugin_functions)
{
	op_memset(plugin_functions, 0, sizeof(*plugin_functions));
	plugin_functions->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;

	plugin_functions->newp = NPP_New;
	plugin_functions->destroy = NPP_Destroy;
	plugin_functions->setwindow = NPP_SetWindow;
	plugin_functions->newstream = NPP_NewStream;
	plugin_functions->destroystream = NPP_DestroyStream;
	plugin_functions->asfile = NPP_StreamAsFile;
	plugin_functions->writeready = NPP_WriteReady;
	plugin_functions->write = NPP_Write;
	plugin_functions->print = NPP_Print;
	plugin_functions->event = NPP_HandleEvent;
	plugin_functions->urlnotify = NPP_URLNotify;
	plugin_functions->getvalue = NPP_GetValue;
	plugin_functions->setvalue = NPP_SetValue;
	plugin_functions->gotfocus = NPP_GotFocus;
	plugin_functions->lostfocus = NPP_LostFocus;
	plugin_functions->urlredirectnotify = NPP_URLRedirectNotify;
	plugin_functions->clearsitedata = NPP_ClearSiteData;
	plugin_functions->getsiteswithdata = NPP_GetSitesWithData;
}

/* static */ void
PluginFunctions::InitializeClass(NPClass* npclass)
{
	op_memset(npclass, 0, sizeof(*npclass));
	npclass->structVersion = NP_CLASS_STRUCT_VERSION;

	npclass->allocate = NPO_Allocate;
	npclass->deallocate = NPO_Deallocate;
	npclass->invalidate = NPO_Invalidate;
	npclass->hasMethod = NPO_HasMethod;
	npclass->invoke = NPO_Invoke;
	npclass->invokeDefault = NPO_InvokeDefault;
	npclass->hasProperty = NPO_HasProperty;
	npclass->getProperty = NPO_GetProperty;
	npclass->setProperty = NPO_SetProperty;
	npclass->removeProperty = NPO_RemoveProperty;
	npclass->enumerate = NPO_Enumerate;
	npclass->construct = NPO_Construct;
}

/* static */ OP_STATUS
PluginFunctions::HandleWindowlessPaint(NPP npp, OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect, bool transparent, OpWindow* parent_opwindow)
{
	return GetPluginInstance(npp)->HandleWindowlessPaint(painter, plugin_rect, paint_rect, transparent, parent_opwindow);
}

/* static */ OP_STATUS
PluginFunctions::HandleWindowlessMouseEvent(NPP npp, OpPluginEventType event_type, const OpPoint& point, int button_or_key_or_delta, ShiftKeyState modifiers, bool* ret)
{
	return GetPluginInstance(npp)->HandleWindowlessMouseEvent(event_type, point, button_or_key_or_delta, modifiers, ret);
}

/* static */ OP_STATUS
PluginFunctions::HandleWindowlessKeyEvent(NPP npp, const OpKey::Code key, const uni_char *key_value, const OpPluginKeyState key_state,
										  const ShiftKeyState modifiers, UINT64 platform_data_1, UINT64 platform_data_2, INT32* is_event_handled)
{
	return GetPluginInstance(npp)->HandleWindowlessKeyEvent(key, key_value, key_state, modifiers, platform_data_1, platform_data_2, is_event_handled);
}

/* static */ OP_STATUS
PluginFunctions::HandleFocusEvent(NPP npp, bool focus, FOCUS_REASON reason, bool* is_event_handled)
{
	return GetPluginInstance(npp)->HandleFocusEvent(focus, reason, is_event_handled);
}

/* static */ PluginInstance*
PluginFunctions::GetPluginInstance(NPP instance)
{
	PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata);
	OP_ASSERT(pi);

	return pi;
}

#endif // NS4P_COMPONENT_PLUGINS
