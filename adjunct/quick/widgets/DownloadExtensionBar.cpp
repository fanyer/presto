/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/widgets/DownloadExtensionBar.h"

#include "adjunct/quick/extensions/ExtensionInstaller.h"
#include "adjunct/quick/widgets/OpStatusField.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "modules/locale/src/opprefsfilelanguagemanager.h"

/* static */
OP_STATUS DownloadExtensionBar::Construct(DownloadExtensionBar** obj)
{
	OpAutoPtr<DownloadExtensionBar> new_bar (OP_NEW(DownloadExtensionBar, ()));
	RETURN_OOM_IF_NULL(new_bar.get());
	RETURN_IF_ERROR(new_bar->init_status);
	RETURN_IF_ERROR(new_bar->Init("Download Extension Toolbar"));

	new_bar->GetBorderSkin()->SetImage("Download Extension Toolbar Skin");

	*obj = new_bar.release();
	return OpStatus::OK;
}

DownloadExtensionBar::~DownloadExtensionBar()
{
	if (m_extension_installer)
		m_extension_installer->CancelDownload();
}

BOOL DownloadExtensionBar::OnInputAction(OpInputAction* action)
{
	if (OpInfobar::OnInputAction(action))
		return TRUE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			if (child_action->GetAction() == OpInputAction::ACTION_CANCEL_EXTENSION_DOWNLOAD)
			{
				child_action->SetEnabled(TRUE);
				return TRUE;
			}

			break;
		}
		case OpInputAction::ACTION_CANCEL_EXTENSION_DOWNLOAD:
		{
			if (m_extension_installer)
				m_extension_installer->CancelDownload();
			return TRUE;
		}
		default:
			break;
	}
	return FALSE;
}

void DownloadExtensionBar::OnExtensionDownloadProgress(OpFileLength len, OpFileLength total)
{
	OpProgressBar* progress = static_cast<OpProgressBar *>(GetWidgetByName(GetProgressFieldName()));
	if(progress)
	{
		progress->SetProgress(len, total);
	}
}

void DownloadExtensionBar::OnExtensionDownloadStarted(ExtensionInstaller& extension_installer)
{
	OP_ASSERT(m_extension_installer == NULL);
	m_extension_installer = &extension_installer;

	OpProgressBar *progress = static_cast<OpProgressBar *>(GetWidgetByName(GetProgressFieldName()));
	if(progress)
	{
		progress->SetType(OpProgressBar::Percentage_Label);
		progress->SetHidden(FALSE);
		progress->SetGrowValue(2);
	}

	OpStatusField* label = static_cast<OpStatusField*> (GetWidgetByName(GetStatusFieldName()));
	if(label)
	{
		OpString text;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_DOWNLOADING_EXTENSION, text));
		RETURN_VOID_IF_ERROR(label->SetText(text.CStr()));
	}

	OpInfobar::Show();
}

void DownloadExtensionBar::OnExtensionDownloadFinished()
{
	OpInfobar::Hide();
	OP_ASSERT(m_extension_installer != NULL);
	m_extension_installer->RemoveListener(this);
	m_extension_installer = NULL;
}
