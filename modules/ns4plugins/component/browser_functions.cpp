/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/ns4plugins/component/browser_functions.h"
#include "modules/ns4plugins/component/plugin_component.h"
#include "modules/ns4plugins/component/plugin_component_instance.h"
#include "modules/ns4plugins/component/plugin_component_helpers.h"
#include "modules/hardcore/component/OpSyncMessenger.h"
#include "modules/opdata/UniStringUTF8.h"

/* static */ NPError
BrowserFunctions::NPN_GetValue(NPP npp, NPNVariable variable, void* ret_value)
{
	RETURN_VALUE_IF_NULL(ret_value, NPERR_INVALID_PARAM);

	/* Give the plug-in PI a chance to handle the request first. */
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	NPError error = NPERR_NO_ERROR;
	if (pci.get() && pci->GetTranslator() && pci->GetTranslator()->GetValue(variable, ret_value, &error))
		return error;
	else if (OpPluginTranslator::GetGlobalValue(variable, ret_value, &error))
		return error;

	/* Look up remote entity, be it PluginLib or PluginInstance. */
	PluginComponent* component = NULL;
	OpChannel* remote = NULL;
	if (pci.get())
	{
		component = pci->GetComponent();
		remote = pci->GetChannel();
	}
	else if (!npp)
	{
		component = PluginComponent::FindPluginComponent();
		remote = component->GetChannel();
	}
	if (!component || !remote->IsDirected())
		return NPERR_INVALID_PLUGIN_ERROR;

	/* Send request to remote entity. */
	OpSyncMessenger sync(component->CreateChannel(remote->GetDestination()));
	RETURN_NPERR_IF_ERROR(sync.SendMessage(OpPluginGetValueMessage::Create(variable)));

	/* Interpret response. */
	OpPluginGetValueResponseMessage* response = OpPluginGetValueResponseMessage::Cast(sync.GetResponse());
	if (response->GetNpError() != NPERR_NO_ERROR)
		return response->GetNpError();

	if (response->HasObjectValue())
	{
		NPObject* object = component->Bind(*response->GetObjectValue());
		*static_cast<NPObject**>(ret_value) = object;
	}
	else if (response->HasStringValue())
	{
		NPString nps;
		RETURN_VALUE_IF_ERROR(PluginComponentHelpers::SetNPString(&nps, response->GetStringValue()), NPERR_GENERIC_ERROR);
		*static_cast<NPUTF8**>(ret_value) = const_cast<NPUTF8*>(nps.UTF8Characters);
	}
	else if (response->HasIntegerValue())
		*static_cast<int*>(ret_value) = response->GetIntegerValue();
	else if (response->HasBooleanValue())
		*static_cast<NPBool*>(ret_value) = response->GetBooleanValue();

	return NPERR_NO_ERROR;
}

/* static */ NPError
BrowserFunctions::NPN_SetValue(NPP npp, NPPVariable variable, void* value)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return NPERR_INVALID_PLUGIN_ERROR;

	/* Give the plug-in PI a chance to handle the request first. */
	NPError error = NPERR_NO_ERROR;
	if (pci->GetTranslator() && pci->GetTranslator()->SetValue(variable, value, &error))
		return error;

	/* Convert value. */
	OpAutoPtr<OpPluginSetValueMessage> message(OpPluginSetValueMessage::Create(variable));
	RETURN_VALUE_IF_NULL(message.get(), NPERR_OUT_OF_MEMORY_ERROR);

	if (PluginComponentHelpers::IsBooleanValue(variable))
		message->SetBooleanValue(value != NULL);
	else if (PluginComponentHelpers::IsIntegerValue(variable))
		message->SetIntegerValue(*reinterpret_cast<int*>(&value));
	else if (PluginComponentHelpers::IsObjectValue(variable))
	{
		UINT64 obj = pci->GetComponent()->Lookup(*static_cast<NPObject**>(value));
		if (obj == 0)
			return NPERR_INVALID_PARAM;

		PluginComponent::PluginObject* object = PluginComponentHelpers::ToPluginObject(obj);
		RETURN_VALUE_IF_NULL(object, NPERR_OUT_OF_MEMORY_ERROR);

		message->SetObjectValue(object);
	}
	else
	{
		/* Unknown variable. */
		return NPERR_GENERIC_ERROR;
	}

	/* Send message. */
	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_NPERR_IF_ERROR(sync.SendMessage(message.release()));

	return OpPluginErrorMessage::Cast(sync.GetResponse())->GetNpError();
}

/* static */ NPError
BrowserFunctions::NPN_GetURLNotify(NPP npp, const char* url, const char* window, void* notifyData)
{
	RETURN_VALUE_IF_NULL(url, NPERR_INVALID_URL);

	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return NPERR_INVALID_PLUGIN_ERROR;

	OpAutoPtr<OpPluginGetURLNotifyMessage> message(OpPluginGetURLNotifyMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), NPERR_OUT_OF_MEMORY_ERROR);

	RETURN_VALUE_IF_ERROR(message->SetURL(url, op_strlen(url)), NPERR_OUT_OF_MEMORY_ERROR);

	if (window)
		RETURN_VALUE_IF_ERROR(message->SetWindow(window, op_strlen(window)), NPERR_OUT_OF_MEMORY_ERROR);

	message->SetNotifyData(static_cast<UINT64>(reinterpret_cast<UINTPTR>(notifyData)));

	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), NPERR_GENERIC_ERROR);

	return OpPluginErrorMessage::Cast(sync.GetResponse())->GetNpError();
}

/* static */ NPError
BrowserFunctions::NPN_PostURLNotify(NPP npp, const char* url, const char* window, uint32_t len, const char* buf, NPBool file, void* notifyData)
{
	RETURN_VALUE_IF_NULL(url, NPERR_INVALID_URL);

	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return NPERR_INVALID_PLUGIN_ERROR;

	OpAutoPtr<OpPluginPostURLNotifyMessage> message(OpPluginPostURLNotifyMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), NPERR_OUT_OF_MEMORY_ERROR);

	RETURN_VALUE_IF_ERROR(message->SetURL(url, op_strlen(url)), NPERR_OUT_OF_MEMORY_ERROR);

	if (window)
		RETURN_VALUE_IF_ERROR(message->SetWindow(window, op_strlen(window)), NPERR_OUT_OF_MEMORY_ERROR);

	RETURN_VALUE_IF_ERROR(message->SetData(buf, len) , NPERR_OUT_OF_MEMORY_ERROR);
	message->SetIsFile(file);
	message->SetNotifyData(reinterpret_cast<UINT64>(notifyData));

	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), NPERR_GENERIC_ERROR);

	return OpPluginErrorMessage::Cast(sync.GetResponse())->GetNpError();
}

