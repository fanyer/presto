/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "HomepageDialog.h"
#include "modules/url/url2.h"
#include "modules/dochand/win.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/widgets/OpEdit.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "adjunct/quick/WindowCommanderProxy.h"

/***********************************************************************************
**
**	Init
**
***********************************************************************************/
void HomepageDialog::Init(DesktopWindow* parent_window, Window* current_window)
{
	Init(parent_window, current_window ? current_window->GetWindowCommander() : NULL); 
}

void HomepageDialog::Init(DesktopWindow* parent_window, OpWindowCommander* current_windowcommander)
{
	m_current_windowcommander = current_windowcommander;

	Dialog::Init(parent_window);
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void HomepageDialog::OnInit()
{
	OpWidget* widget = GetWidgetByName("Use_own_homepage");
	if (widget)
	{
		widget->SetValue(1);
	}

	SetWidgetText("Own_homepage_edit", g_pccore->GetStringPref(PrefsCollectionCore::HomeURL).CStr());
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void HomepageDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if(widget->IsNamed("Own_homepage_edit"))
	{
		SetWidgetValue("Use_own_homepage", TRUE);
	}
}

/***********************************************************************************
**
**	OnClick
**
***********************************************************************************/

void HomepageDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if(widget->IsNamed("Use_current_homepage"))
	{
		if( m_current_windowcommander )
		{
			OpString curr_url;
			WindowCommanderProxy::GetCurrentURL(m_current_windowcommander).GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, curr_url, TRUE);
			SetWidgetText("Own_homepage_edit", curr_url.CStr());
		}
		else
		{
			SetWidgetText("Own_homepage_edit", UNI_L("") );
		}
		SetWidgetValue("Use_current_homepage", TRUE);
	}
	else if(widget->IsNamed("Use_opera_homepage"))
	{
		OpStringC homepage = g_pccore->GetStringPref(PrefsCollectionCore::HomeURL);
		SetWidgetText("Own_homepage_edit", homepage);
		SetWidgetValue("Use_opera_homepage", TRUE);
	}
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 HomepageDialog::OnOk()
{
	OpString homepage;

	GetWidgetText("Own_homepage_edit", homepage);

	TRAPD(err, g_pccore->WriteStringL(PrefsCollectionCore::HomeURL, homepage.CStr()));

	BOOL permanent_homepage = FALSE;
#ifdef PERMANENT_HOMEPAGE_SUPPORT
	permanent_homepage = (1 == g_pcui->GetIntegerPref(PrefsCollectionUI::PermanentHomepage));
#endif

	if ((!permanent_homepage) && (GetWidgetValue("Start_with_homepage")))
	{
		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::StartupType, STARTUP_HOME));
	}
	
	g_prefsManager->CommitL();

	return Dialog::OnOk();
}

