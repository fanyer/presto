/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/thumbnails/GenericThumbnail.h"

#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/thumbnails/GenericThumbnailContent.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "modules/display/vis_dev.h"
#include "modules/widgets/OpButton.h"
#include "modules/dragdrop/dragdrop_manager.h"

namespace
{

OP_STATUS InitButton(OpButton* button, const GenericThumbnailContent::ButtonInfo& info)
{
	if (!info.HasContent())
	{
		button->SetAction(NULL);
		return OpStatus::OK;
	}

	button->SetName(info.m_name);
	RETURN_IF_ERROR(button->SetAccessibilityText(info.m_accessibility_text));

	OpInputAction* action = OP_NEW(OpInputAction, (info.m_action));
	RETURN_OOM_IF_NULL(action);
	action->SetActionData(info.m_action_data);
	button->SetAction(action);
	OpString shortcut;
	RETURN_IF_ERROR(g_input_manager->GetShortcutStringFromAction(action, shortcut, button));
	RETURN_IF_ERROR(action->GetActionInfo().SetTooltipText(info.m_tooltip_text.CStr()));

	return OpStatus::OK;
}

}

GenericThumbnail::GenericThumbnail()
	: m_content(NULL)
	, m_close_button(NULL)
	, m_title_button(NULL)
	, m_busy_spinner(NULL)
	, m_hovered(FALSE)
	, m_locked(FALSE)
	, m_close_hovered(FALSE)
{
}

