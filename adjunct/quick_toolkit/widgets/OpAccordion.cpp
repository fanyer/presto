/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#include "core/pch.h"

#include "OpAccordion.h"

#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpUnreadBadge.h"
#include "modules/display/vis_dev.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/dragdrop/dragdrop_manager.h"

/*****************************************************************************
**
**	OpAccordion
**
*****************************************************************************/

DEFINE_CONSTRUCT(OpAccordion)

OpAccordion::OpAccordion()
: m_drag_drop_pos(-1)
, m_listener(NULL)
, m_scrollbar(NULL)
, m_animated_widget(NULL)
, m_animation_progress(0)
, m_total_height(0)
, m_animation_start(0)
, m_scroll_direction(DOWN)
{
	GetBorderSkin()->SetImage("Accordion Skin");	
	SetSkinned(TRUE);

	INT32 ignored_width;
	GetSkinManager()->GetSize("Accordion Button Skin", &ignored_width, &m_button_height);

	OP_STATUS status = OpScrollbar::Construct(&m_scrollbar, FALSE);
	CHECK_STATUS(status);
	m_scrollbar->SetListener(this);
	m_scrollbar->SetVisibility(FALSE);
	AddChild(m_scrollbar, TRUE);

	SetTabStop(TRUE);
}

/*****************************************************************************
**
**	OpAccordion::GetTotalHeight
**
*****************************************************************************/

INT32 OpAccordion::GetTotalHeight(UINT32 number_of_items_to_count)
{
	// get the preferred height
	INT32 ignored, item_preferred_height, total_height = 0;
	UINT32 nb_items_to_count = number_of_items_to_count > 0 ? number_of_items_to_count : m_accordion_item_vector.GetCount();
	for (UINT32 i=0; i < nb_items_to_count; i++)
	{
		OpAccordionItem * item = m_accordion_item_vector.Get(i);
		
		item->GetPreferedSize(&ignored, &item_preferred_height, 0, 0);

		total_height += item_preferred_height;
	}
	return total_height;
}

/*****************************************************************************
**
**	OpAccordion:OnLayout
**
*****************************************************************************/

void OpAccordion::OnLayout()
{
	OpRect accordion_available_rect = GetBounds();
	m_total_height = 0;
	INT32 used_height = -1, width = accordion_available_rect.width, offset = 0;

	// optimization, cache total height
	if (m_total_height == 0)
		m_total_height = GetTotalHeight();

	if ((m_total_height > accordion_available_rect.height) != m_scrollbar->IsVisible() && !m_animated_widget)
	{
		m_animated_widget = m_scrollbar;
		if (m_total_height > accordion_available_rect.height)
			m_scrollbar->SetVisibility(TRUE);
		m_animation_progress = 0.0;
		g_animation_manager->startAnimation(this, ANIM_CURVE_SLOW_DOWN, 300, TRUE);
	}

	if (m_scrollbar->IsVisible())
	{
		UINT32 scrollbar_width = m_scrollbar->GetInfo()->GetVerticalScrollbarWidth();
		if (IsAnimating() && m_animated_widget == m_scrollbar)
		{
			if (m_total_height > accordion_available_rect.height)
				scrollbar_width *= m_animation_progress;
			else
				scrollbar_width *= 1 - m_animation_progress;
		}
		width -= scrollbar_width;
		SetChildRect(m_scrollbar, OpRect(width, 0, scrollbar_width, accordion_available_rect.height));
		m_scrollbar->SetSteps(m_button_height, accordion_available_rect.height);
		m_scrollbar->SetLimit(0, m_total_height - accordion_available_rect.height, accordion_available_rect.height);
		offset = m_scrollbar->GetValue();
	}

	INT32 scroll_height = 0; // like the total height, except when there are items that are scrolled off
	// set the height items can have
	for (UINT32 i=0; i < m_accordion_item_vector.GetCount(); i++)
	{
		INT32 ignored, item_preferred_height, height_to_use, minh, minw;
		OpAccordionItem * item = m_accordion_item_vector.Get(i);
		item->GetPreferedSize(&ignored, &item_preferred_height, 0, 0);
		item->GetMinimumSize(&minw, &minh);
		height_to_use = item_preferred_height;
		
		if (m_animated_widget == item)
		{
			if (m_animation_start == scroll_height)
			{
				m_animated_widget = NULL;
				ExpandItem(item, FALSE);
			}

			INT32 wanted_height; 
			if (m_scrollbar->GetValue() >= scroll_height)
				wanted_height = scroll_height + (1.0-m_animation_progress) * (m_animation_start-scroll_height);
			else
				wanted_height = m_animation_start + item->GetAnimationProgress() * (min(60, item_preferred_height-minh+(accordion_available_rect.height - item->rect.y)));
			
			m_scrollbar->SetValue(wanted_height);
		}

		if (offset > 0)
		{
			height_to_use = max(item_preferred_height-offset, minh);
			// collapsed
			item->SetIsReallyVisible(FALSE);
			offset -= (item_preferred_height-height_to_use);
			
		}
		else
		{
			item->SetIsReallyVisible(TRUE);
		}
		scroll_height += item_preferred_height - minh;

		SetChildRect(item, OpRect(0, used_height, width, height_to_use));

		if (accordion_available_rect.height < used_height+height_to_use)
			item->SetIsReallyVisible(FALSE);
		
		used_height += height_to_use;
		if (!item->IsExpanded())
			used_height--;
	}
}

