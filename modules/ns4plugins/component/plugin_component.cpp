/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS
#include "modules/hardcore/component/Messages.h"
#include "modules/ns4plugins/component/plugin_component.h"
#include "modules/ns4plugins/component/browser_functions.h"
#include "modules/ns4plugins/component/plugin_component_instance.h"
#include "modules/ns4plugins/component/plugin_component_helpers.h"
#include "modules/opdata/UniStringUTF8.h"

#define NS4P_IDENTIFIER_CACHE_KEY "ns4plugins-identifier-cache"


PluginComponent::PluginComponent(const OpMessageAddress& address)
	: OpComponent(address, COMPONENT_PLUGIN)
	, m_library(NULL)
	, m_browser_object_class(NULL)
	, m_plugin_functions(NULL)
	, m_browser_functions(NULL)
	, m_user_agent(NULL)
	, m_identifier_cache(NULL)
	, m_scripting_context(0)
{
}

PluginComponent::~PluginComponent()
{
	/* Inform the plug-in instances of our destruction. They'll delete themselves when disconnected and able. */
	InstanceSet::Iterator it;
	for (it = m_instance_set.Begin(); it != m_instance_set.End(); it++)
		(*it)->OnComponentDestroyed(this);

	OP_DELETE(m_library);
	OP_DELETE(m_browser_functions);
	OP_DELETE(m_plugin_functions);
	OP_DELETE(m_browser_object_class);
	BrowserFunctions::NPN_MemFree(const_cast<NPUTF8*>(m_user_agent));
	ReleaseIdentifierCache(NS4P_IDENTIFIER_CACHE_KEY);
}

/* virtual */ OP_STATUS
PluginComponent::ProcessMessage(const OpTypedMessage* message)
{
	switch (message->GetType())
	{
		case OpStatusMessage::Type:
			/* Error returned from a fire-and-forget request. */
			break;

		case OpPeerDisconnectedMessage::Type:
			/* Will automatically cause termination of the component. */
			break;

		case OpPluginNewMessage::Type:
			return OnNewMessage(OpPluginNewMessage::Cast(message));

		case OpPluginInitializeMessage::Type:
			return OnInitMessage(OpPluginInitializeMessage::Cast(message));

		case OpPluginShutdownMessage::Type:
			return OnShutdownMessage();

		case OpPluginLoadMessage::Type:
			return OnLoadMessage(OpPluginLoadMessage::Cast(message));

		case OpPluginEnumerateMessage::Type:
			return OnEnumerateMessage(OpPluginEnumerateMessage::Cast(message));

		case OpPluginProbeMessage::Type:
			return OnProbeMessage(OpPluginProbeMessage::Cast(message));

		case OpPluginObjectInvalidateMessage::Type:
			return OnInvalidateMessage(OpPluginObjectInvalidateMessage::Cast(message));

		case OpPluginObjectDeallocateMessage::Type:
			return OnDeallocateMessage(OpPluginObjectDeallocateMessage::Cast(message));

		case OpPluginHasMethodMessage::Type:
			return OnHasMethodMessage(OpPluginHasMethodMessage::Cast(message));

		case OpPluginHasPropertyMessage::Type:
			return OnHasPropertyMessage(OpPluginHasPropertyMessage::Cast(message));

		case OpPluginInvokeMessage::Type:
			return OnInvokeMessage(OpPluginInvokeMessage::Cast(message));

		case OpPluginInvokeDefaultMessage::Type:
			return OnInvokeDefaultMessage(OpPluginInvokeDefaultMessage::Cast(message));

		case OpPluginGetPropertyMessage::Type:
			return OnGetPropertyMessage(OpPluginGetPropertyMessage::Cast(message));

		case OpPluginSetPropertyMessage::Type:
			return OnSetPropertyMessage(OpPluginSetPropertyMessage::Cast(message));

		case OpPluginRemovePropertyMessage::Type:
			return OnRemovePropertyMessage(OpPluginRemovePropertyMessage::Cast(message));

		case OpPluginObjectEnumerateMessage::Type:
			return OnObjectEnumerateMessage(OpPluginObjectEnumerateMessage::Cast(message));

		case OpPluginObjectConstructMessage::Type:
			return OnObjectConstructMessage(OpPluginObjectConstructMessage::Cast(message));

		case OpPluginClearSiteDataMessage::Type:
			return OnClearSiteData(OpPluginClearSiteDataMessage::Cast(message));

		case OpPluginGetSitesWithDataMessage::Type:
			return OnGetSitesWithData(OpPluginGetSitesWithDataMessage::Cast(message));

		default:
			OP_ASSERT(!"Received unknown message.");
			return OpStatus::ERR;
	}

	return OpComponent::ProcessMessage(message);
}

