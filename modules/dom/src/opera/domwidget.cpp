/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef GADGET_SUPPORT

#include "modules/dom/src/opera/domwidget.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/dommutationevent.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/gadgets/OpGadget.h"

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
#include "modules/database/opstorage.h"
#endif

/* static */ OP_STATUS
DOM_Widget::Make(DOM_Widget *&new_obj, DOM_Runtime *origining_runtime, OpGadget *gadget, BOOL force_admin_privileges)
{
	new_obj = OP_NEW(DOM_Widget, (gadget));

	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::WIDGET_PROTOTYPE), "Widget"));

	if (gadget->SupportsOperaExtendedAPI())
		RETURN_IF_LEAVE(new_obj->InjectOperaExtendedApiL());

#ifdef SELFTEST
	if (g_selftest.running)
		RETURN_IF_LEAVE(new_obj->AddFunctionL(dumpWidget, "dumpWidget"));
#endif // SELFTESTS_SUPPORT


#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
	BOOL allow_mutate_policy = FALSE;

	OpSecurityContext source(origining_runtime);
	OpSecurityContext target(gadget);

	RETURN_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::GADGET_DOM_MUTATE_POLICY, source, target, allow_mutate_policy));

	if (allow_mutate_policy)
		RETURN_IF_LEAVE(new_obj->AddFunctionL(setGlobalPolicy, "setGlobalPolicy", "s-"));
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

#ifdef WEBSERVER_SUPPORT
	// Enable administrative privileges if forced by creator, or
	// if coming from an administration URL.
	if (gadget->IsSubserver())
	{
		new_obj->m_admin_privileges = force_admin_privileges;
		/*
		 * Enable this code to automatically enable privileges for the
		 * widget interface if coming from a URL with KIsUniteServiceAdminURL
		 * set. Doing this likely to create a security hole.
		if (origining_runtime && origining_runtime->GetFramesDocument() &&
		    origining_runtime->GetFramesDocument()->GetURL().GetAttribute(URL::KIsUniteServiceAdminURL))
			new_obj->m_admin_privileges = TRUE;
		 */
	}
	else
#endif
		OP_ASSERT(!force_admin_privileges);

	return OpStatus::OK;
}

