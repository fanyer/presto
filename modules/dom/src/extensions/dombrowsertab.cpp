/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/dombrowsertab.h"
#include "modules/dom/src/extensions/dombrowsertabs.h"
#include "modules/dom/src/extensions/dombrowsertabgroup.h"
#include "modules/dom/src/extensions/dombrowserwindows.h"
#include "modules/dom/src/extensions/dombrowserwindow.h"
#include "modules/dom/src/extensions/domextensionscope.h"
#include "modules/dom/src/extensions/domextension_userjs.h"
#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dochand/winman.h"
#include "modules/dom/src/extensions/domtabsapihelper.h"
#include "modules/dom/src/extensions/domtabapicache.h"
#include "modules/dom/src/extensions/domextension_background.h"


/* static */ OP_STATUS
DOM_BrowserTab::Make(DOM_BrowserTab*& new_obj, DOM_ExtensionSupport* extension_support, unsigned int tab_id, unsigned long window_id, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(tab_id);
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_BrowserTab, (extension_support, window_id, tab_id)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_TAB_PROTOTYPE), "BrowserTab"));
	RETURN_IF_LEAVE(new_obj->InjectOptionalAPIL());
	return OpStatus::OK;
}

void
DOM_BrowserTab::InjectOptionalAPIL()
{
	if (m_extension_support->GetGadget()->GetAttribute(WIDGET_EXTENSION_SCREENSHOT_SUPPORT))
		AddFunctionL(getScreenshot, "getScreenshot", "-");
}

DOM_MessagePort*
DOM_BrowserTab::GetPort()
{
	if (!m_port || !m_port->IsEntangled())
	{
		if (!m_window_id)
			return NULL;
		m_port = NULL; // The previous port will be garbage collected.

		Window* window = GetTabWindow();
		if (DOM_ExtensionScope* scope = m_extension_support->GetExtensionGlobalScope(window))
			OpStatus::Ignore(DOM_ExtensionSupport::GetPortTarget(scope->GetExtension()->GetPort(), m_port, GetRuntime()));
	}
	OP_ASSERT(!m_port || m_port->IsEntangled());
	return m_port;
}

Window*
DOM_BrowserTab::GetTabWindow()
{
	return g_windowManager->GetWindow(GetWindowId());
}

/* static */ int
DOM_BrowserTab::postMessage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);
	if (DOM_MessagePort *port = tab->GetPort())
		return DOM_MessagePort::postMessage(port, argv, argc, return_value, origining_runtime);
	else
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
}