/*****************************************************************************
**
**	OpAccordion::AddAccordionItem
**
*****************************************************************************/

OP_STATUS OpAccordion::AddAccordionItem(UINT32 id, OpString& name, const char* image_name, OpWidget* contained_widget, UINT32 insert_position)
{
	m_total_height = 0;
	OpAccordionItem* new_accordion_item;
	RETURN_IF_ERROR(OpAccordionItem::Construct(&new_accordion_item, id, name, image_name, contained_widget, this));
	RETURN_IF_ERROR(m_accordion_item_vector.Insert(insert_position, new_accordion_item));
	AddChild(new_accordion_item);
	return OpStatus::OK;
}

/*****************************************************************************
**
**	OpAccordion::RemoveAccordionItem
**
*****************************************************************************/

void OpAccordion::RemoveAccordionItem(UINT32 id)
{ 	
	for (UINT32 i = 0; i < m_accordion_item_vector.GetCount(); i++)
	{
		if (m_accordion_item_vector.Get(i)->GetItemID() == id)
		{
			m_accordion_item_vector.Delete(i);
			break;
		}
	}
	m_total_height = 0;
	Relayout();
}

/*****************************************************************************
**
**	OpAccordion::GetItemById
**
*****************************************************************************/

OpAccordion::OpAccordionItem* OpAccordion::GetItemById(UINT32 id)
{
	for (UINT32 i = 0; i < m_accordion_item_vector.GetCount(); i++)
	{
		if (m_accordion_item_vector.Get(i)->GetItemID() == id)
		{
			return m_accordion_item_vector.Get(i);
		}
	}
	return NULL;
}

/*****************************************************************************
**
**	OpAccordion::ExpandItem
**
*****************************************************************************/

void OpAccordion::ExpandItem(OpAccordionItem* item, BOOL expand, BOOL animate)
{
	m_total_height = 0;
	if (item->IsExpanded() == expand)
	{
		return; //nothing to do
	}
	
	item->SetExpanded(expand, animate);

	if (m_listener)
		m_listener->OnItemExpanded(item->GetItemID(), expand);
}

/*****************************************************************************
**
**	OpAccordion::OnDragStart
**
*****************************************************************************/

void OpAccordion::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_ACCORDION_ITEM )
	{
		UINT32 index = 0;
		for (UINT32 i=0; i<m_accordion_item_vector.GetCount(); i++)
		{
			OpAccordionItem * item = m_accordion_item_vector.Get(i);
			if (item == widget)
			{
				index = i;
				break;
			}
		}

		DesktopDragObject* drag_object = widget->GetDragObject(OpTypedObject::WIDGET_TYPE_ACCORDION_ITEM, x, y);

		if (drag_object)
		{
			drag_object->SetID(index);
			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
		}
	}

}

/*****************************************************************************
**
**	OpAccordion::OnDragMove
**
*****************************************************************************/

