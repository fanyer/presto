/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/dialogs/AskPluginDownloadDialog.h"
#ifdef WIDGET_RUNTIME_SUPPORT
#include "adjunct/widgetruntime/GadgetStartup.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"
#include "modules/pi/system/OpPlatformViewers.h"
#endif // WIDGET_RUNTIME_SUPPORT
#include "modules/pi/system/OpPlatformViewers.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

OP_STATUS AskPluginDownloadDialog::Init(DesktopWindow* parent_window, const uni_char* plugin, const uni_char* downloadurl,
		Str::LocaleString message_format_id, BOOL is_widget)
{
	m_is_widget = is_widget;

	OpString title;
	OpString format;
	OpString message;

	g_languageManager->GetString(Str::D_PLUGIN_DOWNLOAD_RESOURCE_TITLE, format);
	RETURN_IF_ERROR(title.AppendFormat(format.CStr(), plugin));
	g_languageManager->GetString(message_format_id, format);	
	RETURN_IF_ERROR(message.AppendFormat(format.CStr(), plugin, plugin));

	RETURN_IF_ERROR(m_askplugindownloadurl.Set(downloadurl));

	return SimpleDialog::Init(WINDOW_NAME_ASK_PLUGIN_DOWNLOAD, title, message, parent_window, TYPE_OK_CANCEL, IMAGE_QUESTION);
}

UINT32 AskPluginDownloadDialog::OnOk()
{
	if (IsDoNotShowDialogAgainChecked())
		g_pcui->WriteIntegerL(PrefsCollectionUI::AskAboutFlashDownload, FALSE);

	// FIXME: Long term solution for this problem is to automatically install 
	// flash from within Opera browser or Widget Runtime. Currently we're opening 
	// Opera or default browser and redirecting to flash download site. It would 
	// be nice to open default browser here on all platforms, but because of 
	// DSK-268209 (problems with IE and flash plugin download) we can't do it
	// on windows.

	// FIXME: We should use OpPlatformViewers::OpenInOpera here, but 
	// before we can do that it has to be implemented differently in 
	// different kind of products (currently we have browser and gadget 
	// products). Current general implementation of OpenInOpera doesn't work 
	// for gadget product.

	if (m_is_widget)
	{
#ifdef _UNIX_DESKTOP_
		RETURN_VALUE_IF_ERROR(g_op_platform_viewers->OpenInDefaultBrowser(
				m_askplugindownloadurl.CStr()), 0);
#else
		OpString command;
		RETURN_VALUE_IF_ERROR(PlatformGadgetUtils::GetOperaExecPath(command), 0);
		OpStatus::Ignore(g_op_system_info->ExecuteApplication(command.CStr(),
				m_askplugindownloadurl.CStr()));
#endif // _UNIX_DESKTOP_
	}
	else
	{
		g_application->GoToPage(m_askplugindownloadurl.CStr(), TRUE);
	}

	return 1;
}

void AskPluginDownloadDialog::OnCancel()
{
	if (IsDoNotShowDialogAgainChecked())
		g_pcui->WriteIntegerL(PrefsCollectionUI::AskAboutFlashDownload, FALSE);
}
