/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/quick_toolkit/widgets/OpTreeViewDropdown.h"
#include "adjunct/desktop_util/actions/action_utils.h"
#include "modules/display/vis_dev.h"
#include "modules/widgets/OpEdit.h"

// --------------------------------------------------------------------------------
//
// class OpTreeViewDropdown
//
// --------------------------------------------------------------------------------

/***********************************************************************************
 **
 **	OpTreeViewDropdown
 **
 ***********************************************************************************/

OP_STATUS OpTreeViewDropdown::Construct(OpTreeViewDropdown ** obj, BOOL edit_field/* = TRUE*/)
{
	*obj = OP_NEW(OpTreeViewDropdown, (edit_field));
	if (*obj == NULL)
		return OpStatus::ERR_NO_MEMORY; 
	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpTreeViewDropdown::OpTreeViewDropdown(BOOL edit_field) :
	OpDropDown(edit_field),
	m_tree_view_window(NULL),
	m_invoke_action_on_click(TRUE),
	m_max_number_of_columns(3),
	m_extra_line_height(0),
	m_show_dropdown_on_activation(FALSE),
	m_deselect_on_scroll_end(TRUE),
	m_allow_wrapping_on_scrolling(FALSE),
	m_enable_dropdown_window(TRUE),
	m_edit_listener(NULL),
	m_widget_type(OpDropDown::GetType()),
	m_candidate_window_shown(false)
{
	// Read the minimum pref
	m_max_lines = g_pcui->GetIntegerPref(PrefsCollectionUI::TreeViewDropDownMaxLines);

	if(edit)
	{
		// ---------------------
		// Make sure we get an OnChange for every char typed
		// ---------------------
		edit->SetOnChangeOnEnter(FALSE);

		// ---------------------
		// Notify the edit field that we will supply our own autocompletion
		// ---------------------
		edit->SetUseCustomAutocompletion();
	}
}


/***********************************************************************************
 **
 ** OnDeleted
 **
 ***********************************************************************************/
void OpTreeViewDropdown::OnDeleted()
{
	if(m_tree_view_window)
	{
		m_tree_view_window->ClosePopup();
	}

	OpDropDown::OnDeleted();
}

/***********************************************************************************
**
** GetColumnWeight - Get the weight of the column
**
***********************************************************************************/
UINT32 OpTreeViewDropdown::GetColumnWeight(UINT32 column)
{
	UINT32 weight = 100;

	switch(GetMaxNumberOfColumns())
	{
		case 3:
			switch(column)
			{
				case 0:
					weight = 45;
					break;
				case 1:
					weight = 40;
					break;
				case 2:
					weight = 15;
					break;
			}
			break;

		case 2:
			switch(column)
			{
			case 0:
				weight = 60;
				break;
			case 1:
				weight = 40;
				break;
			}
			break;
		case 1:
		default:
			weight = 100;
			break;
	}
	return weight;
}

/***********************************************************************************
 **
 ** GetTextForSelectedItem
 **
 ***********************************************************************************/

OP_STATUS OpTreeViewDropdown::GetTextForSelectedItem(OpString& item_text)
{
	OpTreeModelItem* item = GetSelectedItem();

	if (!item)
		return OpStatus::ERR;

	OpTreeModelItem::ItemData item_data(OpTreeModelItem::COLUMN_QUERY);
	item_data.column_query_data.column_text = &item_text;
	item_data.flags |= OpTreeModelItem::FLAG_NO_PAINT;
	return item->GetItemData(&item_data);
}

/***********************************************************************************
 **
 ** OnDesktopWindowClosing
 **
 ** The dropdown window is being closed. Don't hold on to the pointer since the
 ** window will be deleted.
 **
 ***********************************************************************************/
void OpTreeViewDropdown::OnDesktopWindowClosing(DesktopWindow* desktop_window,
												BOOL user_initiated)
{
	BlockPopupByMouse(); // Bug #317855

	m_tree_view_window = NULL;
}


/***********************************************************************************
 **
 ** InitTreeViewDropdown
 **
 ***********************************************************************************/
OP_STATUS OpTreeViewDropdown::InitTreeViewDropdown(const char *transp_window_skin)
{
	// ---------------------
	// Reuse the existing window:
	// ---------------------
	if(m_tree_view_window)
	{
		return m_tree_view_window->Clear(TRUE);
	}

	RETURN_IF_ERROR(OpTreeViewWindow::Create(&m_tree_view_window, this, transp_window_skin));

	// ---------------------
	// Make sure we get notification when it is closed (OnDesktopWindowClosing):
	// ---------------------
	OP_STATUS status = m_tree_view_window->AddDesktopWindowListener(this);
	if (OpStatus::IsError(status))
	{
		m_tree_view_window->ClosePopup();
		m_tree_view_window = NULL;
	}
	else
	{
		if(m_extra_line_height)
		{
			m_tree_view_window->SetExtraLineHeight(m_extra_line_height);
		}
		if(m_allow_wrapping_on_scrolling)
		{
			m_tree_view_window->SetAllowWrappingOnScrolling(TRUE);
		}
	}
	return status;
}


/***********************************************************************************
 **
 ** SetModel
 **
 ***********************************************************************************/
OP_STATUS OpTreeViewDropdown::SetModel(GenericTreeModel * tree_model, BOOL items_owned_by_dropdown)

{
	if(!m_tree_view_window)
		return OpStatus::ERR;

	m_tree_view_window->SetModel(tree_model, items_owned_by_dropdown);

	return OpStatus::OK;
}

void OpTreeViewDropdown::ResizeDropdown()
{
	INT32 w, h;

	OnResize(&w, &h);
}

/***********************************************************************************
 **
 ** ShowDropdown
 **
 ** See comment on WIDGETS_UP_DOWN_MOVES_TO_START_END in the header file.
 **
 ***********************************************************************************/
void OpTreeViewDropdown::ShowDropdown(BOOL show)
{
	if (!m_enable_dropdown_window || m_candidate_window_shown && show)
		return;

	if(!m_tree_view_window)
		return;

	if (show)
	{
		// ---------------------
		// Find the rectangle to display in:
		// ---------------------
		OpRect rect = m_tree_view_window->CalculateRect();

		// allow subclasses to adjust the size
		AdjustSize(rect);

		if(m_tree_view_window->IsPopupVisible())
		{
			// ---------------------
			// Resize the window to that size:
			// ---------------------
			m_tree_view_window->SetPopupOuterSize(rect.width, rect.height);
			m_tree_view_window->SetPopupOuterPos(rect.x, rect.y);
		}
		else
		{
			// ---------------------
			// Display the window using that rectangle:
			// ---------------------
			m_tree_view_window->SetVisible(TRUE, TRUE);
		}

#ifdef WIDGETS_UP_DOWN_MOVES_TO_START_END
		if (edit)
			edit->SetIgnoreGotoLineStart(TRUE);
#endif // WIDGETS_UP_DOWN_MOVES_TO_START_END
	}
	else
	{
		m_tree_view_window->Clear();
		m_tree_view_window->SetVisible(FALSE);

#ifdef WIDGETS_UP_DOWN_MOVES_TO_START_END
		if (edit)
			edit->SetIgnoreGotoLineStart(FALSE);
#endif // WIDGETS_UP_DOWN_MOVES_TO_START_END
	}
}

void OpTreeViewDropdown::ShowDropdown(BOOL show, OpRect rect)
{
	if (show)
	{
		m_tree_view_window->SetVisible(TRUE, rect);
	}
}

/***********************************************************************************
 **
 ** DropdownIsOpen
 **
 ***********************************************************************************/
BOOL OpTreeViewDropdown::DropdownIsOpen()
{
	return m_tree_view_window != NULL && m_tree_view_window->IsPopupVisible();
}


/***********************************************************************************
 **
 ** OnInputAction
 **
 ***********************************************************************************/
BOOL OpTreeViewDropdown::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_PREVIOUS_ITEM:
		{
			// ---------------------
			// If the first line is selected and the user goes up,
			// the user leaves the treeview and enters the edit field.
			// Restore the string the user was typing.
			// ---------------------
			if(m_deselect_on_scroll_end && m_tree_view_window->IsFirstLineSelected())
			{
				m_tree_view_window->UnSelectAndScroll();
				SetText(m_user_string.CStr());
				return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_NEXT_ITEM:
		{
			// ---------------------
			// If the last line is selected and the user goes down,
			// the user leaves the treeview and enters the edit field.
			// Restore the string the user was typing.
			// ---------------------
			if(m_deselect_on_scroll_end && m_tree_view_window->IsLastLineSelected())
			{
				m_tree_view_window->UnSelectAndScroll();
				SetText(m_user_string.CStr());
				return TRUE;
			}
			break;
		}

#if defined(_UNIX_DESKTOP_)		
		// Limited to UNIX due to bug #328337 [espen]
		case OpInputAction::ACTION_CLOSE_WINDOW:
			if( DropdownIsOpen() )
			{
				ShowDropdown(FALSE);
				return TRUE;
			}
			break;
#endif


	}

	// ---------------------
	// Give the treeview a shot at the action:
	// ---------------------
	if(DropdownIsOpen())
	{
		if(m_tree_view_window->SendInputAction(action))
			return TRUE;
	}

	return OpDropDown::OnInputAction(action);
}

/***********************************************************************************
 **
 ** OnKeyboardInputLost
 **
 ***********************************************************************************/
void OpTreeViewDropdown::OnKeyboardInputLost(OpInputContext* new_input_context,
											 OpInputContext* old_input_context,
											 FOCUS_REASON reason)
{
	// Make sure that we are really losing the keyboard input - it might
	// be one of our children (edit box), which means we still have input
	if (new_input_context != this && !IsParentInputContextOf(new_input_context))
	{
		// We lost keyboard - the dropdown window should not be displayed any more.
		ShowDropdown(FALSE);
	}

	OpDropDown::OnKeyboardInputLost(new_input_context, old_input_context, reason);
}

void OpTreeViewDropdown::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if(m_show_dropdown_on_activation)
	{
		OnDropdownMenu(this, TRUE);
	}
	OpDropDown::OnKeyboardInputGained(new_input_context, old_input_context, reason);
}