void OpAccordion::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (drag_object->GetType() == OpTypedObject::WIDGET_TYPE_ACCORDION_ITEM)
	{
		INT32 drag_index = drag_object->GetID();
		INT32 old_pos = m_drag_drop_pos;

		drag_object->SetDesktopDropType(DROP_MOVE);

		// we need to find the nearest item to the cursor so we know where we can insert this item
		y += widget->GetRect(TRUE).y;
		for (UINT32 i=0; i <m_accordion_item_vector.GetCount(); i++)
		{
			OpAccordionItem* item_to_replace = static_cast<OpAccordionItem*>(m_accordion_item_vector.Get(i));
			OpRect item_rect = item_to_replace->GetRect(TRUE);
			if (y <= (item_rect.y + item_rect.height/2))
			{
				m_drag_drop_pos = i;
				break;
			}
			else
				m_drag_drop_pos = i+1;
		}

		if (drag_index == m_drag_drop_pos || drag_index == m_drag_drop_pos-1)
		{
			m_drag_drop_pos = -1;
			drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
		}
		if (old_pos != m_drag_drop_pos)
			InvalidateAll();
	}
}

/*****************************************************************************
**
**	OpAccordion::OnDragDrop
**
*****************************************************************************/

void OpAccordion::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (drag_object->GetType() == OpTypedObject::WIDGET_TYPE_ACCORDION_ITEM)
	{
		INT32 drag_index = drag_object->GetID();

		// remove the dragged object and insert at the new place
		OpAccordionItem* dragged_item = m_accordion_item_vector.Remove(drag_index);
		if (m_drag_drop_pos > drag_index)
			m_drag_drop_pos--;
		m_accordion_item_vector.Insert(m_drag_drop_pos, dragged_item);
		for (int item_pos = m_drag_drop_pos; item_pos >= 0; item_pos--)
			m_accordion_item_vector.Get(item_pos)->SetZ(Z_TOP);

		if (m_listener)
			m_listener->OnItemMoved(dragged_item->GetItemID(), m_drag_drop_pos);

		m_drag_drop_pos = -1;
		StopTimer();
		InvalidateAll();
	}
}

/*****************************************************************************
**
**	OpAccordion::OnDragLeave
**
*****************************************************************************/

void OpAccordion::OnDragLeave(OpWidget* widget, OpDragObject* drag_object)
{
	if (m_drag_drop_pos != -1)
	{
		m_drag_drop_pos = -1;
		InvalidateAll();
	}
	StopTimer();
}

/*****************************************************************************
**
**	OpAccordion::OnPaintAfterChildren
**
*****************************************************************************/

void OpAccordion::OnPaintAfterChildren(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (m_drag_drop_pos >= 0)
	{
		INT32 width, height;
		const char* element = "Horizontal Drop Insert";

		GetSkinManager()->GetSize(element, &width, &height);

		OpRect rect = m_drag_drop_pos != 0 ? m_accordion_item_vector.Get(m_drag_drop_pos - 1)->GetRect(): OpRect(0,0, GetBounds().width, 0);

		rect.y += rect.height - height/2;
		rect.height = height;
		rect.x -= 2;
		rect.width += 4;

		GetSkinManager()->DrawElement(vis_dev, element, rect);
	}
}

/*****************************************************************************
**
**	OpAccordion::OnMouseUp
**
*****************************************************************************/

void OpAccordion::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_2)
	{
		const OpPoint p = point + GetScreenRect().TopLeft();
		g_application->GetMenuHandler()->ShowPopupMenu(m_fallback_popup_menu.CStr(), PopupPlacement::AnchorAt(p));
	}
}

/*****************************************************************************
**
**	OpAccordion::StartDragTimer
**
*****************************************************************************/

void OpAccordion::StartDragTimer(INT32 y)
{
	static const INT32 DRAG_SCROLL_TRESHHOLD = 40;
	if (y < DRAG_SCROLL_TRESHHOLD)
	{
		m_scroll_direction = UP;
	}
	else if (y < rect.height && y > rect.height - DRAG_SCROLL_TRESHHOLD)
	{
		m_scroll_direction = DOWN;
	}
	else 
	{
		if (IsTimerRunning())
			StopTimer();
		return;
	}

	if (IsTimerRunning())
		return;

	StartTimer(100);
}

/*****************************************************************************
**
**	OpAccordion::OnTimer
**
*****************************************************************************/