/* static */ int
DOM_BrowserTab::close(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		call_helper->RequestTabClose(tab->GetTabId());
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		CALL_FAILED_IF_ERROR(call_helper->GetStatus());
		return ES_FAILED;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* static */ int
DOM_BrowserTab::update(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("o");
		ES_Object* properties = argv[0].value.object;

		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		call_helper->RequestTabUpdate(tab->GetTabId(), properties, origining_runtime);
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		TAB_API_CALL_FAILED_IF_ERROR(call_helper->GetStatus());
		return ES_FAILED;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* static */ int
DOM_BrowserTab::focus(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		call_helper->RequestTabFocus(tab->GetTabId());
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		TAB_API_CALL_FAILED_IF_ERROR(call_helper->GetStatus());
		return ES_FAILED;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* static */ int
DOM_BrowserTab::refresh(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);
	Window* window = tab->GetTabWindow();
	if (!window)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
	window->Reload();
	return ES_FAILED;
}

/* static */ int
DOM_BrowserTab::getScreenshot(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);
	Window* window = tab->GetTabWindow();
	FramesDocument* target = window ? window->GetCurrentDoc() : NULL;
	OP_ASSERT(tab->m_extension_support->GetBackground());

	return tab->m_extension_support->GetScreenshot(target, argv, argc, return_value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_BrowserTab::GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	switch (property_name)
	{
	case OP_ATOM_focused:
	case OP_ATOM_selected:
	case OP_ATOM_id:
	case OP_ATOM_locked:
	case OP_ATOM_browserWindow:
	case OP_ATOM_position:
	case OP_ATOM_tabGroup:
	case OP_ATOM_title:
	case OP_ATOM_private:
	case OP_ATOM_closed:
		return GetTabInfo(property_name, value, origining_runtime, restart_object);
	}
	return DOM_Object::GetNameRestart(property_name, value, origining_runtime, restart_object);
}

ES_GetState
DOM_BrowserTab::GetTabInfo(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	if (!value)
		return GET_SUCCESS;

	// Private mode can be obtained synchronously if we have window.
	if (property_name == OP_ATOM_private)
	{
		Window* window = GetTabWindow();
		if (window)
		{
			DOMSetBoolean(value, window->GetPrivacyMode());
			return GET_SUCCESS;
		}
	}

	OP_ASSERT(GetTabId());

	DOM_TabsApiHelper* call_helper;
	if (!restart_object)
	{
		GET_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, static_cast<DOM_Runtime*>(origining_runtime)));
		call_helper->QueryTab(GetTabId());
	}
	else
		call_helper = DOM_HOSTOBJECT(restart_object, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		if (property_name == OP_ATOM_closed)
		{
			DOMSetBoolean(value, OpStatus::IsError(call_helper->GetStatus()));
			return GET_SUCCESS;
		}
		else
			GET_FAILED_IF_ERROR(call_helper->GetStatus());

		switch (property_name)
		{
		case OP_ATOM_browserWindow:
			DOM_BrowserWindow* new_win;
			GET_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateWindow(new_win, m_extension_support, call_helper->GetResult().value.query_tab.browser_window_id, GetRuntime()));
			DOMSetObject(value, new_win);
			break;
		case OP_ATOM_locked:
			DOMSetBoolean(value, call_helper->GetResult().value.query_tab.is_locked);
			break;
		case OP_ATOM_position:
			DOMSetNumber(value, call_helper->GetResult().value.query_tab.position);
			break;
		case OP_ATOM_tabGroup:
			if (call_helper->GetResult().value.query_tab.tab_group_id == 0)
				DOMSetNull(value);
			else
			{
				DOM_BrowserTabGroup* tab_group;
				GET_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateTabGroup(tab_group, m_extension_support, call_helper->GetResult().value.query_tab.tab_group_id, GetRuntime()));
				DOMSetObject(value, tab_group);
			}
			break;
		case OP_ATOM_focused:
		case OP_ATOM_selected:
			DOMSetBoolean(value, call_helper->GetResult().value.query_tab.is_selected);
			break;

		case OP_ATOM_title:
			if (!call_helper->GetResult().value.query_tab.is_private || IsPrivateDataAllowed())
			{
				TempBuffer* tmp = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(tmp->Append(call_helper->GetResult().value.query_tab.title));
				DOMSetString(value, tmp);
			}
			return GET_SUCCESS;

		case OP_ATOM_private:
			DOMSetBoolean(value, call_helper->GetResult().value.query_tab.is_private);
			return GET_SUCCESS;

		default:
			OP_ASSERT(!"Unexpected property");
		}
		return GET_SUCCESS;
	}
	else
		return call_helper->BlockGet(value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_BrowserTab::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_id:
		DOMSetNumber(value, GetTabId());
		return GET_SUCCESS;

	case OP_ATOM_locked:
	case OP_ATOM_browserWindow:
	case OP_ATOM_position:
	case OP_ATOM_tabGroup:
	case OP_ATOM_focused:
	case OP_ATOM_selected:
	case OP_ATOM_title:
	case OP_ATOM_private:
	case OP_ATOM_closed:
		return GetTabInfo(property_name, value, origining_runtime, NULL);

	case OP_ATOM_faviconUrl:
	{
#ifdef SHORTCUT_ICON_SUPPORT
		Window* window = GetTabWindow();
		if (window && (!window->GetPrivacyMode() || IsPrivateDataAllowed()))
			DOMSetString(value, window->GetWindowIconURL().GetAttribute(URL::KUniName_With_Fragment, URL::KNoRedirect).CStr());
#endif // SHORTCUT_ICON_SUPPORT
		return GET_SUCCESS;
	}

	case OP_ATOM_url:
	{
		Window* window = GetTabWindow();
		if (window && (!window->GetPrivacyMode() || IsPrivateDataAllowed()))
		{
			const uni_char* url = window->GetCurrentURL().GetAttribute(URL::KUniName_With_Fragment, URL::KNoRedirect).CStr();
			if (!url || !*url) // if nothing is currently loaded then try if something is loading.
				url = window->GetCurrentLoadingURL().GetAttribute(URL::KUniName_With_Fragment, URL::KNoRedirect).CStr();
			DOMSetString(value, url);
		}
		return GET_SUCCESS;
	}

	case OP_ATOM_readyState:
	{
		Window* window = GetTabWindow();
		if (window && (!window->GetPrivacyMode() || IsPrivateDataAllowed()))
			DOM_Document::GetDocumentReadiness(value, window->GetCurrentDoc());
		return GET_SUCCESS;
	}
#ifdef USE_SPDY
	case OP_ATOM_loadedWithSPDY:
	{
		Window* window = GetTabWindow();
		if (window && (!window->GetPrivacyMode() || IsPrivateDataAllowed()))
			DOMSetBoolean(value, window->GetCurrentURL().GetAttribute(URL::KLoadedWithSpdy));
		return GET_SUCCESS;
	}
#endif // USE_SPDY
	case OP_ATOM_port:
		DOMSetObject(value, GetPort());
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_BrowserTab::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_id:
	case OP_ATOM_locked:
	case OP_ATOM_browserWindow:
	case OP_ATOM_position:
	case OP_ATOM_tabGroup:
	case OP_ATOM_focused:
	case OP_ATOM_selected:
	case OP_ATOM_title:
	case OP_ATOM_private:
	case OP_ATOM_closed:
	case OP_ATOM_faviconUrl:
	case OP_ATOM_url:
	case OP_ATOM_readyState:
#ifdef USE_SPDY
	case OP_ATOM_loadedWithSPDY:
#endif // USE_SPDY
	case OP_ATOM_port:
		return PUT_READ_ONLY;

	default:
		return DOM_TabApiCachedObject::PutName(property_name, value, origining_runtime);
	}
}

BOOL
DOM_BrowserTab::IsPrivateDataAllowed()
{
	if (!GetFramesDocument())
		return FALSE;

	OP_ASSERT(GetFramesDocument()->GetWindow()->GetGadget());

	return GetFramesDocument()->GetWindow()->GetGadget()->GetExtensionFlag(OpGadget::AllowUserJSPrivate);
}

/* virtual */ void
DOM_BrowserTab::GCTrace()
{
	GCMark(m_port);
}

/* virtual */
DOM_BrowserTab::~DOM_BrowserTab()
{
	OP_NEW_DBG("DOM_BrowserTab::~DOM_BrowserTab()", "extensions.dom");
	OP_DBG(("this: %p tab_id: %d", this, GetTabId()));
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_BrowserTab)
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTab, DOM_BrowserTab::postMessage, "postMessage", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTab, DOM_BrowserTab::close, "close", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTab, DOM_BrowserTab::update, "update", "{url:S,title:S,locked:b,focused:b}-")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTab, DOM_BrowserTab::focus, "focus", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTab, DOM_BrowserTab::refresh, "refresh", "")
DOM_FUNCTIONS_END(DOM_BrowserTab)

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