NPObject*
PluginComponent::Bind(const PluginObject& object, NPObject* preallocated_object /* = NULL */)
{
	NPObject* np_object = NULL;
	ObjectMap::Iterator existing_object = m_object_map.Find(object.GetObject());
	if (existing_object != m_object_map.End())
		np_object = existing_object->second;

	OP_ASSERT(!(np_object && preallocated_object) || "Do not request explicit placement of known objects.");

	if (!np_object)
	{
		np_object = preallocated_object;

		if (!np_object)
			np_object = static_cast<NPObject*>(BrowserFunctions::NPN_MemAlloc(sizeof(NPObject)));

		if (np_object)
		{
			OP_ASSERT(m_np_object_map.Count(np_object) == 0);

			if (OpStatus::IsError(m_object_map.Insert(object.GetObject(), np_object)) ||
				OpStatus::IsError(m_np_object_map.Insert(np_object, object.GetObject())))
			{
				Unbind(object);
				if (!preallocated_object)
					BrowserFunctions::NPN_MemFree(np_object);
				return NULL;
			}

			/* Allocate default np object class for objects originating in the browser. */
			if (!preallocated_object)
			{
				if (!m_browser_object_class)
					if (m_browser_object_class = OP_NEW(NPClass, ()))
					{
						op_memset(m_browser_object_class, 0, sizeof(*m_browser_object_class));
						m_browser_object_class->structVersion = NP_CLASS_STRUCT_VERSION;
					}

				np_object->_class = m_browser_object_class;
			}
		}
	}

	if (np_object)
		np_object->referenceCount = object.GetReferenceCount();

	return np_object;
}

bool
PluginComponent::IsLocalObject(NPObject* object)
{
	return Lookup(object) && object->_class && object->_class != m_browser_object_class;
}

UINT64
PluginComponent::Lookup(NPObject* object)
{
	NpObjectMap::Iterator found = m_np_object_map.Find(object);
	return found != m_np_object_map.End() ? found->second : 0;
}

/* static */ PluginComponent*
PluginComponent::FindOwner(NPObject* object)
{
	RETURN_VALUE_IF_NULL(g_component_manager, NULL);

	/* The common case is access while a PluginComponent is running. */
	if (g_component && g_component->GetType() == COMPONENT_PLUGIN)
	{
		PluginComponent* component = static_cast<PluginComponent*>(g_component);
		if (component->Lookup(object))
			return component;
	}

	/* The rare case is random access. */
	OpAutoPtr<OpHashIterator> it(g_component_manager->GetComponentIterator());
	if (it.get() && OpStatus::IsSuccess(it->First()))
		do
		{
			OpComponent* component = static_cast<OpComponent*>(it->GetData());
			if (component && component != g_component && component->GetType() == COMPONENT_PLUGIN)
			{
				PluginComponent* pc = static_cast<PluginComponent*>(component);
				if (pc->Lookup(object))
					return pc;
			}
		}
		while (OpStatus::IsSuccess(it->Next()));

	return NULL;
}

bool
PluginComponent::Owns(PluginComponentInstance* pci)
{
	return m_instance_set.Count(pci) > 0;
}

/* static */ PluginComponent*
PluginComponent::FindOwner(PluginComponentInstance* pci)
{
	RETURN_VALUE_IF_NULL(g_component_manager, NULL);

	/* The common case is access while a PluginComponent is running. */
	if (g_component && g_component->GetType() == COMPONENT_PLUGIN)
	{
		PluginComponent* component = static_cast<PluginComponent*>(g_component);
		if (component->Owns(pci))
			return component;
	}

	/* The rare case is random access. */
	OpAutoPtr<OpHashIterator> it(g_component_manager->GetComponentIterator());
	if (it.get() && OpStatus::IsSuccess(it->First()))
		do
		{
			OpComponent* component = static_cast<OpComponent*>(it->GetData());
			if (component && component != g_component && component->GetType() == COMPONENT_PLUGIN)
			{
				PluginComponent* pc = static_cast<PluginComponent*>(component);
				if (pc->Owns(pci))
					return pc;
			}
		}
		while (OpStatus::IsSuccess(it->Next()));

	return NULL;
}

