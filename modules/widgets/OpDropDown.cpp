/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/DropDownWindow.h"
#include "modules/widgets/OpWidgetStringDrawer.h"
#include "modules/pi/OpWindow.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/display/fontdb.h"
#include "modules/display/vis_dev.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/style/css.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/widgets/WidgetWindow.h"
#include "modules/widgets/WidgetContainer.h"

#include "modules/forms/piforms.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/viewportcontroller.h"

#ifdef NEARBY_ELEMENT_DETECTION
# include "modules/widgets/finger_touch/element_expander.h"
#endif

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
Accessibility::State OpDropDownButtonAccessor::AccessibilityGetState()
{
	return m_parent->AccessibilityGetState();
}

OP_STATUS OpDropDownButtonAccessor::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	int button_width = m_parent->GetInfo()->GetDropdownButtonWidth(m_parent);

	OpRect parent_rect;
	m_parent->AccessibilityGetAbsolutePosition(parent_rect);

	rect.x = parent_rect.x + parent_rect.width - button_width;
	rect.y = parent_rect.y;
	rect.width = button_width;
	rect.height = parent_rect.height;

	return OpStatus::OK;
}

OP_STATUS OpDropDownButtonAccessor::AccessibilityClicked()
{
	return m_parent->AccessibilityClicked();
}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

/***********************************************************************************
**
**	DropDownEdit - an OpEdit inside a dropdown
**
***********************************************************************************/


DropDownEdit::DropDownEdit(OpDropDown* dropdown) : m_dropdown(dropdown),m_hint_enabled(true)
{}

#ifdef WIDGETS_IME_SUPPORT
void DropDownEdit::OnCandidateShow(BOOL visible)
{
	OpEdit::OnCandidateShow(visible);
	m_dropdown->OnCandidateShow(visible);
}
void DropDownEdit::OnStopComposing(BOOL cancel)
{
	OpEdit::OnStopComposing(cancel);
	m_dropdown->OnStopComposing(cancel);
}
#endif // WIDGETS_IME_SUPPORT

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OpAccessibleItem* DropDownEdit::GetAccessibleLabelForControl()
{
	return m_dropdown->GetAccessibleLabelForControl();
}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

void DropDownEdit::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	OpEdit::OnFocus(focus,reason);

#ifdef _MACINTOSH_
	// this is to draw the focusborder (that we want around the entire dropdown widget)
	m_dropdown->Invalidate(m_dropdown->GetBounds().InsetBy(-2,-2), FALSE, FALSE);
#endif

	if (!focus)
	{
		m_dropdown->ClosePopup();
#ifdef WIDGETS_IME_ITEM_SEARCH
		Clear();
#endif // WIDGETS_IME_ITEM_SEARCH
	}
}

void DropDownEdit::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpEdit::OnPaint(widget_painter, paint_rect);
	widget_painter->DrawDropDownHint(GetBounds());
}

#ifdef WIDGETS_IME_ITEM_SEARCH
IM_WIDGETINFO DropDownEdit::OnCompose()
{
	IM_WIDGETINFO info = OpEdit::OnCompose();

	if (m_dropdown->IsDroppedDown())
	{
		// send the last letter to the drop down
		const uni_char* str = string.Get();
		int length = uni_strlen(str);
		if (m_string.DataPtr() && uni_strcmp(str, m_string.DataPtr()) == 0)
		{
			// don't send the character if the string is unchanged
			length = 0;
		}
		m_string.Set(str);
		if (length > 0)
		{
			OpInputAction action(OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED, str[length-1]);
			m_dropdown->OnInputAction(&action);
		}
	}

	return info;
}


BOOL DropDownEdit::OnInputAction(OpInputAction* action)
{
	if ((action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED) &&
		(action->GetActionData() == OP_KEY_LEFT || action->GetActionData() == OP_KEY_RIGHT))
	{
		OpInputAction unfocus_action(OpInputAction::ACTION_UNFOCUS_FORM);
		m_dropdown->OnInputAction(&unfocus_action);
		return FALSE;
	}

	// send inputs to the dropdown first, then the edit box
	if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION || m_dropdown->OnInputAction(action) == FALSE)
	{
		return OpEdit::OnInputAction(action);
	}
	return TRUE;
}
#endif // WIDGETS_IME_ITEM_SEARCH

OP_STATUS DropDownEdit::SetHintText(const uni_char* hint_text)
{
	if (m_hint_string.Get() == NULL || uni_strcmp(m_hint_string.Get(), hint_text))
	{
		Invalidate(GetBounds());
		return m_hint_string.Set(hint_text, this);
	}
	return OpStatus::OK;
}

void DropDownEdit::PaintHint(OpRect inner_rect,UINT32 hcol)
{
	if (!m_hint_enabled)
		return;

	VisualDevice * vd = GetVisualDevice();

	int indent = GetTextIndentation();
	inner_rect.x += indent;
	inner_rect.width -= indent;
	OpRect r = inner_rect;
	AddPadding(r);

	r.x += r.width - m_hint_string.GetWidth();
	if (string.GetWidth() < r.x)
	{
		OpWidgetStringDrawer string_drawer;
		string_drawer.SetCaretSnapForward(m_packed.caret_snap_forward);
		string_drawer.Draw(&m_hint_string, r, vd, hcol, GetEllipsis(), x_scroll);
	}
}


/***********************************************************************************
**
**	OpDropDown
**
***********************************************************************************/