void OpAccordion::OnTimer()
{
	// When dragging, scroll 3 button heights at a time.
	if (!g_drag_manager->IsDragging())
	{
		StopTimer();
		return;
	}
	INT32 scroll_step = m_scroll_direction == DOWN ? m_button_height * 3 : -m_button_height * 3;
	m_scrollbar->SetValue(m_scrollbar->GetValue() + scroll_step , TRUE, TRUE, TRUE);
}

/*****************************************************************************
**
**	OpAccordion::OnScroll
**
*****************************************************************************/

void OpAccordion::OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input)
{
	Relayout();
}

/*****************************************************************************
**
**	OpAccordion::OnMouseWheel
**
*****************************************************************************/

BOOL OpAccordion::OnMouseWheel(INT32 delta,BOOL vertical)
{
	m_scrollbar->OnMouseWheel(delta, vertical);
	return TRUE;
}

/*****************************************************************************
**
**	OpAccordion::OnScrollAction
**
*****************************************************************************/

BOOL OpAccordion::OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth)
{
	return m_scrollbar->OnScroll(delta, vertical, smooth);
}

void OpAccordion::EnsureVisibility(OpAccordionItem* accordion_item, INT32 y, INT32 height)
{
	if (!m_scrollbar->IsVisible() || accordion_item->IsReallyVisible())
		return;

	OpRect bounds = GetBounds();
	OpRect accordion_item_rect = accordion_item->GetRect();
	OpRect widget_rect = accordion_item->GetWidget()->GetRect();
	widget_rect.y -= accordion_item->GetButton()->GetHeight();
	accordion_item_rect.y += accordion_item->GetButton()->GetHeight();
	INT32 visible_area;
	if (accordion_item->GetWidget()->IsVisible() && widget_rect.y < 0 && widget_rect.y < - y)
	{
		m_scrollbar->SetValue(m_scrollbar->GetValue() + y + widget_rect.y);
	}
	else if ((visible_area = bounds.height - accordion_item_rect.y - widget_rect.y) < y+height  )
	{
		m_scrollbar->SetValue(m_scrollbar->GetValue() + y + height - visible_area);
	}
	else if (!accordion_item->GetWidget()->IsVisible() && accordion_item_rect.y > bounds.height)
	{
		m_scrollbar->SetValue(m_scrollbar->GetValue() + accordion_item_rect.y - bounds.height);
	}
}

/*****************************************************************************
**
**	OpAccordionItem::ScrollToItem
**
*****************************************************************************/

void OpAccordion::ScrollToItem(OpAccordionItem* item)
{
	if (!m_scrollbar->IsVisible())
		return;

	m_animated_widget = item;
	m_animation_start = m_scrollbar->GetValue();
	g_animation_manager->startAnimation(this, ANIM_CURVE_SLOW_DOWN, 300, FALSE);
}

/*****************************************************************************
**
**	OpAccordionItem::OnAnimationUpdate
**
*****************************************************************************/

void OpAccordion::OnAnimationUpdate(double position)
{
	m_animation_progress = position;
	OnLayout();
}

/*****************************************************************************
**
**	OpAccordionItem::OnAnimationComplete
**
*****************************************************************************/

void OpAccordion::OnAnimationComplete()
{
	if (m_total_height <= GetBounds().height)
	{
		m_scrollbar->SetVisibility(FALSE);
	}
	OnLayout();
	m_animated_widget = NULL;
}

/*****************************************************************************
**
**	OpAccordionItem()
**
*****************************************************************************/

OpAccordion::OpAccordionItem::OpAccordionItem(UINT32 id, OpAccordionButton *button, OpWidget* contained_widget, OpAccordion* parent)
: m_id(id)
, m_expanded(FALSE)
, m_button(button)
, m_widget(contained_widget)
, m_accordion(parent)
#ifdef QUICK_TOOLKIT_ACCORDION_MAC_STYLE
, m_attention(TRUE)
#else // QUICK_TOOLKIT_ACCORDION_MAC_STYLE
, m_attention(FALSE)
#endif // QUICK_TOOLKIT_ACCORDION_MAC_STYLE
, m_animation_progress(0)
, m_expand_animation(TRUE)
{
	SetSkinned(TRUE);
	if (m_button)
	{
		m_button->GetBorderSkin()->SetImage("Accordion Button Skin");
		m_button->SetSkinned(TRUE);
	}
}