/***********************************************************************************
 **
 ** OnCandidateShow
 **
 ***********************************************************************************/
void OpTreeViewDropdown::OnCandidateShow(BOOL visible)
{
	m_candidate_window_shown = !!visible;
	//(julienp)This method is called when the candidate list of the IME appears
	//It is needed to hide the dropdown when the candidate box is visible since
	//otherwise, the candidate box is hidden under the dropdown.
	if (visible)
	{
		ShowDropdown(FALSE);
	}
	else
	{
		// If subclass doesn't populate contents in ShowDropdown we may
		// end up with a empty window if show it. We have no idea if 
		// sub class will do this so just assume no, override this function
		// if want to have different behavior.
		GenericTreeModel* model = GetModel();
		if (model && model->GetCount())
			ShowDropdown(TRUE);
	}
}

void OpTreeViewDropdown::OnStopComposing(BOOL /*cancel*/)
{
	m_candidate_window_shown = false;
	OnChange(edit,FALSE);
}

/***********************************************************************************
 **
 ** SetAction
 **
 ***********************************************************************************/
void OpTreeViewDropdown::SetAction(OpInputAction* action)
{
	// ---------------------
	// Sets the action that should be performed on enter
	// ---------------------
	if (edit)
		edit->SetAction(action);
}

/***********************************************************************************
 **
 ** GetAction
 **
 ***********************************************************************************/