void
DOM_Widget::InjectOperaExtendedApiL()
{

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	AddFunctionL(setPreferenceForKey, "setPreferenceForKey", "Ss-");
	AddFunctionL(preferenceForKey, "preferenceForKey", "s-");
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT

	AddFunctionL(prepareForTransition, "prepareForTransition", "s-");
	AddFunctionL(performTransition, "performTransition", "-");
	AddFunctionL(openURL, "openURL", "s-"); // Opera
	AddFunctionL(openURL, "openURI", "s-"); // W3C
	AddFunctionL(getAttention, "getAttention", NULL);
	AddFunctionL(showNotification, "showNotification", "s-");
	AddFunctionL(show, "show", NULL);
	AddFunctionL(hide, "hide", NULL);
#ifdef GADGET_BADGE_SUPPORT
	AddFunctionL(setBadge, "setBadge", "s");
#endif // GADGET_BADGE_SUPPORT
	AddFunctionL(DOM_Node::dispatchEvent, "dispatchEvent", NULL);
	AddFunctionL(hasFeature, "hasFeature", "s-");
	AddFunctionL(setIcon, "setIcon", "s-");

	AddFunctionL(DOM_Node::accessEventListener, 0, "addEventListener", "s-b-");
	AddFunctionL(DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-");

	PutNumericConstantL(GetNativeObject(), "VIEW_HIDDEN", VIEW_HIDDEN /* 0 */, GetRuntime());
	PutNumericConstantL(GetNativeObject(), "VIEW_DEFAULT", VIEW_DEFAULT /* 1 */, GetRuntime());
	PutNumericConstantL(GetNativeObject(), "VIEW_WINDOW", VIEW_WINDOW /* 2 */, GetRuntime());
	PutNumericConstantL(GetNativeObject(), "VIEW_FULLSCREEN", VIEW_FULLSCREEN /* 3 */, GetRuntime());

}

OP_STATUS
DOM_Widget::InjectWidgetManagerApi()
{
	RETURN_IF_LEAVE(InjectWidgetManagerApiL());
	return OpStatus::OK;
}

void
DOM_Widget::InjectWidgetManagerApiL()
{
	AddFunctionL(DOM_Widget::setExtensionProperty, "setExtensionProperty", "sb-");
}

DOM_Widget::DOM_Widget(OpGadget *gadget)
	: m_dragstart_handler(NULL)
	, m_dragstop_handler(NULL)
	, m_show_handler(NULL)
	, m_hide_handler(NULL)
	, m_shownotification_handler(NULL)
	, m_viewstate_handler(NULL)
	, m_beforeupdate_handler(NULL)
	, m_afterupdate_handler(NULL)
	, m_current_icon(NULL)
	, m_icons(NULL)
	, m_gadget(gadget)
#ifdef WEBSERVER_SUPPORT
	, m_admin_privileges(FALSE)
#endif
{
}

/* virutal */
DOM_Widget::~DOM_Widget()
{
}

OP_STATUS
DOM_Widget::CreateIconsArray()
{
	OP_ASSERT(!m_icons);

	UINT32 c = m_gadget->GetGadgetIconCount();
	if (c > 0)
	{
		RETURN_IF_ERROR(GetRuntime()->CreateNativeArrayObject(&m_icons, c));

		for (UINT32 i = 0; i < c; i++)
		{
			OpString src;
			INT32 w, h;
			RETURN_IF_ERROR(m_gadget->GetGadgetIcon(i, src, w, h, FALSE));
			DOM_WidgetIcon *obj;
			RETURN_IF_ERROR(DOM_WidgetIcon::Make(obj, GetRuntime(), w, h, src.CStr()));
			ES_Value val;
			val.type = VALUE_OBJECT;
			val.value.object = obj->GetNativeObject();
			RETURN_IF_ERROR(GetRuntime()->PutIndex(m_icons, i, val));
		}
	}

	return OpStatus::OK;
}

ES_Object **
DOM_Widget::GetHandlerVariableAddress (OpAtom property_name)
{
	switch(property_name)
	{
		case OP_ATOM_ondragstart:
			return  &m_dragstart_handler;
		case OP_ATOM_ondragstop:
			return  &m_dragstop_handler;
		case OP_ATOM_onshow:
			return  &m_show_handler;
		case OP_ATOM_onhide:
			return  &m_hide_handler;
		case OP_ATOM_onviewstatechanged:
			return &m_viewstate_handler;
		case OP_ATOM_onbeforeupdate:
			return &m_beforeupdate_handler;
		case OP_ATOM_onafterupdate:
			return &m_afterupdate_handler;
		default:
			return NULL;
	}
}

/* virtual */ ES_GetState
DOM_Widget::GetName(OpAtom property_name, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	ES_GetState state = GetNameW3C(property_name, return_value, origining_runtime);
	if (state == GET_FAILED && m_gadget->SupportsOperaExtendedAPI())
		state = GetNameOperaExt(property_name, return_value, origining_runtime);
#ifdef WEBSERVER_SUPPORT
	if (state == GET_FAILED && m_gadget->IsSubserver())
		state = GetNameWebserver(property_name, return_value, origining_runtime);
#endif //WEBSERVER_SUPPORT
	if (state == GET_FAILED)
		state = DOM_Object::GetName(property_name, return_value, origining_runtime);

	return state;
}

/* virtual */ ES_PutState
DOM_Widget::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	ES_PutState state = PutNameW3C(property_name, value, origining_runtime);
	if (state == PUT_FAILED && m_gadget->SupportsOperaExtendedAPI())
		state = PutNameOperaExt(property_name, value, origining_runtime);
#ifdef WEBSERVER_SUPPORT
	if (state == PUT_FAILED && m_gadget->IsSubserver())
		state = PutNameWebserver(property_name, value, origining_runtime);
#endif //WEBSERVER_SUPPORT
	if (state == PUT_FAILED)
		state = DOM_Object::PutName(property_name, value, origining_runtime);

	return state;
}

