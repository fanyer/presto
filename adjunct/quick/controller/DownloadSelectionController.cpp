/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Erman Doser (ermand)
 */
#include "core/pch.h"

#include "adjunct/quick/controller/DownloadSelectionController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/quick/dialogs/DownloadDialog.h"
#include "adjunct/quick/managers/DownloadManagerManager.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"


DownloadSelectionController::DownloadSelectionController(
		OpDocumentListener::DownloadCallback* callback,	DownloadItem* di)
	: m_download_item(di)
	, m_download_callback(callback)
	, m_release_resource(true)
{
	m_managers = &g_download_manager_manager->GetExternalManagers();
}

DownloadSelectionController::~DownloadSelectionController()
{
	if (m_release_resource)
	{
		OP_DELETE(m_download_item);
		m_download_callback->Abort();
		m_download_callback = NULL;
	}
}

void DownloadSelectionController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Download Selection Dialog"));
	LEAVE_IF_ERROR(GetBinder()->Connect("checkbox_Default",
				*g_pcui, PrefsCollectionUI::ShowDownloadManagerSelectionDialog, 0, 1));

	QuickDropDown* dropdown = m_dialog->GetWidgetCollection()->GetL<QuickDropDown>("Download_dropdown");
	g_download_manager_manager->PopulateDropdown(dropdown->GetOpWidget());
}

void DownloadSelectionController::OnOk()
{
	ExternalDownloadManager* selected = NULL;

	QuickDropDown* dropdown = m_dialog->GetWidgetCollection()->Get<QuickDropDown>("Download_dropdown");
	if (dropdown)
	{
		INT32 selected_item = dropdown->GetOpWidget()->GetSelectedItem();
		// Get the download manager of that item :
		selected = reinterpret_cast<ExternalDownloadManager*>(
				dropdown->GetOpWidget()->GetItemUserData(selected_item));
	}

	// save the choice
	g_download_manager_manager->SetDefaultManager(selected);

	if (!selected)
	{
		DownloadDialog* dialog = OP_NEW(DownloadDialog, (m_download_item));
		if (dialog)
		{
			RETURN_VOID_IF_ERROR(dialog->Init(m_dialog->GetDesktopWindow()->GetParentDesktopWindow()));
			m_release_resource = false;
		}
	}
	else
	{
		selected->Run(m_download_callback->URLName());
	}
}
