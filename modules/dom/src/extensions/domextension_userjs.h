/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_EXTENSION_USERJS_H
#define DOM_EXTENSIONS_EXTENSION_USERJS_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/extensions/domextensionsupport.h"

class DOM_ExtensionScriptLoader;

/** DOM_Extension is the toplevel 'extension.*' collection available to
    UserJS execution contexts. */
class DOM_Extension
	: public DOM_Object
	, public DOM_EventTargetOwner
#ifdef EXTENSION_FEATURE_IMPORTSCRIPT
	, public ES_Runtime::ExceptionCallback
	, public ES_AsyncCallback
#endif // EXTENSION_FEATURE_IMPORTSCRIPT
{
public:
	static OP_STATUS Make(DOM_Extension *&new_obj, DOM_ExtensionScope *toplevel, DOM_Runtime *origining_runtime);

	virtual ~DOM_Extension();

	DOM_DECLARE_FUNCTION(postMessage);
#ifdef EXTENSION_FEATURE_IMPORTSCRIPT
	DOM_DECLARE_FUNCTION(importScript);
#endif // EXTENSION_FEATURE_IMPORTSCRIPT
	DOM_DECLARE_FUNCTION(getFile);

	enum {
		FUNCTIONS_postMessage = 1,
#ifdef EXTENSION_FEATURE_IMPORTSCRIPT
		FUNCTIONS_importScript,
#endif // EXTENSION_FEATURE_IMPORTSCRIPT
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
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION || DOM_Object::IsA(type); }

	/* from DOM_EventTargetOwner */
	virtual DOM_Object *GetOwnerObject() { return this; }

	DOM_ExtensionScope *GetExtensionScope() { return m_toplevel; }

	void SetPort(DOM_MessagePort *port) { m_extension_port = port; }
	DOM_MessagePort* GetPort() { return m_extension_port; }

	void HandleDisconnect();

#ifdef EXTENSION_FEATURE_IMPORTSCRIPT
	OP_STATUS SetLoaderReturnValue(const ES_Value &value);

	virtual OP_STATUS HandleException(const ES_Value &exception, URL *url, const uni_char* msgz, int lineno, const uni_char* full_msgz);
	virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);
	/**< Handle uncaught exceptions stemming from worker's script execution. */
#endif // EXTENSION_FEATURE_IMPORTSCRIPT

	BOOL IsConnected() { return m_has_connected; }
	void SetConnectStatus(BOOL f) { m_has_connected = f; }
private:
	DOM_Extension(DOM_ExtensionScope *toplevel)
		: m_toplevel(toplevel)
		, m_extension_port(NULL)
		, m_ondisconnect_handler(NULL)
#ifdef EXTENSION_FEATURE_IMPORTSCRIPT
		, m_loader(NULL)
		, m_is_evaluating(FALSE)
#endif // EXTENSION_FEATURE_IMPORTSCRIPT
	{
	}
	void InjectOptionalAPIL();

	DOM_ExtensionScope *m_toplevel;

	BOOL m_has_connected;
	/**< TRUE if this this UserJS context has notified extension background
	     process that it is now running. FALSE => in a detached state. */

	DOM_MessagePort *m_extension_port;

	DOM_EventListener *m_ondisconnect_handler;
	/**< Event handler registered for .ondisconnect */

#ifdef EXTENSION_FEATURE_IMPORTSCRIPT
	/* Support for importScript() / script loading */

	ES_Value m_loader_return_value;

	DOM_ExtensionScriptLoader *m_loader;

	BOOL m_is_evaluating;

	int EvalImportedScript(ES_Value *value);
	OP_STATUS HandleError(const ES_Value &exception, ES_Runtime *origining_runtime);
#endif // EXTENSION_FEATURE_IMPORTSCRIPT
};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_EXTENSION_USERJS_H