OpInputAction * OpTreeViewDropdown::GetAction()
{
	// ---------------------
	// Get the action that should be performed on enter
	// ---------------------
	return edit ? edit->GetAction() : NULL;
}

/***********************************************************************************
 **
 ** OnResize
 **
 ***********************************************************************************/
void OpTreeViewDropdown::OnResize(INT32* new_w,
								  INT32* new_h)
{
	OpDropDown::OnResize(new_w, new_h);

	// ---------------------
	// If we are visible, calling ShowDropdown(TRUE) will resize the window correctly
	// ---------------------
	if (m_tree_view_window && m_tree_view_window->IsPopupVisible() )
		ShowDropdown(TRUE);
}


/***********************************************************************************
 **
 ** SetText
 **
 ** Will never result in OnChange being called. Should be used for setting text in
 ** the edit field that should not result in autocompletion or similar action to be
 ** performed.
 **
 ***********************************************************************************/
OP_STATUS OpTreeViewDropdown::SetText(const uni_char* text)
{
	OP_STATUS status = OpDropDown::SetText(text, TRUE);

	if(OpStatus::IsSuccess(status) && edit)
	{
		edit->MoveCaretToStartOrEnd(FALSE, FALSE);
		if (m_edit_listener)
			m_edit_listener->OnTreeViewDropdownEditChange(this, text);
	}

	return status;
}