OP_STATUS OpDropDown::Construct(OpDropDown** obj, BOOL editable_text)
{
#ifdef WIDGETS_IME_ITEM_SEARCH
	editable_text = TRUE;
#endif // WIDGETS_IME_ITEM_SEARCH

	*obj = OP_NEW(OpDropDown, (editable_text));
	if (*obj == NULL || OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}


OpDropDown::OpDropDown(BOOL editable_text)
	: m_posted_close_msg(FALSE)
	, ih(FALSE)
	, m_packed_init(0)
	, edit(NULL)
	, m_dropdown_window(NULL)
	, m_locked_dropdown_window(NULL)
#ifdef PLATFORM_FONTSWITCHING
	, preferred_codepage(OpFontInfo::OP_DEFAULT_CODEPAGE)
#endif
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, m_button_accessor(NULL)
#endif
{
	m_dropdown_packed.show_button = 1;
	m_dropdown_packed.enable_menu = 1;

	OP_STATUS status1 = g_main_message_handler->SetCallBack(this, MSG_CLOSE_AUTOCOMPL_POPUP, (MH_PARAM_1)this);
	OP_STATUS status2 = g_main_message_handler->SetCallBack(this, MSG_DROPDOWN_DELAYED_INVOKE, (MH_PARAM_1)this);

	SetTabStop(g_op_ui_info->IsFullKeyboardAccessActive());

	if (OpStatus::IsError(status1) || OpStatus::IsError(status2))
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}

#ifdef SKIN_SUPPORT
	SetSkinned(TRUE);
#ifdef QUICK
	SetUpdateNeededWhen(UPDATE_NEEDED_WHEN_VISIBLE);
#endif
	GetBorderSkin()->SetImage("Dropdown Skin");
#endif

	if (editable_text)
		init_status = SetEditableText();
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	ih.SetAccessibleListListener(this);
#endif
}

void OpDropDown::OnDeleted()
{
	ClosePopup(TRUE);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	OP_DELETE(m_button_accessor);
#endif
}

#ifdef WIDGETS_CLONE_SUPPORT

OP_STATUS OpDropDown::CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded)
{
	*obj = NULL;

	OpWidget *widget;
	OpListBox *listbox = NULL;
	OpDropDown *dropdown = NULL;
	if (expanded)
	{
		RETURN_IF_ERROR(OpListBox::Construct(&listbox));
		widget = listbox;
	}
	else
	{
		RETURN_IF_ERROR(OpDropDown::Construct(&dropdown));
		widget = dropdown;
	}
	parent->AddChild(widget);

	if (OpStatus::IsError(widget->CloneProperties(this, font_size)) ||
		(expanded && OpStatus::IsError(listbox->ih.CloneOf(ih, widget))) ||
		(!expanded && OpStatus::IsError(dropdown->ih.CloneOf(ih, widget))))
	{
		widget->Remove();
		OP_DELETE(widget);
		return OpStatus::ERR;
	}
	if (expanded)
	{
		listbox->grab_n_scroll = TRUE;
		listbox->UpdateScrollbar();
	}
	else
		dropdown->m_dropdown_packed.enable_menu = 0;
	*obj = widget;
	return OpStatus::OK;
}

#endif // WIDGETS_CLONE_SUPPORT

/***********************************************************************************
**
**	SetEditableText
**
***********************************************************************************/

OP_STATUS OpDropDown::SetEditableText(BOOL auto_update)
{
	if (edit == NULL)
	{
		m_dropdown_packed.auto_update = !!auto_update;

		SetTabStop(FALSE);

		edit = OP_NEW(DropDownEdit, (this));
		if (edit == NULL)
			return OpStatus::ERR_NO_MEMORY;

		edit->SetOnChangeOnEnter(TRUE);
		edit->SetHasCssBorder(TRUE);
		AddChild(edit, TRUE);

		edit->SetListener(GetListener());

		if (IsFocused())
			edit->SetFocus(FOCUS_REASON_OTHER);

#ifdef QUICK
		SetGrowValue(1);
		GetBorderSkin()->SetImage("Dropdown Edit Skin", "Dropdown Skin");
		Relayout();
#endif
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	OnUpdateActionState
**
***********************************************************************************/

#ifdef QUICK
void OpDropDown::OnUpdateActionState()
{
	if (!OpWidget::GetAction())
		return;

	OpInputAction* action = OpWidget::GetAction();

	INT32 i = 0;

	BOOL enabled = FALSE;

	while (action)
	{
		INT32 state = g_input_manager->GetActionState(action, GetParentInputContext());

		//Update the selection to match the action state, but not when the user is
		//selecting a new item in the dropdown window
		if (m_dropdown_window == NULL && state & OpInputAction::STATE_SELECTED)
		{
			SelectItem(i, TRUE);
		}

		if (!(state & OpInputAction::STATE_DISABLED) || state & OpInputAction::STATE_ENABLED || IsCustomizing())
			enabled = TRUE;

		i++;
		action = action->GetActionOperator() != OpInputAction::OPERATOR_PLUS ? action->GetNextInputAction() : NULL;
	}

	SetEnabled(enabled);
}

/***********************************************************************************
**
**	SetEditOnChangeDelay
**
***********************************************************************************/

void OpDropDown::SetEditOnChangeDelay(INT32 onchange_delay)
{
	if (edit)
	{
		edit->SetOnChangeDelay(onchange_delay);
	}
}

/***********************************************************************************
**
**	GetEditOnChangeDelay
**
***********************************************************************************/

INT32 OpDropDown::GetEditOnChangeDelay()
{
	return edit ? edit->GetOnChangeDelay() : 0;
}

#endif

/***********************************************************************************
**
**	SetListener
**
***********************************************************************************/

void OpDropDown::SetListener(OpWidgetListener* listener, BOOL force)
{
	// we must do this manually, because edit is internal child and therefore
	// won't automaticly get the listener set

	if (edit)
		edit->SetListener(listener, force);

	OpWidget::SetListener(listener, force);
}

/***********************************************************************************
**
**	SetAction
**
***********************************************************************************/

void OpDropDown::SetAction(OpInputAction* action)
{
	if (edit)
		edit->SetAction(action);
	else
	{
#ifdef QUICK
		OpWidget::SetAction(action);
		Clear();

		// add each action as items

		OpInputAction* action = GetAction();

		while (action)
		{
			AddItem(action->GetActionText());
			action = action->GetNextInputAction();
		}
#endif
	}
}

/***********************************************************************************
**
**	InvokeAction
**
***********************************************************************************/

#ifdef QUICK
void OpDropDown::InvokeAction(INT32 index)
{
	if (index == -1)
		return;

	OpInputAction* action = GetAction();

	while (action)
	{
		if (index == 0)
		{
			// copy it, since we only want to invoke 1 action

			OpInputAction action_copy;
			RETURN_VOID_IF_ERROR(action_copy.Clone(action));
			g_input_manager->InvokeAction(&action_copy, GetParentInputContext());
			return;
		}
		index--;
		action = action->GetNextInputAction();
	}
}
#endif

void OpDropDown::Changed(BOOL emulate_click)
{
	if (edit && m_dropdown_packed.auto_update)
	{
		OpStringItem* item = ih.GetItemAtNr(GetSelectedItem());
		if (item)
		{
			SetText(item->string.Get());
			edit->SelectText();
		}
	}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventTextChanged));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	if (listener)
	{
		// Note: the OnChange() event will fail to occur between the emulated
		// mouse down and mouse up events if m_dropdown_packed.delay_onchange = 1. As of
		// writing this does not cause any problems.

		if (emulate_click && GetFormObject())
		{
			// Emulate a mouse click to send onClick, since some webpages have
			// functionality on onClick. First send the mouse down event.
			listener->OnMouseEvent(this, GetSelectedItem(), 0, 0, MOUSE_BUTTON_1, TRUE, 1);
		}
		if (m_dropdown_packed.delay_onchange)
		{
			// Delay the OnChange to get off the stack first. Fixes wand
			// crasher 129620.
			g_main_message_handler->PostMessage(MSG_DROPDOWN_DELAYED_INVOKE, (MH_PARAM_1)this, 0);
		}
		else
			listener->OnChange(this); // Invoke listener!
		if (emulate_click && GetFormObject())
		{
			// Send the mouse up event.
			listener->OnMouseEvent(this, GetSelectedItem(), 0, 0, MOUSE_BUTTON_1, FALSE, 1);
		}
	}
	InvalidateAll();
}

