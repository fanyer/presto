/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextensioncontexts.h"
#include "modules/dom/src/extensions/domextensioncontext.h"
#include "modules/dom/src/extensions/domextensionspeeddialcontext.h"
#include "modules/dom/src/extensions/domextensionmenucontext.h"

/* static */ OP_STATUS
DOM_ExtensionContexts::Make(DOM_ExtensionContexts *&new_obj, DOM_ExtensionSupport *extension_support, DOM_Runtime *origining_runtime)
{
	return DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionContexts, (extension_support)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_CONTEXTS_PROTOTYPE), "Contexts");
}

void
DOM_ExtensionContexts::BeforeDestroy()
{
	if (m_toolbar)
		m_toolbar->BeforeDestroy();
}

/* virtual */void
DOM_ExtensionContexts::GCTrace()
{
	GCMark(m_toolbar);
	GCMark(m_speeddial);
#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	GCMark(m_menu);
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
}

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
DOM_ExtensionMenuContext*
DOM_ExtensionContexts::GetMenuContext()
{
	if (!m_menu && m_extension_support->GetGadget()->GetAttribute(WIDGET_EXTENSION_ALLOW_CONTEXTMENUS))
		DOM_ExtensionRootMenuContext::Make(m_menu, m_extension_support, GetRuntime());
	return m_menu;
}
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

/* virtual */ ES_GetState
DOM_ExtensionContexts::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_toolbar:
		if (value)
		{
			if (!m_toolbar)
				GET_FAILED_IF_ERROR(DOM_ExtensionContext::Make(m_toolbar, m_extension_support, DOM_ExtensionContexts::CONTEXT_TOOLBAR, GetRuntime()));

			DOMSetObject(value, m_toolbar);
		}
		return GET_SUCCESS;
	case OP_ATOM_speeddial:
		if (value)
		{
			// Don't make it available if the extension hasn't declared speed dial support.
			if (!m_speeddial && m_extension_support->GetGadget()->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_CAPABLE))
				GET_FAILED_IF_ERROR(DOM_ExtensionSpeedDialContext::Make(m_speeddial, m_extension_support->GetGadget(), GetRuntime()));

			DOMSetObject(value, m_speeddial);
		}
		return GET_SUCCESS;
#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	case OP_ATOM_menu:
		if (value)
			DOMSetObject(value, GetMenuContext());
		return GET_SUCCESS;
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionContexts::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_toolbar:
	case OP_ATOM_speeddial:
#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	case OP_ATOM_menu:
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
		return PUT_READ_ONLY;
	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

#endif // EXTENSION_SUPPORT