/* static */ NPError
BrowserFunctions::NPN_GetURL(NPP npp, const char* url, const char* window)
{
	RETURN_VALUE_IF_NULL(url, NPERR_INVALID_URL);

	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return NPERR_INVALID_PLUGIN_ERROR;

	OpAutoPtr<OpPluginGetURLMessage> message(OpPluginGetURLMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), NPERR_OUT_OF_MEMORY_ERROR);

	RETURN_VALUE_IF_ERROR(message->SetURL(url, op_strlen(url)), NPERR_OUT_OF_MEMORY_ERROR);

	if (window)
		RETURN_VALUE_IF_ERROR(message->SetWindow(window, op_strlen(window)), NPERR_OUT_OF_MEMORY_ERROR);

	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), NPERR_GENERIC_ERROR);

	return OpPluginErrorMessage::Cast(sync.GetResponse())->GetNpError();
}

/* static */ NPError
BrowserFunctions::NPN_PostURL(NPP npp, const char* url, const char* window, uint32_t len, const char* buf, NPBool file)
{
	RETURN_VALUE_IF_NULL(url, NPERR_INVALID_URL);

	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return NPERR_INVALID_PLUGIN_ERROR;

	OpAutoPtr<OpPluginPostURLMessage> message(OpPluginPostURLMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), NPERR_OUT_OF_MEMORY_ERROR);

	RETURN_VALUE_IF_ERROR(message->SetURL(url, op_strlen(url)), NPERR_OUT_OF_MEMORY_ERROR);

	if (window)
		RETURN_VALUE_IF_ERROR(message->SetWindow(window, op_strlen(window)), NPERR_OUT_OF_MEMORY_ERROR);

	RETURN_VALUE_IF_ERROR(message->SetData(buf, len) , NPERR_OUT_OF_MEMORY_ERROR);
	message->SetIsFile(file);

	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), NPERR_GENERIC_ERROR);

	return OpPluginErrorMessage::Cast(sync.GetResponse())->GetNpError();
}

/* static */ NPError
BrowserFunctions::NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
	/* We do not currently support this method. */
	return NPERR_GENERIC_ERROR;
}

/* static */ NPError
BrowserFunctions::NPN_NewStream(NPP npp, NPMIMEType type, const char* window, NPStream** stream)
{
	/* We do not currently support this method. */
	return NPERR_GENERIC_ERROR;
}

/* static */ int32_t
BrowserFunctions::NPN_Write(NPP npp, NPStream* stream, int32_t len, void* buffer)
{
	/**
	 * We do not currently support this method.
	 *
	 * From MDN:
	 * If unsuccessful, the plug-in returns a negative integer. This indicates
	 * that the browser encountered an error while processing the data, so the
	 * plug-in should terminate the stream by calling NPN_DestroyStream().
	 */
	return -1;
}

/* static */ NPError
BrowserFunctions::NPN_DestroyStream(NPP npp, NPStream* stream, NPReason reason)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return NPERR_INVALID_PLUGIN_ERROR;

	UINT64 stream_id = pci->Lookup(stream);
	if (!stream_id)
		return NPERR_INVALID_PARAM;

	OpPluginDestroyStreamMessage* message = OpPluginDestroyStreamMessage::Create(reason);
	if (!message)
		return NPERR_OUT_OF_MEMORY_ERROR;

	message->GetStreamRef().SetIdentifier(stream_id);

	if (OpStatus::IsError(pci->GetChannel()->SendMessage(message)))
		return NPERR_GENERIC_ERROR;

	return NPERR_NO_ERROR;
}

/* static */ void
BrowserFunctions::NPN_Status(NPP npp, const char* status)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return;

	UniString s;
	if (OpStatus::IsError(UniString_UTF8::FromUTF8(s, status)))
		return;

	OpPluginStatusTextMessage* message = OpPluginStatusTextMessage::Create(s);
	if (!message)
		return;

	OpStatus::Ignore(pci->GetChannel()->SendMessage(message));
}

/* static */ const char*
BrowserFunctions::NPN_UserAgent(NPP npp)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));

	PluginComponent* component = pci.get() ? pci->GetComponent() : PluginComponent::FindPluginComponent();
	RETURN_VALUE_IF_NULL(component, NULL);

	const char* ua = pci.get() ? pci->GetUserAgent() : component->GetUserAgent();
	if (ua)
		return ua;

	OpChannel* remote = pci.get() ? pci->GetChannel() : component->GetChannel();
	if (!remote || !remote->IsDirected())
		return NULL;

	OpPluginUserAgentMessage* message = OpPluginUserAgentMessage::Create();
	RETURN_VALUE_IF_NULL(message, NULL);

	OpSyncMessenger sync(component->CreateChannel(remote->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), NULL);

	NPString nps;
	RETURN_VALUE_IF_ERROR(PluginComponentHelpers::SetNPString(&nps, OpPluginUserAgentResponseMessage::Cast(sync.GetResponse())->GetUserAgent()), NULL);

	if (pci.get())
		pci->SetUserAgent(nps.UTF8Characters);
	else
		component->SetUserAgent(nps.UTF8Characters);

	return nps.UTF8Characters;
}

/* static */ void*
BrowserFunctions::NPN_MemAlloc(uint32_t size)
{
	return OP_NEWA(char, size);
}

/* static */ void
BrowserFunctions::NPN_MemFree(void* ptr)
{
	OP_DELETEA(static_cast<char*>(ptr));
}

/* static */ uint32_t
BrowserFunctions::NPN_MemFlush(uint32_t size)
{
	/* There is no memory to free. */
	return 0;
}

