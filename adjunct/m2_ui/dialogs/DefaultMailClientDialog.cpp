/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"
#include "adjunct/m2_ui/dialogs/DefaultMailClientDialog.h"
#include "adjunct/desktop_pi/DesktopMailClientUtils.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionM2.h"
#include "modules/locale/oplanguagemanager.h"

#ifdef M2_MAPI_SUPPORT
void DefaultMailClientDialog::InitL()
{
	LEAVE_IF_ERROR(ConnectDoNotShowAgain(*g_pcm2, PrefsCollectionM2::ShowDefaultMailClientDialog, 0, 1));
	SimpleDialogController::InitL();
}

void DefaultMailClientDialog::OnOk()
{
	SimpleDialogController::OnOk();
	g_desktop_mail_client_utils->SetOperaAsDefaultMailClient();
	g_desktop_mail_client_utils->SignalMailClientIsInitialized(TRUE);
}
#endif //M2_MAPI_SUPPORT