GadgetStringAttribute
DOM_Widget::GetGadgetStringPropForAtom(OpAtom property_name)
{
	switch (property_name)
	{
	case OP_ATOM_name:
		return WIDGET_NAME_TEXT;
	case OP_ATOM_shortName:
		return WIDGET_NAME_SHORT;
	case OP_ATOM_description:
		return WIDGET_DESCRIPTION_TEXT;
	case OP_ATOM_version:
		return WIDGET_ATTR_VERSION;
	case OP_ATOM_id:
		return WIDGET_ATTR_ID;
	case OP_ATOM_author:
		return WIDGET_AUTHOR_TEXT;
	case OP_ATOM_authorEmail:
		return WIDGET_AUTHOR_ATTR_EMAIL;
	case OP_ATOM_authorHref:
		return WIDGET_AUTHOR_ATTR_HREF;
	}
	OP_ASSERT(FALSE);
	return LAST_STRING_ATTRIBUTE;
}

/* virtual */ ES_GetState
DOM_Widget::GetNameW3C(OpAtom property_name, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case OP_ATOM_preferences:
		if (m_gadget->GetClass()->SupportsNamespace(GADGETNS_W3C_1_0))
		{
			ES_GetState state = DOMSetPrivate(return_value, DOM_PRIVATE_preferences);

			if (state == GET_FAILED)
			{
				GET_FAILED_IF_ERROR(m_gadget->InitializePreferences(OpGadget::PREFS_APPLY_FIRST_ACCESS));

				DOM_Storage *storage = NULL;

				GET_FAILED_IF_ERROR(DOM_Storage::Make(storage, WEB_STORAGE_WGT_PREFS, GetRuntime()));

				if (m_gadget->IsExtension())
				{
					OpString gadget_url;
					GET_FAILED_IF_ERROR(m_gadget->GetGadgetUrl(gadget_url, FALSE, TRUE));
					GET_FAILED_IF_ERROR(storage->SetOverrideOrigin(gadget_url.CStr(), m_gadget->UrlContextId()));
				}

				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_preferences, *storage));
				DOMSetObject(return_value, storage);
				return GET_SUCCESS;
			}
			else
				return state;
		}
		return GET_FAILED;
	case OP_ATOM_width:
	case OP_ATOM_height:
		{
			FramesDocument *frames_doc = origining_runtime->GetFramesDocument();
			if (frames_doc && frames_doc->GetWindow())
			{
				int w, h;
				frames_doc->GetWindow()->GetClientSize(w, h);

				if (property_name == OP_ATOM_width)
					DOMSetNumber(return_value, w);
				else
					DOMSetNumber(return_value, h);

			}
			else
				DOMSetNumber(return_value, 0.0);

			return GET_SUCCESS;
		}
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case OP_ATOM_name:
	case OP_ATOM_shortName:
	case OP_ATOM_description:
	case OP_ATOM_version:
	case OP_ATOM_id:
	case OP_ATOM_author:
	case OP_ATOM_authorEmail:
	case OP_ATOM_authorHref:
		DOMSetString(return_value, m_gadget->GetAttribute(GetGadgetStringPropForAtom(property_name)));
		return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_Widget::PutNameW3C(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_author:
	case OP_ATOM_authorEmail:
	case OP_ATOM_authorHref:
	case OP_ATOM_name:
	case OP_ATOM_shortName:
	case OP_ATOM_description:
	case OP_ATOM_version:
	case OP_ATOM_id:
	case OP_ATOM_width:
	case OP_ATOM_height:
		return PUT_READ_ONLY;
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case OP_ATOM_preferences:
		if (m_gadget->GetClass()->SupportsNamespace(GADGETNS_W3C_1_0))
			return PUT_SUCCESS;
		break;
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT
	}
	return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_Widget::GetNameOperaExt(OpAtom property_name, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	TempBuffer *buffer = GetEmptyTempBuf();
	OpString result;
	ES_Object **handler_var = GetHandlerVariableAddress(property_name);
	if (handler_var)
	{
		DOMSetObject(return_value, *handler_var);
		return GET_SUCCESS;
	}

	switch (property_name)
	{
	case OP_ATOM_locale:
		DOMSetString(return_value, m_gadget->GetLocale());
		return GET_SUCCESS;

	case OP_ATOM_identifier:
		DOMSetString(return_value, m_gadget->GetIdentifier());
		return GET_SUCCESS;

	case OP_ATOM_currentIcon:
		DOMSetObject(return_value, m_current_icon);
		return GET_SUCCESS;

	case OP_ATOM_icons:
		if (!m_icons)
			GET_FAILED_IF_ERROR(CreateIconsArray());
		DOMSetObject(return_value, m_icons);
		return GET_SUCCESS;

	case OP_ATOM_widgetMode:
		GET_FAILED_IF_ERROR(m_gadget->GetMode(result));
		GET_FAILED_IF_ERROR(buffer->Append(result.CStr()));
		DOMSetString(return_value, buffer);
		return GET_SUCCESS;

	case OP_ATOM_originURL:
		GET_FAILED_IF_ERROR(m_gadget->GetGadgetDownloadUrl(result));
		GET_FAILED_IF_ERROR(buffer->Append(result.CStr()));
		DOMSetString(return_value, buffer);
		return GET_SUCCESS;

	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_Widget::PutNameOperaExt(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	ES_Object **handler_var = GetHandlerVariableAddress(property_name);
	if (handler_var)
	{
		if (value->type == VALUE_OBJECT)
			*handler_var = value->value.object;
		else
			*handler_var = NULL;

		return PUT_SUCCESS;
	}
	switch(property_name)
	{
	case OP_ATOM_locale:
	case OP_ATOM_identifier:
	case OP_ATOM_currentIcon:

	case OP_ATOM_originURL:
		return PUT_READ_ONLY;

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case OP_ATOM_preferences:
		if (m_gadget->GetClass()->SupportsNamespace(GADGETNS_W3C_1_0))
			return PUT_SUCCESS;
		break;
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT

	case OP_ATOM_widgetMode:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
		{
			OpString strMode;
			strMode.Set(value->value.string);
			if(OpStatus::IsError(m_gadget->SetMode(strMode)))
				return PUT_FAILED;

			return PUT_SUCCESS;
		}
		break;
	}
	return PUT_FAILED;
}

#ifdef WEBSERVER_SUPPORT
/* Extended interface for Unite widgets. */
/* virtual */ ES_GetState
DOM_Widget::GetNameWebserver(OpAtom property_name, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_servicePath:
		{
			const uni_char *servicepath = m_gadget->GetAttribute(WIDGET_SERVICEPATH_TEXT);
			DOMSetString(return_value, servicepath);
			return GET_SUCCESS;
		}

	case OP_ATOM_visibleToLocalNetwork:
		DOMSetBoolean(return_value, m_gadget->IsVisibleUPNP());
		return GET_SUCCESS;

	case OP_ATOM_visibleToSearchEngines:
		DOMSetBoolean(return_value, m_gadget->IsVisibleRobots());
		return GET_SUCCESS;

	case OP_ATOM_visibleToServiceDiscovery:
		DOMSetBoolean(return_value, m_gadget->IsVisibleASD());
		return GET_SUCCESS;

	case OP_ATOM_sharedFolder:
		if (m_admin_privileges)
		{
			DOMSetString(return_value, m_gadget->GetSharedFolder());
			return GET_SUCCESS;
		}
		break;
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_Widget::PutNameWebserver(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	/* Extended interface for Unite widgets. */
	case OP_ATOM_servicePath:
		if (m_gadget->IsSubserver())
			return PUT_READ_ONLY;
		break;

	case OP_ATOM_visibleToLocalNetwork:
		if (m_gadget->IsSubserver())
		{
			if (value->type != VALUE_BOOLEAN)
				return PUT_NEEDS_BOOLEAN;

			m_gadget->SetVisibleUPNP(value->value.boolean);
			return PUT_SUCCESS;
		}
		break;

	case OP_ATOM_visibleToSearchEngines:
		if (m_gadget->IsSubserver())
		{
			if (value->type != VALUE_BOOLEAN)
				return PUT_NEEDS_BOOLEAN;

			m_gadget->SetVisibleRobots(value->value.boolean);
			return PUT_SUCCESS;
		}
		break;

	case OP_ATOM_visibleToServiceDiscovery:
		if (m_gadget->IsSubserver())
		{
			if (value->type != VALUE_BOOLEAN)
				return PUT_NEEDS_BOOLEAN;

			m_gadget->SetVisibleASD(value->value.boolean);
			return PUT_SUCCESS;
		}
		break;

	case OP_ATOM_sharedFolder:
		if (m_gadget->IsSubserver() && m_admin_privileges)
		{
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

			OpStatus::Ignore(m_gadget->SetSharedFolder(value->value.string));
			return PUT_SUCCESS;
		}
		break;
	}
	return PUT_FAILED;
}
#endif //WEBSERVER_SUPPORT

/* virtual */ void
DOM_Widget::GCTrace()
{
	/* From DOM_EventTargetOwner: */
	GCMark(event_target);

	GCMark(m_dragstart_handler);
	GCMark(m_dragstop_handler);
	GCMark(m_show_handler);
	GCMark(m_hide_handler);
	GCMark(m_shownotification_handler);
	GCMark(m_viewstate_handler);
	GCMark(m_beforeupdate_handler);
	GCMark(m_afterupdate_handler);
	GCMark(m_current_icon);
	GCMark(m_icons);
}

OP_STATUS
DOM_Widget::HandleEvent(DOM_Environment::GadgetEvent event, DOM_Environment::GadgetEventData *data)
{
	ES_Object* handler = NULL;

	switch(event)
	{
	case DOM_Environment::GADGET_EVENT_ONDRAGSTART:
		handler = m_dragstart_handler;
		break;
	case DOM_Environment::GADGET_EVENT_ONDRAGSTOP:
		handler = m_dragstop_handler;
		break;
	case DOM_Environment::GADGET_EVENT_ONSHOW:
		handler = m_show_handler;
		break;
	case DOM_Environment::GADGET_EVENT_ONHIDE:
		handler = m_hide_handler;
		break;
	case DOM_Environment::GADGET_EVENT_ONSHOWNOTIFICATIONFINISHED:
		handler = m_shownotification_handler;
		break;
	case DOM_Environment::GADGET_EVENT_ONMODECHANGE:
	{
		DOM_WidgetModeChangeEvent *widget_mode_change_event;
		if(data && OpStatus::IsSuccess(DOM_WidgetModeChangeEvent::Make(widget_mode_change_event, this, data->mode)))
		{
			return this->GetEnvironment()->SendEvent(widget_mode_change_event);
		}
		return OpStatus::ERR;
	}
	case DOM_Environment::GADGET_EVENT_ONRESOLUTION:
	{
		DOM_ResolutionEvent *resolution_event;
		if(data && OpStatus::IsSuccess(DOM_ResolutionEvent::Make(resolution_event, this, data->screen_width, data->screen_height)))
		{
			return this->GetEnvironment()->SendEvent(resolution_event);
		}
		return OpStatus::ERR;
	}
	}

	if (handler)
	{
		ES_AsyncInterface* asyncif = GetEnvironment()->GetAsyncInterface();

		RETURN_IF_ERROR(asyncif->CallFunction(handler, *this, 0, NULL));
	}

	return OpStatus::OK;
}

#ifdef SELFTEST
/* static */ int
DOM_Widget::dumpWidget(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(widget, DOM_TYPE_WIDGET, DOM_Widget);

	TempBuffer *buf = GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(widget->m_gadget->GetClass()->DumpConfiguration(buf));

	DOMSetString(return_value, buf);
	return ES_VALUE;
}
#endif // SELFTEST

/* static */ int
DOM_Widget::setPreferenceForKey(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(widget, DOM_TYPE_WIDGET, DOM_Widget);
	DOM_CHECK_ARGUMENTS("Ss");

	const uni_char *data = argv[0].type == VALUE_STRING ? argv[0].value.string : NULL;
	const uni_char *key  = argv[1].value.string;

	OP_STATUS err = widget->m_gadget->SetPersistentData(key, data);

	if (OpStatus::IsMemoryError(err))
		return ES_NO_MEMORY;

	DOMSetBoolean(return_value, OpStatus::IsSuccess(err));
	return ES_VALUE;
}

/* static */ int
DOM_Widget::preferenceForKey(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(widget, DOM_TYPE_WIDGET, DOM_Widget);
	DOM_CHECK_ARGUMENTS("s");

	const uni_char *key = argv[0].value.string;
	const uni_char *result;

	result = widget->m_gadget->GetPersistentData(key);

	DOMSetString(return_value, result);
	return ES_VALUE;
}

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
/* static */ int
DOM_Widget::setGlobalPolicy(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(widget, DOM_TYPE_WIDGET, DOM_Widget);
	DOM_CHECK_ARGUMENTS("|s");

	if (argc >= 1)
		CALL_FAILED_IF_ERROR(OpSecurityManager_Gadget::SetGlobalGadgetPolicy(widget->GetGadget(), argv->value.string));
	else
		OpSecurityManager_Gadget::ClearGlobalGadgetPolicy(widget->GetGadget());

	return ES_FAILED; // no return value
}
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

/* static */ int
DOM_Widget::prepareForTransition(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGET);
	DOM_CHECK_ARGUMENTS("s");

	// Does nothing for the moment.

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_Widget::performTransition(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGET);
	DOM_CHECK_ARGUMENTS("");

	// Does nothing for the moment.

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_Widget::openURL(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(widget, DOM_TYPE_WIDGET, DOM_Widget);
	DOM_CHECK_ARGUMENTS("s");

	const uni_char *url = argv[0].value.string;

	widget->m_gadget->OpenURLInNewWindow(url);

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_Widget::getAttention(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(widget, DOM_TYPE_WIDGET, DOM_Widget);

	widget->m_gadget->GetAttention();

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_Widget::showNotification(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(widget, DOM_TYPE_WIDGET, DOM_Widget);

	// Opera Widgets specify one string argument before the callback.
	DOM_CHECK_ARGUMENTS("s");

	int callback_offset = 1;
	const uni_char *message = argv[0].value.string;

	OpString w3c_message;
	if (widget->m_gadget->GetClass()->SupportsNamespace(GADGETNS_W3C_1_0))
	{
		// W3C Widgets APIs & Events specify two string arguments before the
		// callback. Since the WindowCommander interface only take one, we
		// concatenate them into one here.
		DOM_CHECK_ARGUMENTS("ss");

		if (OpStatus::IsError(w3c_message.SetConcat(argv[0].value.string, UNI_L("\n\n"), argv[1].value.string)))
		{
			return ES_FAILED;
		}

		message = w3c_message.CStr();
		callback_offset = 2;
	}

	widget->m_shownotification_handler = (argc > callback_offset && argv[callback_offset].type == VALUE_OBJECT) ? argv[callback_offset].value.object  : NULL;

	CALL_FAILED_IF_ERROR(widget->m_gadget->ShowNotification(message));

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_Widget::show(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGET);

	origining_runtime->GetFramesDocument()->GetWindow()->GetWindowCommander()->GetOpWindow()->Show(TRUE);

	return ES_FAILED; // no return value.
}

/* static */ int
DOM_Widget::hide(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGET);

	origining_runtime->GetFramesDocument()->GetWindow()->GetWindowCommander()->GetOpWindow()->Show(FALSE);

	return ES_FAILED; // No return value.
}

#ifdef GADGET_BADGE_SUPPORT
/* static */ int
DOM_Widget::setBadge(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGET);
	DOM_CHECK_ARGUMENTS("s");

	const uni_char *badge = argv[0].value.string;
	m_gadget->SetBadge(badge);

	return ES_FAILED; // no return value
}
#endif // GADGET_BADGE_SUPPORT

/* static */ int
DOM_Widget::hasFeature(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(widget, DOM_TYPE_WIDGET, DOM_Widget);
	DOM_CHECK_ARGUMENTS("s");

	BOOL has_feature = FALSE;

	has_feature = widget->m_gadget->HasFeature(argv[0].value.string);

	DOMSetBoolean(return_value, has_feature);
	return ES_VALUE;
}

/* static */ int
DOM_Widget::setIcon(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(widget, DOM_TYPE_WIDGET, DOM_Widget);
	DOM_CHECK_ARGUMENTS("s");

	widget->m_gadget->SetIcon(argv[0].value.string);

	return ES_FAILED; // No return value.
}

/** gadget.setExtensionProperty(property, value) is used to alter a configuration
  * setting for an extension (gadget.)
  *
  * @param gadget the gadget object.
  * @param property currently either 'allow_https' or 'allow_private',
  *        controlling if an extension is allowed to activate UserJS
  *        on documents served out over HTTPS, or within a window
  *        operating in privacy mode.
  * @param value the new configuration value; both properties are flags, hence
  *        a boolean.
  * @return fails with the DOM exception WRONG_THIS_ERR if not passed a
  *         a widget as 'this'. Otherwise, ES_FAILED to indicate no result.
  *         Note that no error is raised if you attempt to set an unrecognised
  *         property.
  */
/* static */ int
DOM_Widget::setExtensionProperty(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
#ifdef EXTENSION_SUPPORT
	DOM_THIS_OBJECT(widget, DOM_TYPE_WIDGET, DOM_Widget);
	DOM_CHECK_ARGUMENTS("sb");
	if (widget->m_gadget->IsExtension())
	{
		if (uni_str_eq(argv[0].value.string, "allow_https"))
			widget->m_gadget->SetExtensionFlag(OpGadget::AllowUserJSHTTPS, argv[1].value.boolean);
		else if (uni_str_eq(argv[0].value.string, "allow_private"))
			widget->m_gadget->SetExtensionFlag(OpGadget::AllowUserJSPrivate, argv[1].value.boolean);
	}
#endif // EXTENSION_SUPPORT

	return ES_FAILED; // No return value.
}

OP_STATUS
DOM_WidgetIcon::Make(DOM_WidgetIcon *&new_obj, DOM_Runtime *origining_runtime, INT32 w, INT32 h, const uni_char *src)
{
	new_obj = OP_NEW(DOM_WidgetIcon, (w, h));
	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(new_obj->Initialize(src)))
	{
		OP_DELETE(new_obj);
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::WIDGETICON_PROTOTYPE), "WidgetIcon"));

	return OpStatus::OK;
}

