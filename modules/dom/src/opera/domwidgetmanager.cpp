/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined DOM_WIDGETMANAGER_SUPPORT || defined DOM_UNITEAPPMANAGER_SUPPORT || defined DOM_EXTENSIONMANAGER_SUPPORT

#include "modules/gadgets/OpGadget.h"
#include "modules/dom/src/opera/domwidgetmanager.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/ecmascript_utils/esasyncif.h"

/* static */ OP_STATUS
DOM_WidgetManager::Make(DOM_WidgetManager *&new_obj, DOM_Runtime *origining_runtime, BOOL unite, BOOL extensions)
{
	new_obj = OP_NEW(DOM_WidgetManager, ());
	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;
#ifdef WEBSERVER_SUPPORT
	new_obj->m_isUnite = unite;
#else
	OP_ASSERT(!unite);
#endif
#ifdef EXTENSION_SUPPORT
	new_obj->m_isExtensions = extensions;
#else
	OP_ASSERT(!extensions);
#endif

	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::WIDGETMANAGER_PROTOTYPE), "WidgetManager"));

	if (OpStatus::IsError(new_obj->Initialize(unite)))
	{
		new_obj = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

DOM_WidgetManager::DOM_WidgetManager()
{
}

OP_STATUS
DOM_WidgetManager::Initialize(BOOL unite)
{
	DOM_WidgetCollection *widgets;
#ifndef EXTENSION_SUPPORT
	const BOOL m_isExtensions = FALSE;
#endif
	RETURN_IF_ERROR(DOM_WidgetCollection::Make(widgets, GetRuntime(), unite, m_isExtensions));
	RETURN_IF_ERROR(PutPrivate(DOM_PRIVATE_widgets, *widgets));

	RETURN_IF_ERROR(g_gadget_manager->AddListener(this));

	return OpStatus::OK;
}

/* virutal */
DOM_WidgetManager::~DOM_WidgetManager()
{
	if (g_gadget_manager) // Gadgets module may have been destroyed if we are terminating Opera.
		g_gadget_manager->RemoveListener(this);
}

/* virtual */ ES_GetState
DOM_WidgetManager::GetName(OpAtom property_name, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_widgets:
		return DOMSetPrivate(return_value, DOM_PRIVATE_widgets);
	}

	return DOM_Object::GetName(property_name, return_value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_WidgetManager::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_widgets:
		return PUT_READ_ONLY;
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_WidgetManager::GCTrace()
{
	for (UINT32 i = 0; i < m_callbacks.GetCount(); i++)
		GCMark(m_callbacks.Get(i));
}

void
DOM_WidgetManager::HandleGadgetInstallEvent(GadgetInstallEvent &evt)
{
	for (UINT32 i = 0; i < m_callbacks.GetCount(); i++)
	{
		if (evt.userdata == m_callbacks.Get(i))
		{
			char rc;
			TempBuffer *string = GetEmptyTempBuf();
			switch (evt.event)
			{
			case OpGadgetInstallListener::GADGET_INSTALL_USER_CANCELLED:
				rc = 0;
				RAISE_IF_ERROR(string->Append("User cancelled"));
				break;
			case OpGadgetInstallListener::GADGET_INSTALL_DOWNLOAD_FAILED:
				rc = 0;
				RAISE_IF_ERROR(string->Append("Download failed"));
				break;
			case OpGadgetInstallListener::GADGET_INSTALL_NOT_ENOUGH_SPACE:
				rc = 0;
				RAISE_IF_ERROR(string->Append("Not enough space"));
				break;
			case OpGadgetInstallListener::GADGET_INSTALL_USER_CONFIRMED:
				rc = 1;
				RAISE_IF_ERROR(string->Append("User confirmed"));
				break;
			case OpGadgetInstallListener::GADGET_INSTALL_DOWNLOADED:
				rc = 2;
				RAISE_IF_ERROR(string->Append("Download successful"));
				break;
			case OpGadgetInstallListener::GADGET_INSTALL_INSTALLED:
				rc = 3;
				RAISE_IF_ERROR(string->Append("Installation successful"));
				break;
			case OpGadgetInstallListener::GADGET_INSTALL_INSTALLATION_FAILED:
				rc = 4;
				RAISE_IF_ERROR(string->Append("Installation failed"));
				break;
			case OpGadgetInstallListener::GADGET_INSTALL_PROCESS_FINISHED:
				m_callbacks.RemoveByItem(static_cast<ES_Object *>(evt.userdata));
				// follow thru
			default:
				return;
			}

			ES_Value arguments[2];
			DOMSetNumber(&arguments[0], rc);
			DOMSetString(&arguments[1], string);
			OpStatus::Ignore(GetEnvironment()->GetAsyncInterface()->CallFunction(m_callbacks.Get(i), *this, 2, arguments, NULL, DOM_Object::GetCurrentThread(GetRuntime())));
		}
	}
}

OpGadget* DOM_WidgetManager::GadgetArgument(ES_Value *argv, int argc, int n)
{
	DOM_Widget		*obj = (argc > n && argv[n].type == VALUE_OBJECT) ? static_cast<DOM_Widget *>(DOM_GetHostObject(argv[n].value.object)) : NULL;
	const uni_char	*id =  (argc > n && argv[n].type == VALUE_STRING) ? argv[n].value.string : NULL;

	if (obj)
		return obj->GetGadget();
	else if (id)
		return g_gadget_manager->FindGadget(GADGET_FIND_BY_ID, id);
	return NULL;
}

/* static */ int
DOM_WidgetManager::run(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
#ifdef WEBSERVER_SUPPORT
	DOM_THIS_OBJECT(wm, DOM_TYPE_WIDGETMANAGER, DOM_WidgetManager);
#else
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGETMANAGER);
#endif

	OpGadget *gadget = GadgetArgument(argv, argc, 0);

	if (gadget)
	{
#ifdef WEBSERVER_SUPPORT
		if (wm->IsUnite() && gadget->IsRootService())
			CALL_FAILED_IF_ERROR(g_gadget_manager->OpenRootService(gadget));
		else
#endif
		{
#ifdef EXTENSION_SUPPORT
			if (argc > 1 && argv[1].type == VALUE_BOOLEAN)
				gadget->SetExtensionFlag(OpGadget::AllowUserJSHTTPS, argv[1].value.boolean);
			if (argc > 2 && argv[2].type == VALUE_BOOLEAN)
				gadget->SetExtensionFlag(OpGadget::AllowUserJSPrivate, argv[2].value.boolean);
#endif  // EXTENSION_SUPPORT
			CALL_FAILED_IF_ERROR(g_gadget_manager->OpenGadget(gadget));
		}
	}

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_WidgetManager::kill(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGETMANAGER);

	OpGadget *gadget = GadgetArgument(argv, argc, 0);

	Window *gadgetwin;
	if (gadget && (gadgetwin = gadget->GetWindow()) != NULL)
	{
		CALL_FAILED_IF_ERROR(g_gadget_manager->CloseWindowPlease(gadgetwin));
		gadgetwin->Close();
#ifdef EXTENSION_SUPPORT
		if (gadget->IsExtension())
		{
		    gadget->SetIsClosing(FALSE);
		    gadget->SetIsRunning(FALSE);
		}
#endif // EXTENSION_SUPPORT
	}

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_WidgetManager::install(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(wm, DOM_TYPE_WIDGETMANAGER, DOM_WidgetManager);
	DOM_CHECK_ARGUMENTS("sso");

	ES_Object *callback = argv[2].value.object;
	CALL_FAILED_IF_ERROR(wm->m_callbacks.Add(callback));
	CALL_FAILED_IF_ERROR(g_gadget_manager->DownloadAndInstall(argv[0].value.string, argv[1].value.string, callback));

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_WidgetManager::uninstall(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGETMANAGER);
	OpGadget *gadget = GadgetArgument(argv, argc, 0);

	if (gadget)
	{
		// Need to close first if still open.
		if (Window *gadget_window = gadget->GetWindow())
		{
			CALL_FAILED_IF_ERROR(g_gadget_manager->CloseWindowPlease(gadget_window));
			gadget_window->Close();
		}
		CALL_FAILED_IF_ERROR(g_gadget_manager->DestroyGadget(gadget));
	}

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_WidgetManager::checkForUpdate(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGETMANAGER);
	OpGadget *gadget = GadgetArgument(argv, argc, 0);

	if (gadget)
		gadget->Update();

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_WidgetManager::checkForUpdateByUrl(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGETMANAGER);
	DOM_CHECK_ARGUMENTS("s");
	CALL_FAILED_IF_ERROR(g_gadget_manager->Update(argv[0].value.string, NULL, 0));

	return ES_FAILED; // No return value.
}

/* static */ int
DOM_WidgetManager::getWidgetIconURL(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGETMANAGER);
	OpGadget *gadget = GadgetArgument(argv, argc, 0);

	if (!gadget)
		return ES_FAILED; // No return value.

	OpString gadget_url;
	OpString icon_path;
	INT32 w, h;
	CALL_FAILED_IF_ERROR(gadget->GetGadgetIcon(0, icon_path, w, h, FALSE));
	if (icon_path.IsEmpty())
		return ES_FAILED;

#if PATHSEPCHAR != '/'
	// Convert to / separators.
	OpGadgetUtils::ChangeToLocalPathSeparators(icon_path);
#endif

	CALL_FAILED_IF_ERROR(gadget->GetGadgetUrl(gadget_url, FALSE, TRUE));
	TempBuffer* retbuf = GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(retbuf->AppendFormat(UNI_L("%s/%s"), gadget_url.CStr(), icon_path.CStr() + 1)); // first char of icon_path is PATHSEP
	DOMSetString(return_value, retbuf);
	return ES_VALUE;
}

#ifdef EXTENSION_SUPPORT
/* static */ int
DOM_WidgetManager::options(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGETMANAGER);
	OpGadget *gadget = GadgetArgument(argv, argc, 0);

	if (gadget && gadget->IsExtension())
	{
		if (!gadget->IsRunning())
			CALL_FAILED_IF_ERROR(g_gadget_manager->OpenGadget(gadget));
		gadget->OpenExtensionOptionsPage();
	}

	return ES_FAILED; // No return value.
}
#endif // EXTENSION_SUPPORT

