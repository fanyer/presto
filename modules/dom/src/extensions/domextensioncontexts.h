/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_CONTEXTS_H
#define DOM_EXTENSIONS_CONTEXTS_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/extensions/domextensionsupport.h"

class DOM_ExtensionContext;
class DOM_ExtensionSpeedDialContext;
class DOM_ExtensionMenuContext;

/** DOM_ExtensionContexts is the opera.contexts.* collection object available to background page/processes. */
class DOM_ExtensionContexts
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_ExtensionContexts *&new_obj, DOM_ExtensionSupport *extension_support, DOM_Runtime *origining_runtime);

	void BeforeDestroy();

	/** The platform provides a set of predefined contexts that UIItems can be added to.
	    TOOLBAR (opera.contexts.toolbar) is the only context of this type supported so far. */
	enum ContextType {
		CONTEXT_UNKNOWN = 0,
		CONTEXT_TOOLBAR,
		CONTEXT_PAGE,
		CONTEXT_SELECTION,
		CONTEXT_LINK,
		CONTEXT_EDITABLE,
		CONTEXT_IMAGE,
		CONTEXT_VIDEO,
		CONTEXT_AUDIO,
		CONTEXT_MENU
	};

	/* from DOM_Object */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_CONTEXTS || DOM_Object::IsA(type); }
#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	DOM_ExtensionMenuContext *GetMenuContext();
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
private:
	DOM_ExtensionContexts(DOM_ExtensionSupport *extension_support)
		: m_toolbar(NULL),
		  m_speeddial(NULL),
#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
		  m_menu(NULL),
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
		  m_extension_support(extension_support)
	{
	}

	DOM_ExtensionContext *m_toolbar;
	DOM_ExtensionSpeedDialContext *m_speeddial;
#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	DOM_ExtensionMenuContext *m_menu;
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_CONTEXTS_H