void
PluginComponent::Unbind(const PluginObject& object)
{
	ObjectMap::Iterator existing_object = m_object_map.Find(object.GetObject());
	if (existing_object != m_object_map.End())
	{
		NPObject* obj = existing_object->second;
		m_object_map.Erase(object.GetObject());
		m_np_object_map.Erase(obj);
	}
}

/**
 * Container of a list of <name,value> string tuples. Exists only to
 * simplify memory handling in PluginComponent::OnNewMessage().
 */
class ArgumentContainer
{
public:
	typedef OpNs4pluginsMessages_MessageSet::PluginNew_Argument Argument;

	ArgumentContainer()
		: m_args(0), m_argc(0), m_argn(NULL), m_argv(NULL) {}

	~ArgumentContainer() {
		while (m_argc--) {
			OP_DELETEA(m_argn[m_argc]);
			OP_DELETEA(m_argv[m_argc]);
		}

		OP_DELETEA(m_argn);
		OP_DELETEA(m_argv);
	}

	OP_STATUS Construct(size_t arguments) {
		m_args = arguments;

		RETURN_OOM_IF_NULL(m_argn = OP_NEWA(char*, m_args));
		op_memset(m_argn, 0, sizeof(char*) * m_args);

		RETURN_OOM_IF_NULL(m_argv = OP_NEWA(char*, m_args));
		op_memset(m_argv, 0, sizeof(char*) * m_args);

		return OpStatus::OK;
	}

	OP_STATUS Add(Argument* arg) {
		OP_ASSERT(m_argc < m_args || "Insufficient space in argument container.");

		RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&m_argn[m_argc], arg->GetName()));
		if (arg->HasValue())
			RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&m_argv[m_argc], arg->GetValue()));

		++m_argc;
		return OpStatus::OK;
	}

	size_t GetCount() { return m_argc; }
	char** GetNames() { return m_argn; }
	char** GetValues() { return m_argv; }

protected:
	size_t m_args;
	size_t m_argc;
	char** m_argn;
	char** m_argv;
};

OP_STATUS
PluginComponent::OnNewMessage(OpPluginNewMessage* message)
{
	typedef OpNs4pluginsMessages_MessageSet::PluginNew_Argument Argument;

	/* Collect arguments. */
	ArgumentContainer arguments;
	RETURN_IF_ERROR(arguments.Construct(message->GetArguments().GetCount()));
	for (size_t i = 0; i < message->GetArguments().GetCount(); i++)
		RETURN_IF_ERROR(arguments.Add(message->GetArguments().Get(i)));

	char* content_type_;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&content_type_, message->GetContentType()));
	OpAutoArray<char> content_type(content_type_);

	OpMessageAddress address;
	if (message->HasAdapterAddress())
	{
		address.cm = message->GetAdapterAddress()->GetManager();
		address.co = message->GetAdapterAddress()->GetComponent();
		address.ch = message->GetAdapterAddress()->GetChannel();
	}

	/* Construct PluginComponentInstance. */
	OpAutoPtr<PluginComponentInstance> pci(OP_NEW(PluginComponentInstance, (this)));
	RETURN_OOM_IF_NULL(pci.get());

	RETURN_IF_ERROR(m_instance_set.Insert(pci.get()));

	RETURN_IF_ERROR(pci->Construct(message->GetSrc(), content_type.get(), message->GetMode(),
								   arguments.GetCount(), arguments.GetNames(), arguments.GetValues(),
								   reinterpret_cast<NPSavedData*>(message->GetSaved()), &address));

	pci.release();
	return OpStatus::OK;
}

void
PluginComponent::OnInstanceDestroyed(PluginComponentInstance* pci)
{
	m_instance_set.Erase(pci);
}

OP_STATUS
PluginComponent::InitializeIdentifierCache(const char* key)
{
	if (m_identifier_cache)
		return OpStatus::OK;

	OpComponentManager::Value* v = NULL;
	if (OpStatus::IsSuccess(g_component_manager->LoadValue(key, v)))
		m_identifier_cache = static_cast<GlobalIdentifierCache*>(v);
	else
	{
		OpAutoPtr<GlobalIdentifierCache> gic(OP_NEW(GlobalIdentifierCache, ()));
		RETURN_OOM_IF_NULL(gic.get());

		RETURN_IF_ERROR(g_component_manager->StoreValue(key, gic.get()));
		m_identifier_cache = gic.release();
		m_identifier_cache->ref_count = 0;
	}

	m_identifier_cache->ref_count++;

	return OpStatus::OK;
}

