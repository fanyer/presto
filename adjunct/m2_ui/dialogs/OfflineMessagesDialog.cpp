/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2_ui/dialogs/OfflineMessagesDialog.h"
#include "adjunct/m2/src/engine/engine.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
 ** Initialization
 **
 ** OfflineMessagesDialog::Init
 ** @param parent_window
 ** @param account_id
 ***********************************************************************************/
OP_STATUS OfflineMessagesDialog::Init(DesktopWindow* parent_window,
										UINT16 account_id)
{
	// Save account_id for use in OnOk()
	m_account_id = account_id;

	// Construct dialog
	OpString title, message;

	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_MAIL_MAKE_OFFLINE_MESSAGES_AVAILABLE_TITLE, title));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_MAIL_MAKE_OFFLINE_MESSAGES_AVAILABLE, message));

	return SimpleDialog::Init(WINDOW_NAME_OFFLINE_MESSAGES, title, message, parent_window, Dialog::TYPE_YES_NO, Dialog::IMAGE_QUESTION);
}


/***********************************************************************************
 **
 **
 ** OfflineMessagesDialog::OnOk
 ***********************************************************************************/
UINT32 OfflineMessagesDialog::OnOk()
{
	g_m2_engine->EnsureAllBodiesDownloaded(IndexTypes::FIRST_ACCOUNT + m_account_id);

	return SimpleDialog::OnOk();
}

#endif // M2_SUPPORT
