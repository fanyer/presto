/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GADGET_PRODUCT_H
#define GADGET_PRODUCT_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "adjunct/widgetruntime/GadgetUiContext.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

class NonBrowserApplication;


/**
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetProduct : public DesktopBootstrap::Product
{
public:
	virtual OP_STATUS BootMe(BOOL& boot_me) const;
	virtual OP_STATUS GetOperaProfileName(OpString& profile_name) const;
	virtual DesktopBootstrap::Listener* CreateBootstrapListener() const;
};


/**
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetBootstrapListener : public DesktopBootstrap::Listener
{
public:
	virtual Application* OnCreateApplication();
	virtual StartupStepResult OnStepStartupSequence(MH_PARAM_1 par1,
			MH_PARAM_2 par2);
	virtual OP_STATUS OnInitDesktopManagers(ManagerHolder& manager_holder);
	virtual void OnShutDown();

private:
	NonBrowserApplication* CreateApplication();

	OpAutoPtr<OpUiWindowListener> m_ui_window_listener;
	OpAutoPtr<GadgetUiContext> m_global_context;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_PRODUCT_H