/***********************************************************************************
**
**	SetGhostText
**
***********************************************************************************/

OP_STATUS OpDropDown::SetGhostText(const uni_char* ghost_text)
{
	if (edit)
		return edit->SetGhostText(ghost_text);

	InvalidateAll();
	return ghost_string.Set(ghost_text, this);
}

void OpDropDown::SetAlwaysInvoke(BOOL always_invoke)
{
	m_dropdown_packed.always_invoke = !!always_invoke;
}

void OpDropDown::OnMove()
{
	UpdateMenu(FALSE);
}

#ifdef SKIN_SUPPORT
void OpDropDown::OnImageChanged(OpWidgetImage* widget_image)
{
#ifdef QUICK
	Relayout();
#endif
	OpWidget::OnImageChanged(widget_image);
}
#endif

void OpDropDown::OnLayout()
{
#ifdef SKIN_SUPPORT
	if (!edit)
		return;

	OpRect inner_rect = GetBounds();

	AddPadding(inner_rect);

	if (GetForegroundSkin()->HasContent())
	{
		OpRect rect = GetForegroundSkin()->CalculateScaledRect(inner_rect, FALSE, TRUE);

		UINT32 image_width = rect.width + 4;
		inner_rect.x += image_width;
		inner_rect.width -= image_width;
	}

	if (!IsMiniSize())
		GetInfo()->AddBorder(this, &inner_rect);
	inner_rect.width -= GetRTL() ? GetInfo()->GetDropdownLeftButtonWidth(this) : GetInfo()->GetDropdownButtonWidth(this);

	if (GetRTL())
		inner_rect.x = rect.width - inner_rect.Right();
	edit->SetRect(inner_rect);
#endif
}

#ifdef QUICK
void OpDropDown::OnDragStart(const OpPoint& point)
{
	if (IsCustomizing())
	{
		OpWidget::OnDragStart(point);
		return;
	}

	if (GetForegroundSkin()->HasContent())
	{
		OpRect inner_rect = GetBounds();

		AddPadding(inner_rect);

		inner_rect = GetForegroundSkin()->CalculateScaledRect(inner_rect, FALSE, TRUE);
		inner_rect.width += 2;

		if (inner_rect.Contains(point))
			OpWidget::OnDragStart(point);
	}
}
#endif

#ifdef WIDGETS_IME_SUPPORT

void OpDropDown::OnCandidateShow(BOOL visible)
{
	if (m_dropdown_window)
		m_dropdown_window->SetVisible(!visible);
}
#endif

void OpDropDown::AddMargin(OpRect *rect)
{
	OpStringItem::AddItemMargin(GetInfo(), this, *rect);
}

OP_STATUS OpDropDown::AddItem(const uni_char* txt, INT32 index, INT32 *got_index, INTPTR user_data)
{
	OP_STATUS status = ih.AddItem(txt, index, got_index, this, user_data);

	if (got_index)
	{
		if (index == -1)
			*got_index = ih.CountItems() - 1;
		else
			*got_index = index;
	}

	UpdateMenu(TRUE);

	return status;
}

void OpDropDown::Clear()
{
	while(CountItems())
	{
		ih.RemoveItem(CountItems() - 1);
	}
	UpdateMenu(TRUE);
}

void OpDropDown::RemoveItem(INT32 index)
{
	ih.RemoveItem(index);
	UpdateMenu(TRUE);
}

void OpDropDown::RemoveAll()
{
	ih.RemoveAll();
	UpdateMenu(TRUE);
}

OP_STATUS OpDropDown::ChangeItem(const uni_char* txt, INT32 index)
{
	return ih.ChangeItem(txt, index, this);
}

OP_STATUS OpDropDown::BeginGroup(const uni_char* txt)
{
	return ih.BeginGroup(txt, this);
}

void OpDropDown::EndGroup()
{
	ih.EndGroup(this);
}

void OpDropDown::RemoveGroup(int group_nr)
{
	ih.RemoveGroup(group_nr, this);
}

void OpDropDown::RemoveAllGroups()
{
	ih.RemoveAllGroups();
}