OP_STATUS GenericThumbnail::Init(const Config& config)
{
	m_config = config;

	SetSkinned(TRUE);

	RETURN_IF_ERROR(OpButton::Construct(&m_title_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	OP_ASSERT(m_title_button != NULL);
	AddChild(m_title_button);
	m_title_button->SetEllipsis(g_pcui->GetIntegerPref(PrefsCollectionUI::EllipsisInCenter) == 1 ? ELLIPSIS_CENTER : ELLIPSIS_END);
	m_title_button->SetJustify(JUSTIFY_CENTER, FALSE);
	m_title_button->GetBorderSkin()->SetImage(m_config.m_title_border_image);
	m_title_button->SetIgnoresMouse(TRUE);

	RETURN_IF_ERROR(OpButton::Construct(&m_close_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	OP_ASSERT(m_close_button != NULL);
	AddChild(m_close_button);
	m_close_button->GetBorderSkin()->SetImage(m_config.m_close_border_image);
	m_close_button->GetForegroundSkin()->SetImage(m_config.m_close_foreground_image);
	m_close_button->SetIgnoresMouse(TRUE);
	m_close_button->SetVisibility(FALSE);

	RETURN_IF_ERROR(OpProgressBar::Construct(&m_busy_spinner));
	OP_ASSERT(m_busy_spinner != NULL);
	AddChild(m_busy_spinner);
	m_busy_spinner->GetBorderSkin()->SetImage(m_config.m_busy_border_image);
	m_busy_spinner->SetSpinnerImage(m_config.m_busy_foreground_image);
	m_busy_spinner->SetType(OpProgressBar::Spinner);
	m_busy_spinner->SetIgnoresMouse(TRUE);
	m_busy_spinner->SetVisibility(FALSE);

	SetListener(this);

	m_blend.AddWidgetToBlend(this);
	m_blend.AddWidgetToBlend(m_title_button);

	return OpStatus::OK;
}

OP_STATUS GenericThumbnail::SetContent(GenericThumbnailContent* content)
{
	if (m_content != NULL && m_content->GetButton()->GetParent())
	{
		m_blend.RemoveWidgetToBlend(m_content->GetButton());
		m_content->GetButton()->SetListener(NULL);
		OP_DELETE(m_content);
	}

	m_content = content;
	if (m_content != NULL)
	{
		RETURN_IF_ERROR(OnNewContent());

		m_blend.AddWidgetToBlend(m_content->GetButton());
	}
	return OpStatus::OK;
}

OP_STATUS GenericThumbnail::OnNewContent()
{
	AddChild(m_content->GetButton());
	m_content->GetButton()->SetIgnoresMouse(TRUE);

	return OnContentChanged();
}

OP_STATUS GenericThumbnail::OnContentChanged()
{
	RETURN_IF_ERROR(m_title_button->SetText(m_content->GetTitle()));

	m_busy_spinner->SetVisibility(m_content->IsBusy());

	GenericThumbnailContent::ButtonInfo close_button_info;
	RETURN_IF_ERROR(m_content->GetCloseButtonInfo(close_button_info));
	RETURN_IF_ERROR(InitButton(m_close_button, close_button_info));

	GenericThumbnailContent::ButtonInfo button_info;
	RETURN_IF_ERROR(m_content->GetButtonInfo(button_info));
	RETURN_IF_ERROR(InitButton(m_content->GetButton(), button_info));
	// Watir requirement:
	SetName(button_info.m_name);

	return OpStatus::OK;
}

const uni_char* GenericThumbnail::GetTitle() const
{
	 return m_content ? m_content->GetTitle() : NULL;
}

int GenericThumbnail::GetNumber() const
{
	return m_content->GetNumber();
}

bool GenericThumbnail::HasCloseButton() const
{
	return m_close_button != NULL && m_close_button->GetAction() != NULL;
}

void GenericThumbnail::OnDeleted()
{
	OpStatus::Ignore(SetContent(NULL));
	OpWidget::OnDeleted();
}

void GenericThumbnail::OnLayout()
{
	DoLayout();
	OpWidget::OnLayout();
}


namespace
{
	enum thumbnail_widget_rects
	{
		RECT_TITLE_BUTTON = 0,
		RECT_THUMBNAIL_BUTTON,
		RECT_BUSY_SPINNER,
		RECT_CLOSE_BUTTON,
		NUM_THUMBNAIL_WIDGET_RECTS
	};

	const INT32 FAKE_RECT_SIZE = -10000;

	void expand_wrapped_rect(OpRect &r, INT32 width, INT32 height)
	{
		if(r.x      < FAKE_RECT_SIZE/2) r.x      += width  - FAKE_RECT_SIZE;
		if(r.y      < FAKE_RECT_SIZE/2) r.y      += height - FAKE_RECT_SIZE;
		if(r.width  < FAKE_RECT_SIZE/2) r.width  += width  - FAKE_RECT_SIZE;
		if(r.height < FAKE_RECT_SIZE/2) r.height += height - FAKE_RECT_SIZE;
	}
}

void GenericThumbnail::ComputeRelativeLayoutRects(OpRect* rects)
{
	OP_ASSERT(m_content != NULL);

	//  So the idea here is to keep GetRequiredSize and DoLayout
	//  small and simple.  They both need much of the same information,
	//  but having them each do these calculations was a disaster.
	//  However, since one knows the size of the widget, and the other is being
	//  asked for the best size, we have to "compute the rects" without
	//  knowing the size of the widget.  Hence the word "relative".
	//
	//  We do this by allowing rects to have absurdly negative positions or sizes.
	//  A rect of width FAKE_RECT_SIZE-5 means that it is the width of the entire widget,
	//  minus 5 pixels.  A y value of FAKE_RECT_SIZE-20 means it is positioned 20 pixels
	//  above the bottom of the widget.
	//  
	//  When we need actual rects (which means we must know the width and
	//  height of the whole widget), we can use expand_wrapped_rect() to
	//  convert to a rect we can actually use.

	int left, top, right, bottom;

	int close_left = 0, close_top = 0, close_right = 0, close_bottom = 0;
	int title_left = 0, title_top = 0, title_right = 0, title_bottom = 0;
	int  busy_left = 0,  busy_top = 0,  busy_right = 0,  busy_bottom = 0;

	int tmp_w = 0, tmp_h = 0;

	const int fake_w = FAKE_RECT_SIZE, fake_h = FAKE_RECT_SIZE;
	int plus_button_bottom_shift = 0;

	for (int i = 0; i < NUM_THUMBNAIL_WIDGET_RECTS; i++)
	{
		rects[i].Empty();
	}

	GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);

	m_close_button->GetRequiredSize(tmp_w, tmp_h);
	m_close_button->GetBorderSkin()->GetMargin(&close_left, &close_top, &close_right, &close_bottom);
	rects[RECT_CLOSE_BUTTON].Set(fake_w - tmp_w - right  - close_right,
	                             fake_h - tmp_h - bottom - close_bottom,
	                             tmp_w,
	                             tmp_h);

	int title_indent_for_centering = fake_w - rects[RECT_CLOSE_BUTTON].x;
	m_title_button->GetRequiredSize(tmp_w, tmp_h);
	plus_button_bottom_shift = tmp_h;
	m_title_button->GetBorderSkin()->GetMargin(&title_left, &title_top, &title_right, &title_bottom);
	rects[RECT_TITLE_BUTTON].Set(title_indent_for_centering + title_left,
	                             fake_h - tmp_h - bottom - title_bottom,
	                             rects[RECT_CLOSE_BUTTON].x - title_indent_for_centering - title_left - title_right,
	                             tmp_h);

	int thumbnail_height = rects[RECT_TITLE_BUTTON].y - top - title_top;

	if (!HasTitle())
		thumbnail_height += plus_button_bottom_shift;

	rects[RECT_THUMBNAIL_BUTTON].Set(left,
	                                 top,
	                                 fake_w - left - right,
	                                 thumbnail_height);

	if (m_busy_spinner)
	{
		m_busy_spinner->GetRequiredSize(tmp_w, tmp_h);
		m_busy_spinner->GetBorderSkin()->GetMargin(&busy_left, &busy_top, &busy_right, &busy_bottom);
		rects[RECT_BUSY_SPINNER].Set(left + busy_left,
		                             fake_h - tmp_h - bottom - busy_bottom,
		                             tmp_w,
		                             tmp_h);
	}
}

void GenericThumbnail::DoLayout()
{
	if (m_content == NULL)
		return;

	//  Let's get all the widgets we need rects for.
	OpWidget* widgets[NUM_THUMBNAIL_WIDGET_RECTS] = { NULL };
	widgets[RECT_TITLE_BUTTON]     = m_title_button;
	widgets[RECT_THUMBNAIL_BUTTON] = m_content->GetButton();
	widgets[RECT_BUSY_SPINNER]     = m_busy_spinner;
	widgets[RECT_CLOSE_BUTTON]     = m_close_button;

	OpRect rects[NUM_THUMBNAIL_WIDGET_RECTS];
	ComputeRelativeLayoutRects(rects);

	//  Find out the actual rects, given the available (required) size,
	//  and set the widget rects.
	for (int i = 0; i < NUM_THUMBNAIL_WIDGET_RECTS; i++)
	{
		if (widgets[i])
		{
			expand_wrapped_rect(rects[i], rect.width, rect.height);
			SetChildRect(widgets[i], rects[i]);
		}
	}
}

void GenericThumbnail::GetRequiredThumbnailSize(INT32& width, INT32& height)
{
	width  = THUMBNAIL_WIDTH;
	height = THUMBNAIL_HEIGHT;
}

void GenericThumbnail::GetRequiredSize(INT32& width, INT32& height)
{
	GetRequiredThumbnailSize(width, height);
	INT32 padding_width = 0, padding_height = 0;
	GetPadding(padding_width, padding_height);
	width  += padding_width;
	height += padding_height;
}

void GenericThumbnail::GetPadding(INT32& width, INT32& height)
{
	if (m_content)
	{
		//  Get the relative positions of the rects (relative, because we don't know the width yet)
		OpRect rects[NUM_THUMBNAIL_WIDGET_RECTS];
		ComputeRelativeLayoutRects(rects);
		INT32 t_left, t_top, t_right, t_bottom;
		m_content->GetButton()->GetBorderSkin()->GetPadding(&t_left, &t_top, &t_right, &t_bottom);
		width  = t_left + t_right  + (FAKE_RECT_SIZE - rects[RECT_THUMBNAIL_BUTTON].width);
		height = t_top  + t_bottom + (FAKE_RECT_SIZE - rects[RECT_THUMBNAIL_BUTTON].height);
	}
	else
	{
		width  = 0;
		height = 0;
	}
}

void GenericThumbnail::SetSelected(BOOL selected)
{
	INT32 state = GetBorderSkin()->GetState();
	state = selected ? state | SKINSTATE_SELECTED : state & ~SKINSTATE_SELECTED;
	GetBorderSkin()->SetState(state, -1, 0);
	m_title_button->GetBorderSkin()->SetState(state, -1, 0);
}

void GenericThumbnail::OnMouseMove(const OpPoint& point)
{
	//  We'll allow a negative margin to signify the inactive
	//  area around the widget.  Kind of a hack.
	INT32 m_left, m_top, m_right, m_bottom;
	GetBorderSkin()->GetMargin(&m_left, &m_top, &m_right, &m_bottom);

	SetCloseButtonHovered(m_close_button->GetRect().Contains(point));

	const OpRect active_rect(-m_left, -m_top, rect.width+m_left+m_right, rect.height+m_top+m_bottom);
	SetHovered(active_rect.Contains(point));

	OpWidget::OnMouseMove(point);
}

void GenericThumbnail::OnMouseLeave()
{
	SetHovered(FALSE);
	SetCloseButtonHovered(FALSE);
	OpWidget::OnMouseLeave();
}

void GenericThumbnail::SetHovered(BOOL hovered)
{
	if (hovered && !m_hovered)
	{
		m_hovered = TRUE;

		m_blend.HoverBlendIn();
		if (HasCloseButton() && !m_locked)
			m_close_button->SetVisibility(TRUE);
	}
	else if (!hovered && m_hovered)
	{
		m_hovered = FALSE;
		m_blend.HoverBlendOut();
		if (HasCloseButton())
			m_close_button->SetVisibility(FALSE);
	}
}

void GenericThumbnail::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_3)
		OpStatus::Ignore(m_content->HandleMidClick());
	else
		OpWidget::OnMouseDown(point, button, nclicks);
}

void GenericThumbnail::OnMouseUp(const OpPoint& point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1 && nclicks)	// only if it's a real click - unix tend to not like it otherwise
	{
		if (point.x >= 0 && point.y >= 0)
		{
			if (m_close_button->GetRect().Contains(point))
				m_close_button->Click();
			else
				m_content->GetButton()->Click(); // This feels like a hack... but is there a better way?
		}
	}
	else
	{
		OpWidget::OnMouseUp(point, button, nclicks);
	}
}

