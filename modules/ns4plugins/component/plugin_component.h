/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef NS4P_PLUGIN_COMPONENT_H
#define NS4P_PLUGIN_COMPONENT_H

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/hardcore/component/OpComponent.h"
#include "modules/otl/map.h"
#include "modules/otl/set.h"
#include "modules/pi/OpPluginLibrary.h"
#include "modules/pi/OpPluginTranslator.h"
#include "modules/otl/list.h"
#include "modules/util/OpHashTable.h"
#include "modules/ns4plugins/src/generated/g_message_ns4plugins_messages.h"
#include "modules/ns4plugins/component/identifier_cache.h"
#include "modules/ns4plugins/src/plug-inc/npfunctions.h"
#include "modules/pi/OpThreadTools.h"

class PluginComponentInstance;

class PluginComponent
	: public OpComponent
{
public:
	typedef OpNs4pluginsMessages_MessageSet::PluginObject PluginObject;

	/**
	 * Construct new component.
	 *
	 * @param address Component address.
	 */
	PluginComponent(const OpMessageAddress& address);

	/**
	 * Destroy component.
	 */
	~PluginComponent();

	/**
	 * Process an incoming message.
	 *
	 * @param message The message to process.
	 *
	 * @return OpStatus::OK on success.
	 */
	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);

	/**
	 * Retrieve identifier cache.
	 */
	IdentifierCache* GetIdentifierCache() { return &m_identifier_cache->cache; }

	/**
	 * Retrieve channel to PluginLib.
	 */
	OpChannel* GetChannel() { return OpComponent::GetChannel(OpMessageAddress(GetAddress().cm, GetAddress().co, OpChannel::ROOT)); }

	/**
	 * Retrieve the current OpPluginLibrary.
	 *
	 * @return Pointer to OpPluginLibrary, or NULL if there is none.
	 */
	OpPluginLibrary* GetLibrary() const { return m_library; }

	/**
	 * Access the list of active plug-in instances.
	 *
	 * @return Iterator of instance map.
	 */
	const OtlSet<PluginComponentInstance*>& GetInstances() const { return m_instance_set; }

	/**
	 * Check if this plug-in component is intact and ready to serve.
	 */
	bool IsUsable() { return GetChannel() && m_identifier_cache; }

	/**
	 * Access cached user agent.
	 */
	const char* GetUserAgent() { return m_user_agent; }
	void SetUserAgent(const char* ua) { m_user_agent = ua; }

	/**
	 * Bind a PluginObject received from the browser and return an NPObject that can be passed to
	 * the plug-in library.
	 *
	 * If the PluginObject is already bound, the previously returned NPObject will be updated and
	 * returned.
	 *
	 * @param object The object received from the browser.
	 * @param preallocated_object An uninitialized NPObject structure allocated by the caller. If
	 *        non-NULL, this will be used rather than allocating a new NPObject.
	 *
	 * @return NPObject or NULL on OOM. The caller does not assume ownership of the returned object.
	 */
	NPObject* Bind(const PluginObject& object, NPObject* preallocated_object = NULL);

	/**
	 * Check if the given NPObject is a local NPObject.
	 *
	 * A local NPObject is an object created by the local plug-in using NPN_CreateObject. Non-local
	 * objects are proxy objects that represent objects in the browser document's EcmaScript
	 * runtime.
	 *
	 * We explicitly differentiate these classes because some plug-ins, such as Silverlight, opt to use
	 * NPAPI functions to act on their own objects. By recognizing these objects as local, we can
	 * short-circuit the operation and call directly back into the plug-in instead of taking the
	 * long route through the browser.
	 *
	 * @param object NPObject.
	 *
	 * @return True if the given object is owned and controlled by our plug-in, false if it as a
	 *         proxy object that can only be manipulated through the browser.
	 */
	bool IsLocalObject(NPObject* object);

	/**
	 * Look up the browser side NPObject pointer of an NPObject.
	 *
	 * @param object Object to look up.
	 *
	 * @return Browser-side NPObject pointer or NULL if the object was not bound.
	 */
	UINT64 Lookup(NPObject* object);

	/**
	 * Find the owner of an NPObject.
	 *
	 * @param object Object whose owner to find.
	 *
	 * @return Component owning the object, or NULL if no owner was found.
	 */
	static PluginComponent* FindOwner(NPObject* object);

	/**
	 * Check if this component owns a plug-in instance with the given handle.
	 *
	 * @param pci Plug-in instance.
	 *
	 * @return True if the instance is owned by this component.
	 */
	bool Owns(PluginComponentInstance* pci);

	/**
	 * Find the owner of a plug-in instance.
	 *
	 * @param pci Plug-in instance.
	 *
	 * @return Component owning the instance, or NULL if no owner was found.
	 */
	static PluginComponent* FindOwner(PluginComponentInstance* pci);

	/**
	 * Unbind a PluginObject.
	 *
	 * @param object Object to forget.
	 */
	void Unbind(const PluginObject& object);

	/**
	 * Set the active scripting context.
	 *
	 * Used by all NPN functions sending scripting requests to the browser. See
	 * PluginHandler::PushScriptingContext().
	 */
	unsigned int GetScriptingContext() { return m_scripting_context; }
	void SetScriptingContext(unsigned int context) { m_scripting_context = context; }

	/**
	 * Retrieve the currently running PluginComponent, or first usable PluginComponent found.
	 *
	 * @return Pointer to PluginComponent, or NULL if we are not in a plug-in component RunSlice().
	 */
	static PluginComponent* FindPluginComponent();

