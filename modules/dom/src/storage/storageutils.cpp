/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#include "core/pch.h"

#if defined OPERA_CONSOLE && (defined DATABASE_STORAGE_SUPPORT || defined CLIENTSIDE_STORAGE_SUPPORT)

#include "modules/dom/src/storage/storageutils.h"
#include "modules/console/opconsoleengine.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domobj.h"
#include "modules/database/src/opdatabase_base.h"
#include "modules/doc/frm_doc.h"

/* status */ OP_STATUS
DOM_PSUtils::GetPersistentStorageOriginInfo(DOM_Runtime *runtime, PS_OriginInfo &poi)
{
	RETURN_IF_ERROR(runtime->GetSerializedOrigin(poi.m_origin_buf, DOM_Utils::SERIALIZE_FILE_SCHEME));
	Window *window = runtime->GetFramesDocument() != NULL ? runtime->GetFramesDocument()->GetWindow() : NULL;

	if (window != NULL && poi.m_origin_buf.Length() != 0)
	{
		poi.m_origin = poi.m_origin_buf.GetStorage();
		poi.m_is_persistent = !window->GetPrivacyMode();
		poi.m_context_id = window->GetMainUrlContextId(runtime->GetOriginURL().GetContextId());
	}
	else
	{
		RETURN_IF_ERROR(poi.m_origin_buf.Append(runtime->GetOriginURL().UniName(PASSWORD_HIDDEN)));
		poi.m_origin = poi.m_origin_buf.GetStorage();
		poi.m_is_persistent = FALSE;
		poi.m_context_id = runtime->GetOriginURL().GetContextId();
	}
	return OpStatus::OK;
}

/* static */ BOOL
DOM_PSUtils::IsStorageStatusFileAccessError(OP_STATUS status)
{
	return status == PS_Status::ERR_CORRUPTED_FILE ||
			status ==  OpStatus::ERR_NO_DISK ||
			status == OpStatus::ERR_NO_ACCESS;
}

#ifdef DATABASE_STORAGE_SUPPORT
#include "modules/dom/src/storage/database.h"
#include "modules/dom/src/domglobaldata.h"

void
DOM_PSUtils::PostWebDatabaseErrorToConsole(DOM_Runtime *runtime, const uni_char *es_thread_name, DOM_Database *dom_db, OP_STATUS status)
{
	if (!g_console->IsLogging() || dom_db == NULL || dom_db->HasPostedErrMsg() || !IsStorageStatusFileAccessError(status))
		return;

	WSD_Database *dbo = dom_db->GetDb();

	if (dbo == NULL)
		return;

	OpConsoleEngine::Message message(OpConsoleEngine::PersistentStorage, OpConsoleEngine::Error);

	if (FramesDocument *doc = runtime->GetFramesDocument())
		message.window = doc->GetWindow()->Id();
	RETURN_VOID_IF_ERROR(message.context.Set(es_thread_name ? es_thread_name : UNI_L("Unknown thread")));
	RETURN_VOID_IF_ERROR(runtime->GetDisplayURL(message.url));

	RETURN_VOID_IF_ERROR(message.message.Set("Error opening database"));
	if (dbo->GetOrigin() != NULL)
	{
		RETURN_VOID_IF_ERROR(message.message.Append(" for origin "));
		RETURN_VOID_IF_ERROR(message.message.Append(dbo->GetOrigin()));
	}

	switch (OpStatus::GetIntValue(status))
	{
	case PS_Status::ERR_CORRUPTED_FILE:
		RETURN_VOID_IF_ERROR(message.message.Append("\nFile at '"));
		RETURN_VOID_IF_ERROR(message.message.Append(dbo->GetFileAbsPath()));
		RETURN_VOID_IF_ERROR(message.message.Append("' corrupted"));
		break;

	case OpStatus::ERR_NO_DISK:
		RETURN_VOID_IF_ERROR(message.message.Append("\nOut of disk when accessing '"));
		RETURN_VOID_IF_ERROR(message.message.Append(dbo->GetFileAbsPath()));
		RETURN_VOID_IF_ERROR(message.message.Append("'"));
		break;

	case OpStatus::ERR_NO_ACCESS:
		RETURN_VOID_IF_ERROR(message.message.Append("\nFile at '"));
		RETURN_VOID_IF_ERROR(message.message.Append(dbo->GetFileAbsPath()));
		RETURN_VOID_IF_ERROR(message.message.Append("' unaccessible"));
		break;

	default:
		OP_ASSERT(!"Missing one case");
	}

	TRAP(status, g_console->PostMessageL(&message));
	OpStatus::Ignore(status);

	dom_db->SetHasPostedErrMsg(TRUE);
}