/***********************************************************************************
 **
 ** OnMouseEvent
 **
 ***********************************************************************************/
void OpTreeViewDropdown::OnMouseEvent(OpWidget *widget,
									  INT32 pos,
									  INT32 x,
									  INT32 y,
									  MouseButton button,
									  BOOL down,
									  UINT8 nclicks)
{
	if(widget == this)
	{
		// ---------------------
		// Only allow left clicks on items:
		// ---------------------

		if (button == MOUSE_BUTTON_1 && !down && pos != -1)
		{

			if (!OnBeforeInvokeAction(m_invoke_action_on_click))
				return;

			if (edit)
				edit->UnsetForegroundColor();

			// Get text to display in edit field
			OpString item_text;
			if (OpStatus::IsSuccess(GetTextForSelectedItem(item_text)) &&
				item_text.HasContent())
			{
				SetText(item_text.CStr());
			}

			// ---------------------
			// Invoke the action if that should be done:
			// ---------------------
			if(m_invoke_action_on_click)
			{
				// ---------------------
				// If control + shift is pressed then the page will be opened
				// in the background. Restore the text to the user typed text.
				// ---------------------
				BOOL control = vis_dev->GetView()->GetShiftKeys() & SHIFTKEY_CTRL;
				BOOL shift   = vis_dev->GetView()->GetShiftKeys() & SHIFTKEY_SHIFT;

				if(control && shift)
					SetText(m_user_string.CStr());

				// ---------------------
				// Invoke the action on the item's text:
				// ---------------------
				OnInvokeAction(this, TRUE);
				GetAction()->SetActionDataString(item_text.CStr());
				g_input_manager->InvokeAction(GetAction(), GetParentInputContext());
			}

			ShowDropdown(FALSE);
		}

		((OpWidgetListener*)GetParent())->OnMouseEvent(this,
													   pos,
													   x,
													   y,
													   button,
													   down,
													   nclicks);
	}
	else
	{
		OpDropDown::OnMouseEvent(widget, pos, x, y, button, down, nclicks);
	}
}

/***********************************************************************************
**
**  OnDragMove
**
***********************************************************************************/

void OpTreeViewDropdown::OnDragMove(OpWidget* widget,
									OpDragObject* drag_object,
									INT32 pos,
									INT32 x,
									INT32 y)
{
	if (widget != this)
	{
		// Translate (x,y) so that it's relative to this widget.
		const OpRect child_rect = widget->GetRect();
		x += child_rect.x;
		y += child_rect.y;
	}
	static_cast<OpWidgetListener *>(GetParent())->OnDragMove(this, drag_object, pos, x, y);
}