/*****************************************************************************
**
**	OpAccordionItem::Construct
**
*****************************************************************************/

OP_STATUS OpAccordion::OpAccordionItem::Construct(OpAccordionItem** obj, UINT32 id, OpString& name, const char* image_name, OpWidget* contained_widget, OpAccordion* parent)
{
	OpUnreadBadge* tmp_label;
	RETURN_IF_ERROR(OpUnreadBadge::Construct(&tmp_label));
	OpAutoPtr<OpUnreadBadge> label (tmp_label);
	
	OpString8 el_name;
	RETURN_IF_ERROR(el_name.AppendFormat("CategoryButton%d", id));

	label->SetVisibility(FALSE);
	label->GetBorderSkin()->SetImage("Accordion Button Label Skin");

	OpAccordionButton* button = OP_NEW(OpAccordionButton, (label.get(), parent));
	if (!button)
		return OpStatus::ERR_NO_MEMORY;
	
	label.release();

	button->GetForegroundSkin()->SetImage(image_name);
	button->SetTabStop(TRUE);
	button->SetName(el_name);

	*obj = OP_NEW(OpAccordionItem, (id, button, contained_widget, parent));

	if (!*obj)
	{
		button->Delete();
		return OpStatus::ERR_NO_MEMORY;
	}

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		button->Delete();
		return OpStatus::ERR_NO_MEMORY;
	}

	// Add the button and widget as children and register as listener
	(*obj)->AddChild(button);
	(*obj)->SetButtonText(name);
	button->SetListener(*obj);
	button->SetEllipsis(ELLIPSIS_END);
	if (contained_widget)
	{
		(*obj)->AddChild(contained_widget);
		contained_widget->SetListener(*obj);
	}
	return OpStatus::OK;
}


void OpAccordion::OpAccordionItem::SetExpanded(BOOL expanded, BOOL animate)
{
	if (expanded)
	{
		m_button->ShowLabel(FALSE);
		m_expanded = expanded; 
	}

	if (animate)
	{
		m_expand_animation = expanded;
		g_animation_manager->startAnimation(this, ANIM_CURVE_SLOW_DOWN, 300, TRUE);
	}
}

void OpAccordion::OpAccordionItem::SetAttention(BOOL attention, UINT32 number_of_notifications)
{ 
	
	m_button->SetNotifcationNumber(number_of_notifications);
#if 0 //QUICK_TOOLKIT_ACCORDION_MAC_STYLE
	m_button->SetAttention(attention); 
	m_attention = attention;
#endif  // QUICK_TOOLKIT_ACCORDION_MAC_STYLE
}

/*****************************************************************************
**
**	OpAccordionItem::OnAnimationUpdate
**
*****************************************************************************/

void OpAccordion::OpAccordionItem::OnAnimationUpdate(double position)
{
	m_animation_progress = position;
	m_accordion->Relayout();
}

/*****************************************************************************
**
**	OpAccordionItem::OnAnimationComplete
**
*****************************************************************************/

void OpAccordion::OpAccordionItem::OnAnimationComplete()
{
	// if we were animating a closing animation, we want to close the item now
	if (!m_expand_animation)
	{
		m_button->ShowLabel(TRUE);
		m_expanded = FALSE;
		m_button->SetUpdateNeeded(TRUE);
		m_button->UpdateActionStateIfNeeded();
	}
	m_accordion->Relayout();
}

/*****************************************************************************
**
**	OpAccordionButton
**
*****************************************************************************/

OpAccordion::OpAccordionButton::OpAccordionButton(OpUnreadBadge* label, OpAccordion* accordion) 
: OpButton(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE_AND_TEXT_ON_LEFT)
, m_show_label(TRUE)
, m_label(label) 
, m_unread_badge_width(0)
, m_accordion(accordion)
{
	AddChild(m_label);
	m_label->SetListener(this);
}

void OpAccordion::OpAccordionButton::SetNotifcationNumber(UINT32 value)
{
	if (value == 0)
	{
		OpStatus::Ignore(m_label->SetText(UNI_L("")));
		m_label->SetVisibility(FALSE);
		return;
	}
	// if it used to be hidden and it's > 0 the accordion item is open, don't display it
	if (value != 0 && m_show_label)
		m_label->SetVisibility(TRUE);
	OpString number;
	if (number.Reserve(12) != NULL)
		OpStatus::Ignore(m_label->SetText(uni_itoa(value, number.CStr(), 10)));
}

