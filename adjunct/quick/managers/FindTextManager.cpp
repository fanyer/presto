/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/managers/FindTextManager.h"

#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/dialogs/FindTextDialog.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/doc/doc.h"
#include "modules/dochand/win.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/widgets/OpSearchEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/locale/oplanguagemanager.h"

class TextNotFoundDialog : public SimpleDialog
{
	public:
		OP_STATUS Init(DesktopWindow* parent_window, const uni_char* text)
		{
			OpString title;
			OpString message, message_with_text;

			RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_SEARCH_FAIL_MSG, message));
			RETURN_IF_ERROR(message_with_text.AppendFormat(message.CStr(), text));

			RETURN_IF_ERROR(g_languageManager->GetString( Str::SI_IDSTR_SEARCH_FAIL_CAPTION, title));

			return SimpleDialog::Init(WINDOW_NAME_TEXT_NOT_FOUND, title, message_with_text, parent_window);
		}

		UINT32 OnOk()
		{
			return 0;
		}

		DialogType GetDialogType() {return TYPE_OK;}
};


FindTextManager::FindTextManager()
:	m_forward(TRUE),
	m_match_case(FALSE),
	m_whole_word(FALSE),
	m_only_links(FALSE),
	m_mode(FIND_MODE_NORMAL),
	m_parent_window(NULL),
	m_dialog_present(FALSE),
	m_timer(NULL)
{
}

FindTextManager::~FindTextManager()
{
	OP_DELETE(m_timer);
}


OP_STATUS FindTextManager::Init(DesktopWindow* parent_window)
{
	m_parent_window = parent_window;
	m_timer = OP_NEW(OpTimer, ());
	if (!m_timer)
		return OpStatus::ERR_NO_MEMORY;

	m_timer->SetTimerListener(this);
	return OpStatus::OK;
}


void FindTextManager::ShowFindTextDialog()
{
	if (!m_dialog_present)
	{
		FindTextDialog* dialog = OP_NEW(FindTextDialog, (this));

		if (dialog)
		{
			dialog->Init(m_parent_window);
			m_dialog_present = TRUE;
		}
	}
}


void FindTextManager::SetFlags(BOOL forward, BOOL match_case, BOOL whole_word)
{
	m_forward = forward;
	m_match_case = match_case;
	m_whole_word = whole_word;
}


void FindTextManager::SetSearchText(const uni_char* search_text)
{
	if (m_text.Compare(search_text) == 0)
		return;
	m_text.Set(search_text);

	// Text changed, so we should update the timer to postpone the close
	UpdateTimer();
}



void FindTextManager::OnDialogKilled()
{
	m_dialog_present = FALSE;
}


void FindTextManager::Search()
{
	DoSearch(m_forward, FALSE);
}


void FindTextManager::SearchAgain(BOOL forward, BOOL search_inline)
{
	if (m_parent_window)
	{
		if (m_text.IsEmpty())
		{
			if (!search_inline)
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_FINDTEXT, 1);
			}
		}
		else
		{
			DoSearch(forward, FALSE);
		}
	}
}


void FindTextManager::SearchInline(const uni_char* text)
{
	SetSearchText(text);
	DoSearch(TRUE, TRUE);
}

void FindTextManager::SetFindType(FIND_TYPE type)
{
	m_whole_word = FALSE;
	m_only_links = FALSE;
	if (type == FIND_TYPE_LINKS)
		m_only_links = TRUE;
	else if (type == FIND_TYPE_WORDS)
		m_whole_word = TRUE;
}
void FindTextManager::SetMatchCase(BOOL match_case)
{
	m_match_case = match_case;
}