void
PluginComponent::ReleaseIdentifierCache(const char* key)
{
	if (!m_identifier_cache)
		return;

	OP_ASSERT(m_identifier_cache->ref_count > 0);

	if (!--m_identifier_cache->ref_count)
	{
		m_identifier_cache->cache.Clear();
		m_identifier_cache = NULL;
	}
}

OP_STATUS
PluginComponent::OnInitMessage(OpPluginInitializeMessage* message)
{
	OP_ASSERT(m_library);

	RETURN_IF_ERROR(InitializeIdentifierCache(NS4P_IDENTIFIER_CACHE_KEY));

	if (!m_browser_functions)
	{
		RETURN_OOM_IF_NULL(m_browser_functions = OP_NEW(NPNetscapeFuncs, ()));
		BrowserFunctions::InitializeFunctionTable(m_browser_functions);
	}

	if (!m_plugin_functions)
	{
		RETURN_OOM_IF_NULL(m_plugin_functions = OP_NEW(NPPluginFuncs, ()));
		m_plugin_functions->size = sizeof(*m_plugin_functions);
		m_plugin_functions->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
	}

	int error = m_library->Initialize(m_plugin_functions, m_browser_functions);

	return message->Reply(OpPluginInitializeResponseMessage::Create(error));
}

OP_STATUS
PluginComponent::OnShutdownMessage()
{
	OP_ASSERT(m_library);
	OP_ASSERT(m_instance_set.Empty());

	/* FIXME: The thing is broken, we must never unload library before
	   unloading instances. But we do that now. */
	if (m_instance_set.Empty())
		m_library->Shutdown();

	return OpStatus::OK;
}

OP_STATUS
PluginComponent::OnLoadMessage(OpPluginLoadMessage* message)
{
	OP_ASSERT(!m_library);

	RETURN_IF_ERROR(OpPluginLibrary::Create(&m_library, message->GetPath()));

	OP_STATUS s = m_library->Load();
	if (OpStatus::IsError(s))
	{
		OP_DELETE(m_library);
		m_library = NULL;
	}

	return message->Reply(OpPluginLoadResponseMessage::Create(s));
}

OP_STATUS
PluginComponent::OnEnumerateMessage(OpPluginEnumerateMessage* message)
{
	OpAutoPtr<OpPluginEnumerateResponseMessage> response (OpPluginEnumerateResponseMessage::Create());
	RETURN_OOM_IF_NULL(response.get());

	OtlList<OpPluginLibrary::LibraryPath> library_paths;
	RETURN_IF_ERROR(OpPluginLibrary::EnumerateLibraries(&library_paths, message->GetSearchPaths()));

	for (OtlList<OpPluginLibrary::LibraryPath>::Range lib_path = library_paths.All(); lib_path; ++lib_path)
	{
		typedef OpNs4pluginsMessages_MessageSet::PluginEnumerateResponse_LibraryPath LibraryPath;
		LibraryPath* lib = OP_NEW(LibraryPath, (UniString(), lib_path->type));
		RETURN_OOM_IF_NULL(lib);
		RETURN_IF_ERROR(lib->SetPath(lib_path->path));

		RETURN_IF_ERROR(response->GetLibraryPathsRef().Add(lib));
	}

	return message->Reply(response.release());
}