DOM_WidgetIcon::DOM_WidgetIcon(INT32 width, INT32 height)
: width(width)
, height(height)
, active(FALSE)
{
}

OP_STATUS
DOM_WidgetIcon::Initialize(const uni_char *src)
{
	return this->src.Set(src);
}

/* virtual */
DOM_WidgetIcon::~DOM_WidgetIcon()
{
}

/* virtual */ ES_GetState
DOM_WidgetIcon::GetName(OpAtom property_name, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_active:
		DOMSetBoolean(return_value, active);
		return GET_SUCCESS;
	case OP_ATOM_src:
		DOMSetString(return_value, src.CStr());
		return GET_SUCCESS;
	case OP_ATOM_width:
		DOMSetNumber(return_value, static_cast<double>(width));
		return GET_SUCCESS;
	case OP_ATOM_height:
		DOMSetNumber(return_value, static_cast<double>(width));
		return GET_SUCCESS;
	default:
		return GET_FAILED;

	};
}

/* virtual */ ES_PutState
DOM_WidgetIcon::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_src:
	case OP_ATOM_width:
	case OP_ATOM_height:
		return PUT_READ_ONLY;

	case OP_ATOM_active:
		// Currently not supported.
		return PUT_SUCCESS;

	default:
		return PUT_FAILED;
	}
}