protected:
	OP_STATUS OnNewMessage(OpPluginNewMessage* message);
	OP_STATUS OnInitMessage(OpPluginInitializeMessage* message);
	OP_STATUS OnShutdownMessage();
	OP_STATUS OnLoadMessage(OpPluginLoadMessage* message);
	OP_STATUS OnEnumerateMessage(OpPluginEnumerateMessage* message);
	OP_STATUS OnProbeMessage(OpPluginProbeMessage* message);
	OP_STATUS PopulateEnumerateResponse(OpPluginEnumerateResponseMessage* response,
										const OtlList<UniString>& libraries);
	OP_STATUS OnInvalidateMessage(OpPluginObjectInvalidateMessage* message);
	OP_STATUS OnDeallocateMessage(OpPluginObjectDeallocateMessage* message);
	OP_STATUS OnHasMethodMessage(OpPluginHasMethodMessage* message);
	OP_STATUS OnHasPropertyMessage(OpPluginHasPropertyMessage* message);
	OP_STATUS OnInvokeMessage(OpPluginInvokeMessage* message);
	OP_STATUS OnInvokeDefaultMessage(OpPluginInvokeDefaultMessage* message);
	OP_STATUS OnGetPropertyMessage(OpPluginGetPropertyMessage* message);
	OP_STATUS OnSetPropertyMessage(OpPluginSetPropertyMessage* message);
	OP_STATUS OnRemovePropertyMessage(OpPluginRemovePropertyMessage* message);
	OP_STATUS OnObjectEnumerateMessage(OpPluginObjectEnumerateMessage* message);
	OP_STATUS OnObjectConstructMessage(OpPluginObjectConstructMessage* message);
	OP_STATUS OnClearSiteData(OpPluginClearSiteDataMessage* message);
	OP_STATUS OnGetSitesWithData(OpPluginGetSitesWithDataMessage* message);

	OP_STATUS InitializeIdentifierCache(const char* key);
	void ReleaseIdentifierCache(const char* key);

	const NPPluginFuncs* GetPluginFunctions() { return m_plugin_functions; }

	/**
	 * Called by instance when it is destroyed.
	 *
	 * @param pci Instance to forget.
	 */
	void OnInstanceDestroyed(PluginComponentInstance* pci);

	OpPluginLibrary* m_library;

	/** Blank NPClass used for browser-created objects. */
	NPClass* m_browser_object_class;

	NPPluginFuncs* m_plugin_functions;
	NPNetscapeFuncs* m_browser_functions;

	/** PluginObject -> Object map. See Bind(). */
	typedef OtlMap<UINT64, NPObject*> ObjectMap;
	ObjectMap m_object_map;

	/** NPObject -> PluginObject map. See Lookup(). */
	typedef OtlMap<NPObject*, UINT64> NpObjectMap;
	NpObjectMap m_np_object_map;

	/** Registry of active plug-in instances. */
	typedef OtlSet<PluginComponentInstance*> InstanceSet;
	InstanceSet m_instance_set;

	/** Cached user agent string. May be NULL. */
	const char* m_user_agent;

	/** Cached NPAPI identifiers. Shared by all plug-in components in the same component manager. */
	struct GlobalIdentifierCache
		: public OpComponentManager::Value
	{
		IdentifierCache cache;
		int ref_count;
	};

	GlobalIdentifierCache* m_identifier_cache;

	/** Current scripting context. See PluginHandler::PushScriptingContext(). */
	unsigned int m_scripting_context;

	/* Allow PluginComponentInstance to access GetPluginFunctions(). */
	friend class PluginComponentInstance;
};

#endif // NS4P_COMPONENT_PLUGINS

#endif // NS4P_PLUGIN_COMPONENT_H