void OpDropDown::SetTextDecoration(INT32 index, INT32 text_decoration, BOOL replace)
{
	OpStringItem* item = ih.GetItemAtIndex(index);
	if (item)
		item->SetTextDecoration(text_decoration, replace);
}

void OpDropDown::SelectItemAndInvoke(INT32 nr, BOOL selected, BOOL emulate_click)
{
	INT32 old = m_dropdown_window ? old_item_nr : GetSelectedItem();
	ih.SelectItem(nr, selected);
	if (m_dropdown_packed.always_invoke || GetSelectedItem() != old)
	{
#ifdef QUICK
		InvokeAction(GetSelectedItem());
#endif
		Changed(emulate_click);
	}
}

void OpDropDown::SelectItem(INT32 nr, BOOL selected)
{
	if (selected && GetSelectedItem() == nr)
		return;

	// If item -1 should be selected, we won't set focused_item to -1. We will focus item 0 and set it to unselected instead.
	if (nr == -1)
	{
		ih.SelectItem(ih.focused_item, FALSE);
		ih.focused_item = 0;
	}
	else
		ih.SelectItem(nr, selected);
	InvalidateAll();

#ifndef MOUSELESS
	if (m_dropdown_window)
	{
		if (selected)
			old_item_nr = GetSelectedItem();
		m_dropdown_window->SelectItem(nr, selected);
		m_dropdown_window->ResetMouseOverItem();
	}
#endif
	UpdateMenu(FALSE);
}

void OpDropDown::EnableItem(INT32 nr, BOOL enabled)
{
	ih.EnableItem(nr, enabled);
}

INT32 OpDropDown::GetSelectedItem()
{
	if (CountItems() == 0) // Has no items
		return -1;
	if (IsSelected(ih.focused_item) == FALSE) // Item -1 is selected
		return -1;
	return ih.focused_item;
}

BOOL OpDropDown::IsScrollable(BOOL vertical)
{
	if (m_dropdown_window)
		return TRUE;
	return FALSE;
}

void OpDropDown::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	// This method is in quick used for many types of dropdowns but we want the normal
	// dropdown size for all of them (unless they have a GetPreferedSize of their own).
	GetInfo()->GetPreferedSize(this, OpTypedObject::WIDGET_TYPE_DROPDOWN, w, h, cols, rows);
}

OP_STATUS OpDropDown::SetText(const uni_char* text, BOOL force_no_onchange, BOOL with_undo)
{
	OP_STATUS status = OpStatus::OK;
	if (edit)
	{
#ifdef WIDGETS_UNDO_REDO_SUPPORT
		if (with_undo)
			status = edit->SetTextWithUndo(text, force_no_onchange);
		else
#endif // WIDGETS_UNDO_REDO_SUPPORT
			status = edit->SetText(text, force_no_onchange);
		if( GetType() == WIDGET_TYPE_ADDRESS_DROPDOWN )
		{
			//we should show the first of the url in case of spoofing... ref. bug #143370
			edit->ResetScroll();
		}
		else
		{
			edit->SetCaretOffset( edit->GetTextLength() );
		}
	}

	if (!text)
		return status;

	// If the text matches a item, select it.
	for(int i = 0; i < CountItems(); i++)
	{
		OpStringItem* item = ih.GetItemAtNr(i);
		if (uni_stri_eq(text, item->string.Get()) && i != ih.focused_item)
		{
			SelectItem(i, TRUE);
			break;
		}
	}

	return status;
}

OP_STATUS OpDropDown::GetText(OpString& text)
{
	if (edit)
		return edit->GetText(text);
	return OpStatus::ERR;
}


const uni_char* OpDropDown::GetItemText(INT32 index)
{
	OpStringItem* item = ih.GetItemAtNr(index);

	if (item)
	{
		return item->string.Get();
	}
	return NULL;
}

#ifdef SKIN_SUPPORT

void OpDropDown::SetMiniSize(BOOL is_mini)
{
	OpWidget::SetMiniSize(is_mini);

	// Set the edit to mini if it exists
	if (edit)
		edit->SetMiniSize(is_mini);
}

#endif // SKIN_SUPPORT

/***********************************************************************************
**
**	ShowMenu
**
***********************************************************************************/

BOOL OpDropDown::ShowMenu()
{
	// Prevent opening of the plain dropdown widget when not focused. Other
	// types that inherit from OpDropDown are allowed to do what they please.
	if (GetType() == WIDGET_TYPE_DROPDOWN && !(IsFocused() || IsFocused(TRUE)))
		return FALSE;

#ifdef WIDGETS_IMS_SUPPORT
#ifdef NEARBY_ELEMENT_DETECTION
	if (!ElementExpander::IsEnabled())
#endif //NEARBY_ELEMENT_DETECTION
	{
		// Only use old code if ERR_NOT_SUPPORTED is returned, otherwise
		// platform might have failed to open IMS but we still don't want
		// core to try
		if (StartIMS() != OpStatus::ERR_NOT_SUPPORTED)
			return TRUE;
	}
#endif //WIDGETS_IMS_SUPPORT

	if (packed.locked)
		return FALSE;

#ifndef NO_SHOW_DROPDOWN_MENU
	if (m_dropdown_window)
	{
		ClosePopup();
		return FALSE;
	}

	if (listener)
		listener->OnDropdownMenu(this, TRUE);

	if (CountItems() == 0) // Has no items
		return FALSE;

	old_item_nr = GetSelectedItem();
	OP_STATUS status = OpDropDownWindow::Create(&m_dropdown_window, this);


	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
			g_memory_manager->RaiseCondition(status);//FIXME:OOM - Can't return OOM error.
		return FALSE;
	}

	m_dropdown_window->SetVisible(TRUE, TRUE);

#ifdef PLATFORM_FONTSWITCHING
	OpView *view = m_dropdown_window->GetWidgetContainer()->GetOpView();
	view->SetPreferredCodePage(m_script != WritingSystem::Unknown ?
							   OpFontInfo::CodePageFromScript(m_script) : preferred_codepage);
