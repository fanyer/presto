/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextensionuipopup.h"
#include "modules/dom/src/extensions/domextensionuiitem.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/winman.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/domutils.h"

/* static */ OP_STATUS
DOM_ExtensionUIPopup::Make(DOM_ExtensionUIPopup *&new_obj, ES_Object *properties, DOM_ExtensionUIItem *owner, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionUIPopup, ()), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_UIPOPUP_PROTOTYPE), "Popup"));

	return new_obj->Initialize(properties, owner, return_value, origining_runtime);
}

static BOOL GetPixelValue(unsigned int &pixel_value, ES_Value &value)
{
	if (value.type == VALUE_NUMBER)
	{
		double val = value.value.number;
		if (op_isfinite(val) && val > 0 && val < static_cast<double>(UINT_MAX))
		{
			pixel_value = static_cast<unsigned int>(val);
			return TRUE;
		}
	}
	else if (value.type == VALUE_STRING)
	{
		const uni_char *str_value = value.value.string;
		unsigned int str_len = uni_strlen(str_value);
		if (str_len > 2 && str_len < 15)
		{
			if (str_value[str_len - 1] == 'x' && str_value[str_len - 2] == 'p')
			{
				int px_value = uni_atoi(str_value);
				if (px_value > 0 && px_value <= INT_MAX)
				{
					pixel_value = static_cast<unsigned int>(px_value);
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

OP_STATUS
DOM_ExtensionUIPopup::ResolvePopupURL(const uni_char *popup_href_string)
{
	URL origin_url = GetRuntime()->GetOriginURL();
	URL popup_url = g_url_api->GetURL(origin_url, popup_href_string);

	if (popup_url.Type() == URL_NULL_TYPE || popup_url.Type() == URL_UNKNOWN || DOM_Utils::IsOperaIllegalURL(popup_url))
		RETURN_IF_ERROR(UniSetConstStr(m_href, NULL));
	else
		RETURN_IF_ERROR(UniSetConstStr(m_href, popup_url.GetAttribute(URL::KUniName_With_Fragment, URL::KNoRedirect).CStr()));

	return OpStatus::OK;
}

OP_STATUS
DOM_ExtensionUIPopup::Initialize(ES_Object *properties, DOM_ExtensionUIItem *owner, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	OP_BOOLEAN result;
	ES_Value value;

	m_owner = owner;
	RETURN_IF_ERROR(result = origining_runtime->GetName(properties, UNI_L("height"), &value));
	m_height = DEFAULT_POPUP_HEIGHT;
	if (result == OpBoolean::IS_TRUE && !GetPixelValue(m_height, value))
	{
		owner->CallDOMException(DOM_Object::TYPE_MISMATCH_ERR, return_value);
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(result = origining_runtime->GetName(properties, UNI_L("width"), &value));
	m_width = DEFAULT_POPUP_WIDTH;
	if (result == OpBoolean::IS_TRUE && !GetPixelValue(m_width, value))
	{
		owner->CallDOMException(DOM_Object::TYPE_MISMATCH_ERR, return_value);
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(result = origining_runtime->GetName(properties, UNI_L("href"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING)
		RETURN_IF_ERROR(ResolvePopupURL(value.value.string));

	m_window_id = DEFAULT_POPUP_WINDOW;
	RETURN_IF_ERROR(result = origining_runtime->GetName(properties, UNI_L("window"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
	{
		DOM_Object *window_object = DOM_GetHostObject(value.value.object);
		if (window_object && window_object->IsA(DOM_TYPE_WINDOW))
		{
			JS_Window *window = static_cast<JS_Window*>(window_object);
			if (window->GetEnvironment() && window->GetEnvironment()->GetFramesDocument())
				m_window_id = window->GetEnvironment()->GetFramesDocument()->GetWindow()->Id();
		}
	}

	return OpStatus::OK;
}

DOM_ExtensionUIPopup::DOM_ExtensionUIPopup()
	: m_owner(NULL)
	, m_href(NULL)
	, m_window_id(DEFAULT_POPUP_WINDOW)
	, m_width(0)
	, m_height(0)
{
}

DOM_ExtensionUIPopup::~DOM_ExtensionUIPopup()
{
	OP_NEW_DBG("DOM_ExtensionUIPopup::~DOM_ExtensionUIPopup()", "extensions.dom");
	OP_DBG(("this: %p href: %s", this, m_href));

	OP_DELETEA(m_href);
}

/* static */ int
DOM_ExtensionUIPopup::update(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(popup, DOM_TYPE_EXTENSION_UIPOPUP, DOM_ExtensionUIPopup);

	if (argc < 0)
	{
		popup->m_owner->SetBlockedThread(NULL);
		popup->m_owner->GetThreadListener()->Remove();
		OpExtensionUIListener::ItemAddedStatus call_status = popup->m_owner->GetAddedCallStatus();
		popup->m_owner->ResetCallStatus();

		return DOM_ExtensionUIItem::HandleItemAddedStatus(this_object, call_status, return_value);
	}

	DOM_CHECK_ARGUMENTS("o");

	ES_Object *properties = argv[0].value.object;
	DOM_Runtime *runtime = popup->GetRuntime();

	BOOL has_changed = FALSE;
	OP_BOOLEAN result;
	ES_Value value;

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("height"), &value));
	if (result == OpBoolean::IS_TRUE && !GetPixelValue(popup->m_height, value))
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	else if (result == OpBoolean::IS_TRUE)
		has_changed = TRUE;

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("width"), &value));
	if (result == OpBoolean::IS_TRUE && !GetPixelValue(popup->m_width, value))
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	else if (result == OpBoolean::IS_TRUE)
		has_changed = TRUE;

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("href"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING)
	{
		has_changed = TRUE;
		CALL_FAILED_IF_ERROR(popup->ResolvePopupURL(value.value.string));
	}

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("window"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
	{
		has_changed = TRUE;
		DOM_Object *window_object = DOM_GetHostObject(value.value.object);
		if (window_object && window_object->IsA(DOM_TYPE_WINDOW))
		{
			JS_Window *window = static_cast<JS_Window*>(window_object);
			if (window->GetEnvironment() && window->GetEnvironment()->GetFramesDocument())
				popup->m_window_id = window->GetEnvironment()->GetFramesDocument()->GetWindow()->Id();
		}
	}

	if (has_changed)
	{
		CALL_INVALID_IF_ERROR(popup->m_owner->NotifyItemUpdate(origining_runtime, popup->m_owner));
		return (ES_SUSPEND | ES_RESTART);
	}
	else
		return ES_FAILED;
}

/* virtual */ ES_GetState
DOM_ExtensionUIPopup::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_href:
		if (m_href)
			DOMSetString(value, m_href);
		else
			DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_window:
	{
		DOMSetNull(value);

		Window *window = NULL;
		if (m_window_id != DEFAULT_POPUP_WINDOW && (window = g_windowManager->GetWindow(m_window_id)))
		{
			DocumentManager *docman = window->DocManager();
			GET_FAILED_IF_ERROR(docman->ConstructDOMProxyEnvironment());
			DOM_ProxyEnvironmentImpl *environment = static_cast<DOM_ProxyEnvironmentImpl*>(docman->GetDOMEnvironment());
			DOM_Object *proxy_window;
			GET_FAILED_IF_ERROR(environment->GetProxyWindow(proxy_window, origining_runtime));
			DOMSetObject(value, proxy_window);
		}

		return GET_SUCCESS;
	}

	case OP_ATOM_width:
		DOMSetNumber(value, m_width);
		return GET_SUCCESS;

	case OP_ATOM_height:
		DOMSetNumber(value, m_height);
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionUIPopup::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_href:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (!m_href || !uni_str_eq(value->value.string, m_href))
		{
			PUT_FAILED_IF_ERROR(ResolvePopupURL(value->value.string));
			PUT_FAILED_IF_ERROR_EXCEPTION(m_owner->NotifyItemUpdate(origining_runtime, m_owner), INVALID_STATE_ERR);
			DOMSetNull(value);
			return PUT_SUSPEND;
		}
		else
			return PUT_SUCCESS;

	case OP_ATOM_window:
		if (value->type == VALUE_OBJECT)
		{
			if (DOM_Object *window_object = DOM_GetHostObject(value->value.object))
			{
				m_window_id = DEFAULT_POPUP_WINDOW;
				if (window_object->IsA(DOM_TYPE_WINDOW))
				{
					JS_Window *window = static_cast<JS_Window*>(window_object);
					if (window->GetEnvironment() && window->GetEnvironment()->GetFramesDocument())
						m_window_id = window->GetEnvironment()->GetFramesDocument()->GetWindow()->Id();
				}
				PUT_FAILED_IF_ERROR_EXCEPTION(m_owner->NotifyItemUpdate(origining_runtime, m_owner), INVALID_STATE_ERR);
				DOMSetNull(value);
				return PUT_SUSPEND;
			}
		}
		return PUT_SUCCESS;

	case OP_ATOM_width:
		if (GetPixelValue(m_width, *value))
		{
			PUT_FAILED_IF_ERROR_EXCEPTION(m_owner->NotifyItemUpdate(origining_runtime, m_owner), INVALID_STATE_ERR);
			DOMSetNull(value);
			return PUT_SUSPEND;
		}
		return PUT_SUCCESS;
	case OP_ATOM_height:
		if (GetPixelValue(m_height, *value))
		{
			PUT_FAILED_IF_ERROR_EXCEPTION(m_owner->NotifyItemUpdate(origining_runtime, m_owner), INVALID_STATE_ERR);
			DOMSetNull(value);
			return PUT_SUSPEND;
		}
		return PUT_SUCCESS;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionUIPopup::PutNameRestart(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	switch (property_name)
	{
	case OP_ATOM_height:
	case OP_ATOM_width:
	case OP_ATOM_window:
	case OP_ATOM_href:
		{
			m_owner->SetBlockedThread(NULL);
			m_owner->GetThreadListener()->Remove();
			OpExtensionUIListener::ItemAddedStatus call_status = m_owner->GetAddedCallStatus();
			m_owner->ResetCallStatus();

			return ConvertCallToPutName(DOM_ExtensionUIItem::HandleItemAddedStatus(this, call_status, value), value);
		}
	default:
		return DOM_Object::PutNameRestart(property_name, value, origining_runtime, restart_object);
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_ExtensionUIPopup)
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionUIPopup, DOM_ExtensionUIPopup::update, "update", "o-")
DOM_FUNCTIONS_END(DOM_ExtensionUIPopup)

#endif // EXTENSION_SUPPORT
