/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author spoon / Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/desktop_pi_module.h"

#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/desktop_pi/DesktopPopupMenuHandler.h"
#include "adjunct/desktop_pi/DesktopMailClientUtils.h"

DesktopPiModule::DesktopPiModule()
  : desktop_global_application(0)
  , desktop_popup_menu_handler(0)
#ifdef M2_MAPI_SUPPORT
  , desktop_mail_client_utils(0)
#endif //M2_MAPI_SUPPORT
{
}

void DesktopPiModule::InitL(const OperaInitInfo& info)
{
	LEAVE_IF_ERROR(DesktopGlobalApplication::Create(&desktop_global_application));
	LEAVE_IF_ERROR(DesktopPopupMenuHandler::Create(&desktop_popup_menu_handler));
#ifdef M2_MAPI_SUPPORT
	LEAVE_IF_ERROR(DesktopMailClientUtils::Create(&desktop_mail_client_utils));
#endif //M2_MAPI_SUPPORT
}

void DesktopPiModule::Destroy()
{
	OP_DELETE(desktop_global_application);
	desktop_global_application = 0;

	OP_DELETE(desktop_popup_menu_handler);
	desktop_popup_menu_handler = 0;
#ifdef M2_MAPI_SUPPORT
	OP_DELETE(desktop_mail_client_utils);
	desktop_mail_client_utils = 0;
#endif //M2_MAPI_SUPPORT
}
