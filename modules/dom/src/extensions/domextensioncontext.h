/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_CONTEXT_H
#define DOM_EXTENSIONS_CONTEXT_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/dom/src/extensions/domextensioncontexts.h"
#include "modules/windowcommander/OpExtensionUIListener.h"

class DOM_ExtensionUIItem;

/** This class implements opera.contexts.toolbar (and other platform-builtin contexts),
 *  + DOM_ExtensionUIItem derives from DOM_ExtensionContext.
 */
class DOM_ExtensionContext
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_ExtensionContext *&new_obj, DOM_ExtensionSupport *opera, DOM_ExtensionContexts::ContextType type, DOM_Runtime *origining_runtime);

	virtual ~DOM_ExtensionContext();

	virtual void BeforeDestroy();

	/* from DOM_Object */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL TypeofYieldsObject() { return TRUE; }
	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_CONTEXT || DOM_Object::IsA(type); }

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	static int CreateItem(DOM_Object *this_object, DOM_ExtensionSupport *opera_extension, ES_Object *properties, ES_Value *return_value, DOM_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION(createItem);
	DOM_DECLARE_FUNCTION(addItem);
	DOM_DECLARE_FUNCTION(removeItem);

	DOM_DECLARE_FUNCTION(item);

	enum {
		FUNCTIONS_createItem = 1,
		FUNCTIONS_addItem,
		FUNCTIONS_removeItem,
		FUNCTIONS_item,
		FUNCTIONS_ARRAY_SIZE
	};

	class ExtensionUIListenerCallbacks
		: public OpExtensionUIListener::ExtensionUIItemNotifications
	{
	public:
		virtual void ItemAdded(OpExtensionUIListener::ExtensionId id, OpExtensionUIListener::ItemAddedStatus status);
		virtual void ItemRemoved(OpExtensionUIListener::ExtensionId id, OpExtensionUIListener::ItemRemovedStatus status);

		virtual void OnClick(OpExtensionUIListener::ExtensionId id);
		virtual void OnRemove(OpExtensionUIListener::ExtensionId id);
	private:
		friend class DOM_ExtensionContext;

		ExtensionUIListenerCallbacks()
			: m_owner(NULL)
		{
		}

		DOM_ExtensionContext *m_owner;
	};
	friend class ExtensionUIListenerCallbacks;

	unsigned int GetId() { return m_id; }

	ES_Thread *GetBlockedThread() { return m_blocked_thread; }
	void SetBlockedThread(ES_Thread *thread, unsigned int id = 0) { m_blocked_thread = thread; m_active_extension_id = id; }

	OpExtensionUIListener::ItemAddedStatus GetAddedCallStatus() { return m_add_status; }
	OpExtensionUIListener::ItemRemovedStatus GetRemovedCallStatus() { return m_remove_status; }

	void ResetCallStatus() { m_add_status = OpExtensionUIListener::ItemAddedUnknown; m_remove_status = OpExtensionUIListener::ItemRemovedUnknown; }

	ES_ThreadListener *GetThreadListener() { return &m_thread_listener; }

protected:

	DOM_ExtensionContext(DOM_ExtensionSupport *extension_support, unsigned int id = 0)
		: m_id(id)
		, m_parent_id(0)
		, m_parent(NULL)
		, m_max_items(-1)
		, m_active_extension_id(0)
		, m_add_status(OpExtensionUIListener::ItemAddedUnknown)
		, m_remove_status(OpExtensionUIListener::ItemRemovedUnknown)
		, m_blocked_thread(NULL)
		, m_extension_support(extension_support)
	{
		m_notifications.m_owner = this;
		m_thread_listener.m_owner = this;
	}

	unsigned int m_id;
	unsigned int m_parent_id;
	DOM_ExtensionContext *m_parent;

	List<DOM_ExtensionUIItem> m_items;
	/**< The list of UIItems currently registered with this context */

	int m_max_items;
	/**< Limit on the number of items the extension can attach; < 0 if unbounded. */

	ExtensionUIListenerCallbacks m_notifications;

	OpExtensionUIListener::ExtensionId m_active_extension_id;
	OpExtensionUIListener::ItemAddedStatus m_add_status;
	OpExtensionUIListener::ItemRemovedStatus m_remove_status;

	ES_Thread *m_blocked_thread;
	/**< If not NULL, thread blocked on completion of async platform call. */

	class ThreadListener
		: public ES_ThreadListener
	{
		/* ES_ThreadListener implementation; done as a local class to avoid
		   ambiguity for List<> over UIItem. */
	public:
		ThreadListener()
			: m_owner(NULL)
		{
		}

		virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);
	private:
		friend class DOM_ExtensionContext;

		DOM_ExtensionContext *m_owner;
	};
	friend class ThreadListener;

	ThreadListener m_thread_listener;

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_CONTEXT_H
