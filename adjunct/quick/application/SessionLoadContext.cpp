/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/quick/ClassicApplication.h"
#include "adjunct/quick/application/SessionLoadContext.h"
#include "adjunct/quick/dialogs/SessionManagerDialog.h"

class CreateSessionSelectionListener : public DesktopFileChooserListener
{
public:
	DesktopFileChooserRequest&	GetRequest() { return request; }
	CreateSessionSelectionListener(ClassicApplication& application, BOOL insert)
		: m_application(&application), insert(insert) {}

	void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
	{
		if (result.files.GetCount())
		{
			OpFile file;
			OP_STATUS err = file.Construct(result.files.Get(0)->CStr());
			if (OpStatus::IsSuccess(err))
			{
				OpSharedPtr<OpSession> session(g_session_manager->CreateSessionFromFileL(&file));
				OpStatus::Ignore(m_application->CreateSessionWindows(session, insert));
			}
		}
	}

private:
	DesktopFileChooserRequest request;
	ClassicApplication* m_application;
	BOOL insert;
};

SessionLoadContext::SessionLoadContext(ClassicApplication& application)
	: m_application(&application)
	, m_chooser(NULL)
	, m_chooser_listener(NULL)
{}

SessionLoadContext::~SessionLoadContext()
{
	OP_DELETE(m_chooser_listener);
	m_chooser_listener = NULL;

	OP_DELETE(m_chooser);
	m_chooser = NULL;
}

BOOL SessionLoadContext::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_REOPEN_WINDOW:
				{
					int index = child_action->GetActionData();
					OpSession *session = m_closed_sessions.Get(index);
					BOOL enabled = (session != NULL);
					child_action->SetEnabled(enabled);
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_INSERT_SESSION:
		case OpInputAction::ACTION_OPEN_SESSION:
		{
			if (!m_chooser)
				RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), TRUE);

			BOOL insert = (action->GetAction() == OpInputAction::ACTION_INSERT_SESSION);
			CreateSessionSelectionListener *selection_listener = OP_NEW(CreateSessionSelectionListener, (*m_application, insert));
			if (selection_listener)
			{
				m_chooser_listener = selection_listener;
				DesktopFileChooserRequest& request = selection_listener->GetRequest();
				request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
				OpString tmp_storage;
				request.initial_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SESSION_FOLDER, tmp_storage));
				g_languageManager->GetString(Str::S_SELECT_SESSION_FILE, request.caption);
				OpString filter;
				OP_STATUS rc = g_languageManager->GetString(Str::SI_WIN_SETUP_FILES, filter);
				if (OpStatus::IsSuccess(rc) && filter.HasContent())
				{
					StringFilterToExtensionFilter(filter.CStr(), &request.extension_filters);
				}
				DesktopWindow* parent = m_application->GetActiveDesktopWindow(FALSE);
				OpStatus::Ignore(m_chooser->Execute(parent ? parent->GetOpWindow() : NULL, selection_listener, request));
			}
			return TRUE;
		}

		case OpInputAction::ACTION_REOPEN_WINDOW:
		{
			int index = action->GetActionData();
			OpSharedPtr<OpSession> session(m_closed_sessions.Get(index));
			if (OpStatus::IsSuccess(m_application->CreateSessionWindows(session)))
				m_closed_sessions.Remove(index);

			break;
		}

		case OpInputAction::ACTION_SELECT_SESSION:
		{
			INT32 session_no = action->GetActionData();

			if (session_no + 1 == 0)
			{
				SessionManagerDialog* dialog = OP_NEW(SessionManagerDialog, (*m_application));
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow(TRUE));
				return TRUE;
			}

			OpSharedPtr<OpSession> session(g_session_manager->ReadSession(session_no));
			OpStatus::Ignore(m_application->CreateSessionWindows(session));

			return TRUE;
		}
	}

	return FALSE;
}
