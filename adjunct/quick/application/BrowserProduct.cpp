/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/ClassicApplication.h"
#include "adjunct/quick/application/BrowserProduct.h"
#include "adjunct/quick/managers/ManagerHolder.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

OP_STATUS BrowserProduct::BootMe(BOOL& boot_me) const
{
	boot_me = TRUE;
	return OpStatus::OK;
}


OP_STATUS BrowserProduct::GetOperaProfileName(OpString& profile_name) const
{
	profile_name.Empty();
	return OpStatus::OK;
}


DesktopBootstrap::Listener* BrowserProduct::CreateBootstrapListener() const
{
	return OP_NEW(BrowserBootstrapListener, ());
}



BrowserBootstrapListener::BrowserBootstrapListener()
	: m_application(NULL)
{
}


Application* BrowserBootstrapListener::OnCreateApplication()
{
	m_browser_startup_sequence.reset(OP_NEW(OpStartupSequence, ()));
	if (m_browser_startup_sequence.get() == NULL)
		return NULL;
	{
		OP_PROFILE_METHOD("Created application");
		OpAutoPtr<ClassicApplication> application(OP_NEW(ClassicApplication, (*m_browser_startup_sequence)));
		if (application.get() != NULL && OpStatus::IsSuccess(application->Init()))
			m_application = application.release();
	}
	return m_application;
}


BrowserBootstrapListener::StartupStepResult
		BrowserBootstrapListener::OnStepStartupSequence(
				MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	return m_browser_startup_sequence->StepStartupSequence(par1, par2);
}


OP_STATUS BrowserBootstrapListener::OnInitDesktopManagers(
		ManagerHolder& manager_holder)
{
	// Initialize the default set of managers.
	return manager_holder.InitManagers();
}


void BrowserBootstrapListener::OnShutDown()
{
	if (m_application)
		m_application->Finalize();
}
