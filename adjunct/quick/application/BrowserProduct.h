/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef BROWSER_PRODUCT_H
#define BROWSER_PRODUCT_H

#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "adjunct/quick/application/OpStartupSequence.h"


/**
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class BrowserProduct : public DesktopBootstrap::Product
{
public:
	virtual OP_STATUS BootMe(BOOL& boot_me) const;
	virtual OP_STATUS GetOperaProfileName(OpString& profile_name) const;
	virtual DesktopBootstrap::Listener* CreateBootstrapListener() const;
};


/**
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class BrowserBootstrapListener : public DesktopBootstrap::Listener
{
public:
	BrowserBootstrapListener();

	virtual Application* OnCreateApplication();
	virtual StartupStepResult OnStepStartupSequence(MH_PARAM_1 par1,
			MH_PARAM_2 par2);
	virtual OP_STATUS OnInitDesktopManagers(ManagerHolder& manager_holder);
	virtual void OnShutDown();

private:
	OpAutoPtr<OpStartupSequence> m_browser_startup_sequence;
	class ClassicApplication* m_application;
};

#endif // BROWSER_PRODUCT_H