void OpAccordion::OpAccordionButton::ShowLabel(BOOL show_label) 
{
	if (m_show_label == show_label)
		return;

	m_show_label = show_label; 
	OpString text;
	m_label->GetText(text);
	if (show_label && text.HasContent())
		m_label->SetVisibility(TRUE);
	if (!show_label) 
		m_label->SetVisibility(FALSE);
}

void OpAccordion::OpAccordionButton::OnLayout()
{
	if (m_label->IsVisible())
	{
		OpRect bounds = GetBounds();
		INT32 w, h, ignored, top_margin, bottom_margin;
		m_label->GetRequiredSize(w, h);
		w = max(w, m_unread_badge_width);
		g_skin_manager->GetMargin("Accordion Button Label Skin", &ignored, &top_margin, &ignored, &bottom_margin);
		SetChildRect(m_label, OpRect(bounds.x + bounds.width - w, bounds.y + top_margin, w, bounds.height - bottom_margin - top_margin));
	}
}

void OpAccordion::OpAccordionButton::SetFocus(FOCUS_REASON reason)
{
	OpInputContext::SetFocus(reason);
	if (reason == FOCUS_REASON_KEYBOARD)
		m_accordion->EnsureVisibility(static_cast<OpAccordion::OpAccordionItem*>(GetParent()), 0, rect.height);
}


/*****************************************************************************
**
**	OpAccordionItem::SetButtonText
**
*****************************************************************************/

OP_STATUS OpAccordion::OpAccordionItem::SetButtonText(OpString button_text)
{
#ifdef QUICK_TOOLKIT_ACCORDION_MAC_STYLE
	button_text.MakeUpper();
#endif // QUICK_TOOLKIT_ACCORDION_MAC_STYLE
	return m_button->SetText(button_text.CStr());
}
/*****************************************************************************
**
**	OpAccordionItem::OnInputAction
**
*****************************************************************************/

BOOL OpAccordion::OpAccordionItem::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_OPEN_ACCORDION_ITEM:
				{
					BOOL can_open = !IsExpanded() || !IsReallyVisible();
					child_action->SetEnabled(can_open);
					child_action->SetAttention(m_attention);
					m_button->ShowLabel(can_open);
					return TRUE;
				}
			case OpInputAction::ACTION_CLOSE_ACCORDION_ITEM:
				{
					child_action->SetEnabled(IsExpanded());
					child_action->SetAttention(m_attention);
					return TRUE;
				}
			}
			break;
		}
	case OpInputAction::ACTION_OPEN_ACCORDION_ITEM:
		{
			if (IsAnimating())
				return TRUE;
			BOOL handled = FALSE;
			if (!IsExpanded())
			{
				m_accordion->ExpandItem(this, TRUE);
				handled = TRUE;
			}
			if (!IsReallyVisible() && m_widget->GetRect().y < 0 || (handled && rect.y + 60 > GetParent()->GetRect().height))
			{
				m_accordion->ScrollToItem(this);
				handled = TRUE;
			}
			return handled;
		}
	case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
		{
			ShowContextMenu(OpPoint(m_button->GetRect().width, 0), FALSE, TRUE);
			return TRUE;
		}
	case OpInputAction::ACTION_CLOSE_ACCORDION_ITEM:
		{
			if (IsAnimating())
				return TRUE;
			m_accordion->ExpandItem(this, FALSE);
			return TRUE;
		}
	case OpInputAction::ACTION_SHOW_POPUP_MENU:
		if (m_button->GetRect().Contains(m_button->GetMousePos()))
		{
			OpString8 menu;
			menu.Set(action->GetActionDataString());
			// we have to handle this action to set the right coordinate for the menu
			g_application->GetMenuHandler()->ShowPopupMenu(menu.CStr(),
					PopupPlacement::AnchorRightOf(GetScreenRect()),
					action->GetActionData(), action->IsKeyboardInvoked());
			return TRUE;
		}
	case OpInputAction::ACTION_NEXT_ITEM:
	case OpInputAction::ACTION_PREVIOUS_ITEM:
		{
			OpAccordionItem* item = NULL;
			OpWidget* widget_to_focus = NULL;
			INT32 item_pos = m_accordion->GetAccordionItemPos(this);
			// there are three possible locations of the keyboard focus:
			// the button, the end of the treeview or the beginning of the treeview
			// first find out what widget to focus

			if (action->GetFirstInputContext() == m_button)
			{
				if (action->GetAction() == OpInputAction::ACTION_NEXT_ITEM)
				{
					if (IsExpanded())
					{
						widget_to_focus = m_widget;
					}
					else
					{
						item = m_accordion->GetItemByIndex(++item_pos);
						if (!item)
							return TRUE;
						widget_to_focus = item->GetButton();
					}
				}
				else
				{
					item = m_accordion->GetItemByIndex(--item_pos);
					if (!item)
						return TRUE;
					if (item->IsExpanded())
					{
						widget_to_focus = item->GetWidget();
					}
					else
					{
						widget_to_focus = item->GetButton();
					}
				}
			}
			else
			{
				if (action->GetAction() == OpInputAction::ACTION_NEXT_ITEM)
				{
					item = m_accordion->GetItemByIndex(++item_pos);
					if (!item)
						return TRUE;

					widget_to_focus = item->GetButton();
				}
				else
				{
					widget_to_focus = m_button;
				}
			}

			
			widget_to_focus->SetFocus(FOCUS_REASON_KEYBOARD);

			if (widget_to_focus->GetType() == WIDGET_TYPE_TREEVIEW)
			{
				// special case for treeviews, select first or last item 
				OpTreeView* treeview = static_cast<OpTreeView*>(widget_to_focus);
				treeview->SetSelectedItem(action->GetAction() == OpInputAction::ACTION_NEXT_ITEM ? 0 : treeview->GetItemCount()-1);
			}
			else if (widget_to_focus->GetType() == WIDGET_TYPE_BUTTON)
			{
				widget_to_focus->SetHasFocusRect(TRUE);
			}
			return TRUE;
		}
	}
	return FALSE;
}