BOOL GenericThumbnail::OnContextMenu(OpWidget* widget, INT32 child_index,
		const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	const GenericThumbnailContent::MenuInfo info = m_content->GetContextMenuInfo();
	if (info.m_name != NULL && !m_locked)
		g_application->GetMenuHandler()->ShowPopupMenu(info.m_name, PopupPlacement::AnchorAtCursor(), info.m_context, keyboard_invoked);
	return TRUE;
}

void GenericThumbnail::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (m_config.m_drag_type != UNKNOWN_TYPE)
	{
		DesktopDragObject* drag_object = m_content->GetButton()->GetDragObject(m_config.m_drag_type, x, y);
		if (drag_object != NULL && m_content->HandleDragStart(*drag_object))
			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

void GenericThumbnail::OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* desktop_drag_object = static_cast<DesktopDragObject*>(drag_object);
	if (m_content->AcceptsDragDrop(*desktop_drag_object))
	{
		if (desktop_drag_object->GetType() == DRAG_TYPE_LINK || desktop_drag_object->GetType() == DRAG_TYPE_BOOKMARK || desktop_drag_object->GetType() == DRAG_TYPE_WINDOW)
			desktop_drag_object->SetDesktopDropType(DROP_COPY);
		else
			desktop_drag_object->SetDesktopDropType(DROP_MOVE);
	}
	else
	{
		desktop_drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
	}

}