/* static */ OP_STATUS
DOM_WidgetModeChangeEvent::Make(DOM_WidgetModeChangeEvent *&event, DOM_Object *target, const uni_char *widgetMode)
{
	event = OP_NEW(DOM_WidgetModeChangeEvent, ());
	DOM_Runtime *runtime = target->GetRuntime();
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "widgetModeChangeEvent"));
	event->InitEvent(DOM_EVENT_CUSTOM, target);
	RETURN_IF_ERROR(event->SetType(UNI_L("widgetModeChange")));
	return event->m_widgetMode.Set(widgetMode);
}

/* virtual */ ES_GetState
DOM_WidgetModeChangeEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_widgetMode)
	{
		DOMSetString(value, m_widgetMode.CStr());
		return GET_SUCCESS;
	}
	else
		return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* static */ OP_STATUS
DOM_ResolutionEvent::Make(DOM_ResolutionEvent *&event, DOM_Object *target, int width, int height)
{
	event = OP_NEW(DOM_ResolutionEvent, ());
	DOM_Runtime *runtime = target->GetRuntime();
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "resolutionEvent"));
	event->InitEvent(DOM_EVENT_CUSTOM, target);
	RETURN_IF_ERROR(event->SetType(UNI_L("resolution")));
	event->m_width = width;
	event->m_height = height;

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_ResolutionEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_height:
		DOMSetNumber(value, m_height);
		return GET_SUCCESS;

	case OP_ATOM_width:
		DOMSetNumber(value, m_width);
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

DOM_FUNCTIONS_WITH_DATA_START(DOM_WidgetIcon)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_WidgetIcon, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_WidgetIcon, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_WidgetIcon)

#endif // GADGET_SUPPORT