/* static */ int
DOM_WidgetManager::setWidgetMode(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WIDGETMANAGER);
	DOM_CHECK_ARGUMENTS("-s");

	OpGadget* gadget = GadgetArgument(argv, argc, 0);

	if (gadget)
		gadget->SetMode(argv[1].value.string);
	return ES_FAILED; // No return value.
}


DOM_FUNCTIONS_START(DOM_WidgetManager)
	DOM_FUNCTIONS_FUNCTION(DOM_WidgetManager, DOM_WidgetManager::install, "install", "sso")
	DOM_FUNCTIONS_FUNCTION(DOM_WidgetManager, DOM_WidgetManager::uninstall, "uninstall", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_WidgetManager, DOM_WidgetManager::run, "run", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_WidgetManager, DOM_WidgetManager::kill, "kill", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_WidgetManager, DOM_WidgetManager::checkForUpdate, "checkForUpdate", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_WidgetManager, DOM_WidgetManager::checkForUpdateByUrl, "checkForUpdateByUrl", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_WidgetManager, DOM_WidgetManager::getWidgetIconURL, "getWidgetIconURL", "-")
#ifdef EXTENSION_SUPPORT
	DOM_FUNCTIONS_FUNCTION(DOM_WidgetManager, DOM_WidgetManager::options, "options", "-")
