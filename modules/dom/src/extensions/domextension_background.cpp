/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextension_background.h"
#include "modules/dom/src/extensions/domextensioncontexts.h"
#include "modules/dom/src/extensions/dombrowsertabs.h"
#include "modules/dom/src/extensions/dombrowsertabgroups.h"
#include "modules/dom/src/extensions/dombrowsertabgroups.h"
#include "modules/dom/src/extensions/dombrowserwindows.h"
#include "modules/dom/src/extensions/domextensionurlfilter.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domwebworkers/domcrossmessage.h"

#include "modules/dom/src/extensions/domtabapicache.h"
#include "modules/doc/frm_doc.h"

/* static */ OP_STATUS
DOM_ExtensionBackground::Make(DOM_ExtensionBackground *&new_obj, DOM_ExtensionSupport *extension_support, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionBackground, (extension_support)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_BACKGROUND_PROTOTYPE), "Extension"));
	return new_obj->Initialize(origining_runtime);
}

void
DOM_ExtensionBackground::BeforeDestroy()
{
	OP_NEW_DBG("DOM_ExtensionBackground::BeforeDestroy()", "extensions.dom");
	OP_DBG(("this: %p", this));

	m_contexts->BeforeDestroy();
#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
	m_tabs->BeforeDestroy();
	m_tab_groups->BeforeDestroy();
	m_windows->BeforeDestroy();
#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
	m_extension_support->UnsetExtensionBackground(this);
}

/* virtual */
DOM_ExtensionBackground::~DOM_ExtensionBackground()
{
	OP_NEW_DBG("DOM_ExtensionBackground::~DOM_ExtensionBackground()", "extensions.dom");
	OP_DBG(("this: %p", this));
#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
	OP_DELETE(m_tab_api_cache);
#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
}

OP_STATUS
DOM_ExtensionBackground::Initialize(DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOM_ExtensionContexts::Make(m_contexts, m_extension_support, runtime));

	RETURN_IF_ERROR(DOM_MessagePort::Make(m_port, runtime));

	ES_Value value;
	if (DOM_MessagePort::start(m_port, NULL, 0, &value /*unused*/, runtime) == ES_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	/* Extend the opera. namespace with extra properties. */
	DOM_Object *global = runtime->GetEnvironment()->GetWindow();
	OP_BOOLEAN result = runtime->GetName(*global, UNI_L("opera"), &value);

	if (OpStatus::IsSuccess(result) && result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
	{
		ES_Object *opera = value.value.object;
#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
		RETURN_IF_ERROR(DOM_BrowserWindowManager::Make(m_windows, m_extension_support, runtime));
		DOM_Object::DOMSetObject(&value, m_windows);
		RETURN_IF_ERROR(runtime->PutName(*this, UNI_L("windows"), value));

		RETURN_IF_ERROR(DOM_BrowserTabManager::MakeGlobalTabs(m_tabs, m_extension_support, runtime));
		DOM_Object::DOMSetObject(&value, m_tabs);
		RETURN_IF_ERROR(runtime->PutName(*this, UNI_L("tabs"), value));

		RETURN_IF_ERROR(DOM_BrowserTabGroupManager::Make(m_tab_groups, m_extension_support, 0, runtime));
		DOM_Object::DOMSetObject(&value, m_tab_groups);
		RETURN_IF_ERROR(runtime->PutName(GetNativeObject(), UNI_L("tabGroups"), value));

		RETURN_IF_ERROR(DOM_TabApiCache::Make(m_tab_api_cache, m_extension_support));
#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
#ifdef URL_FILTER
		if (m_extension_support->GetGadget()->HasURLFilterAccess())
		{
			DOM_ExtensionURLFilter *urlfilter;

			RETURN_IF_ERROR(DOM_ExtensionURLFilter::Make(urlfilter, m_extension_support->GetGadget(), runtime, TRUE, FALSE));
			DOM_Object::DOMSetObject(&value, urlfilter);
			RETURN_IF_ERROR(runtime->PutName(*this, UNI_L("urlfilter"), value));
		}
#endif // URL_FILTER

		DOM_Object::DOMSetObject(&value, m_contexts);
		RETURN_IF_ERROR(runtime->PutName(opera, UNI_L("contexts"), value));

		if (GetExtensionSupport()->GetGadget()->GetAttribute(WIDGET_EXTENSION_SCREENSHOT_SUPPORT))
			RETURN_IF_LEAVE(AddFunctionL(getScreenshot, "getScreenshot", "-"));
		DOM_Object::DOMSetObject(&value, *this);
		return runtime->PutName(opera, UNI_L("extension"), value);
	}
	else
	{
		OP_ASSERT(!"Unable to locate 'opera' object");
		return OpStatus::IsSuccess(result) ? OpStatus::ERR : result;
	}
}

/* virtual */ void
DOM_ExtensionBackground::GCTrace()
{
	GCMark(FetchEventTarget());

	GCMark(static_cast<DOM_Object*>(m_port));

	GCMark(m_contexts);
#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
	GCMark(m_windows);
	GCMark(m_tab_groups);
	GCMark(m_tabs);
	m_tab_api_cache->GCTrace();
#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
}

/* static */ int
DOM_ExtensionBackground::broadcastMessage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION_BACKGROUND, DOM_ExtensionBackground);
	DOM_CHECK_ARGUMENTS("-|O");

	URL extension_url = extension->GetEnvironment()->GetDOMRuntime()->GetOriginURL();

	DOM_ExtensionSupport *support = extension->GetExtensionSupport();

	for (UINT32 i=0; i < support->m_ports.GetCount(); i++)
	{
		DOM_MessagePort *target_port = support->m_ports.Get(i);
		CALL_FAILED_IF_ERROR(DOM_ExtensionSupport::PostMessageToPort(extension, target_port, target_port, extension_url, argv, argc, return_value, target_port->GetRuntime()));
	}

	return ES_FAILED;
}