OP_STATUS
PluginComponent::OnProbeMessage(OpPluginProbeMessage* message)
{
	typedef OpNs4pluginsMessages_MessageSet::PluginProbeResponse_ContentType ContentType;

	OpPluginLibrary* library_;
	RETURN_IF_ERROR(OpPluginLibrary::Create(&library_, message->GetPath()));
	OpAutoPtr<OpPluginLibrary> library(library_);

	OpAutoPtr<OpPluginProbeResponseMessage> response(OpPluginProbeResponseMessage::Create());
	RETURN_OOM_IF_NULL(response.get());

	/* Transfer library text fields. */
	UniString tmp;
	RETURN_IF_ERROR(response->SetPath(message->GetPath()));

	RETURN_IF_ERROR(library->GetName(&tmp));
	RETURN_IF_ERROR(response->SetName(tmp));

	RETURN_IF_ERROR(library->GetDescription(&tmp));
	RETURN_IF_ERROR(response->SetDescription(tmp));

	RETURN_IF_ERROR(library->GetVersion(&tmp));
	RETURN_IF_ERROR(response->SetVersion(tmp));

	/* Transfer content-types. */
	OtlList<UniString> content_types;
	RETURN_IF_ERROR(library->GetContentTypes(&content_types));

	for (OtlList<UniString>::ConstRange content_type = content_types.ConstAll(); content_type; ++content_type)
	{
		OpAutoPtr<ContentType> res_type(OP_NEW(ContentType, ()));
		RETURN_OOM_IF_NULL(res_type.get());

		RETURN_IF_ERROR(res_type->SetContentType(*content_type));

		/* Transfer description of a content type. */
		RETURN_IF_ERROR(library->GetContentTypeDescription(&tmp, *content_type));
		RETURN_IF_ERROR(res_type->SetDescription(tmp));

		/* Transfer extensions. */
		OtlList<UniString> extensions;
		RETURN_IF_ERROR(library->GetExtensions(&extensions, *content_type));

		for (OtlList<UniString>::ConstRange ext = extensions.ConstAll(); ext; ++ext)
			RETURN_IF_ERROR(res_type->GetExtensionsRef().Add(*ext));

		RETURN_IF_ERROR(response->GetContentTypesRef().Add(res_type.get()));
		res_type.release();  // We release res_type because it belongs to response now.
	}

	return message->Reply(response.release());
}

OP_STATUS
PluginComponent::OnInvalidateMessage(OpPluginObjectInvalidateMessage* message)
{
	if (NPObject* np_object = Bind(message->GetObject()))
	{
		if (np_object->_class && np_object->_class->invalidate)
			np_object->_class->invalidate(np_object);
	}

	return OpStatus::OK;
}

OP_STATUS
PluginComponent::OnDeallocateMessage(OpPluginObjectDeallocateMessage* message)
{
	if (NPObject* np_object = Bind(message->GetObject()))
	{
		Unbind(message->GetObject());

		if (np_object->_class && np_object->_class->deallocate)
			np_object->_class->deallocate(np_object);
		else
			BrowserFunctions::NPN_MemFree(np_object);
	}

	return OpStatus::OK;
}

OP_STATUS
PluginComponent::OnHasMethodMessage(OpPluginHasMethodMessage* message)
{
	bool success = false;

	if (NPObject* np_object = Bind(message->GetObject()))
		if (np_object->_class && np_object->_class->hasMethod)
			if (NPIdentifier method = GetIdentifierCache()->Bind(message->GetMethod()))
				success = np_object->_class->hasMethod(np_object, method);

	return message->Reply(OpPluginResultMessage::Create(success));
}

OP_STATUS
PluginComponent::OnHasPropertyMessage(OpPluginHasPropertyMessage* message)
{
	bool success = false;

	if (NPObject* np_object = Bind(message->GetObject()))
		if (np_object->_class && np_object->_class->hasProperty)
			if (NPIdentifier property = GetIdentifierCache()->Bind(message->GetProperty()))
				success = np_object->_class->hasProperty(np_object, property);

	return message->Reply(OpPluginResultMessage::Create(success));
}

OP_STATUS
PluginComponent::OnInvokeMessage(OpPluginInvokeMessage* message)
{
	bool success = false;
	NPVariant result;
	result.type = NPVariantType_Void;

	if (NPObject* np_object = Bind(message->GetObject()))
		if (np_object->_class && np_object->_class->invoke)
			if (NPIdentifier method = GetIdentifierCache()->Bind(message->GetMethod()))
				if (NPVariant* args = OP_NEWA(NPVariant, message->GetArguments().GetCount()))
				{
					success = true;

					unsigned int i;
					for (i = 0; success && i < message->GetArguments().GetCount(); i++)
						if (OpStatus::IsError(PluginComponentHelpers::SetNPVariant(&args[i], *message->GetArguments().Get(i))))
							success = false;

					if (success)
						success = np_object->_class->invoke(np_object, method, args, i, &result);

					while (i)
						PluginComponentHelpers::ReleaseNPVariant(&args[--i]);
					OP_DELETEA(args);
				}

	return message->Reply(PluginComponentHelpers::BuildPluginResultMessage(success, &result));
}

