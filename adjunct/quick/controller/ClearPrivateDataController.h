/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef CLEAR_PRIVATE_DATA_CONTROLLER_H
#define CLEAR_PRIVATE_DATA_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"
#include "adjunct/quick_toolkit/widgets/DialogListener.h"

class ClearPrivateDataController
		: public OkCancelDialogContext
		, public DialogListener
{
public:
	ClearPrivateDataController();

	// UiContext
	virtual BOOL DisablesAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);

	// DialogListener
	virtual void OnOk(Dialog* dialog, UINT32 result);

private:
	// DialogContext
	virtual void InitL();

	OP_STATUS InitOptions();
	void ClearPrivateData();

	PrivacyManager::Flags m_flags;

	UINT32 m_sync_passwords_deletion;
	bool m_disable_action_handling;

	OpProperty<bool> m_del_all_cookies;
	OpProperty<bool> m_del_tmp_cookies;
	OpProperty<bool> m_del_pwddocs_data;
	OpProperty<bool> m_clear_cache;
	OpProperty<bool> m_clear_plugin_data;
	OpProperty<bool> m_clear_geoloc_data;
	OpProperty<bool> m_clear_camera_permissions;
	OpProperty<bool> m_clear_visited_pages;
	OpProperty<bool> m_clear_trnsf_files_hist;
	OpProperty<bool> m_clear_email_acc_pwds;
	OpProperty<bool> m_clear_wand_pwds;
	OpProperty<bool> m_clear_bkmrk_visited_time;
	OpProperty<bool> m_close_all_windows;
	OpProperty<bool> m_clear_web_storage;
	OpProperty<bool> m_clear_extension_data;
};

#endif // CLEAR_PRIVATE_DATA_CONTROLLER_H