/* static */ void
BrowserFunctions::NPN_ReloadPlugins(NPBool reloadPages)
{
	PluginComponent* component = PluginComponent::FindPluginComponent();
	if (!component || !component->GetChannel()->IsDirected())
		return;

	/* Synchronize, as the plug-in may expect reload to have taken place before returning. */
	OpSyncMessenger sync(component->CreateChannel(component->GetChannel()->GetDestination()));
	OpStatus::Ignore(sync.SendMessage(OpPluginReloadMessage::Create(reloadPages)));
}

/* static */ void*
BrowserFunctions::NPN_GetJavaEnv()
{
	return NULL;
}

/* static */ void*
BrowserFunctions::NPN_GetJavaPeer(NPP instance)
{
	return NULL;
}

/* static */ void
BrowserFunctions::NPN_InvalidateRect(NPP npp, NPRect* rect)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return;

	if (pci->GetTranslator() && pci->GetTranslator()->Invalidate(rect))
		/* The platform handled invalidation by itself. */
		return;

	OpPluginInvalidateMessage* message = OpPluginInvalidateMessage::Create();
	if (!message)
		return;

	PluginComponentHelpers::SetPluginRect(message->GetRectRef(), *rect);
	OpStatus::Ignore(pci->GetChannel()->SendMessage(message));
}

/* static */ void
BrowserFunctions::NPN_InvalidateRegion(NPP npp, NPRegion region)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get() || !pci->GetTranslator())
		return;

	NPRect rect;
	if (!pci->GetTranslator()->ConvertPlatformRegionToRect(region, rect))
		return;

	NPN_InvalidateRect(npp, &rect);
}

/* static */ void
BrowserFunctions::NPN_ForceRedraw(NPP npp)
{
	/*
	 * "As of Firefox 4, this function no longer has any effect when running with separate plugin processes."
	 *
	 * "Browsers may ignore this request."
	 *
	 *  -- https://developer.mozilla.org/en/NPN_ForceRedraw
	 */
}

/* static */ NPIdentifier
BrowserFunctions::NPN_GetStringIdentifier(const NPUTF8* name)
{
	RETURN_VALUE_IF_NULL(name, NULL);

	NPIdentifier ret = NULL;
	NPN_GetStringIdentifiers(&name, 1, &ret);
	return ret;
}

/* static */ void
BrowserFunctions::NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers)
{
	if (!names || !identifiers || !nameCount)
		return;

	/* There is no way to report error, so ensure all return values are valid or NULL. */
	op_memset(identifiers, 0, sizeof(NPIdentifier) * nameCount);

	PluginComponent* component = PluginComponent::FindPluginComponent();
	if (!component)
	{
		/* We are on a third party thread with no access to the global identifier cache. Create
		 * orphan identifiers that will be registered and merged with the global cache when they
		 * are introduced to the main plug-in thread. */
		for (int i = 0; i < nameCount; i++)
			identifiers[i] = IdentifierCache::MakeOrphan(names[i]);
		return;
	}

	/* Check cache first. A bit crude, we perform the browser lookup for all strings if any are missing. */
	bool cache_complete = true;

	for (int i = 0; i < nameCount; i++)
		if ((identifiers[i] = component->GetIdentifierCache()->Lookup(names[i])) == NULL)
		{
			cache_complete = false;
			break;
		}

	if (cache_complete)
		return;

	/* Request lookup. */
	if (!component->GetChannel()->IsDirected())
		return;

	OpAutoPtr<OpPluginGetStringIdentifiersMessage> message(OpPluginGetStringIdentifiersMessage::Create());
	if (!message.get())
		return;

	for (int i = 0; i < nameCount; i++)
	{
		UniString s;

		RETURN_VOID_IF_ERROR(UniString_UTF8::FromUTF8(s, names[i]));
		RETURN_VOID_IF_ERROR(message->GetNamesRef().Add(s));
	}

	OpSyncMessenger sync(component->CreateChannel(component->GetChannel()->GetDestination()));
	if (OpStatus::IsError(sync.SendMessage(message.release())))
		return;

	/* Bind strings in cache, and return them. */
	OpPluginIdentifiersMessage* response = OpPluginIdentifiersMessage::Cast(sync.GetResponse());
	for (unsigned int i = 0; i < MIN(static_cast<unsigned int>(nameCount), response->GetIdentifiers().GetCount()); i++)
		identifiers[i] = component->GetIdentifierCache()->Bind(*response->GetIdentifiers().Get(i), names[i]);
}

/* static */ NPIdentifier
BrowserFunctions::NPN_GetIntIdentifier(int32_t intid)
{
	PluginComponent* component = PluginComponent::FindPluginComponent();
	if (!component)
	{
		/* We are on a third party thread with no access to the global identifier cache. Create
		 * an orphan identifier that will be registered and merged with the global cache when
		 * it is introduced to the main plug-in thread. */
		return IdentifierCache::MakeOrphan(intid);
	}

	/* Check cache first. */
	if (NPIdentifier id = component->GetIdentifierCache()->Lookup(intid))
		return id;

	if (!component->GetChannel()->IsDirected())
		return NULL;

	/* Request new identifier. */
	OpSyncMessenger sync(component->CreateChannel(component->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(OpPluginGetIntIdentifierMessage::Create(intid)), NULL);

	OpPluginIdentifiersMessage* response = OpPluginIdentifiersMessage::Cast(sync.GetResponse());
	OP_ASSERT(response->GetIdentifiers().GetCount() < 2
	          || !"BrowserFunctions::NPN_GetIntIdentifier: PluginGetIntIdentifier shouldn't ever return more than *one* identifier");
	if (response->GetIdentifiers().GetCount() == 0)
		return NULL;

	/* And bind it in the cache. */
	return component->GetIdentifierCache()->Bind(*response->GetIdentifiers().Get(0));
}

/* static */ bool
BrowserFunctions::NPN_IdentifierIsString(NPIdentifier identifier)
{
	return identifier && IdentifierCache::IsString(identifier);
}

/* static */ NPUTF8*
BrowserFunctions::NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
	if (!identifier || !NPN_IdentifierIsString(identifier))
		return NULL;

	/* If the string is known, return it directly. */
	if (IdentifierCache::GetString(identifier))
		return IdentifierCache::GetStringCopy(identifier);

	/* Otherwise, ask the browser for the string. */
	PluginComponent* component = PluginComponent::FindPluginComponent();
	if (!component || !component->GetChannel()->IsDirected())
		return NULL;

	OpPluginLookupIdentifierMessage* message = OpPluginLookupIdentifierMessage::Create();
	RETURN_VALUE_IF_NULL(message, NULL);
	message->GetIdentifierRef().SetIdentifier(component->GetIdentifierCache()->GetBrowserIdentifier(identifier));

	OpSyncMessenger sync(component->CreateChannel(component->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), NULL);

	OpPluginLookupIdentifierResponseMessage* response = OpPluginLookupIdentifierResponseMessage::Cast(sync.GetResponse());

	NPString s;
	RETURN_VALUE_IF_ERROR(PluginComponentHelpers::SetNPString(&s, response->GetStringValue()), NULL);

	/* Update the cache to include the value. */
	component->GetIdentifierCache()->SetString(identifier, s.UTF8Characters);

	return const_cast<NPUTF8*>(s.UTF8Characters);
}