void
DOM_PSUtils::PostExceptionToConsole(DOM_Runtime *runtime, const uni_char *es_thread_name, const uni_char *message)
{
	if (message == NULL || runtime->GetFramesDocument() == NULL || !g_console->IsLogging())
		return;

	OpConsoleEngine::Message msg(OpConsoleEngine::PersistentStorage,
	                             OpConsoleEngine::Error,
	                             runtime->GetFramesDocument()->GetWindow()->Id());

	RETURN_VOID_IF_ERROR(msg.context.Set(es_thread_name ? es_thread_name : UNI_L("Unknown thread")));
	RETURN_VOID_IF_ERROR(runtime->GetDisplayURL(msg.url));

	RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("message: ")));
	RETURN_VOID_IF_ERROR(msg.message.Append(message));

	TRAPD(status, g_console->PostMessageL(&msg));
	OpStatus::Ignore(status);
}

#endif // DATABASE_STORAGE_SUPPORT

#ifdef CLIENTSIDE_STORAGE_SUPPORT

#include "modules/dom/src/storage/storage.h"

void
DOM_PSUtils::PostWebStorageErrorToConsole(DOM_Runtime *runtime, const uni_char *es_thread_name, DOM_Storage *dom_st, OP_STATUS status)
{
	if (!g_console->IsLogging() || dom_st == NULL || dom_st->HasPostedErrMsg() || !IsStorageStatusFileAccessError(status))
		return;

	OpStorage *ops = dom_st->GetOpStorage();

	OpConsoleEngine::Message message(OpConsoleEngine::PersistentStorage, OpConsoleEngine::Error);

	if (FramesDocument *doc = runtime->GetFramesDocument())
		message.window = doc->GetWindow()->Id();
	RETURN_VOID_IF_ERROR(message.context.Set(es_thread_name ? es_thread_name : UNI_L("Unknown thread")));
	RETURN_VOID_IF_ERROR(runtime->GetDisplayURL(message.url));

	RETURN_VOID_IF_ERROR(message.message.Set("Error accessing data file for "));
	switch (dom_st->GetStorageType())
	{
	case WEB_STORAGE_LOCAL:
		RETURN_VOID_IF_ERROR(message.message.Append("localStorage"));
		break;

	case WEB_STORAGE_SESSION:
		RETURN_VOID_IF_ERROR(message.message.Append("sessionStorage"));
		break;

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	case WEB_STORAGE_USERJS:
		RETURN_VOID_IF_ERROR(message.message.Append("UserJS scriptStorage"));
		break;
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case WEB_STORAGE_WGT_PREFS:
		RETURN_VOID_IF_ERROR(message.message.Append("widget.preferences"));
		break;
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT

	default:
		OP_ASSERT(!"Missing a type");
	}

	if (ops && ops->GetOrigin() != NULL)
	{
		RETURN_VOID_IF_ERROR(message.message.Append(" for origin "));
		RETURN_VOID_IF_ERROR(message.message.Append(ops->GetOrigin()));
	}

	switch (OpStatus::GetIntValue(status))
	{
	case PS_Status::ERR_CORRUPTED_FILE:
		RETURN_VOID_IF_ERROR(message.message.Append("\nData file corrupted"));
		break;

	case OpStatus::ERR_NO_DISK:
		RETURN_VOID_IF_ERROR(message.message.Append("\nOut of disk"));
		break;

	case OpStatus::ERR_NO_ACCESS:
		RETURN_VOID_IF_ERROR(message.message.Append("\nFile unaccessible"));
		break;

	default:
		OP_ASSERT(!"Missing one case");
	}

	TRAP(status, g_console->PostMessageL(&message));
	OpStatus::Ignore(status);

	dom_st->SetHasPostedErrMsg(TRUE);
}
#endif // CLIENTSIDE_STORAGE_SUPPORT

#endif // defined OPERA_CONSOLE && (defined DATABASE_STORAGE_SUPPORT || defined CLIENTSIDE_STORAGE_SUPPORT)