#endif // PLATFORM_FONTSWITCHING

	InvalidateAll();
#endif // !NO_SHOW_DROPDOWN_MENU
	return TRUE;
}

void OpDropDown::UpdateMenu(BOOL items_changed)
{
#ifdef WIDGETS_IMS_SUPPORT
#ifdef NEARBY_ELEMENT_DETECTION
	if (!ElementExpander::IsEnabled())
#endif //NEARBY_ELEMENT_DETECTION
		OpStatus::Ignore(UpdateIMS());
#endif // WIDGETS_IMS_SUPPORT

	if (m_dropdown_window)
	{
		m_dropdown_window->UpdateMenu(items_changed);
	}
}

void OpDropDown::ShowButton(BOOL show)
{
	if (!!m_dropdown_packed.show_button != show)
	{
		m_dropdown_packed.show_button = !!show;
		InvalidateAll();
	}
}

#ifndef MOUSELESS

void OpDropDown::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (m_dropdown_window && m_dropdown_window->IsListWidget(widget))
	{
		if (!down)
		{
			if (ih.GetItemAtNr(pos)->IsEnabled())
			{
				SelectItemAndInvoke(pos, TRUE, FALSE);
			}
			ClosePopup();
		}
	}
	if (listener)
		listener->OnMouseEvent(this, pos, x, y, button, down, nclicks); // Invoke listener!
}

OpPoint OpDropDown::MousePosInListbox()
{
	return m_dropdown_window->GetMousePos();
}

void OpDropDown::OnMouseLeave()
{
	if (m_dropdown_packed.is_hovering_button)
	{
		m_dropdown_packed.is_hovering_button = 0;
		InvalidateAll();
	}
}

void OpDropDown::OnMouseMove(const OpPoint &point)
{
	BOOL is_inside = GetBounds().Contains(point) && !IsDead();

	// set button hover.. this should all go away when button becomes a real button..

	const BOOL hover_button = IsInsideButton(point);

	if (hover_button != !!m_dropdown_packed.is_hovering_button)
	{
		m_dropdown_packed.is_hovering_button = !!hover_button;
		InvalidateAll();
	}

	g_widget_globals->m_blocking_popup_dropdown_widget = 0;

	// If we drag outside the dropdown, we will start hottrack the listbox (bypass all mouse input to it)

	// The HasClosed() is required on UNIX to fix bug #174490. The problem is that on a slow
	// machine or over a network connection the m_dropdown_window will not be closed and destroyed
	// before new move-events arrive. The result is that the dropdown window gets opened for a fraction
	// of a second and that will corrupt the displayed selected value [espen 2005-11-01]
	if (m_dropdown_window && !m_dropdown_window->HasClosed() && !is_inside)
	{
		OpPoint tpoint = MousePosInListbox();

		if (!m_dropdown_window->HasHotTracking())
		{
#if defined (DROPDOWN_MINIMUM_DRAG)
			OpRect listrect = m_dropdown_window->CalculateRect();
			OpPoint spoint;
			spoint.x = (tpoint.x * vis_dev->GetScale()) / 100 + listrect.x;
			spoint.y = (tpoint.y * vis_dev->GetScale()) / 100 + listrect.y;

			if ((spoint.y-m_drag_start.y) > DROPDOWN_MINIMUM_DRAG ||
				(m_drag_start.y-spoint.y) > DROPDOWN_MINIMUM_DRAG ||
				(spoint.x-m_drag_start.x) > DROPDOWN_MINIMUM_DRAG ||
				(m_drag_start.x-spoint.x) > DROPDOWN_MINIMUM_DRAG )
#endif // DROPDOWN_MINIMUM_DRAG
			{
				m_dropdown_window->SetHotTracking(TRUE);
			}
		}
		else
		{
			m_dropdown_window->OnMouseMove(tpoint);
			m_dropdown_window->ScrollIfNeeded();
		}
	}
}

void OpDropDown::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
#if defined (DROPDOWN_MINIMUM_DRAG)
	m_drag_start.x  =  (( point.x + GetRect(TRUE).x ) * vis_dev->GetScale()) / 100;
	m_drag_start.y  =  (( point.y + GetRect(TRUE).y ) * vis_dev->GetScale()) / 100;
	m_drag_start = vis_dev->GetView()->ConvertToScreen(m_drag_start);
#endif // DROPDOWN_MINIMUM_DRAG
	if (listener)
		listener->OnMouseEvent(this, -1, point.x, point.y, button, TRUE, nclicks);

	if (button == MOUSE_BUTTON_3)
		ClosePopup();

	if (button != MOUSE_BUTTON_1 || IsDead())
		return;

	if (!IsInWebForm())
	{
		SetFocus(FOCUS_REASON_MOUSE);
		if (edit)
		{
			// This is only required for Qt/X11. Because of focus handling
			// we set 'is_selecting' to TRUE in OpEdit::OnFocus() but in
			// this case that must be undone sine the function we are
			// inside now is only called when outside the edit field
			// rectangle [espen 2003-03-16]
			edit->is_selecting = 0;

			// don't moved any selection when hovering if mouse button isn't down
			edit->SetThisClickSelectedAll(FALSE);
		}
	}

	if (IsInsideButton(point))
	{
		InvalidateAll();

#if defined(_X11_SELECTION_POLICY_)
		if( g_widget_globals->m_blocking_popup_dropdown_widget == this )
		{
			g_widget_globals->m_blocking_popup_dropdown_widget = 0;
			return;
		}
#endif

		if (m_dropdown_packed.enable_menu)
			ShowMenu();
	}
}