/* static */ int32_t
BrowserFunctions::NPN_IntFromIdentifier(NPIdentifier identifier)
{
	if (identifier && !IdentifierCache::IsString(identifier))
		return IdentifierCache::GetInteger(identifier);

	return 0;
}

/* static */ NPObject*
BrowserFunctions::NPN_CreateObject(NPP npp, NPClass* aClass)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return NULL;

	NPObject* np_object = NULL;

	/* The plug-in may opt to perform the allocation itself. */
	if (aClass->allocate)
		RETURN_VALUE_IF_NULL(np_object = aClass->allocate(npp, aClass), NULL);

	/* Whereupon we register it with the browser and fill in the details. */
	if (NPObject* new_object = pci->RegisterObject(np_object))
	{
		new_object->_class = aClass;
		return new_object;
	}

	/* Alas, registering failed, release the memory claimed. */
	if (np_object)
	{
		if (aClass->deallocate)
			aClass->deallocate(np_object);
		else
			NPN_MemFree(np_object);
	}

	return NULL;
}

/* static */ NPObject*
BrowserFunctions::NPN_RetainObject(NPObject* obj)
{
	PluginComponent* component = PluginComponent::FindOwner(obj);
	if (!component || !component->GetChannel()->IsDirected())
		return NULL;

	OpPluginRetainObjectMessage* message = OpPluginRetainObjectMessage::Create();
	RETURN_VALUE_IF_NULL(message, NULL);

	PluginComponentHelpers::SetPluginObject(message->GetObjectRef(), component->Lookup(obj));

	/* We don't really need to wait for the browser to ack our change, and plug-ins are rather firm
	 * in believing this method to be 'atomic' and not trigger nested requests. */
	OpStatus::Ignore(component->GetChannel()->SendMessage(message));

	obj->referenceCount++;
	return obj;
}

/* static */ void
BrowserFunctions::NPN_ReleaseObject(NPObject* obj)
{
	PluginComponent* component = PluginComponent::FindOwner(obj);

	if (!component || !component->GetChannel()->IsDirected())
		return;

	OpPluginReleaseObjectMessage* message = OpPluginReleaseObjectMessage::Create();
	if (!message)
		return;

	PluginComponentHelpers::SetPluginObject(message->GetObjectRef(), component->Lookup(obj));

	/* Mark all PCIs as in-use during this call. This prevents them from being destroyed, which is a
	 * rather good thing considering Flash crashes if we try to destroy an instance inside
	 * NPN_ReleaseObject. */
	OtlList<CountedPluginComponentInstance> pcis;
	for(OtlSet<PluginComponentInstance*>::ConstIterator i = component->GetInstances().Begin();
		i != component->GetInstances().End(); i++)
		pcis.Append(*i);

	OpSyncMessenger sync(component->CreateChannel(component->GetChannel()->GetDestination()));
	if (OpStatus::IsError(sync.SendMessage(message)))
		return;

	OpPluginObjectStateMessage* response = OpPluginObjectStateMessage::Cast(sync.GetResponse());
	if (response->GetObject().GetReferenceCount() > 0)
		component->Bind(response->GetObject());
}

/* static */ bool
BrowserFunctions::NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return false;

	/* If the object is a local object (owned by the plug-in hosted in this plug-in component), we
	 * can short-circuit the operation and call the plug-in directly. Silverlight frequently uses
	 * the NPAPI to act on its own methods and benefits greatly from this. */
	if (pci->GetComponent()->IsLocalObject(obj))
		return obj->_class->invoke && obj->_class->invoke(obj, methodName, args, argCount, result);

	OpAutoPtr<OpPluginInvokeMessage> message(OpPluginInvokeMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), false);

	message->GetObjectRef().SetObject(pci->GetComponent()->Lookup(obj));
	PluginComponentHelpers::SetPluginIdentifier(message->GetMethodRef(), methodName, pci->GetComponent()->GetIdentifierCache());

	return HandleInvoke(pci, message, args, argCount, result);
}

/* static */ bool
BrowserFunctions::NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return false;

	/* If the object is a local object (owned by the plug-in hosted in this plug-in component), we
	 * can short-circuit the operation and call the plug-in directly. Silverlight frequently uses
	 * the NPAPI to act on its own methods and benefits greatly from this. */
	if (pci->GetComponent()->IsLocalObject(obj))
		return obj->_class->invokeDefault && obj->_class->invokeDefault(obj, args, argCount, result);

	OpAutoPtr<OpPluginInvokeDefaultMessage> message(OpPluginInvokeDefaultMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), false);

	message->GetObjectRef().SetObject(pci->GetComponent()->Lookup(obj));

	return HandleInvoke(pci, message, args, argCount, result);
}

