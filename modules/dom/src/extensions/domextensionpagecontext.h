/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_PAGECONTEXT_H
#define DOM_EXTENSIONS_PAGECONTEXT_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/extensions/domextensionsupport.h"

/** DOM_ExtensionPageContext - communicating with the extension from within
    a scripting environment on a window carrying an extension-internal page (e.g. the popup
    or the options page). Available to scripts via opera.extension object.
	*/
class DOM_ExtensionPageContext
	: public DOM_Object
	, public DOM_EventTargetOwner
	, public ListElement<DOM_ExtensionPageContext>
{
public:
	static OP_STATUS Make(DOM_ExtensionPageContext *&context, DOM_ExtensionSupport *extension, DOM_Runtime *runtime);

	virtual ~DOM_ExtensionPageContext();
	void BeforeDestroy();

	/* The window has to initiate communication and connection by calling start(). */
	DOM_DECLARE_FUNCTION(start);
	DOM_DECLARE_FUNCTION(postMessage);
	DOM_DECLARE_FUNCTION(getFile);

	enum {
		FUNCTIONS_start = 1,
		FUNCTIONS_postMessage,
		FUNCTIONS_getFile,
		FUNCTIONS_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(accessEventListener);

	enum
	{
		FUNCTIONS_WITH_DATA_addEventListener = 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
#ifdef DOM3_EVENTS
		FUNCTIONS_WITH_DATA_addEventListenerNS,
		FUNCTIONS_WITH_DATA_removeEventListenerNS,
		FUNCTIONS_WITH_DATA_willTriggerNS,
		FUNCTIONS_WITH_DATA_hasEventListenerNS,
#endif
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	 // APIs injected conditionally - not in the prototype.
	DOM_DECLARE_FUNCTION(getScreenshot);

	/* from DOM_Object */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_PAGE_CONTEXT || DOM_Object::IsA(type); }

	/* from DOM_EventTargetOwner */
	virtual DOM_Object *GetOwnerObject();

	DOM_MessagePort *GetPort() { return m_extension_port; }

	void HandleDisconnect();

	DOM_ExtensionSupport *GetExtensionSupport() { return m_extension_support; }
private:
	DOM_ExtensionPageContext(DOM_ExtensionSupport* extension_support)
		: m_extension_port(NULL)
		, m_ondisconnect_handler(NULL)
		, m_extension_support(extension_support)
	{
	}

	void InjectOptionalAPIL();

	OP_STATUS Start();

	DOM_MessagePort *m_extension_port;

	DOM_EventListener *m_ondisconnect_handler;

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_PAGECONTEXT_H
