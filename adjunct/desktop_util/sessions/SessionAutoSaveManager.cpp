/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/sessions/SessionAutoSaveManager.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/windows/DesktopGadget.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/util/opfile/opfile.h"

SessionAutoSaveManager::SessionAutoSaveManager()
	: m_is_dirty(FALSE)
	, m_session_write_lock(0)
{
	// set up the delayed trigger with 3000ms delay
	m_delayed_trigger.SetDelayedTriggerListener(this);
	m_delayed_trigger.SetTriggerDelay(0, 3000);
}

void SessionAutoSaveManager::SaveCurrentSession(const uni_char* save_as_filename, BOOL shutdown, BOOL delay, BOOL only_save_active_window)
{
	if (m_session_write_lock > 0)
		return;
#ifdef _MACINTOSH_
	// Mac needs still save its state when there are no windows open
    if (g_application->IsExiting())
    {
    	return;
	}
#else
	// The g_application->GetDesktopWindowCollection().GetCount() == 0 is there because a few users 
	// have reported empty session files after clean exits (using icewm under linux)

	if( g_application->IsExiting() || g_application->GetDesktopWindowCollection().GetCount() == 0 || !g_application->IsBrowserStarted())
	{
		return;
	}
#endif // _MACINTOSH_

	OpAutoPtr<OpSession> session_p;
	OP_STATUS err;

	if (save_as_filename)
	{
		OpSession* session = OP_NEW(OpSession, ());
		if (!session)
			return;

		session_p.reset(session);

		OpFile file;

		RETURN_VOID_IF_ERROR(file.Construct(save_as_filename));

		PrefsFile* sessionfile = OP_NEW(PrefsFile, (PREFS_INI));
		if (!sessionfile)
			return;

		OpAutoPtr<PrefsFile> sessionfile_p(sessionfile);

		TRAP(err, sessionfile->ConstructL(); \
			sessionfile->SetFileL(&file););
		RETURN_VOID_IF_ERROR(err);

		TRAP(err, session->InitL(sessionfile_p.release()));
		if(OpStatus::IsError(err))
		{
			return;
		}
	}
	else
	{
		if (delay && !shutdown)
		{
 			PostSaveRequest();
			return;
		}
		OpSession* session;
		TRAP(err, session = g_session_manager->CreateSessionL(UNI_L("autosave")));
		if(OpStatus::IsError(err))
		{
			return;
		}
		session_p.reset(session);

		SaveDefaultStartupSize();
	}
	// not critical if it fails
	OpStatus::Ignore(session_p->InitForWrite());

	//Version number for session files (.win). If this numbers isn't
	//available the file will be handled as an old .win file.
	TRAP(err, session_p->SetVersionL(7000));
	OP_ASSERT(OpStatus::IsSuccess(err)); 
	if(OpStatus::IsError(err))
	{
		return;
	}
	if(!only_save_active_window)
	{
		DesktopWindowCollection& model = g_application->GetDesktopWindowCollection();
		for (DesktopWindowCollectionItem* item = model.GetFirstToplevel(); item; item = item->GetSiblingItem())
		{
			DesktopWindow* desktopwindow = item->GetDesktopWindow();
			if (desktopwindow && !desktopwindow->PrivacyMode() && desktopwindow->VisibleInWindowList())
				desktopwindow->AddToSession(session_p.get(), 0, shutdown);
		}
	}
	else
	{
		DesktopWindow* desktopwindow = g_application->GetActiveDesktopWindow();

		if(OpStatus::IsError(desktopwindow->AddToSession(session_p.get(), 0, shutdown)))
		{
			return;
		}
	}
#ifdef GADGET_SUPPORT
	//Let the gadget manager save all gadgets and its data
    g_gadget_manager->SaveGadgets();
#endif // GADGET_SUPPORT

	TRAP(err, g_session_manager->WriteSessionToFileL(session_p.get(), TRUE));
	OP_ASSERT(OpStatus::IsSuccess(err)); 
}

void SessionAutoSaveManager::SaveDefaultStartupSize()
{
	if (g_application->GetDesktopWindowCollection().GetFirstToplevel())
	{
		DesktopWindow* desktopwindow = g_application->GetDesktopWindowCollection().GetFirstToplevel()->GetDesktopWindow();
		if( desktopwindow )
		{
			const char* window_name = desktopwindow->GetWindowName();
			if( window_name )
			{
				OpRect rect;
				OpWindow::State state;
				WinSizeState old_type_state = INVALID_WINSIZESTATE;

				desktopwindow->GetOpWindow()->GetDesktopPlacement(rect, state);
				switch (state)
				{
					case OpWindow::RESTORED: old_type_state = NORMAL; break;
					case OpWindow::MINIMIZED: old_type_state = ICONIC; break;
					case OpWindow::MAXIMIZED: old_type_state = MAXIMIZED; break;
#ifdef RESTORE_MAC_FULLSCREEN_ON_STARTUP
					case OpWindow::FULLSCREEN: old_type_state = FULLSCREEN; break;
#else
					case OpWindow::FULLSCREEN: old_type_state = MAXIMIZED; break;
#endif //RESTORE_MAC_FULLSCREEN_ON_STARTUP
				}

				if (old_type_state != ICONIC)
				{
					OpString name_str;
					name_str.Set(window_name);
					TRAPD(err, g_pcui->WriteWindowInfoL(name_str, rect, old_type_state));
				}
			}
		}
	}
}

void SessionAutoSaveManager::SetDirty( BOOL state )
{
	if( m_is_dirty != state )
	{
		m_is_dirty = state;
		if( m_is_dirty )
		{
			// Start delayed save
			m_delayed_trigger.InvokeTrigger();
		}
	}
}

void SessionAutoSaveManager::OnTrigger(OpDelayedTrigger*)
{
	SaveCurrentSession(NULL,FALSE,FALSE);
	m_is_dirty = FALSE;
}
