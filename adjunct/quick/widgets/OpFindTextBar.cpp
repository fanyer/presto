/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "adjunct/quick/widgets/OpFindTextBar.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/widgets/OpSearchEdit.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/managers/FindTextManager.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "modules/locale/oplanguagemanager.h"

DEFINE_CONSTRUCT(OpFindTextBar)

OpFindTextBar::OpFindTextBar()
	: m_match_case(NULL)
	, m_match_word(NULL)
{
	SetListener(this);
	SetWrapping(OpBar::WRAPPING_OFF);
	SetShrinkToFit(TRUE);
	SetStandardToolbar(FALSE);
	SetButtonType(OpButton::TYPE_TOOLBAR);
	GetBorderSkin()->SetImage("Find Text Toolbar Skin");
	SetName("Find Text Toolbar");

	if (OpWidget *search_edit = GetWidgetByType(OpTypedObject::WIDGET_TYPE_SEARCH_EDIT, FALSE))
		((OpSearchEdit*)search_edit)->SetForceSearchInpage(TRUE);
}

void OpFindTextBar::ClearStatus()
{
	SetInfoLabel(UNI_L(""));
	if (OpWidget *search_edit = GetWidgetByType(OpTypedObject::WIDGET_TYPE_SEARCH_EDIT, FALSE))
		((OpSearchEdit*)search_edit)->SetSearchStatus(OpSearchEdit::SEARCH_STATUS_NORMAL);
}

void OpFindTextBar::SetInfoLabel(const uni_char *text)
{
	if (OpLabel *label = (OpLabel*) GetWidgetByType(OpTypedObject::WIDGET_TYPE_LABEL))
		label->SetText(text);
}

BOOL OpFindTextBar::SetAlignmentAnimated(Alignment alignment, double animation_duration)
{
	if (alignment == ALIGNMENT_OFF && animation_duration == -1)
	{
		FindTextManager *find_text_manager = g_application->GetActiveBrowserDesktopWindow()->GetFindTextManager();
		if (find_text_manager &&
			find_text_manager->GetMode() == FindTextManager::FIND_MODE_TIMED &&
			IsFocused(TRUE))
		{
			// Slowed animation when closing by the timeout.
			animation_duration = 1000;
		}
	}
	return OpToolbar::SetAlignmentAnimated(alignment, animation_duration);
}

void OpFindTextBar::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	if (focus)
	{
		if (OpWidget* widget = GetWidgetByType(WIDGET_TYPE_SEARCH_EDIT))
			widget->SetFocus(reason);
	}
}

void OpFindTextBar::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if (new_input_context &&
		new_input_context->GetType() != OpTypedObject::WIDGET_TYPE_SEARCH_EDIT &&
		new_input_context->IsChildInputContextOf(this) &&
		g_application->GetActiveBrowserDesktopWindow())
	{
		// If we focus something else than the searchfield in the toolbar, we should abort the close timer.
		FindTextManager *find_text_manager = g_application->GetActiveBrowserDesktopWindow()->GetFindTextManager();
		if (find_text_manager)
			find_text_manager->StopTimer();
	}
	OpToolbar::OnKeyboardInputGained(new_input_context, old_input_context, reason);
}

void OpFindTextBar::OnReadContent(PrefsSection *section)
{
	OpToolbar::OnReadContent(section);

	m_match_case = GetWidgetByNameInSubtree("MatchCase");
	m_match_word = GetWidgetByNameInSubtree("WholeWord");

	if (OpWidget *search_edit = GetWidgetByType(OpTypedObject::WIDGET_TYPE_SEARCH_EDIT, FALSE))
		((OpSearchEdit*)search_edit)->SetForceSearchInpage(TRUE);
}

void OpFindTextBar::OnAlignmentChanged()
{
	ClearStatus();
	
	if (g_application->GetActiveBrowserDesktopWindow())
	{
		FindTextManager *find_text_manager = g_application->GetActiveBrowserDesktopWindow()->GetFindTextManager();

		if (IsFocused(TRUE) && GetAlignment() == ALIGNMENT_OFF)
		{
			// Give focus back to the page when find bar is closed.
			if (OpWidget* widget = find_text_manager->GetTargetWidget())
				widget->SetFocus(FOCUS_REASON_OTHER);
			else if (g_application->GetActiveBrowserDesktopWindow())
				g_application->GetActiveBrowserDesktopWindow()->SetFocus(FOCUS_REASON_OTHER);
		}

		if (find_text_manager->GetMode() == FindTextManager::FIND_MODE_TIMED)
		{
			if (GetAlignment() == ALIGNMENT_OFF)
			{
				// Make sure we end timed mode immediately.
				find_text_manager->EndTimedInlineFind(FALSE);
			}
			else
			{
				// Find bar is being enabled. For timed mode, we will update UI to reflect info in FindTextManager.

				if (m_match_case)
					m_match_case->SetValue(find_text_manager->GetMatchCaseFlag());

				// Never match only whole words by default.
				if (m_match_word)
					m_match_word->SetValue(0);

				if (OpWidget *search_edit = GetWidgetByType(OpTypedObject::WIDGET_TYPE_SEARCH_EDIT, FALSE))
				{
					search_edit->SetText(find_text_manager->GetSearchText().CStr());

					// Enter should activate element instead of finding next match
					search_edit->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_ACTIVATE_ELEMENT)));
				}
			}
		}
	}
}