/* static */ bool
BrowserFunctions::NPN_Evaluate(NPP npp, NPObject* obj, NPString* script, NPVariant* result)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get() || !script || (obj && !pci->GetComponent()->Lookup(obj)))
		return false;

	/* Build message. */
	UniString uni_script;
	RETURN_VALUE_IF_ERROR(UniString_UTF8::FromUTF8(uni_script, script->UTF8Characters, script->UTF8Length), false);

	OpAutoPtr<OpPluginEvaluateMessage> message(OpPluginEvaluateMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), false);

	message->SetContext(pci->GetComponent()->GetScriptingContext());
	message->SetScript(uni_script);

	if (obj)
	{
		PluginComponent::PluginObject* object = PluginComponentHelpers::ToPluginObject(pci->GetComponent()->Lookup(obj));
		RETURN_VALUE_IF_NULL(object, false);

		message->SetObject(object);
	}

	/* Perform request. */
	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	if (!response->GetSuccess())
		return false;

	if (result && (!response->HasResult() || OpStatus::IsError(
					   PluginComponentHelpers::SetNPVariant(result, *response->GetResult(), pci->GetComponent()))))
		return false;

	return true;
}

/* static */ bool
BrowserFunctions::NPN_GetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName, NPVariant* result)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get() || !propertyName || (obj && !pci->GetComponent()->Lookup(obj)))
		return false;

	/* If the object is a local object (owned by the plug-in hosted in this plug-in component), we
	 * can short-circuit the operation and call the plug-in directly. Silverlight frequently uses
	 * the NPAPI to act on its own methods and benefits greatly from this. */
	if (pci->GetComponent()->IsLocalObject(obj))
		return obj->_class->getProperty && obj->_class->getProperty(obj, propertyName, result);

	/* Build message. */
	OpAutoPtr<OpPluginGetPropertyMessage> message(OpPluginGetPropertyMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), false);

	message->SetContext(pci->GetComponent()->GetScriptingContext());
	PluginComponentHelpers::SetPluginIdentifier(message->GetPropertyRef(), propertyName, pci->GetComponent()->GetIdentifierCache());
	if (obj)
		PluginComponentHelpers::SetPluginObject(message->GetObjectRef(), pci->GetComponent()->Lookup(obj));

	/* Perform request. */
	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	if (!response->GetSuccess())
		return false;

	if (result && (!response->HasResult() || OpStatus::IsError(
					   PluginComponentHelpers::SetNPVariant(result, *response->GetResult(), pci->GetComponent()))))
		return false;

	return true;
}

/* static */ bool
BrowserFunctions::NPN_SetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName, const NPVariant* value)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get() || !propertyName || !value || (obj && !pci->GetComponent()->Lookup(obj)))
		return false;

	/* If the object is a local object (owned by the plug-in hosted in this plug-in component), we
	 * can short-circuit the operation and call the plug-in directly. Silverlight frequently uses
	 * the NPAPI to act on its own methods and benefits greatly from this. */
	if (pci->GetComponent()->IsLocalObject(obj))
		return obj->_class->setProperty && obj->_class->setProperty(obj, propertyName, value);

	/* Build message. */
	OpAutoPtr<OpPluginSetPropertyMessage> message(OpPluginSetPropertyMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), false);

	message->SetContext(pci->GetComponent()->GetScriptingContext());
	PluginComponentHelpers::SetPluginIdentifier(message->GetPropertyRef(), propertyName, pci->GetComponent()->GetIdentifierCache());
	if (obj)
		PluginComponentHelpers::SetPluginObject(message->GetObjectRef(), pci->GetComponent()->Lookup(obj));

	RETURN_VALUE_IF_ERROR(PluginComponentHelpers::SetPluginVariant(message->GetValueRef(), *value, pci->GetComponent()),
						  false);

	/* Perform request. */
	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), false);

	return !!OpPluginResultMessage::Cast(sync.GetResponse())->GetSuccess();
}

/* static */ bool
BrowserFunctions::NPN_RemoveProperty(NPP npp, NPObject* obj, NPIdentifier propertyName)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get() || !propertyName || (obj && !pci->GetComponent()->Lookup(obj)))
		return false;

	/* If the object is a local object (owned by the plug-in hosted in this plug-in component), we
	 * can short-circuit the operation and call the plug-in directly. Silverlight frequently uses
	 * the NPAPI to act on its own methods and benefits greatly from this. */
	if (pci->GetComponent()->IsLocalObject(obj))
		return obj->_class->removeProperty && obj->_class->removeProperty(obj, propertyName);

	/* Build message. */
	OpAutoPtr<OpPluginRemovePropertyMessage> message(OpPluginRemovePropertyMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), false);

	message->SetContext(pci->GetComponent()->GetScriptingContext());
	PluginComponentHelpers::SetPluginIdentifier(message->GetPropertyRef(), propertyName, pci->GetComponent()->GetIdentifierCache());
	if (obj)
		PluginComponentHelpers::SetPluginObject(message->GetObjectRef(), pci->GetComponent()->Lookup(obj));

	/* Perform request. */
	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), false);

	return !!OpPluginResultMessage::Cast(sync.GetResponse())->GetSuccess();
}

/* static */ bool
BrowserFunctions::NPN_HasProperty(NPP npp, NPObject* obj, NPIdentifier propertyName)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get() || !propertyName || (obj && !pci->GetComponent()->Lookup(obj)))
		return false;

	/* If the object is a local object (owned by the plug-in hosted in this plug-in component), we
	 * can short-circuit the operation and call the plug-in directly. Silverlight frequently uses
	 * the NPAPI to act on its own methods and benefits greatly from this. */
	if (pci->GetComponent()->IsLocalObject(obj))
		return obj->_class->hasProperty && obj->_class->hasProperty(obj, propertyName);

	/* Build message. */
	OpAutoPtr<OpPluginHasPropertyMessage> message(OpPluginHasPropertyMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), false);

	message->SetContext(pci->GetComponent()->GetScriptingContext());
	PluginComponentHelpers::SetPluginIdentifier(message->GetPropertyRef(), propertyName, pci->GetComponent()->GetIdentifierCache());
	if (obj)
		PluginComponentHelpers::SetPluginObject(message->GetObjectRef(), pci->GetComponent()->Lookup(obj));

	/* Perform request. */
	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), false);

	return !!OpPluginResultMessage::Cast(sync.GetResponse())->GetSuccess();
}