void OpDropDown::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	g_widget_globals->m_blocking_popup_dropdown_widget = 0;

	if (listener)
	{
		listener->OnMouseEvent(this, -1, point.x, point.y, button, FALSE, nclicks);
	}

	if (button != MOUSE_BUTTON_1)
		return;

	if (m_dropdown_window && m_dropdown_window->HasHotTracking()) // Mouseinput should be sent to the listbox
	{
		OpPoint tpoint = MousePosInListbox();
		m_dropdown_window->OnMouseDown(tpoint, MOUSE_BUTTON_1, nclicks);
		m_dropdown_window->OnMouseUp(tpoint, MOUSE_BUTTON_1, nclicks);
		m_dropdown_window->SetHotTracking(FALSE);

		ClosePopup();
	}

	InvalidateAll();

	if (listener && GetBounds().Contains(point) && !IsDead())
		listener->OnClick(this); // Invoke!
}

BOOL OpDropDown::OnMouseWheel(INT32 delta,BOOL vertical)
{
	if (m_dropdown_window)
		return m_dropdown_window->OnMouseWheel(delta,vertical);
	return FALSE;
}

bool OpDropDown::IsInsideButton(const OpPoint& point)
{
	// If we're not editable, we're a button.
	bool inside_button = edit == NULL;

	// Otherwise, must check the part beside the edit field.
	if (!inside_button && m_dropdown_packed.show_button)
	{
		OpRect r = GetBounds();
		OpWidgetInfo* info = GetInfo();
		info->AddBorder(this, &r);
		if (GetRTL())
		{
			r.x += info->GetDropdownLeftButtonWidth(this);
			inside_button = point.x <= r.x;
		}
		else
		{
			r.width -= info->GetDropdownButtonWidth(this);
			inside_button = point.x >= r.Right();
		}
	}

	return inside_button;
}

#endif // !MOUSLESS

BOOL OpDropDown::ClosePopup(BOOL immediately)
{
#ifdef WIDGETS_IMS_SUPPORT
#ifdef NEARBY_ELEMENT_DETECTION
	if (!ElementExpander::IsEnabled())
#endif //NEARBY_ELEMENT_DETECTION
		DestroyIMS();
#endif // WIDGETS_IMS_SUPPORT

	if (!m_dropdown_window && m_locked_dropdown_window)
	{
		m_dropdown_window = m_locked_dropdown_window;
		m_locked_dropdown_window = NULL;
	}

	if (!m_dropdown_window)
		return FALSE;

	m_dropdown_window->SetClosed(TRUE);

	if (immediately)
	{
		m_dropdown_window->ClosePopup();
		g_widget_globals->m_blocking_popup_dropdown_widget = 0;

#ifdef WIDGETS_IME_ITEM_SEARCH
#ifdef WIDGETS_IME_SUPPORT
		// end text input when the dropdown is closed
		if (edit && vis_dev && vis_dev->view)
		{
			vis_dev->view->GetOpView()->AbortInputMethodComposing();
		}
#endif // WIDGETS_IME_SUPPORT
#endif // WIDGETS_IME_ITEM_SEARCH
	}
	else
	{
		if (IsFocused())
			GenerateHighlightRectChanged(GetBounds());

		m_dropdown_window->SetVisible(FALSE); // Must hide. It's important that we don't get any more OnMouseMove etc.
		g_main_message_handler->PostMessage(MSG_CLOSE_AUTOCOMPL_POPUP, (MH_PARAM_1)this, 0);
	}
	return TRUE;
}

void OpDropDown::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_CLOSE_AUTOCOMPL_POPUP)
		ClosePopup(TRUE);
	else if (msg == MSG_DROPDOWN_DELAYED_INVOKE && listener)
		listener->OnChange(this);
	else
		OpWidget::HandleCallback(msg, par1, par2);
}

OpStringItem* OpDropDown::GetItemToPaint()
{
	if (CountItems())
	{
		INT32 index = GetSelectedItem();
		if (index == -1 && m_dropdown_window)
			index = old_item_nr;
		if (index != -1)
			return ih.GetItemAtNr(index);
	}
	return NULL;
}

void OpDropDown::SetBackOldItem()
{
	if (m_dropdown_window && ih.focused_item != m_dropdown_window->GetFocusedItem())
	{
		m_dropdown_window->SelectItem(m_dropdown_window->GetFocusedItem(), FALSE);
		m_dropdown_window->SelectItem(old_item_nr, TRUE);
		OP_ASSERT(GetSelectedItem() == old_item_nr);
		InvalidateAll();
	}
}

void OpDropDown::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
#ifdef WIDGETS_IME_ITEM_SEARCH
	// hide the edit box while the drop down is drawn
	OpEdit* tmpEdit = edit;
	edit = 0;
#endif // WIDGETS_IME_ITEM_SEARCH

	if (m_dropdown_window && !m_dropdown_window->HasClosed())
		UpdateMenu(FALSE);

	widget_painter->DrawDropdown(GetBounds());

#ifdef WIDGETS_IME_ITEM_SEARCH
	edit = tmpEdit;
#endif // WIDGETS_IME_ITEM_SEARCH
}

void OpDropDown::OnFocus(BOOL focus,FOCUS_REASON reason)
{
#ifdef _MACINTOSH_
	// this is to draw the focusborder (that we want around the entire dropdown widget)
	Invalidate(GetBounds().InsetBy(-3,-3), FALSE, TRUE);
#endif

	if (edit && focus)
		edit->SetFocus(reason);
	else
		InvalidateAll();

	if (!focus)
		ClosePopup();
#ifdef WIDGETS_DROPDOWN_ON_FOCUS
	else if (reason != FOCUS_REASON_MOUSE)
		ShowMenu();
#endif
}