OP_STATUS
PluginComponent::OnInvokeDefaultMessage(OpPluginInvokeDefaultMessage* message)
{
	bool success = false;
	NPVariant result;
	result.type = NPVariantType_Void;

	if (NPObject* np_object = Bind(message->GetObject()))
		if (np_object->_class && np_object->_class->invokeDefault)
			if (NPVariant* args = OP_NEWA(NPVariant, message->GetArguments().GetCount()))
			{
				success = true;

				unsigned int i;
				for (i = 0; success && i < message->GetArguments().GetCount(); i++)
					if (OpStatus::IsError(PluginComponentHelpers::SetNPVariant(&args[i], *message->GetArguments().Get(i))))
						success = false;

				if (success)
					success = np_object->_class->invokeDefault(np_object, args, i, &result);

				while (i)
					PluginComponentHelpers::ReleaseNPVariant(&args[--i]);
				OP_DELETEA(args);
			}

	return message->Reply(PluginComponentHelpers::BuildPluginResultMessage(success, &result));
}

OP_STATUS
PluginComponent::OnGetPropertyMessage(OpPluginGetPropertyMessage* message)
{
	bool success = false;
	NPVariant result;
	result.type = NPVariantType_Void;

	if (NPObject* np_object = Bind(message->GetObject()))
		if (np_object->_class && np_object->_class->getProperty)
			if (NPIdentifier property = GetIdentifierCache()->Bind(message->GetProperty()))
				success = np_object->_class->getProperty(np_object, property, &result);

	return message->Reply(PluginComponentHelpers::BuildPluginResultMessage(success, &result));
}

OP_STATUS
PluginComponent::OnSetPropertyMessage(OpPluginSetPropertyMessage* message)
{
	bool success = false;

	if (NPObject* np_object = Bind(message->GetObject()))
		if (np_object->_class && np_object->_class->setProperty)
			if (NPIdentifier property = GetIdentifierCache()->Bind(message->GetProperty()))
			{
				NPVariant value;
				if (OpStatus::IsSuccess(PluginComponentHelpers::SetNPVariant(&value, message->GetValue())))
				{
					success = np_object->_class->setProperty(np_object, property, &value);
					PluginComponentHelpers::ReleaseNPVariant(&value);
				}
			}

	return message->Reply(OpPluginResultMessage::Create(success));
}

OP_STATUS
PluginComponent::OnRemovePropertyMessage(OpPluginRemovePropertyMessage* message)
{
	bool success = false;

	if (NPObject* np_object = Bind(message->GetObject()))
		if (np_object->_class && np_object->_class->removeProperty)
			if (NPIdentifier property = GetIdentifierCache()->Bind(message->GetProperty()))
				success = np_object->_class->removeProperty(np_object, property);

	return message->Reply(OpPluginResultMessage::Create(success));
}

OP_STATUS
PluginComponent::OnObjectEnumerateMessage(OpPluginObjectEnumerateMessage* message)
{
	OpPluginObjectEnumerateResponseMessage* response = OpPluginObjectEnumerateResponseMessage::Create();
	RETURN_OOM_IF_NULL(response);

	bool success = false;

	if (NPObject* np_object = Bind(message->GetObject()))
	{
		/* Our instruction is to enumerate the properties of the object. If the plug-in does not
		 * support the enumerate function, we have successfully determined that the object has zero
		 * enumerable properties. */
		success = true;

		if (np_object->_class && NP_CLASS_STRUCT_VERSION_HAS_ENUM(np_object->_class) && np_object->_class->enumerate)
		{
			NPIdentifier* identifiers;
			uint32_t count;

			if ((success = np_object->_class->enumerate(np_object, &identifiers, &count)))
			{
				for (unsigned int i = 0; i < count; i++)
				{
					typedef OpNs4pluginsMessages_MessageSet::PluginIdentifier PluginIdentifier;

					PluginIdentifier* plugin_identifier = OP_NEW(PluginIdentifier, ());
					if (!plugin_identifier)
					{
						success = false;
						break;
					}

					PluginComponentHelpers::SetPluginIdentifier(*plugin_identifier, identifiers[i], GetIdentifierCache());

					if (OpStatus::IsError(response->GetPropertiesRef().Add(plugin_identifier)))
					{
						OP_DELETE(plugin_identifier);
						success = false;
						break;
					}
				}

				BrowserFunctions::NPN_MemFree(identifiers);
			}
		}
	}

	response->SetSuccess(success);
	return message->Reply(response);
}