/* static */ bool
BrowserFunctions::NPN_HasMethod(NPP npp, NPObject* obj, NPIdentifier propertyName)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get() || !propertyName || (obj && !pci->GetComponent()->Lookup(obj)))
		return false;

	/* If the object is a local object (owned by the plug-in hosted in this plug-in component), we
	 * can short-circuit the operation and call the plug-in directly. Silverlight frequently uses
	 * the NPAPI to act on its own methods and benefits greatly from this. */
	if (pci->GetComponent()->IsLocalObject(obj))
		return obj->_class->hasMethod && obj->_class->hasMethod(obj, propertyName);

	/* Build message. */
	OpAutoPtr<OpPluginHasMethodMessage> message(OpPluginHasMethodMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), false);

	message->SetContext(pci->GetComponent()->GetScriptingContext());
	PluginComponentHelpers::SetPluginIdentifier(message->GetMethodRef(), propertyName, pci->GetComponent()->GetIdentifierCache());
	if (obj)
		PluginComponentHelpers::SetPluginObject(message->GetObjectRef(), pci->GetComponent()->Lookup(obj));

	/* Perform request. */
	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), false);

	return !!OpPluginResultMessage::Cast(sync.GetResponse())->GetSuccess();
}

/* static */ void
BrowserFunctions::NPN_ReleaseVariantValue(NPVariant* variant)
{
	if (variant->type == NPVariantType_Object)
		BrowserFunctions::NPN_ReleaseObject(variant->value.objectValue);

	PluginComponentHelpers::ReleaseNPVariant(variant);
}

/* static */ void
BrowserFunctions::NPN_SetException(NPObject* obj, const NPUTF8* message_text)
{
	PluginComponent* component = PluginComponent::FindOwner(obj);
	if (!component || !component->GetChannel()->IsDirected())
		return;

	UniString s;
	if (OpStatus::IsError(UniString_UTF8::FromUTF8(s, message_text)))
		return;

	OpPluginExceptionMessage* message = OpPluginExceptionMessage::Create();
	if (!message)
		return;

	if (obj)
		PluginComponentHelpers::SetPluginObject(message->GetObjectRef(), component->Lookup(obj));

	message->SetMessage(s);
	OpStatus::Ignore(component->GetChannel()->SendMessage(message));
}

/* static */ void
BrowserFunctions::NPN_PushPopupsEnabledState(NPP npp, NPBool b)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return;

	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	OpStatus::Ignore(sync.SendMessage(OpPluginPushPopupsEnabledMessage::Create(b)));
}

/* static */ void
BrowserFunctions::NPN_PopPopupsEnabledState(NPP npp)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return;

	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	OpStatus::Ignore(sync.SendMessage(OpPluginPopPopupsEnabledMessage::Create()));
}

/* static */ bool
BrowserFunctions::NPN_Enumerate(NPP npp, NPObject* obj, NPIdentifier** identifier, uint32_t* count)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!identifier || !count || !pci.get() || (obj && !pci->GetComponent()->Lookup(obj)))
		return false;

	/* If the object is a local object (owned by the plug-in hosted in this plug-in component), we
	 * can short-circuit the operation and call the plug-in directly. Silverlight frequently uses
	 * the NPAPI to act on its own methods and benefits greatly from this. */
	if (pci->GetComponent()->IsLocalObject(obj))
		return NP_CLASS_STRUCT_VERSION_HAS_ENUM(obj->_class) && obj->_class->enumerate
			&& obj->_class->enumerate(obj, identifier, count);

	OpPluginObjectEnumerateMessage* message = OpPluginObjectEnumerateMessage::Create();
	RETURN_VALUE_IF_NULL(message, false);

	message->SetContext(pci->GetComponent()->GetScriptingContext());
	if (obj)
		PluginComponentHelpers::SetPluginObject(message->GetObjectRef(), pci->GetComponent()->Lookup(obj));

	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message), false);

	OpPluginObjectEnumerateResponseMessage* response = OpPluginObjectEnumerateResponseMessage::Cast(sync.GetResponse());
	if (!response->GetSuccess())
		return false;

	*count = response->GetProperties().GetCount();
	*identifier = static_cast<NPIdentifier*>(BrowserFunctions::NPN_MemAlloc(sizeof(NPIdentifier) * *count));
	if (!*identifier)
		return false;

	for (unsigned int i = 0; i < *count; i++)
		(*identifier)[i] = pci->GetComponent()->GetIdentifierCache()->Bind(*response->GetProperties().Get(i));

	return true;
}

/* static */ void
BrowserFunctions::NPN_PluginThreadAsyncCall(NPP npp, void (*func)(void*), void* userData)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp, true));
	if (pci.get())
		pci->PluginThreadAsyncCall(func, userData);
}

/* static */ bool
BrowserFunctions::NPN_Construct(NPP npp, NPObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get() || (obj && !pci->GetComponent()->Lookup(obj)))
		return false;

	/* If the object is a local object (owned by the plug-in hosted in this plug-in component), we
	 * can short-circuit the operation and call the plug-in directly. Silverlight frequently uses
	 * the NPAPI to act on its own methods and benefits greatly from this. */
	if (pci->GetComponent()->IsLocalObject(obj))
		return NP_CLASS_STRUCT_VERSION_HAS_CTOR(obj->_class) && obj->_class->construct
			&& obj->_class->construct(obj, args, argCount, result);

	OpAutoPtr<OpPluginObjectConstructMessage> message(OpPluginObjectConstructMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), false);

	message->GetObjectRef().SetObject(pci->GetComponent()->Lookup(obj));

	return HandleInvoke(pci, message, args, argCount, result);
}

/* static */ NPError
BrowserFunctions::NPN_GetValueForURL(NPP npp, NPNURLVariable variable, const char* url, char** value, uint32_t* len)
{
	if (!url || !value)
		return NPERR_INVALID_PARAM;

	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return NPERR_INVALID_PLUGIN_ERROR;

	OpAutoPtr<OpPluginGetValueForURLMessage> message(OpPluginGetValueForURLMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), NPERR_OUT_OF_MEMORY_ERROR);

	message->SetVariable(variable);

	UniString s;
	RETURN_VALUE_IF_ERROR(UniString_UTF8::FromUTF8(s, url), NPERR_OUT_OF_MEMORY_ERROR);
	message->SetURL(s);

	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), NPERR_GENERIC_ERROR);

	NPString nps;
	RETURN_VALUE_IF_ERROR(PluginComponentHelpers::SetNPString(&nps, OpPluginGetValueForURLResponseMessage::Cast(sync.GetResponse())->GetValue()),
						  NPERR_OUT_OF_MEMORY_ERROR);

	*value = const_cast<char*>(nps.UTF8Characters);
	if (len)
		*len = nps.UTF8Length;

	return NPERR_NO_ERROR;
}

