/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick/application/ApplicationAdapter.h"
#include "adjunct/quick/managers/ManagerHolder.h"
#include "adjunct/widgetruntime/GadgetApplication.h"
#include "adjunct/widgetruntime/GadgetProduct.h"
#include "adjunct/widgetruntime/GadgetStartup.h"


OP_STATUS GadgetProduct::BootMe(BOOL& boot_me) const
{
	boot_me = !GadgetStartup::IsBrowserStartupRequested();
	return OpStatus::OK;
} 


OP_STATUS GadgetProduct::GetOperaProfileName(OpString& profile_name) const
{
	return GadgetStartup::GetProfileName(profile_name);
}


DesktopBootstrap::Listener* GadgetProduct::CreateBootstrapListener() const
{
	return OP_NEW(GadgetBootstrapListener, ());
}


NonBrowserApplication* GadgetBootstrapListener::CreateApplication()
{
	NonBrowserApplication* application = NULL;

	if (GadgetStartup::IsGadgetStartupRequested())
	{
		application = OP_NEW(GadgetApplication, ());
	}
	else
	{
		OP_ASSERT(!"Failed to determine application type");
	}

	return application;
}


Application* GadgetBootstrapListener::OnCreateApplication()
{
	OpAutoPtr<NonBrowserApplication> application_holder(CreateApplication());
	if (NULL == application_holder.get())
	{
		return NULL;
	}

	OpAutoPtr<ApplicationAdapter> adapter(
			OP_NEW(ApplicationAdapter, (application_holder.get())));
	if (NULL == adapter.get())
	{
		return NULL;
	}
	NonBrowserApplication* application = application_holder.release();

	// OK if listener is NULL.
	m_ui_window_listener.reset(application->CreateUiWindowListener());
	g_windowCommanderManager->SetUiWindowListener(m_ui_window_listener.get());

	m_global_context.reset(OP_NEW(GadgetUiContext, (*adapter)));
	if (NULL == m_global_context.get())
	{
		return NULL;
	}
	application->SetInputContext(*m_global_context);

	return adapter.release();
}


GadgetBootstrapListener::StartupStepResult
		GadgetBootstrapListener::OnStepStartupSequence(
				MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	return DONE;
}


OP_STATUS GadgetBootstrapListener::OnInitDesktopManagers(
		ManagerHolder& manager_holder)
{
	m_global_context->SetEnabled(TRUE);
	return manager_holder.InitManagers(ManagerHolder::TYPE_NOTIFICATION
			| ManagerHolder::TYPE_AUTO_UPDATE
			| ManagerHolder::TYPE_CONSOLE
			| ManagerHolder::TYPE_FAV_ICON /* for "save image" option */
			| ManagerHolder::TYPE_SESSION_AUTO_SAVE
#ifdef VEGA_OPPAINTER_SUPPORT
			| ManagerHolder::TYPE_QUICK_ANIMATION /* for OpToolbar */
#endif // VEGA_OPPAINTER_SUPPORT
#ifdef PLUGIN_AUTO_INSTALL
			| ManagerHolder::TYPE_PLUGIN_INSTALL
#endif // PLUGIN_AUTO_INSTALL
			);
}


void GadgetBootstrapListener::OnShutDown()
{
	m_global_context->SetEnabled(FALSE);
	g_windowCommanderManager->SetUiWindowListener(NULL);
}

#endif // WIDGET_RUNTIME_SUPPORT
