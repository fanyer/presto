// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
//
#include "core/pch.h"

#include "adjunct/quick/managers/DownloadManagerManager.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/widgets/OpDropDown.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DownloadManagerManager::DownloadManagerManager()
{
	m_inited = FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DownloadManagerManager::~DownloadManagerManager()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DownloadManagerManager::Init()
{
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DownloadManagerManager::InitManagers()
{
	if (!m_inited)
	{
		RETURN_IF_ERROR(g_desktop_op_system_info->GetExternalDownloadManager(m_managers));
		m_inited = TRUE;
	}
	return OpStatus::OK;
}

ExternalDownloadManager* DownloadManagerManager::GetByName(const uni_char* name)
{
	if (!name || uni_stricmp(name,"opera") == 0)
		return NULL;

	InitManagers();
	for (UINT32 i=0; i<m_managers.GetCount(); i++)
	{
		if (m_managers.Get(i)->GetName() 
			&& uni_stricmp(m_managers.Get(i)->GetName(), name) == 0)
			return m_managers.Get(i);
	}
	return NULL;
}

ExternalDownloadManager* DownloadManagerManager::GetDefaultManager()
{
	return GetByName(g_pcui->GetStringPref(PrefsCollectionUI::DownloadManager).CStr());
}

void DownloadManagerManager::SetDefaultManager(ExternalDownloadManager* manager)
{
	OpString selected;
	selected.Set(manager ? manager->GetName() : NULL);
	TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::DownloadManager, selected));
}
BOOL DownloadManagerManager::GetExternalManagerEnabled()
{
	return g_pcui->GetIntegerPref(PrefsCollectionUI::UseExternalDownloadManager);
}

BOOL DownloadManagerManager::GetShowDownloadManagerSelectionDialog()
{
	return g_pcui->GetIntegerPref(PrefsCollectionUI::ShowDownloadManagerSelectionDialog);
}

BOOL DownloadManagerManager::GetHasExternalDownloadManager()
{
	InitManagers();
	return m_managers.GetCount() > 0;
}


void DownloadManagerManager::PopulateDropdown(OpDropDown* dropdown)
{
	if (!dropdown)
		return;

	InitManagers();

	// Avoid streched favicons :
	dropdown->SetRestrictImageSize(TRUE);

	// Add Opera
	INT32 got_index = -1;
	dropdown->AddItem(UNI_L("Opera"), -1, &got_index);
	dropdown->ih.SetImage(got_index, "Window Browser Icon", dropdown);

	INT32 select = 0;
	ExternalDownloadManager* default_manager = GetDefaultManager();

	// Add external application
	for (UINT32 i=0; i<m_managers.GetCount(); i++)
	{
		ExternalDownloadManager* manager = m_managers.Get(i);
		if (default_manager == manager)
			select = i+1; // the first one is Opera

		const uni_char* name = manager->GetName();
		INT32 got_index = -1;
		dropdown->AddItem(name, -1, &got_index, reinterpret_cast<INTPTR>(manager));

		OpWidgetImage widget_image;
		Image img = manager->GetIcon();
		widget_image.SetBitmapImage(img);
		dropdown->ih.SetImage(got_index, &widget_image, dropdown);
	}

	// select the last used
	dropdown->SelectItem(select, TRUE);
}