/* static */ NPError
BrowserFunctions::NPN_SetValueForURL(NPP npp, NPNURLVariable variable, const char* url, const char* value, uint32_t len)
{
	if (!url || !value)
		return NPERR_INVALID_PARAM;

	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return NPERR_INVALID_PLUGIN_ERROR;

	OpAutoPtr<OpPluginSetValueForURLMessage> message(OpPluginSetValueForURLMessage::Create());
	if (!message.get())
		return NPERR_OUT_OF_MEMORY_ERROR;

	message->SetVariable(variable);

	UniString s;
	RETURN_VALUE_IF_ERROR(UniString_UTF8::FromUTF8(s, url), NPERR_OUT_OF_MEMORY_ERROR);
	message->SetURL(s);

	RETURN_VALUE_IF_ERROR(UniString_UTF8::FromUTF8(s, value, len), NPERR_OUT_OF_MEMORY_ERROR);
	message->SetValue(s);

	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), NPERR_GENERIC_ERROR);

	return OpPluginErrorMessage::Cast(sync.GetResponse())->GetNpError();
}

/* static */ NPError
BrowserFunctions::NPN_GetAuthenticationInfo(NPP npp, const char* protocol, const char* host, int32_t port, const char* scheme, const char* realm, char** username, uint32_t* ulen, char** password, uint32_t* plen)
{
	if (!protocol || !host || !scheme || !realm || !username || ulen == 0 || !password || plen == 0)
		return NPERR_INVALID_PARAM;

	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return NPERR_INVALID_PLUGIN_ERROR;

	/* Build request message. */
	OpAutoPtr<OpPluginGetAuthenticationInfoMessage> message(OpPluginGetAuthenticationInfoMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), NPERR_OUT_OF_MEMORY_ERROR);

	UniString s;
	if (OpStatus::IsError(UniString_UTF8::FromUTF8(s, protocol)) || OpStatus::IsError(message->SetProtocol(s)) ||
		OpStatus::IsError(UniString_UTF8::FromUTF8(s, host))     || OpStatus::IsError(message->SetHost(s))     ||
		OpStatus::IsError(UniString_UTF8::FromUTF8(s, scheme))   || OpStatus::IsError(message->SetScheme(s))   ||
		OpStatus::IsError(UniString_UTF8::FromUTF8(s, realm))    || OpStatus::IsError(message->SetRealm(s)))
		return NPERR_OUT_OF_MEMORY_ERROR;

	/* Send request. */
	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), NPERR_GENERIC_ERROR);

	/* Extract username and password from response. */
	OpPluginGetAuthenticationInfoResponseMessage* response = OpPluginGetAuthenticationInfoResponseMessage::Cast(sync.GetResponse());
	if (response->GetNpError() != NPERR_NO_ERROR)
		return response->GetNpError();

	NPString nps;
	RETURN_VALUE_IF_ERROR(PluginComponentHelpers::SetNPString(&nps, response->GetUsername()), NPERR_OUT_OF_MEMORY_ERROR);
	*username = const_cast<char*>(nps.UTF8Characters);
	*ulen = nps.UTF8Length;

	RETURN_VALUE_IF_ERROR(PluginComponentHelpers::SetNPString(&nps, response->GetPassword()), NPERR_OUT_OF_MEMORY_ERROR);
	*password = const_cast<char*>(nps.UTF8Characters);
	*plen = nps.UTF8Length;

	return NPERR_NO_ERROR;
}

/* static */ uint32_t
BrowserFunctions::NPN_ScheduleTimer(NPP npp, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return 0;

	return pci->ScheduleTimer(interval, !!repeat, timerFunc);
}

/* static */ void
BrowserFunctions::NPN_UnscheduleTimer(NPP npp, uint32_t timerID)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get())
		return;

	pci->UnscheduleTimer(timerID);
}

/* static */ NPError
BrowserFunctions::NPN_PopUpContextMenu(NPP npp, NPMenu* menu)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get() || !pci->GetTranslator())
		return NPERR_INVALID_INSTANCE_ERROR;

	RETURN_VALUE_IF_ERROR(pci->GetTranslator()->PopUpContextMenu(menu), NPERR_GENERIC_ERROR);

	return NPERR_NO_ERROR;
}

/* static */ NPBool
BrowserFunctions::NPN_ConvertPoint(NPP npp, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double* destX, double* destY, NPCoordinateSpace destSpace)
{
	CountedPluginComponentInstance pci(GetPluginComponentInstance(npp));
	if (!pci.get() || !pci->GetTranslator())
		return false;

	RETURN_VALUE_IF_ERROR(pci->GetTranslator()->ConvertPoint(sourceX, sourceY, sourceSpace, destX, destY, destSpace), false);

	return true;
}

/* static */ NPBool
BrowserFunctions::NPN_HandleEvent(NPP npp, void* event, NPBool handled)
{
	/* We do not currently support this method. */
	return false;
}

/* static */ NPBool
BrowserFunctions::NPN_UnfocusInstance(NPP npp, NPFocusDirection direction)
{
	/* We do not currently support this method. */
	return false;
}

/* static */ void
BrowserFunctions::NPN_URLRedirectResponse(NPP npp, void* notifyData, NPBool allow)
{
	/* We do not currently support this method. */
}

