/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SESSION_AUTO_SAVE_MANAGER_H
#define SESSION_AUTO_SAVE_MANAGER_H

#include "modules/widgets/OpWidget.h" // OpDelayedTriggerListener
#include "adjunct/quick/managers/DesktopManager.h"

#define g_session_auto_save_manager (SessionAutoSaveManager::GetInstance())

class SessionAutoSaveManager : public OpDelayedTriggerListener, public DesktopManager<SessionAutoSaveManager>
{
public:
	SessionAutoSaveManager();
	~SessionAutoSaveManager() {}

	OP_STATUS Init() { return OpStatus::OK; }

	void SaveCurrentSession(const uni_char* save_as_filename = NULL, BOOL shutdown = FALSE, BOOL delay = TRUE, BOOL only_save_active_window = FALSE);

	void PostSaveRequest() { SetDirty(TRUE); }
	void CancelSaveRequest() { SetDirty(FALSE); }

	void SaveDefaultStartupSize();

	void LockSessionWriting() { m_session_write_lock++; }
	void UnlockSessionWriting() { m_session_write_lock--; }

private:
	void SetDirty( BOOL state );
	void OnTrigger(OpDelayedTrigger*);

	BOOL  m_is_dirty;
	OpDelayedTrigger m_delayed_trigger;
	int m_session_write_lock;
};

#endif // SESSION_AUTO_SAVE_MANAGER_H
