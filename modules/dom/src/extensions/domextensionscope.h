/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_EXTENSIONSCOPE_H
#define DOM_EXTENSIONS_EXTENSIONSCOPE_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/ecmascript_utils/esenvironment.h"

class DOM_ExtensionScopePrototype
	: public DOM_Prototype
{
public:
	DOM_ExtensionScopePrototype();

	static void InitializeL(DOM_Prototype *prototype, DOM_ExtensionScope *scope);

	enum {
		FUNCTIONS_WITH_DATA_BASIC = 9,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};
};

class DOM_ExtensionMenuContextProxy;

/** DOM_ExtensionScope is the toplevel host object of the UserJS execution
    context for an extension. */
class DOM_ExtensionScope
	: public DOM_Object,
	  public DOM_EventTargetOwner,
	  public OpGadgetExtensionListener
{
public:
	static OP_STATUS Make(DOM_Runtime *runtime);

	static OP_STATUS Initialize(ES_Environment *es_environment, OpGadget *owner);
	/**< An extension JS execution environment with has been successfully
	     created. Complete the preparation by performing the final
	     initialization steps, informing the owning extension that this
	     page-side environment is ready and operational.

	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual ~DOM_ExtensionScope();

	static void Shutdown(DOM_ExtensionScope *toplevel);
	/**< The UserJS is shutting down, either because the page it is on is
	     going away or the user dismissed it, so perform any cleanup actions.
	     Most importantly, notify the extension that it is going away.
	     May only be called once. */

	/* From DOM_Object */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_SCOPE || DOM_Object::IsA(type); }

	/* From OpGadgetExtensionListener */
	virtual BOOL HasGadgetConnected();
	virtual void OnGadgetRunning();
	virtual void OnGadgetTerminated();

	/* From DOM_EventTargetOwner */
	virtual DOM_Object *GetOwnerObject() { return this; }

	static DOM_ExtensionScope *GetGlobalScope(ES_Environment *es_environment);

	OpGadget *GetExtensionGadget() { return m_owner; }
	DOM_Extension *GetExtension() { return m_extension; }
#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	DOM_ExtensionMenuContextProxy *GetMenuContext() { return m_menu_context; }
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	void BeforeDestroy();

	DOM_DECLARE_FUNCTION_WITH_DATA(dialog);

	DOM_ExtensionSupport *GetExtensionSupport() { return m_extension_support; }

	void InitializeL();

private:
	DOM_ExtensionScope();

	OP_STATUS Start();

	OP_STATUS RegisterWithExtension();

	OpGadget *m_owner;
	/**< Keep a backreference to the owning extension in order to be able to resolve the postMessage() target document of the
		 background process. UserJS execution contexts register with the gadget/background process that they are listening, so
		 as to correctly handle the event where this reference becomes dead (=> extension is deleted/shutdown.) */

	DOM_Extension *m_extension;
	/**< The toplevel extension.* namespace object. */

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	DOM_ExtensionMenuContextProxy* m_menu_context;
	/**< opera.context.menu object. */
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_EXTENSIONSCOPE_H