/* static */ PluginComponentInstance*
BrowserFunctions::GetPluginComponentInstance(NPP npp, bool allow_other_thread /* = false */)
{
	RETURN_VALUE_IF_NULL(npp, NULL);

	/* Verify that the instance handle is valid. See CT-2279. May return a false negative if the
	 * component is dead and the instance is merely scheduled to die, but graceful failure in that
	 * case is permissible. */
	if (!allow_other_thread /* Other threads may lack the thread-local g_component_manager we need for lookup. */)
		RETURN_VALUE_IF_NULL(PluginComponent::FindOwner(static_cast<PluginComponentInstance*>(npp->ndata)), NULL);

	PluginComponentInstance* pci = static_cast<PluginComponentInstance*>(npp->ndata);
	RETURN_VALUE_IF_NULL(pci, NULL);

	/* Ensure that we have a connection to the browser instance, and that the plug-in component is intact. */
	if (!pci->GetChannel()->IsDirected() || !pci->GetComponent() || !pci->GetComponent()->IsUsable())
		return NULL;

	/* Optionally ensure that we're running on the same thread as the PluginComponentInstance(). */
	if (!allow_other_thread)
		if (!g_component_manager || g_component_manager->GetAddress().cm != pci->GetComponent()->GetAddress().cm)
			return NULL;

	return pci;
}

/* static */ void
BrowserFunctions::InitializeFunctionTable(NPNetscapeFuncs* browser_functions)
{
	op_memset(browser_functions, 0, sizeof(*browser_functions));
	browser_functions->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
	browser_functions->size = sizeof(*browser_functions);

	browser_functions->geturl = NPN_GetURL;
	browser_functions->posturl = NPN_PostURL;
	browser_functions->requestread = NPN_RequestRead;
	browser_functions->newstream = NPN_NewStream;
	browser_functions->write = NPN_Write;
	browser_functions->destroystream = NPN_DestroyStream;
	browser_functions->status = NPN_Status;
	browser_functions->uagent = NPN_UserAgent;
	browser_functions->memalloc = NPN_MemAlloc;
	browser_functions->memfree = NPN_MemFree;
	browser_functions->memflush = NPN_MemFlush;
	browser_functions->reloadplugins = NPN_ReloadPlugins;
	browser_functions->getJavaEnv = NPN_GetJavaEnv;
	browser_functions->getJavaPeer = NPN_GetJavaPeer;
	browser_functions->geturlnotify = NPN_GetURLNotify;
	browser_functions->posturlnotify = NPN_PostURLNotify;
	browser_functions->getvalue = NPN_GetValue;
	browser_functions->setvalue = NPN_SetValue;
	browser_functions->invalidaterect = NPN_InvalidateRect;
	browser_functions->invalidateregion = NPN_InvalidateRegion;
	browser_functions->forceredraw = NPN_ForceRedraw;
	browser_functions->getstringidentifier = NPN_GetStringIdentifier;
	browser_functions->getstringidentifiers = NPN_GetStringIdentifiers;
	browser_functions->getintidentifier = NPN_GetIntIdentifier;
	browser_functions->identifierisstring = NPN_IdentifierIsString;
	browser_functions->utf8fromidentifier = NPN_UTF8FromIdentifier;
	browser_functions->intfromidentifier = NPN_IntFromIdentifier;
	browser_functions->createobject = NPN_CreateObject;
	browser_functions->retainobject = NPN_RetainObject;
	browser_functions->releaseobject = NPN_ReleaseObject;
	browser_functions->invoke = NPN_Invoke;
	browser_functions->invokeDefault = NPN_InvokeDefault;
	browser_functions->evaluate = NPN_Evaluate;
	browser_functions->getproperty = NPN_GetProperty;
	browser_functions->setproperty = NPN_SetProperty;
	browser_functions->removeproperty = NPN_RemoveProperty;
	browser_functions->hasproperty = NPN_HasProperty;
	browser_functions->hasmethod = NPN_HasMethod;
	browser_functions->releasevariantvalue = NPN_ReleaseVariantValue;
	browser_functions->setexception = NPN_SetException;
	browser_functions->pushpopupsenabledstate = NPN_PushPopupsEnabledState;
	browser_functions->poppopupsenabledstate = NPN_PopPopupsEnabledState;
	browser_functions->enumerate = NPN_Enumerate;
	browser_functions->pluginthreadasynccall = NPN_PluginThreadAsyncCall;
	browser_functions->construct = NPN_Construct;
	browser_functions->getvalueforurl = NPN_GetValueForURL;
	browser_functions->setvalueforurl = NPN_SetValueForURL;
	browser_functions->getauthenticationinfo = NPN_GetAuthenticationInfo;
	browser_functions->scheduletimer = NPN_ScheduleTimer;
	browser_functions->unscheduletimer = NPN_UnscheduleTimer;
	browser_functions->popupcontextmenu = NPN_PopUpContextMenu;
	browser_functions->convertpoint = NPN_ConvertPoint;
	browser_functions->handleevent = NPN_HandleEvent;
	browser_functions->unfocusinstance = NPN_UnfocusInstance;
	browser_functions->urlredirectresponse = NPN_URLRedirectResponse;
}

template<class T> bool
BrowserFunctions::HandleInvoke(CountedPluginComponentInstance &pci,
                                    OpAutoPtr<T> &message,
                                    const NPVariant* args,
                                    uint32_t argCount,
                                    NPVariant* result)
{
	message->SetContext(pci->GetComponent()->GetScriptingContext());

	for (unsigned int i = 0; i < argCount; ++i)
	{
		typedef OpNs4pluginsMessages_MessageSet::PluginVariant PluginVariant;

		OpAutoPtr<PluginVariant> v(OP_NEW(PluginVariant, ()));
		RETURN_VALUE_IF_NULL(v.get(), false);

		RETURN_VALUE_IF_ERROR(PluginComponentHelpers::SetPluginVariant(*v, args[i], pci->GetComponent()), false);
		RETURN_VALUE_IF_ERROR(message->GetArgumentsRef().Add(v.get()), false);

		v.release();
	}

	OpSyncMessenger sync(pci->GetComponent()->CreateChannel(pci->GetChannel()->GetDestination()));
	RETURN_VALUE_IF_ERROR(sync.SendMessage(message.release()), false);

	OpPluginResultMessage* response = OpPluginResultMessage::Cast(sync.GetResponse());
	if (!response->GetSuccess())
		return false;

	if (result && (!response->HasResult() ||
				   OpStatus::IsError(PluginComponentHelpers::SetNPVariant(result, *response->GetResult(), pci->GetComponent()))))
		return false;

	return true;
}

#endif // NS4P_COMPONENT_PLUGINS
