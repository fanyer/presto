/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "OpQuickFind.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick/Application.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/widgets/OpEdit.h"
#include "modules/dragdrop/dragdrop_manager.h"

/***********************************************************************************
**
**	OpQuickFind
**
***********************************************************************************/
DEFINE_CONSTRUCT(OpQuickFind)

OpQuickFind::OpQuickFind()
  : OpDropDown(TRUE)
  , m_target_treeview(NULL)
  , m_auto_target(TRUE)
{
	RETURN_VOID_IF_ERROR(init_status);

	edit->SetOnChangeOnEnter(FALSE);
	edit->UpdateSkinPadding();
	GetBorderSkin()->SetImage("Dropdown Search Skin", "Dropdown Skin");
	GetForegroundSkin()->SetImage("Search");

	OpString ghost_text;
	g_languageManager->GetString(Str::S_QUICK_FIND, ghost_text);

	SetGhostText(ghost_text.CStr());
	SetListener(this);
}

/***********************************************************************************
**
**	SetBusy
**
***********************************************************************************/

void OpQuickFind::SetBusy(BOOL busy)
{
	if (m_target_treeview && m_target_treeview->GetAsyncMatch())
		GetForegroundSkin()->SetImage(busy ? "Quick Find Busy" : "Search");
}

/***********************************************************************************
**
**	OnDeleted
**
***********************************************************************************/

void OpQuickFind::OnDeleted()
{
	if (m_target_treeview)
	{
		OpString filter;
		GetText(filter);

		m_target_treeview->AddSavedMatch(filter);
		m_target_treeview->RemoveTreeViewListener(this);
		m_target_treeview = NULL;
	}

	OpDropDown::OnDeleted();
}

/***********************************************************************************
**
**	SetTargetTreeView
**
***********************************************************************************/

void OpQuickFind::SetTarget(OpTreeView* target_treeview, bool auto_target)
{
	m_auto_target = auto_target;

	if (m_target_treeview == target_treeview)
		return;

	if (m_target_treeview)
		m_target_treeview->RemoveTreeViewListener(this);

	m_target_treeview = target_treeview;

	if (m_target_treeview)
	{
		m_target_treeview->AddTreeViewListener(this);
		SetText(m_target_treeview->GetMatchText());
	}
}

/***********************************************************************************
**
**	UpdateTarget
**
***********************************************************************************/

void OpQuickFind::UpdateTarget()
{
	if (!m_auto_target)
		return;

	SetTarget((OpTreeView*) g_input_manager->GetTypedObject(OpTypedObject::WIDGET_TYPE_TREEVIEW, GetParent()), true);

	if (!m_target_treeview)
		SetTarget((OpTreeView*) g_input_manager->GetTypedObject(OpTypedObject::WIDGET_TYPE_TREEVIEW, GetParent()), true);
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void OpQuickFind::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (m_target_treeview)
	{
		OpString filter;

		if (widget == this)
		{
			INT32 selected = GetSelectedItem();
			INTPTR userdata = (INTPTR) GetItemUserData(selected);

			if (userdata == 1)
			{
				SetText(NULL);
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
		m_target_treeview->SetText(filter.CStr());
	}
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void OpQuickFind::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == this)
	{
		if (!down && pos != -1)
		{
		}

		((OpWidgetListener*)GetParent())->OnMouseEvent(this, pos, x, y, button, down, nclicks);
	}
	else
	{
		OpDropDown::OnMouseEvent(widget, pos, x, y, button, down, nclicks);
	}
}

/***********************************************************************************
**
**	OnFocus
**
***********************************************************************************/

void OpQuickFind::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	OpDropDown::OnFocus(focus, reason);

	if (!focus && m_target_treeview)
	{
		OpString filter;
		GetText(filter);

		m_target_treeview->AddSavedMatch(filter);
	}
}

/***********************************************************************************
**
**	OnDropdownMenu
**
***********************************************************************************/

void OpQuickFind::OnDropdownMenu(OpWidget *widget, BOOL show)
{
	if (!m_target_treeview)
		return;

	if (show)
	{
		INT32 count = m_target_treeview->GetSavedMatchCount();
		INT32 got_index;

		OpString clear_text;
		g_languageManager->GetString(Str::S_CLEAR_EDIT_FIELD, clear_text);

		AddItem(clear_text.CStr(), -1, &got_index, 1);

		OpString filter;
		GetText(filter);

		if (filter.IsEmpty())
		{
			ih.GetItemAtNr(got_index)->SetEnabled(FALSE);
		}

		if (count)
		{
			AddItem(NULL, -1, &got_index);
			ih.GetItemAtNr(got_index)->SetSeperator(TRUE);
		}

		for (INT32 i = 0; i < count; i++)
		{
			OpString match;
			m_target_treeview->GetSavedMatch(i, match);

			if (match.HasContent())
			{
				AddItem(match.CStr());
			}
		}

	}
	else
	{
		Clear();
	}
}

/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void OpQuickFind::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	DesktopDragObject* drag_object = GetDragObject(GetType() == WIDGET_TYPE_MAIL_SEARCH_FIELD ? OpTypedObject::WIDGET_TYPE_MAIL_SEARCH_FIELD : OpTypedObject::DRAG_TYPE_QUICK_FIND, x, y);

	if (drag_object)
	{
		drag_object->SetObject(this);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpQuickFind::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			switch (action->GetChildAction()->GetAction())
			{
				case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
				{
					OpString filter;
					GetText(filter);

					if (action->GetChildAction()->GetActionData() == OP_KEY_ENTER)
					{
						if (m_target_treeview)
						{
							m_target_treeview->SetText(filter.CStr());
							m_target_treeview->AddSavedMatch(filter);
						}
						return TRUE;
					}
					else if (action->GetChildAction()->GetActionData() == OP_KEY_ESCAPE)
					{
						if (filter.HasContent())
						{
							SetText(NULL);
							if (m_target_treeview)
								m_target_treeview->SetText(NULL);
						}
						else
						{
							if (m_target_treeview)
								m_target_treeview->SetFocus(FOCUS_REASON_ACTION);
						}
						return TRUE;
					}
					break;
				}
			}
			break;
		}
	}

	if (OpDropDown::OnInputAction(action))
		return TRUE;

	if (m_target_treeview)
		return m_target_treeview->OnInputAction(action);

	return FALSE;
}

/***********************************************************************************
**
**	MatchText
**	Finds the text which is being matched in the treemodel and sets it in the edit field
**
***********************************************************************************/

void OpQuickFind::MatchText()
{
	if (!m_target_treeview)
		return;

	if (m_target_treeview->GetMatchText() && *m_target_treeview->GetMatchText())
		SetBusy(TRUE);

	//get the cursor position before the text in the edit field is updated
	int last_caret_pos = edit->GetCaretPos();
	//get the matched text (ie. filter) in the treeview and set it in the edit field
	SetText(m_target_treeview->GetMatchText());
	//restore the cursor position (bug #194378)
	edit->SetCaretPos(last_caret_pos);
}
