/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author spoon / Arjan van Leeuwen (arjanl)
 */

#ifndef DESKTOP_PI_MODULE_H
#define DESKTOP_PI_MODULE_H

#include "modules/hardcore/opera/module.h"

class DesktopPiModule : public OperaModule
{
public:
	DesktopPiModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	class DesktopGlobalApplication* desktop_global_application;
	class DesktopPopupMenuHandler* desktop_popup_menu_handler;
#ifdef M2_MAPI_SUPPORT
	class DesktopMailClientUtils* desktop_mail_client_utils;
#endif //M2_MAPI_SUPPORT
};

#define g_desktop_global_application g_opera->desktop_pi_module.desktop_global_application
#define g_desktop_popup_menu_handler g_opera->desktop_pi_module.desktop_popup_menu_handler
#ifdef M2_MAPI_SUPPORT
#define g_desktop_mail_client_utils g_opera->desktop_pi_module.desktop_mail_client_utils
#endif //M2_MAPI_SUPPORT

#define DESKTOP_PI_MODULE_REQUIRED

#endif // DESKTOP_PI_MODULE_H