BOOL FindTextManager::DoSearch(BOOL forward, BOOL incremental)
{
	UpdateTimer();

	OpWidget* widget = GetTargetWidget();

	if (!widget)
		return FALSE;

	switch (widget->GetType())
	{
		case OpTypedObject::WIDGET_TYPE_BROWSERVIEW:
		{
			OpBrowserView* browser_view = (OpBrowserView*) widget;

			if (m_text.IsEmpty())
			{
				WindowCommanderProxy::ClearSelection(browser_view, TRUE, TRUE);
				return TRUE;
			}

#ifdef SEARCH_MATCHES_ALL
			BOOL wrapped = FALSE;
			if (WindowCommanderProxy::HighlightNextMatch(browser_view,
														 m_text.CStr(),
														 m_match_case,
														 m_whole_word,
														 m_only_links,
														 forward,
														 TRUE,
														 wrapped))
			{
				if (wrapped)
					g_input_manager->InvokeAction(OpInputAction::ACTION_SEARCH_WRAPPED);
				return TRUE;
			}
#else // SEARCH_MATCHES_ALL
			if (browser_view->GetWindow()->SearchText(m_text.CStr(), forward, m_match_case, m_whole_word, !incremental, TRUE, m_only_links))
				return TRUE;
#endif // SEARCH_MATCHES_ALL

			g_input_manager->InvokeAction(OpInputAction::ACTION_SEARCH_NOT_FOUND);

			return FALSE;
		}
		case OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT:
		{
			OpMultilineEdit* multi_edit = (OpMultilineEdit*) widget;

			if (m_text.IsEmpty())
			{
				multi_edit->SelectNothing();
				return TRUE;
			}

			if (multi_edit->SearchText(m_text.CStr(), m_text.Length(), forward, m_match_case, m_whole_word, SEARCH_FROM_CARET, TRUE, TRUE, FALSE, !incremental))
				return TRUE;
			else
			{
				BOOL found;
				if (forward)
					found = multi_edit->SearchText(m_text.CStr(), m_text.Length(), forward, m_match_case, m_whole_word, SEARCH_FROM_BEGINNING, TRUE, TRUE, FALSE, !incremental);
				else
					found = multi_edit->SearchText(m_text.CStr(), m_text.Length(), forward, m_match_case, m_whole_word, SEARCH_FROM_END, TRUE, TRUE, FALSE, !incremental);

				if (found)
				{
					g_input_manager->InvokeAction(OpInputAction::ACTION_SEARCH_WRAPPED);
					return TRUE;
				}
			}

			g_input_manager->InvokeAction(OpInputAction::ACTION_SEARCH_NOT_FOUND);

			multi_edit->SelectNothing();

			return FALSE;
		}
	}

	return FALSE;
}

OpWidget* FindTextManager::GetTargetWidget()
{
	if (!m_parent_window)
		return NULL;

	DesktopWindow* desktop_window = m_parent_window;

	if (m_parent_window->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		desktop_window = ((BrowserDesktopWindow*)m_parent_window)->GetActiveDesktopWindow();
	}

	if (!desktop_window)
		return NULL;

	OpBrowserView* browser_view = desktop_window->GetBrowserView();

	if (browser_view)
		return browser_view;

	return desktop_window->GetWidgetByType(OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT);
}

void FindTextManager::ShowNotFoundMsg()
{
	TextNotFoundDialog* dlg = OP_NEW(TextNotFoundDialog, ());
	if (!dlg)
		return;

	DesktopWindow* find_text_dialog = g_application->GetDesktopWindowCollection().GetDesktopWindowByType(OpTypedObject::DIALOG_TYPE_FIND_TEXT);

	DesktopWindow* desktop_window = m_parent_window;

	if (m_parent_window->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		desktop_window = ((BrowserDesktopWindow*)m_parent_window)->GetActiveDesktopWindow();
	}

	dlg->Init(find_text_dialog ? find_text_dialog : desktop_window, m_text.CStr());
}

/***********************************************************************************
**
**	UpdateTimer
**
***********************************************************************************/

void FindTextManager::UpdateTimer()
{
	if (m_mode == FIND_MODE_TIMED)
		StartTimer();
}

/***********************************************************************************
**
**	StartTimedInlineFind
**
***********************************************************************************/

void FindTextManager::StartTimedInlineFind(BOOL only_links)
{
	OpWidget* widget = GetTargetWidget();

	if (!widget)
		return;

	m_mode = FIND_MODE_TIMED;
	m_only_links = only_links;
	m_match_case = FALSE;
	m_text.Empty();

	g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_FINDTEXT, 1);

	UpdateTimer();
}

/***********************************************************************************
**
**	EndTimedInlineFind
**
***********************************************************************************/

void FindTextManager::EndTimedInlineFind(BOOL close_with_fade)
{
	g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_FINDTEXT, 0);
	m_mode = FIND_MODE_NORMAL;
	m_timer->Stop();

	if (!close_with_fade)
	{
		// Only focus page if we're not closing with fade.
		// When closing with fade the user should be able to abort the close by typing again, so it will be unfocused after the fadeout instead.
		if (OpWidget* widget = GetTargetWidget())
			widget->SetFocus(FOCUS_REASON_OTHER);
		else if (m_parent_window)
			m_parent_window->SetFocus(FOCUS_REASON_OTHER);
	}
}

void FindTextManager::OnTimeOut(OpTimer* timer)
{
	if (OpWidget *focused = OpWidget::GetFocused())
		if (focused->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_EDIT && ((OpSearchEdit*)focused)->imstring)
		{
			// We don't get changes to the search text during IME compose. So during compose we
			// will update the timer instead of ending the search.
			UpdateTimer();
			return;
		}
	EndTimedInlineFind(TRUE);
}
