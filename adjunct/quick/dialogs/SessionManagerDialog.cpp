/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "adjunct/quick/dialogs/SessionManagerDialog.h"

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "adjunct/quick/ClassicApplication.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/inputmanager/inputmanager.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
**
**	SessionManagerDialog
**
***********************************************************************************/

void SessionManagerDialog::OnInit()
{
	PrefsFile* defaultsessionfile = NULL;
	OpSession* OP_MEMORY_VAR tempdefsession = NULL;
	OpFile winstore;
	TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::WindowsStorageFile, winstore));
	if (OpStatus::IsSuccess(rc))
	{
		defaultsessionfile = OP_NEW(PrefsFile, (PREFS_INI));
		if (defaultsessionfile)
		{
			TRAPD(err,defaultsessionfile->ConstructL());
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
			TRAP(err,defaultsessionfile->SetFileL(&winstore));
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
			OpSession* tds = OP_NEW(OpSession, ());
			if (tds)
			{
				tempdefsession = tds;
				TRAP(err, tds->InitL(defaultsessionfile));
				OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
			}
			else
				OP_DELETE(defaultsessionfile);
		}
	}
	OP_MEMORY_VAR OP_STATUS err;
	TRAP(err, g_session_manager->ScanSessionFolderL());
	OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ? Fails when unable to create folder.

	OP_MEMORY_VAR INT32 defaultpos = -1;
	for (OP_MEMORY_VAR UINTPTR i = 0; i < g_session_manager->GetSessionCount(); i++)
	{
		const uni_char* OP_MEMORY_VAR name = g_session_manager->GetSessionNameL(i).CStr();
		INT32 sort_order = 1;

		OpString auto_saved;
		g_languageManager->GetString(Str::S_PREVIOUS_SESSION_AUTOSAVED, auto_saved);

		if (uni_stricmp(name, UNI_L("autosave")) == 0)
		{
			// if autosave is the only one there, don't list it

			if (g_session_manager->GetSessionCount() == 1)
			{
				break;
			}
			else
			{
				name = auto_saved.CStr();
				sort_order = 0;
			}
		}

		m_sessions_model.AddItem(name, "Session", sort_order, -1, reinterpret_cast<void*>(i));

		if(tempdefsession)
		{
			OpSession* session = g_session_manager->ReadSessionL(i);

			if(*session == (const OpSession*)tempdefsession)
			{
				defaultpos = i;
			}

			OP_DELETE(session);
			session = NULL;
		}
	}

	OP_DELETE(tempdefsession);
	tempdefsession=NULL;

	OpTreeView* sessions = (OpTreeView*) GetWidgetByName("Sessions_treeview");

	sessions->SetShowColumnHeaders(FALSE);
	sessions->SetTreeModel(&m_sessions_model);
	sessions->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_SELECT_SESSION)));
	sessions->Sort(0, TRUE);
	sessions->SetListener(this);
	if( defaultpos != -1 )
	{
		sessions->ToggleItem(defaultpos);
		sessions->SetSelectedItem(defaultpos);
	}
	else
	{
		sessions->SetSelectedItem(0);
	}
	sessions->StopPendingOnChange();
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 SessionManagerDialog::OnOk()
{
	return 0;
}

/***********************************************************************************
**
**	OnCancel
**
***********************************************************************************/

void SessionManagerDialog::OnCancel()
{

}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void SessionManagerDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("Sessions_treeview"))
	{
		//
	}

	Dialog::OnChange(widget, changed_by_mouse);
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL SessionManagerDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE:
		{
			SimpleTreeModelItem* item = (SimpleTreeModelItem*)((OpTreeView*)GetWidgetByName("Sessions_treeview"))->GetSelectedItem();
			if (item)
			{
				INTPTR pos = (INTPTR)item->GetUserData();
				if (-1 != pos)
				{
					if(g_session_manager->DeleteSessionL(pos))
					{
						m_sessions_model.Delete(pos);

						// Fix for bug #166171. We use the 'user_data' to store the index into the array of sessions in the
						// session manager. Once we delete a session, the 'user_data' entries must be re-calculated so that
						// they refer to the correct entry in the session manager.
						// This is not solid code in my view. We should perhaps delete by name or a unique id that gets
						// provided by the session manager [espen 2006-01-17]

						for( INT32 i=0; i<m_sessions_model.GetCount(); i++ )
						{
							SimpleTreeModelItem* item = (SimpleTreeModelItem*)((OpTreeView*)GetWidgetByName("Sessions_treeview"))->GetItemByPosition(i);
							if( (INTPTR)item->GetUserData() > pos )
							{
								item->SetUserData((void*) (((INTPTR)item->GetUserData()) - 1));
							}
						}

					}
					else
						return FALSE;
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_SELECT_SESSION:
		{
			BOOL insert = GetWidgetValue("Insert_session_checkbox");

			SimpleTreeModelItem* item = (SimpleTreeModelItem*)((OpTreeView*)GetWidgetByName("Sessions_treeview"))->GetSelectedItem();
			if (item)
			{
				INTPTR pos = (INTPTR)item->GetUserData();

				if (-1 != pos)
				{
					OpSharedPtr<OpSession> session(g_session_manager->ReadSession(pos));
					OpStatus::Ignore(m_application->CreateSessionWindows(session, insert));
				}
				CloseDialog(FALSE);
			}
			return TRUE;
		}
	}
	return Dialog::OnInputAction(action);
}