void GenericThumbnail::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* desktop_drag_object = static_cast<DesktopDragObject*>(op_drag_object);
	if (m_content->AcceptsDragDrop(*desktop_drag_object))
	{
		m_content->HandleDragDrop(*desktop_drag_object);
	}
	else
	{
		desktop_drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
	}
}

BOOL GenericThumbnail::HasToolTipText(OpToolTip* tooltip)
{
	if (m_content)
		return m_content->GetButton()->HasToolTipText(tooltip);
	return FALSE;
}

void GenericThumbnail::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	if (IsDebugToolTipActive())
		OpWidget::GetToolTipText(tooltip, text);
	else
		m_content->GetButton()->GetToolTipText(tooltip, text);
}


BOOL GenericThumbnail::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
		{
			const GenericThumbnailContent::MenuInfo menu_info = m_content->GetContextMenuInfo();
			if (menu_info.m_name == NULL)
				break;

			OpRect rect;
			if (action->IsKeyboardInvoked())
			{
				rect = m_content->GetButton()->GetRect(TRUE);
				OpPoint point = vis_dev->GetView()->ConvertToScreen(rect.Center());
				rect.OffsetBy(point.x - rect.x, point.y - rect.y);
			}
			else
			{
				action->GetActionPosition(rect);
			}
			if (!m_locked)
			{
				g_application->GetMenuHandler()->ShowPopupMenu(
						menu_info.m_name, PopupPlacement::AnchorBelow(rect), menu_info.m_context,
						action->IsKeyboardInvoked());
			}
			return TRUE;
		}
	}
	return OpWidget::OnInputAction(action);
}

void GenericThumbnail::SetCloseButtonHovered(BOOL close_hovered)
{
	if (!m_close_hovered && close_hovered)
	{
		m_close_button->GetBorderSkin()->SetState(SKINSTATE_HOVER, SKINSTATE_HOVER, 100);
		m_close_button->GetForegroundSkin()->SetState(SKINSTATE_HOVER, SKINSTATE_HOVER, 100);
		m_close_hovered = TRUE;
	}
	else if (m_close_hovered && !close_hovered)
	{
		m_close_button->GetBorderSkin()->SetState(0, SKINSTATE_HOVER, 0);
		m_close_button->GetForegroundSkin()->SetState(0, SKINSTATE_HOVER, 0);
		m_close_hovered = FALSE;
	}	
}
