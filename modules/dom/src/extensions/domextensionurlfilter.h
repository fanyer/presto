/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_URLFILTER_H
#define DOM_EXTENSIONS_URLFILTER_H

#ifdef EXTENSION_SUPPORT

#ifdef URL_FILTER
#include "modules/dom/src/domobj.h"
#include "modules/content_filter/content_filter.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/src/domevents/domeventlistener.h"

class DOM_ExtensionRuleList;
class DOM_ExtensionURLFilterEventTarget;

/** Expose content_filter to the extensions */
class DOM_ExtensionURLFilter
	: public DOM_Object, public URLFilterExtensionListener, public DOM_EventTargetOwner
{
public:
	static OP_STATUS Make(DOM_ExtensionURLFilter *&url_filter, OpGadget *extension_owner, DOM_Runtime *runtime, BOOL allow_rules, BOOL allow_events);
	/**< Create the object used for opera.extension.urlfilter */
	virtual ~DOM_ExtensionURLFilter();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_URLFILTER || DOM_Object::IsA(type); }
	virtual void GCTrace();
	virtual OP_STATUS CreateEventTarget();
	virtual void URLBlocked(const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx) { if (IsValidContext(dom_ctx)) OpStatus::Ignore(SendEvent(ExtensionList::BLOCKED, url, wic, dom_ctx)); }
	virtual void URLUnBlocked(const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx) { if (IsValidContext(dom_ctx)) OpStatus::Ignore(SendEvent(ExtensionList::UNBLOCKED, url, wic, dom_ctx)); }
#ifdef SELFTEST
	virtual void URLAllowed(const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx) { if (IsValidContext(dom_ctx)) OpStatus::Ignore(SendEvent(ExtensionList::ALLOWED, url, wic, dom_ctx)); }
#endif // SELFTEST

	/// Initialize constants
	static void ConstructExtensionURLFilterL(ES_Object *object, DOM_Runtime *runtime);

	OpGadget *GetExtensionGadget() { return m_owner; }

	DOM_DECLARE_FUNCTION_WITH_DATA(accessEventListener);

	enum
	{
		FUNCTIONS_WITH_DATA_addEventListener = 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	/* from DOM_EventTargetOwner */
	virtual DOM_Object *GetOwnerObject() { return this; }


private:
	DOM_ExtensionURLFilter();
	/**< Default constructor */
	OP_STATUS SendEvent(ExtensionList::EventType evt_type, const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx);
	/**< Fires a URLFilterEvent event to the DOM listener */

	/** Returns TRUE if the current context (in particular the environment) is the same of the element that originated the load, and thus
	    the event sent by content_filter.
		This is done to send the event only to the page that contains the element, not to all the pages opened in Opera.

		A side effect of this method is that events without a DOM context are not sent; the alternative is sending to all the pages.
	*/
	BOOL IsValidContext(DOMLoadContext *dom_ctx)
	{
		if (dom_ctx && dom_ctx->GetEnvironment() == GetRuntime()->GetEnvironment())
			return TRUE;

		return FALSE;
	}

	OpGadget *m_owner;
	/**< Pointer to the gadget that owns this object */
	DOM_ExtensionRuleList *m_block;
	/**< The opera.extensions.urlfilter.block object */
	DOM_ExtensionRuleList *m_allow;
	/**< The opera.extensions.urlfilter.allow object */
	BOOL m_allow_rules;
	/**< TRUE if the methods to modify the rules are allowed (enable access to m_block and m_allow) */
	BOOL m_allow_events;
	/**< TRUE if the methods to listen to events are allowed (enable access to addEventListener) */
};

/** Class used to manage listener of the vents supported by DOM_ExtensionURLFilter */
class DOM_ExtensionURLFilterEventTarget
	: public DOM_EventTarget
{
public:
	DOM_ExtensionURLFilterEventTarget(DOM_ExtensionURLFilter *owner)
		: DOM_EventTarget(owner)
	{
	}

	DOM_ExtensionURLFilter *GetDOMExtensionURLFilter() { return (DOM_ExtensionURLFilter *) GetOwner(); }
	/**< Returns the DOM_ExtensionURLFilter associated with this event target */

	virtual void AddListener(DOM_EventListener *listener);
	virtual void RemoveListener(DOM_EventListener *listener);
};

/** Expose content_filter to the extensions. This represents a list of patterns, for a black or white list */
class DOM_ExtensionRuleList: public DOM_Object
{
private:
	friend class DOM_ExtensionURLFilter;

	OP_STATUS DOMGetDictionaryStringArray(ES_Object *dictionary, const uni_char *name, OpVector<uni_char> *vector);
	/**< Gets an array of strings from a dictionary.
		@param dictionary Dictionary containing the values requested. The variable can either be a string or an array of strings
		@param name Name of the variable to read
		@param vector OpVector that will contain the strings found
	*/

public:
	static OP_STATUS Make(DOM_ExtensionRuleList *&rule_list, DOM_Runtime *runtime, OpGadget *ext_ptr, BOOL exclude);
	/**< Create the object used for opera.extension.urlfilter.block */
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_RULELIST || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(add);
	DOM_DECLARE_FUNCTION(remove);
	
	enum {
		FUNCTIONS_add = 1,
		FUNCTIONS_remove = 2,
		FUNCTIONS_ARRAY_SIZE
	};

private:
	DOM_ExtensionRuleList(OpGadget *ext_ptr, BOOL exclude)
		: m_ext_ptr(ext_ptr), m_exclude(exclude) { }
	/**< Default constructor */

	OpGadget *m_ext_ptr;
	/**< Pointer used to manage the extensions */
	BOOL m_exclude;
	/**< TRUE if this object manages the exclude list */
};

/** Event of type URLFilterEvent, fired when a URL is blocked, unblocked (which means blocked by the black list but then allowed by the white list) or allowed (not in black list at all) */
class DOM_ExtensionURLFilterEvent
	: public DOM_Event
{
public:
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_URLFILTER_EVENT || DOM_Event::IsA(type); }
};
#endif // URL_FILTER

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_URLFILTER_H