/*****************************************************************************
**
**	OpAccordionItem::OnDragMove
**
*****************************************************************************/

void OpAccordion::OpAccordionItem::OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{ 
	INT32 combined_y = rect.y+y + widget->GetRect().y;

	// maybe we should start scrolling
	m_accordion->StartDragTimer(combined_y);

	// start the timer in case we want to open the button
	if (widget == m_button && !m_expanded)
		StartTimer(300);
	else
		StopTimer();

	if (drag_object->GetType() == WIDGET_TYPE_ACCORDION_ITEM) 
		m_accordion->OnDragMove(widget, drag_object, pos, x, y);
	else if (widget != m_button)
		static_cast<OpWidgetListener*>(m_widget)->OnDragMove(widget, drag_object, pos, x, y);
}

/*****************************************************************************
**
**	OpAccordionItem::OnDragDrop
**
*****************************************************************************/

void OpAccordion::OpAccordionItem::OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{ 
	INT32 combined_x = rect.x + x, combined_y = rect.y+y + widget->GetRect().y;

	if (drag_object->GetType() == WIDGET_TYPE_ACCORDION_ITEM) 
		m_accordion->OnDragDrop(widget, drag_object, pos, combined_x, combined_y);
	else if (widget != m_button)
		static_cast<OpWidgetListener*>(m_widget)->OnDragDrop(widget, drag_object, pos, combined_x, combined_y);

	m_accordion->StopTimer();
}

/*****************************************************************************
**
**	OpAccordionItem::OnTimer
**
*****************************************************************************/

void OpAccordion::OpAccordionItem::OnTimer()
{ 
	if (g_drag_manager->IsDragging())
		m_accordion->ExpandItem(this, TRUE);

	StopTimer();
}

/*****************************************************************************
**
**	OpAccordionItem::OnDragStart
**
*****************************************************************************/

void OpAccordion::OpAccordionItem::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y) 
{ 
	if (widget == m_button || widget == m_button->GetNotificationLabel())
		m_accordion->OnDragStart(this, pos, x, y);
	else
	{
		if (m_expanded)
		{
			OpRect rect = m_button->GetRect();
			y += rect.height;
		}
		static_cast<OpWidgetListener*>(m_widget)->OnDragStart(widget, pos, x, y);
	}
}

/*****************************************************************************
**
**	OpAccordionItem::OnMouseEvent
**
*****************************************************************************/