void OpFindTextBar::UpdateFindTextManager()
{
	if (!g_application->GetActiveBrowserDesktopWindow())
		return;
	FindTextManager *find_text_manager = g_application->GetActiveBrowserDesktopWindow()->GetFindTextManager();

	if (find_text_manager->GetOnlyLinksFlag())
		find_text_manager->SetFindType(FindTextManager::FIND_TYPE_LINKS);
	else if (m_match_word && m_match_word->GetValue())
		find_text_manager->SetFindType(FindTextManager::FIND_TYPE_WORDS);
	else
		find_text_manager->SetFindType(FindTextManager::FIND_TYPE_ALL);

	if (m_match_case)
		find_text_manager->SetMatchCase(m_match_case->GetValue());

	if (OpWidget *search_edit = GetWidgetByType(OpTypedObject::WIDGET_TYPE_SEARCH_EDIT))
	{
		OpString text;
		search_edit->GetText(text);
		find_text_manager->SetSearchText(text.CStr());
	}
}

void OpFindTextBar::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (!g_application->GetActiveBrowserDesktopWindow())
		return;
	FindTextManager *find_text_manager = g_application->GetActiveBrowserDesktopWindow()->GetFindTextManager();

	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_DROPDOWN ||
		widget->GetType() == OpTypedObject::WIDGET_TYPE_CHECKBOX)
	{
		UpdateFindTextManager();

		ClearStatus();
		find_text_manager->SearchAgain(TRUE, TRUE);
	}
}

BOOL OpFindTextBar::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CANCEL:
		{
			if (!this->GetParentDesktopWindow()->IsDialog())
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);
				g_input_manager->InvokeAction(OpInputAction::ACTION_DESELECT_ALL);
				SetAlignmentAnimated(ALIGNMENT_OFF);
			}
			break;
		}
		case OpInputAction::ACTION_SEARCH_WRAPPED:
		{
			OpString message;
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_SEARCH_WRAPPED, message));
			SetInfoLabel(message.CStr());
			return TRUE;
		}
		case OpInputAction::ACTION_SEARCH_NOT_FOUND:
		{
			OpString message, message_with_text, text;
			if (GetWidgetByType(WIDGET_TYPE_SEARCH_EDIT))
				GetWidgetByType(WIDGET_TYPE_SEARCH_EDIT)->GetText(text);
			RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_SEARCH_FAIL_MSG, message));
			RETURN_IF_ERROR(message_with_text.AppendFormat(message.CStr(), text.CStr()));

			SetInfoLabel(message_with_text.CStr());

			if (OpWidget *search_edit = GetWidgetByType(OpTypedObject::WIDGET_TYPE_SEARCH_EDIT, FALSE))
				((OpSearchEdit*)search_edit)->SetSearchStatus(OpSearchEdit::SEARCH_STATUS_NOMATCH);
			return TRUE;
		}
		case OpInputAction::ACTION_FIND_INLINE:
		case OpInputAction::ACTION_FIND_NEXT:
		case OpInputAction::ACTION_FIND_PREVIOUS:
			UpdateFindTextManager();
			ClearStatus();
			break;
		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			OpInputAction* child_action = action->GetChildAction();
			if (IsAnimatingOut() && child_action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED)
			{
				// If the user press a key when the toolbar is being animated out, we should reactivate timed mode.
				int key = child_action->GetActionData();
				if (key != OP_KEY_ESCAPE)
				{
					if (BrowserDesktopWindow* bwin = g_application->GetActiveBrowserDesktopWindow())
						bwin->GetFindTextManager()->StartTimedInlineFind(bwin->GetFindTextManager()->GetOnlyLinksFlag());
				}
			}
			if (child_action->IsMoveAction())
			{
				// Update timer on user activity (such as move action). FindTextManager automatically does it for changed text.
				if (g_application->GetActiveBrowserDesktopWindow())
					g_application->GetActiveBrowserDesktopWindow()->GetFindTextManager()->UpdateTimer();
			}
		}
		break;
	};
	return OpToolbar::OnInputAction(action);
}
