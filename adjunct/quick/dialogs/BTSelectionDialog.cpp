/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _BITTORRENT_SUPPORT_

# include "adjunct/bittorrent/bt-info.h"
# include "adjunct/desktop_util/handlers/DownloadManager.h"
# include "adjunct/quick/Application.h"
# include "adjunct/quick/dialogs/BTConfigDialog.h"
# include "adjunct/quick/dialogs/BTSelectionDialog.h"
# include "adjunct/quick_toolkit/widgets/OpLabel.h"
# include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
# include "adjunct/quick/windows/DocumentDesktopWindow.h"
# include "modules/prefs/prefsmanager/prefsmanager.h"
# include "modules/widgets/OpButton.h"
# include "modules/widgets/OpDropDown.h"
# include "modules/widgets/OpMultiEdit.h"

# ifdef ENABLE_USAGE_REPORT
#  include "adjunct/quick/usagereport/UsageReport.h"
# endif



/*****************************************************************************
***                                                                        ***
***                                                                        ***
*****************************************************************************/
BTSelectionDialog::BTSelectionDialog(DownloadItem *download_item)
	:	DownloadDialog(download_item)
{
	SetFilterOpera(TRUE);
}

BTSelectionDialog::~BTSelectionDialog()
{
}

BOOL BTSelectionDialog::OnInputAction(OpInputAction *action)
{
	switch(action->GetAction())
	{
		case OpInputAction::ACTION_OPEN_BT_PREFS:
			{
				BTConfigDialog *dlg = OP_NEW(BTConfigDialog, ());
				if (dlg)
					dlg->Init(this);

				return TRUE;
			}
			break;

		case OpInputAction::ACTION_OPEN:
			{
				UINT32 use_opera = 0;
				OpButton *button = (OpButton *)GetWidgetByName("Radio_use_Opera");
				if(button)
				{
					use_opera = button->GetValue();
				}
				if(use_opera)
				{
					BOOL save_action = GetWidgetValue("Save_action_checkbox");
					m_download_item->SetOperaAsViewer();
					m_download_item->Open(save_action, TRUE, FALSE);
					CloseDialog(FALSE);
					return TRUE;
				}
				// else fall through to the base class
			}
	}
	return DownloadDialog::OnInputAction(action);
}
OP_STATUS BTSelectionDialog::Init(DesktopWindow* parent_window)
{
#ifdef ENABLE_USAGE_REPORT
	if(g_bittorrent_report)
	{
		SetDialogListener(g_bittorrent_report);
	}
#endif
	return DownloadDialog::Init(parent_window);
}

void BTSelectionDialog::OnInit()
{
	OpString fileinfo;
	URL moved_to_url;
	OpDropDown * handlers_dropdown = (OpDropDown*) GetWidgetByName("Download_dropdown");

	OpLabel* labelwidget = (OpLabel*)GetWidgetByName("Open_icon");
	if(labelwidget != NULL)
	{
		labelwidget->GetBorderSkin()->SetImage("Window Browser Icon");
	}
	OpButton *button = (OpButton *)GetWidgetByName("Prefs_button");
	if(button != NULL)
	{
		button->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_BT_PREFS)));
	}
	button = (OpButton *)GetWidgetByName("Radio_use_Opera");
	if(button)
	{
		button->SetValue(1);
		if(handlers_dropdown)
		{
			handlers_dropdown->SetEnabled(FALSE);
		}
	}
	else if(handlers_dropdown)
	{
		handlers_dropdown->SetEnabled(TRUE);
	}
	OpLabel * serverinfo_label = (OpLabel*) GetWidgetByName("label_for_infotext_2");

	if(!serverinfo_label)
		return;

	serverinfo_label->SetWrap(TRUE);

	DownloadDialog::OnInit();
}

void BTSelectionDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OpDropDown * handlers_dropdown = (OpDropDown*) GetWidgetByName("Download_dropdown");

	OpButton* button = (OpButton *)GetWidgetByName("Radio_use_Opera");
	OpButton *button_prefs = (OpButton *)GetWidgetByName("Prefs_button");
	if(button && button == widget)
	{
		if(handlers_dropdown)
		{
			handlers_dropdown->SetEnabled(FALSE);
		}
		if(button_prefs)
		{
			button_prefs->SetEnabled(TRUE);
		}
		return;
	}
	button = (OpButton *)GetWidgetByName("Radio_default_application");
	if(button && button == widget && handlers_dropdown)
	{
		handlers_dropdown->SetEnabled(TRUE);
		if(button_prefs)
		{
			button_prefs->SetEnabled(FALSE);
		}
	}
	DownloadDialog::OnChange(widget, changed_by_mouse);
}

#endif // _BITTORRENT_SUPPORT_