void OpDropDown::BlockPopupByMouse()
{
	g_widget_globals->m_blocking_popup_dropdown_widget = this;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

void OpDropDown::CreateButtonAccessorIfNeeded()
{
	if (!m_button_accessor)
		m_button_accessor = OP_NEW(OpDropDownButtonAccessor, (this));
}

OP_STATUS OpDropDown::AccessibilityGetText(OpString& str)
{
	OpStringItem* item = GetItemToPaint();
	if (item)
		str.Set(item->string.Get());
	return OpStatus::OK;
}

int OpDropDown::GetAccessibleChildrenCount()
{
	return 2+ OpWidget::GetAccessibleChildrenCount();
}

OpAccessibleItem* OpDropDown::GetAccessibleChild(int index)
{
	OpAccessibleItem* child= OpWidget::GetAccessibleChild(index);
	if (!child)
		index -= OpWidget::GetAccessibleChildrenCount();
	else
		return child;

	switch (index)
	{
		case 0:
			CreateButtonAccessorIfNeeded();
			return m_button_accessor;
		case 1:
			return &ih;
		default:
			return NULL;
	}
}

int OpDropDown::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int index = OpWidget::GetAccessibleChildIndex(child);
	if (index != Accessibility::NoSuchChild)
		return index;

	index = OpWidget::GetAccessibleChildrenCount();
	CreateButtonAccessorIfNeeded();
	if (child == m_button_accessor)
		return index;
	if (child == &ih)
		return index +1;

	return Accessibility::NoSuchChild;
}

OpAccessibleItem* OpDropDown::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpRect button_rect;
	CreateButtonAccessorIfNeeded();
	m_button_accessor->AccessibilityGetAbsolutePosition(button_rect);

	if (button_rect.Contains(OpPoint(x,y)))
		return m_button_accessor;

	return OpWidget::GetAccessibleChildOrSelfAt(x, y);
}

OpAccessibleItem* OpDropDown::GetAccessibleFocusedChildOrSelf()
{
	if (m_dropdown_window)
		if (ih.GetAccessibleFocusedChildOrSelf())
			return &ih;

	return OpWidget::GetAccessibleFocusedChildOrSelf();
}

OP_STATUS OpDropDown::AccessibilityClicked()
{
	SetFocus(FOCUS_REASON_MOUSE);
	ShowMenu();
	return OpStatus::OK;
}

BOOL OpDropDown::IsListFocused() const
{
	return this == g_input_manager->GetKeyboardInputContext();
}

BOOL OpDropDown::IsListVisible() const
{
	return m_dropdown_window != NULL;
}

OP_STATUS OpDropDown::GetAbsolutePositionOfList(OpRect &rect)
{
	if (!m_dropdown_window)
	{
		rect.Empty();
		return OpStatus::OK;
	}
	else
		return m_dropdown_window->GetAbsolutePositionOfList(rect);
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef WIDGETS_IME_ITEM_SEARCH
BOOL OpDropDown::SelectCurrentEntryAndDismiss(OpInputAction* action)
{
	BOOL close = m_dropdown_window != NULL;
	BOOL changed = FALSE;
	INT32 index = 0;
	if (close)
	{
		index = m_dropdown_window->GetFocusedItem();
		changed = (index != old_item_nr);
		ClosePopup();
	}

	if (changed || (m_dropdown_packed.always_invoke && GetValue() == -1))
		SelectItemAndInvoke(index, TRUE);

	if (edit && close)
	{
		return edit->OnInputAction(action->GetChildAction());
	}
	else if (close)
	{
		OpInputAction unfocus_action(OpInputAction::ACTION_UNFOCUS_FORM);
		OnInputAction(&unfocus_action);
	}

	return close;
}
#endif // WIDGETS_IME_ITEM_SEARCH

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/
BOOL OpDropDown::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GO:
		case OpInputAction::ACTION_SEARCH:
		case OpInputAction::ACTION_CLEAR:
		{
			if (edit)
			{
				return edit->OnInputAction(action);
			}
			return FALSE;
		}

		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			switch (action->GetChildAction()->GetAction())
			{
				case OpInputAction::ACTION_LOWLEVEL_KEY_UP:
				{
#ifdef WIDGETS_IME_ITEM_SEARCH
					if ((action->GetChildAction()->GetActionData() == OP_KEY_LEFT) ||
						(action->GetChildAction()->GetActionData() == OP_KEY_RIGHT))
						return SelectCurrentEntryAndDismiss(action);
#endif // WIDGETS_IME_ITEM_SEARCH

					if (m_dropdown_window)
						return m_dropdown_window->SendInputAction(action->GetChildAction());
					break;
				}
			}

			return FALSE;
		}

		case OpInputAction::ACTION_LOWLEVEL_KEY_DOWN:
		{
			if (m_dropdown_window)
			{
				return m_dropdown_window->SendInputAction(action);
			}
			return FALSE;
		}

		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
#ifdef WIDGETS_IME_ITEM_SEARCH
			g_widget_globals->itemsearch->Clear();