/* static */ int
DOM_ExtensionBackground::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION_BACKGROUND, DOM_ExtensionBackground);

	if (argc > 0 && argv[0].type == VALUE_STRING && uni_str_eq(argv[0].value.string, "message"))
		return DOM_MessagePort::accessEventListenerStart(extension->m_port, argv, argc, return_value, origining_runtime, data);
	else
		return DOM_Node::accessEventListener(this_object, argv, argc, return_value, origining_runtime, data);
}

/* static */ int
DOM_ExtensionBackground::getFile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION_BACKGROUND, DOM_ExtensionBackground);
	DOM_CHECK_ARGUMENTS("s");

	if (!extension->GetFramesDocument())
		return ES_FAILED;

	const uni_char* resource_path = argv[0].value.string;
	return extension->GetExtensionSupport()->GetExtensionFile(resource_path, extension->GetFramesDocument()->GetURL(), return_value, this_object);
}

/* static */ int
DOM_ExtensionBackground::getScreenshot(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION_BACKGROUND, DOM_ExtensionBackground);
	return extension->m_extension_support->GetScreenshot(extension->GetFramesDocument(), argv, argc, return_value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_ExtensionBackground::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_onconnect:
		DOMSetObject(value, m_onconnect_handler);
		return GET_SUCCESS;

	case OP_ATOM_ondisconnect:
		DOMSetObject(value, m_ondisconnect_handler);
		return GET_SUCCESS;

	case OP_ATOM_onmessage:
		DOMSetObject(value, m_onmessage_handler);
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionBackground::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_onconnect:
		PUT_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(this, m_onconnect_handler, NULL, value, origining_runtime, UNI_L("onconnect"), ONCONNECT));
		return PUT_SUCCESS;
	case OP_ATOM_ondisconnect:
		PUT_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(this, m_ondisconnect_handler, NULL, value, origining_runtime, UNI_L("ondisconnect"), ONDISCONNECT));
		return PUT_SUCCESS;
	case OP_ATOM_onmessage:
		PUT_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(m_port, m_onmessage_handler, NULL, value, origining_runtime, UNI_L("onmessage"), ONMESSAGE));
		return PUT_SUCCESS;
	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

#include "modules/dom/src/domglobaldata.h"
DOM_FUNCTIONS_START(DOM_ExtensionBackground)
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionBackground, DOM_ExtensionBackground::broadcastMessage, "broadcastMessage", "")
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionBackground, DOM_ExtensionBackground::getFile, "getFile", "s-")
DOM_FUNCTIONS_END(DOM_ExtensionBackground)

DOM_FUNCTIONS_WITH_DATA_START(DOM_ExtensionBackground)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionBackground, DOM_ExtensionBackground::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionBackground, DOM_ExtensionBackground::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_ExtensionBackground)

#endif // EXTENSION_SUPPORT