void OpAccordion::OpAccordionItem::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	// on right click on the button, launch the popup menu if there is one in the button action
	
	if (widget == m_button && !down && (button == MOUSE_BUTTON_3 || (nclicks == 2 && button == MOUSE_BUTTON_1)))
	{
		if (!m_expanded)
			m_accordion->ExpandItem(this, TRUE);

		for (UINT32 i = 0; i < m_accordion->GetCount(); i++)
		{
			if (m_accordion->GetItemByIndex(i)->GetItemID() != m_id)
				m_accordion->ExpandItem(m_accordion->GetItemByIndex(i), FALSE, FALSE);
		}
		m_accordion->Relayout();
	}
}

/*****************************************************************************
**
**	OpAccordionItem::ShowContextMenu
**
*****************************************************************************/

BOOL OpAccordion::OpAccordionItem::ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context)
{
	OpInputAction* action = m_button->GetAction();
		
	while (action && action->GetAction() != OpInputAction::ACTION_SHOW_POPUP_MENU)
	{
		action = action->GetNextInputAction();
	}

	if (action && action->GetAction() == OpInputAction::ACTION_SHOW_POPUP_MENU)
	{
		OpRect rect(GetRect(TRUE));
		OpPoint translated_point = point + GetVisualDevice()->GetView()->ConvertToScreen(rect.TopLeft());
		OpString8 menu;
		menu.Set(action->GetActionDataString());
		// we have to handle this action to set the right coordinate for the menu
		g_application->GetMenuHandler()->ShowPopupMenu(menu.CStr(),
				PopupPlacement::AnchorAt(translated_point, center),
				action->GetActionData(), use_keyboard_context);
	}
	return TRUE;
}

/*****************************************************************************
**
**	OpAccordionItem::OnContextMenu
**
*****************************************************************************/

BOOL OpAccordion::OpAccordionItem::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	if (widget == m_button || widget->GetType() == WIDGET_TYPE_LABEL)
	{
		return ShowContextMenu(menu_point, FALSE, FALSE);
	}

	return FALSE;
}

/*****************************************************************************
**
**	OpAccordionItem::GetPreferedSize
**
*****************************************************************************/

void OpAccordion::OpAccordionItem::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	INT32 width, skin_button_height;
	GetSkinManager()->GetSize("Accordion Button Skin", &width, &skin_button_height);
	if ((IsExpanded() || IsAnimating()) && m_widget)
	{
		m_widget->GetPreferedSize(w, h, cols, rows);
		if (IsAnimating())
		{
			*h = *h * (m_expand_animation ? m_animation_progress : 1 - m_animation_progress);
		}
		*h += skin_button_height;
	}
	else
	{
		*h = skin_button_height;
	}
}

/*****************************************************************************
**
**	OpAccordionItem::GetMinimumSize
**
*****************************************************************************/

void OpAccordion::OpAccordionItem::GetMinimumSize(INT32* minw, INT32* minh)
{
	if ((IsExpanded() || IsAnimating()) && m_widget)
	{
		*minh = 0;
		*minw = 0;
		INT32 min_button_height, min_button_width;
		m_widget->GetMinimumSize(minw, minh);

		GetSkinManager()->GetSize("Accordion Button Skin", &min_button_width, &min_button_height);

		*minw = max(min_button_width, *minw);
		*minh += min_button_height;
	}
	else
	{
		GetSkinManager()->GetSize("Accordion Button Skin", minw, minh);
	}
}

/*****************************************************************************
**
**	OpAccordionItem::OnLayout
**
*****************************************************************************/

void OpAccordion::OpAccordionItem::OnLayout()
{
	INT32 width, skin_button_height;
	GetSkinManager()->GetSize("Accordion Button Skin", &width, &skin_button_height);
	OpRect rect = GetBounds();
	if (IsExpanded() && m_widget)
	{
		INT32 w, h;
		m_button->SetRect(OpRect(0, 0, rect.width, skin_button_height));
		m_widget->GetPreferedSize(&w, &h, 0, 0);
		m_widget->SetRect(OpRect(0, rect.height-h, rect.width, h));
		m_widget->SetVisibility(TRUE);
	}
	else
	{
		m_button->SetRect(rect);
		m_widget->SetVisibility(FALSE);
	}
}
