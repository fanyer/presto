/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _BITTORRENT_SUPPORT_

# include "adjunct/bittorrent/bt-info.h"
# include "adjunct/desktop_util/handlers/DownloadItem.h"
# include "adjunct/desktop_util/handlers/DownloadManager.h"
# include "adjunct/quick/Application.h"
# include "adjunct/quick/dialogs/BTConfigDialog.h"
# include "adjunct/quick/dialogs/BTStartDownloadDialog.h"
# include "adjunct/quick_toolkit/widgets/OpLabel.h"
# include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
# include "adjunct/quick/windows/DocumentDesktopWindow.h"
# include "modules/prefs/prefsmanager/prefsmanager.h"
# include "modules/widgets/OpMultiEdit.h"
# include "modules/windowcommander/src/WindowCommander.h"
# include "modules/locale/oplanguagemanager.h"
# include "modules/widgets/OpButton.h"

# ifdef ENABLE_USAGE_REPORT
#  include "adjunct/quick/usagereport/UsageReport.h"
# endif



/*****************************************************************************
***                                                                        ***
*** BTStartDownloadDialog                                                  ***
***                                                                        ***
*****************************************************************************/

BTStartDownloadDialog::BTStartDownloadDialog()
{
}


BTStartDownloadDialog::~BTStartDownloadDialog()
{
}

BOOL BTStartDownloadDialog::OnInputAction(OpInputAction *action)
{
	if(action->GetAction() == OpInputAction::ACTION_OPEN_BT_PREFS)
	{
		BTConfigDialog *dlg = OP_NEW(BTConfigDialog, ());
		if (dlg)
			dlg->Init(this);

		return TRUE;
	}
	return Dialog::OnInputAction(action);
}

OP_STATUS BTStartDownloadDialog::Init(DesktopWindow* parent_window, BTDownloadInfo *info, INT32 start_page)
{
	m_info = info;

#ifdef ENABLE_USAGE_REPORT
	if(g_bittorrent_report)
	{
		SetDialogListener(g_bittorrent_report);
	}
#endif

	return Dialog::Init(parent_window, start_page);
}

void BTStartDownloadDialog::OnInit()
{
	OpLabel * serverinfo_label = (OpLabel*) GetWidgetByName("label_for_Server_info");

	if(!serverinfo_label)
		return;

	serverinfo_label->SetJustify(JUSTIFY_RIGHT, FALSE);

	OpLabel * fileinfo_label = (OpLabel*) GetWidgetByName("label_for_File_info");

	if(!fileinfo_label)
		return;

	fileinfo_label->SetJustify(JUSTIFY_RIGHT, FALSE);

	OpLabel * filename_text = (OpLabel*) GetWidgetByName("File_info");

	if(!filename_text)
		return;

	filename_text->SetEllipsis(ELLIPSIS_CENTER);

	filename_text = (OpLabel*) GetWidgetByName("Server_info");

	if(!filename_text)
		return;

	filename_text->SetEllipsis(ELLIPSIS_CENTER);

	OpString fileinfo;
	URL moved_to_url;

	moved_to_url = m_info->url.GetAttribute(URL::KMovedToURL, TRUE);

	if(!moved_to_url.IsEmpty())
	{
		TRAPD(err, moved_to_url.GetAttributeL(URL::KSuggestedFileName_L, fileinfo, TRUE));
	}
	else
	{
		TRAPD(err, m_info->url.GetAttributeL(URL::KSuggestedFileName_L, fileinfo, TRUE));
	}
	SetWidgetText("File_info", fileinfo.CStr());

//		OpString string;

//		g_languageManager->GetString(Str::S_BITTORRENT_DOWNLOAD_START, string);

//		msg.AppendFormat(string.CStr(), server.CStr());

	if(!moved_to_url.IsEmpty())
	{
		fileinfo.Set(moved_to_url.GetServerName()->UniName());
	}
	else
	{
		if(m_info->url.GetServerName())
		{
			fileinfo.Set(m_info->url.GetServerName()->UniName());
		}
		else
		{
			g_languageManager->GetString(Str::SI_IDSTR_DOWNLOAD_DLG_UNKNOWN_SERVER, fileinfo);
		}
	}
	SetWidgetText("Server_info", fileinfo.CStr());

	OpLabel* label = (OpLabel*)GetWidgetByName("label_for_infotext_2");
	if(label)
	{
		label->SetWrap(TRUE);
	}

	OpLabel* labelwidget = (OpLabel*)GetWidgetByName("Open_icon");
	if(labelwidget != NULL)
	{
		labelwidget->GetBorderSkin()->SetImage("Window Browser Icon");
	}
	DesktopFileChooserEdit *folder = (DesktopFileChooserEdit *)GetWidgetByName("Download_directory_chooser");
	if(folder != NULL)
	{
		folder->SetText(m_info->savepath.CStr());
	}
	OpButton *button = (OpButton *)GetWidgetByName("Prefs_button");
	if(button != NULL)
	{
		button->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_BT_PREFS)));
	}
	Dialog::OnInit();
}

void BTStartDownloadDialog::OnCancel()
{
	m_info->btinfo->Cancel(m_info);
}

UINT32 BTStartDownloadDialog::OnOk()
{
	DesktopFileChooserEdit *folder = (DesktopFileChooserEdit *)GetWidgetByName("Download_directory_chooser");
	if(folder != NULL)
	{
		folder->GetText(m_info->savepath);
	}
	m_info->btinfo->InitiateDownload(m_info);
	return 0;
}

#endif // _BITTORRENT_SUPPORT_
