/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/prefs/PrefsCollectionM2.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2_ui/widgets/MailSearchField.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/quick/Application.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpEdit.h"

/***********************************************************************************
**
**	MailSearchField
**
***********************************************************************************/
DEFINE_CONSTRUCT(MailSearchField)

MailSearchField::MailSearchField()
  : OpQuickFind()
  , m_ignore_match_text(FALSE)
{
	RETURN_VOID_IF_ERROR(init_status);

	OpString ghost_text;
	if (g_application->HasFeeds() && !g_application->HasMail())
		g_languageManager->GetString(Str::S_SEARCH_FOR_FEEDS, ghost_text);
	else
		g_languageManager->GetString(Str::S_SEARCH_FOR_MAIL, ghost_text);

	SetGhostText(ghost_text.CStr());
	SetListener(this);
	TRAP(init_status, g_pcm2->RegisterListenerL(this));
	/* reenable when performance problems have been fixed:
	edit->autocomp.SetType(AUTOCOMPLETION_M2_MESSAGES);
	edit->autocomp.SetTargetWidget(this);*/
}

/***********************************************************************************
**
**	~MailSearchField
**
***********************************************************************************/
MailSearchField::~MailSearchField()
{
	//edit->autocomp.SetTargetWidget(NULL);
	g_pcm2->UnregisterListener(this);
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void MailSearchField::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (m_target_treeview)
	{
		OpString filter;

		if (widget == this)
		{
			INT32 selected = GetSelectedItem();
			INTPTR userdata = (INTPTR) GetItemUserData(selected);

			if (userdata == CLEAR_ALL_SEARCHES)
			{
				m_target_treeview->DeleteAllSavedMatches();
				SetText(NULL);
			}
			else if (userdata == SEARCH_IN_CURRENT_VIEW || userdata == SEARCH_IN_ALL_MESSAGES)
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_SET_MAIL_SEARCH_TYPE, userdata == SEARCH_IN_CURRENT_VIEW ? 0 : 1, 0, this);
			}
			else
			{
				OpStringItem* item = ih.GetItemAtNr(selected);
				if (item)
				{
					SetText(item->string.Get());
					edit->SelectText();
				}
			}
		}

		GetText(filter);
		DesktopWindow* window = g_application->GetActiveDesktopWindow(FALSE);
		if (window && window->GetType() == WINDOW_TYPE_MAIL_VIEW)
		{
			if (filter.HasContent())
			{
				// in some cases searching will create a new treeview and therefore we must ignore MatchText (otherwise the search is aborted)
				m_ignore_match_text = TRUE;
				static_cast<MailDesktopWindow*>(window)->OnSearchingStarted(filter);
				m_ignore_match_text = FALSE;
			}
			else
			{
				static_cast<MailDesktopWindow*>(window)->OnSearchingFinished();
			}
		}
	}
}

/***********************************************************************************
**
**	OnDropdownMenu
**
***********************************************************************************/

void MailSearchField::OnDropdownMenu(OpWidget *widget, BOOL show)
{
	if (!m_target_treeview)
		return;

	if (show)
	{
		RemoveAllGroups();

		INT32 count = m_target_treeview->GetSavedMatchCount();
		INT32 got_index;

		{
			OpString current_view;
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_SEARCH_IN_CURRENT_VIEW, current_view));
			RETURN_VOID_IF_ERROR(AddItem(current_view.CStr(), -1, &got_index, SEARCH_IN_CURRENT_VIEW));

			OpString previous_searches;
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_SEARCH_IN_ALL_MESSAGES, previous_searches));
			RETURN_VOID_IF_ERROR(AddItem(previous_searches.CStr(), -1, &got_index, SEARCH_IN_ALL_MESSAGES));

			if (g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSearch) == 0)
			{
				got_index--;
			}
			ih.GetItemAtNr(got_index)->SetEnabled(FALSE);

			RETURN_VOID_IF_ERROR(AddItem(NULL, -1, &got_index));
			ih.GetItemAtNr(got_index)->SetSeperator(TRUE);
		}


		if (count > 0)
		{
			OpString previous_searches;
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_SEARCH_FIELD_PREVIOUS_SEARCHES, previous_searches));

			RETURN_VOID_IF_ERROR(BeginGroup(previous_searches.CStr()));

			for (INT32 i = 0; i < count; i++)
			{
				OpString match;
				m_target_treeview->GetSavedMatch(i, match);

				if (match.HasContent())
				{
					RETURN_VOID_IF_ERROR(AddItem(match.CStr()));
				}
			}
			EndGroup();

			RETURN_VOID_IF_ERROR(AddItem(NULL, -1, &got_index));
			ih.GetItemAtNr(got_index)->SetSeperator(TRUE);
		}

		{
			OpString clear_text;
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_CLEAR_SEARCHES, clear_text));

			RETURN_VOID_IF_ERROR(AddItem(clear_text.CStr(), -1, &got_index, CLEAR_ALL_SEARCHES));
		}

		OpString filter;
		RETURN_VOID_IF_ERROR(GetText(filter));

		if (count == 0)
		{
			ih.GetItemAtNr(got_index)->SetEnabled(FALSE);
		}
	}
	else
	{
		Clear();
	}
}

/***********************************************************************************
**
**	MatchText
**	Finds the text which is being matched in the treemodel and sets it in the edit field
**
***********************************************************************************/

void MailSearchField::MatchText()
{
	if (!m_ignore_match_text)
	{
		OpString text;
		OpQuickFind::MatchText();
		if (OpStatus::IsSuccess(GetText(text)) && text.IsEmpty())
			OnChange(NULL, FALSE);
	}
}

/***********************************************************************************
**
**	PrefChanged
**
***********************************************************************************/
void MailSearchField::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if(pref == PrefsCollectionM2::DefaultMailSearch)
	{
		OnChange(NULL, FALSE);
	}
}

#endif // M2_SUPPORT