/***********************************************************************************
**
**  OnDragDrop
**
***********************************************************************************/

void OpTreeViewDropdown::OnDragDrop(OpWidget* widget,
									OpDragObject* drag_object,
									INT32 pos,
									INT32 x,
									INT32 y)
{
	if (widget != this)
	{
		// Translate (x,y) so that it's relative to this widget.
		const OpRect child_rect = widget->GetRect();
		x += child_rect.x;
		y += child_rect.y;
	}
	static_cast<OpWidgetListener *>(GetParent())->OnDragDrop(this, drag_object, pos, x, y);
}

/***********************************************************************************
**
**  OnDragLeave
**
***********************************************************************************/

void OpTreeViewDropdown::OnDragLeave(OpWidget* widget,
									 OpDragObject* drag_object)
{
	static_cast<OpWidgetListener *>(GetParent())->OnDragLeave(this, drag_object);
}

/***********************************************************************************
**
**  OnContextMenu
**
***********************************************************************************/

BOOL OpTreeViewDropdown::OnContextMenu(OpWidget* widget,
									   INT32 child_index, 
									   const OpPoint& menu_point,
									   const OpRect* avoid_rect,
									   BOOL keyboard_invoked)
{
	// ---------------------
	// If the dropdown is open - do not allow context menus
	// ---------------------
	if(DropdownIsOpen())
		return FALSE;

	BOOL handled = static_cast<OpWidgetListener*>(GetParent())->OnContextMenu(this, child_index, menu_point, avoid_rect, keyboard_invoked);
	if (!handled)
	{
		HandleWidgetContextMenu(widget, menu_point);
	}
	return handled;
}

/***********************************************************************************
**
**	OnInvokeAction
**
***********************************************************************************/

BOOL OpTreeViewDropdown::HasFocus()
{
	return edit ? edit->IsFocused() : FALSE;
}

/***********************************************************************************
**
**	OnInvokeAction
**
***********************************************************************************/

void OpTreeViewDropdown::OnInvokeAction(OpWidget *widget, BOOL invoked_by_mouse)
{
	OpString text;
	GetText(text);

	BOOL invoked_on_user_typed_string =
		!invoked_by_mouse &&
		m_user_string.HasContent() &&
		(m_user_string.Compare(text) == 0);

	// Call hook
	OnInvokeAction(invoked_on_user_typed_string);
}

/***********************************************************************************
 **
 ** GetModel
 **
 ***********************************************************************************/
GenericTreeModel * OpTreeViewDropdown::GetModel()
{
	return m_tree_view_window ? m_tree_view_window->GetModel() : 0;
}


/***********************************************************************************
 **
 ** GetTreeView
 **
 ***********************************************************************************/
OpTreeView * OpTreeViewDropdown::GetTreeView()
{
	return m_tree_view_window ? m_tree_view_window->GetTreeView() : 0;
}


/***********************************************************************************
 **
 ** GetSelectedItem
 **
 ***********************************************************************************/
OpTreeModelItem * OpTreeViewDropdown::GetSelectedItem(int * position)
{
	return m_tree_view_window ? m_tree_view_window->GetSelectedItem(position) : 0;
}

/***********************************************************************************
**
** SetSelectedItem
**
***********************************************************************************/
void OpTreeViewDropdown::SetSelectedItem(OpTreeModelItem *item, BOOL selected)
{
	if(m_tree_view_window)
	{
		m_tree_view_window->SetSelectedItem(item, selected);
	}
}

void OpTreeViewDropdown::SetMaxLines(int max_lines)
{ 
	m_max_lines = max_lines < 0 ? 0 : max_lines;
}

void OpTreeViewDropdown::SetTreeViewName(const char* name) 
{ 
	if (m_tree_view_window) m_tree_view_window->SetTreeViewName(name); 
}