#endif // EXTENSION_SUPPORT
	DOM_FUNCTIONS_FUNCTION(DOM_WidgetManager, DOM_WidgetManager::setWidgetMode, "setWidgetMode", "-s-")
DOM_FUNCTIONS_END(DOM_WidgetManager)

//////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
DOM_WidgetCollection::Make(DOM_WidgetCollection *&new_obj, DOM_Runtime *origining_runtime, BOOL unite, BOOL extensions)
{
	new_obj = OP_NEW(DOM_WidgetCollection, (unite, extensions));
	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::WIDGETCOLLECTION_PROTOTYPE), "WidgetCollection"));

	return OpStatus::OK;
}

DOM_WidgetCollection::DOM_WidgetCollection(BOOL unite, BOOL extensions)
#if defined WEBSERVER_SUPPORT || defined EXTENSION_SUPPORT
	:
#endif
#ifdef WEBSERVER_SUPPORT
	  m_isUniteCollection(unite)
# ifdef EXTENSION_SUPPORT
	,
# endif
#endif
#ifdef EXTENSION_SUPPORT
	  m_isExtensionCollection(extensions)
#endif
{
}

DOM_WidgetCollection::~DOM_WidgetCollection()
{
}

#if defined WEBSERVER_SUPPORT || defined EXTENSION_SUPPORT
BOOL
DOM_WidgetCollection::IncludeWidget(OpGadget *gadget)
{
#ifdef WEBSERVER_SUPPORT
	if (m_isUniteCollection || gadget->GetClass()->IsSubserver())
		return m_isUniteCollection && gadget->GetClass()->IsSubserver();
#endif

#ifdef EXTENSION_SUPPORT
	if (m_isExtensionCollection || gadget->GetClass()->IsExtension())
		return m_isExtensionCollection && gadget->GetClass()->IsExtension();
#endif

	return TRUE;
}
#endif

ES_GetState
DOM_WidgetCollection::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_count:
#if defined WEBSERVER_SUPPORT || defined EXTENSION_SUPPORT
		int gadgetCount = 0;
		for (OpGadget *gadget = g_gadget_manager->GetFirstGadget(); gadget; gadget = static_cast<OpGadget*>(gadget->Suc()))
		{
			if (IncludeWidget(gadget))
				gadgetCount++;
		}
		DOMSetNumber(value, gadgetCount);
#else
		DOMSetNumber(value, g_gadget_manager->NumGadgets());
#endif
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

ES_PutState
DOM_WidgetCollection::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_WidgetCollection::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	int gadgetCount = 0;
	for (OpGadget *gadget = g_gadget_manager->GetFirstGadget(); gadget; gadget = static_cast<OpGadget*>(gadget->Suc()))
	{
		if (IncludeWidget(gadget))
			gadgetCount++;

		if (gadgetCount > 0 && gadgetCount-1 == property_index)
		{
			DOM_Widget *widget;
#ifndef WEBSERVER_SUPPORT
			static const BOOL m_isUniteCollection = FALSE;
#endif
			GET_FAILED_IF_ERROR(DOM_Widget::Make(widget, GetRuntime(), gadget, m_isUniteCollection));
			GET_FAILED_IF_ERROR(widget->InjectWidgetManagerApi());
			DOMSetObject(value, widget);
			return GET_SUCCESS;
		}
	}

	return DOM_Object::GetIndex(property_index, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_WidgetCollection::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	return PUT_READ_ONLY;
}

#endif // DOM_WIDGETMANAGER_SUPPORT || DOM_UNITEAPPMANAGER_SUPPORT || DOM_EXTENSIONMANAGER_SUPPORT