OP_STATUS
PluginComponent::OnObjectConstructMessage(OpPluginObjectConstructMessage* message)
{
	bool success = false;
	NPVariant result;
	result.type = NPVariantType_Void;

	if (NPObject* np_object = Bind(message->GetObject()))
		if (np_object->_class && NP_CLASS_STRUCT_VERSION_HAS_CTOR(np_object->_class) && np_object->_class->construct)
			if (NPVariant* args = OP_NEWA(NPVariant, message->GetArguments().GetCount()))
			{
				success = true;

				unsigned int i;
				for (i = 0; success && i < message->GetArguments().GetCount(); i++)
					if (OpStatus::IsError(PluginComponentHelpers::SetNPVariant(&args[i], *message->GetArguments().Get(i))))
						success = false;

				if (success)
					success = np_object->_class->construct(np_object, args, i, &result);

				while (i)
					PluginComponentHelpers::ReleaseNPVariant(&args[--i]);
				OP_DELETEA(args);
			}

	return message->Reply(PluginComponentHelpers::BuildPluginResultMessage(success, &result));
}

OP_STATUS
PluginComponent::OnClearSiteData(OpPluginClearSiteDataMessage* message)
{
	if (!m_plugin_functions || !m_plugin_functions->clearsitedata)
		return message->Reply(OpPluginErrorMessage::Create(NPERR_GENERIC_ERROR));

	/**
	 * If NULL, all site-specific data and more generic data on browsing history
	 * (for instance, number of sites visited) should be cleared.
	 *
	 * Since we can't send a NULL character over protobuf, and empty strings
	 * aren't legal parameters (per specification), we'll assume empty strings
	 * are NULL.
	 */
	char* site = 0;
	const UniString& u_site = message->GetSite();
	if (u_site.Length() > 0)
		RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&site, u_site));

	const INT64 flags = message->GetFlags();
	const INT64 max_age = message->GetMaxAge();

	NPError success = m_plugin_functions->clearsitedata(site, flags, max_age);
	OP_DELETEA(site);

	return message->Reply(OpPluginErrorMessage::Create(success));
}

OP_STATUS
PluginComponent::OnGetSitesWithData(OpPluginGetSitesWithDataMessage* message)
{
	OpPluginGetSitesWithDataResponseMessage* response = OpPluginGetSitesWithDataResponseMessage::Create();
	RETURN_OOM_IF_NULL(response);

	if (!m_plugin_functions || !m_plugin_functions->getsiteswithdata)
		return message->Reply(response);

	char** sites_with_data = m_plugin_functions->getsiteswithdata();
	if (!sites_with_data)
		return message->Reply(response);

	unsigned int i = 0;
	UniString tmp;
	OpProtobufValueVector<UniString>& sites = response->GetSitesRef();
	while (*sites_with_data[i])
	{
		/**
		 * If an error occurs while converting a C-string to UniString, we'll
		 * continue on in order to free the whole string array returned from the
		 * plugin.
		 */
		if (OpStatus::IsSuccess(UniString_UTF8::FromUTF8(tmp, sites_with_data[i])))
			sites.Add(tmp);
		BrowserFunctions::NPN_MemFree(sites_with_data[i]);
		++i;
	}
	BrowserFunctions::NPN_MemFree(sites_with_data);

	return message->Reply(response);
}

/* static */ PluginComponent*
PluginComponent::FindPluginComponent()
{
	/* Verify that we are on a component thread. */
	RETURN_VALUE_IF_NULL(g_component_manager, NULL);

	/* Prefer currently running component, if it is of plug-in type. */
	if (g_component && g_component->GetType() == COMPONENT_PLUGIN)
		return static_cast<PluginComponent*>(g_component);

	/* Search for a usable plug-in component. We assume the user doesn't care which one it receives. */
	OpAutoPtr<OpHashIterator> it(g_component_manager->GetComponentIterator());
	if (it.get() && OpStatus::IsSuccess(it->First()))
		do
		{
			OpComponent* component = static_cast<OpComponent*>(it->GetData());
			if (component && component->GetType() == COMPONENT_PLUGIN)
			{
				PluginComponent* pc = static_cast<PluginComponent*>(component);
				if (pc->IsUsable())
					return pc;
			}
		}
		while (OpStatus::IsSuccess(it->Next()));

	return NULL;
}

#endif // NS4P_COMPONENT_PLUGINS