#endif // WIDGETS_IME_ITEM_SEARCH
			if (m_dropdown_window)
			{
				return m_dropdown_window->SendInputAction(action);
			}

			OpKey::Code key = static_cast<OpKey::Code>(action->GetActionData());
			const uni_char *key_value = action->GetKeyValue();

			ShiftKeyState modifiers = action->GetShiftKeys();
			if ((modifiers == SHIFTKEY_NONE || modifiers == SHIFTKEY_SHIFT) && key >= OP_KEY_SPACE && key_value != NULL)
			{
				// open dropdown when user press enter
				if (key == OP_KEY_SPACE && !g_widget_globals->itemsearch->IsSearching())
				{
					if (
#ifdef WIDGETS_DROPDOWN_NO_TOGGLE
						!m_dropdown_window &&
#endif // WIDGETS_DROPDOWN_NO_TOGGLE
						ShowMenu())
						if (m_dropdown_window)
							m_dropdown_window->HighlightFocusedItem();
				}
				else
				{
					g_widget_globals->itemsearch->PressKey(key_value);
					INT32 found_item = ih.FindItem(g_widget_globals->itemsearch->GetSearchString());
					if (found_item != -1)
					{
						SelectItemAndInvoke(found_item, TRUE, TRUE);
						InvalidateAll();
					}
				}
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_SELECT_ITEM:
		{
			BOOL changed = FALSE;
			INT32 index = 0;
			BOOL close = m_dropdown_window != NULL;

			if (close)
			{
				changed = m_dropdown_window->GetFocusedItem() != old_item_nr;
				index = m_dropdown_window->GetFocusedItem() ;
				ClosePopup();
			}

			if (changed || (m_dropdown_packed.always_invoke && GetValue() == -1))
				SelectItemAndInvoke(index, TRUE, TRUE);

			if (close && edit && action->GetChildAction())
				return edit->OnInputAction(action->GetChildAction());

			return close;
		}

		case OpInputAction::ACTION_CLOSE_DROPDOWN:
		{
			return ClosePopup();
		}

		case OpInputAction::ACTION_SHOW_DROPDOWN:
		{
			if (
#ifdef WIDGETS_DROPDOWN_NO_TOGGLE
				!m_dropdown_window &&
#endif // WIDGETS_DROPDOWN_NO_TOGGLE
				ShowMenu())
			{
				if (m_dropdown_window)
					m_dropdown_window->HighlightFocusedItem();
				return TRUE;
			}
			break;
		}

		case OpInputAction::ACTION_PREVIOUS_ITEM:
		case OpInputAction::ACTION_NEXT_ITEM:
		case OpInputAction::ACTION_GO_TO_START:
		case OpInputAction::ACTION_GO_TO_END:
		{
			if (m_dropdown_window)
			{
				return m_dropdown_window->SendInputAction(action);
			}

			INT32 num_items = CountItems();
			if (num_items == 0)
				return TRUE;

			INT32 index;
			if (GetValue() == -1)
				index = 0;
			else
			{
				INT32 findfrom = ih.focused_item;
				BOOL forward = FALSE;
				switch (action->GetAction())
				{
					case OpInputAction::ACTION_GO_TO_START:
						findfrom = -1;
						// fall through...
					case OpInputAction::ACTION_NEXT_ITEM:
						forward = TRUE;
						break;
					case OpInputAction::ACTION_GO_TO_END:
						findfrom = num_items;
				}
				index = ih.FindEnabled(findfrom, forward);
			}

			if (index != -1)
			{
				SelectItemAndInvoke(index, TRUE, TRUE);
				InvalidateAll();
			}
			else if (m_dropdown_packed.always_invoke)
				Changed(TRUE);
#ifdef ESCAPE_FOCUS_AT_TOP_BOTTOM
			else
				return FALSE;
#endif

			return TRUE;
		}
#ifdef _SPAT_NAV_SUPPORT_
		case OpInputAction::ACTION_UNFOCUS_FORM:
			return g_input_manager->InvokeAction(action, GetParentInputContext());
#endif // _SPAT_NAV_SUPPORT_
	}

	if (m_dropdown_window)
	{
		return m_dropdown_window->SendInputAction(action);
	}

	return FALSE;

}

void OpDropDown::GetRelevantOptionRange(const OpRect& area, UINT32& start, UINT32& stop)
{
	// find out if area intersects us at all
	OpRect r = GetDocumentRect();
	if (!r.Intersecting(area))
	{
		start = stop = 0;
		return;
	}

	// if the dropdown is open we need to update _all_
	// options, since scrolling won't cause layout to repaint.
	if (m_dropdown_window)
	{
		start = 0;
		stop = ih.CountItems();
	}
	// only update the currently selected item
	else
	{
		INT32 s = GetSelectedItem();
		if (s < 0)
			start = stop = 0;
		else
		{
			start = (UINT32)s;
			stop = start + 1;
		}
	}
}

void OpDropDown::SetLocked(BOOL locked)
{
	OpWidget::SetLocked(locked);
	if (m_dropdown_window)
	{
		if (locked && (!m_posted_close_msg && !g_locked_dd_closer.Contains(this)))
		{
			m_dropdown_window->SetVisible(FALSE);
			if (OpStatus::IsError(g_locked_dd_closer.OnDropdownLocked(this)))
			{
				g_main_message_handler->PostMessage(MSG_CLOSE_AUTOCOMPL_POPUP, (MH_PARAM_1)this, 0, CLOSE_POPUP_AFTER_RELAYOUT_TIMEOUT);
				m_posted_close_msg = TRUE;
			}
		}
		else if (!locked && (m_posted_close_msg || g_locked_dd_closer.Contains(this)))
		{
			if (m_posted_close_msg)
			{
				g_main_message_handler->RemoveDelayedMessage(MSG_CLOSE_AUTOCOMPL_POPUP, (MH_PARAM_1)this, 0);
				m_posted_close_msg = FALSE;
			}
			if (!m_dropdown_window->HasClosed())
				m_dropdown_window->SetVisible(TRUE);
			g_locked_dd_closer.OnDropdownUnLocked(this);
		}
	}
}

void OpDropDown::Update()
{
	if (!IsLocked())
	{
		UpdateMenu(TRUE);
		if (m_dropdown_window)
		{
			old_item_nr = GetSelectedItem();
			m_dropdown_window->SelectItem(GetSelectedItem(), TRUE);
#ifndef MOUSELESS
			m_dropdown_window->ResetMouseOverItem();
#endif // MOUSELESS
		}
	}
}

#ifdef WIDGETS_IMS_SUPPORT
void OpDropDown::DoCommitIMS(OpIMSUpdateList *updates)
{
	Invalidate(GetBounds());

	int idx = updates->GetFirstIndex();
	if (idx == -1)
		return;

	OpStringItem * item = ih.GetItemAtIndex(idx);

	if (item->IsEnabled())
	{
		int value = updates->GetValueAt(idx);
		int nr = ih.FindItemNr(idx);
		SelectItemAndInvoke(nr, value != 0, TRUE);
	}
}

/* virtual */
void OpDropDown::DoCancelIMS()
{
	// Do nothing right now
}

OP_STATUS OpDropDown::SetIMSInfo(OpIMSObject& ims_object)
{
	OpWidget::SetIMSInfo(ims_object);
	ims_object.SetIMSAttribute(OpDocumentListener::IMSCallback::DROPDOWN, 1);
	ims_object.SetItemHandler(&ih);
	return OpStatus::OK;
}
#endif //WIDGETS_IMS_SUPPORT
