/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"
#include "adjunct/quick/windows/OpRichMenuWindow.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/Application.h"

#include "modules/skin/OpSkinElement.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/WidgetWindow.h"

#define ANIMATION_OFFSET 15

DEFINE_CONSTRUCT(OpRichMenuToolbar);

OpRichMenuWindow::OpRichMenuWindow(OpWidget *owner, DesktopWindow* parent_window)
	: m_close_on_complete(FALSE)
	, m_animate(FALSE)
	, m_toolbar(NULL)
	, m_owner(owner)
	, m_listener(NULL)
	, m_parent_window(parent_window)
{
	/* Setting the object's parent input context like this is plain
	 * wrong!
	 *
	 * Someone must have the responsibility to unhook this object from
	 * 'm_owner' before 'm_owner' dies.  But only this object knows
	 * about the ownership relation, and it isn't told of the imminent
	 * death of the parent.  (See DSK-328063 and DSK-329679)
	 */
	if (m_owner)
		SetParentInputContext(m_owner);
}

OpRichMenuWindow::~OpRichMenuWindow()
{
	if (m_listener)
		m_listener->OnMenuClosed();
}

OP_STATUS OpRichMenuWindow::Construct(const char *toolbar_name, OpWindow::Style style)
{
	m_animate = style == OpWindow::STYLE_NOTIFIER ? TRUE : FALSE ;
	RETURN_IF_ERROR(Init(style, m_parent_window, OpWindow::EFFECT_TRANSPARENT));
	
	WidgetContainer *widget_container = GetWidgetContainer();
	OpWidget *root_widget = widget_container->GetRoot();

	// Init toolbar
	RETURN_IF_ERROR(OpRichMenuToolbar::Construct(&m_toolbar));
	root_widget->AddChild(m_toolbar);
	m_toolbar->SetName(toolbar_name);

	SetSkin("Rich Popup Menu Skin");
	
	root_widget->SetHasCssBorder(FALSE); // Disable CSS border which WidgetContainer set to avoid painting the skin on the root.
	root_widget->SetCanHaveFocusInPopup(TRUE);

	// Fix: We draw the skin twice! (In TreeViewWindow too!)
	m_toolbar->SetHasCssBorder(TRUE);
	m_toolbar->SetAlignment(OpBar::ALIGNMENT_LEFT);
	m_toolbar->SetButtonSkinType(SKINTYPE_DEFAULT);

	for (OpWidget *w = m_toolbar->GetFirstChild(); w != NULL; w = w->GetNextSibling())
	{
		if (w->GetType() == WIDGET_TYPE_BUTTON)
		{
			OpButton *b = static_cast<OpButton*>(w);
			b->SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
			b->SetButtonType(OpButton::TYPE_OMENU);
			b->SetFixedImage(FALSE);
			b->SetShowShortcut(TRUE);
			b->SetTabStop(TRUE);
		}
	}

	return OpStatus::OK;
}

void OpRichMenuWindow::Show(Listener &listener)
{
	m_listener = &listener;

	m_toolbar->SetGrowToFit(FALSE);
	// Check dimensions needed for the toolbar contents
	INT32 width, height;
	m_toolbar->OnLayout(TRUE, 0x40000000, 0x40000000, width, height);

	// Enable grow to fit so all items expand to the used width.
	m_toolbar->SetGrowToFit(TRUE);

	OP_ASSERT(m_owner);

	// Calculate window rect
	OpRect rect = WidgetWindow::GetBestDropdownPosition(m_owner, width, height, TRUE);
	// GetBestDropdownPosition returns the best rectangle, but the height and/or width can
	// be far bigger that the window we are going to show. Happens if the rectangle is 
	// partially off-screen.
	if (rect.width > width)
		rect.width = width;
	if (rect.height > height)
		rect.height = height;

	OpRect edit_rect  = m_owner->GetRect();
	OpRect edit_rect_screen = m_owner->GetScreenRect();

	INT32 left, top, bottom, right;
	GetSkinImage()->GetPadding(&left, &top, &right, &bottom);
	rect.width += left + right;
	rect.height += top + bottom;
	int padding_height = top + bottom;

	GetSkinImage()->GetMargin(&left, &top, &right, &bottom);
	GetSkinImage()->GetArrow()->Reset();


	rect.x += left;
	if (rect.y < edit_rect_screen.y)
		rect.y -= bottom + padding_height;
	else
		rect.y += top;

	// Move the window inside the top level window
	OpWindow *top_win = m_owner->GetWidgetContainer()->GetWindow();
	top_win = top_win->GetRootWindow();
	if (top_win)
	{
		INT32 x, y;
		UINT32 w, h;
		INT32 left_margin, top_margin, right_margin, bottom_margin;
		top_win->GetInnerPos(&x, &y);
		top_win->GetInnerSize(&w, &h);
		top_win->GetMargin(&left_margin, &top_margin, &right_margin, &bottom_margin);

		const INT32 max_x = left_margin + x + w - right_margin;
		const INT32 max_y = top_margin + y + h - bottom_margin;

		if (rect.x + rect.width + right > max_x)
			rect.x = max_x - rect.width - right;
		if (rect.x < x)
			rect.x = x;
		if (rect.y + rect.height + bottom > max_y)
			rect.y = max_y - rect.height - bottom - edit_rect.height;
	}

	GetSkinImage()->PointArrow(rect, edit_rect_screen);

	// This kicks in if popup was positioned without problems inside browser
	// window and owner rect is very small.
	// Makes arrow accuracy really nice :)
	SkinArrow *arrow = GetSkinImage()->GetArrow();
	if (arrow && ( arrow->part == SKINPART_TILE_TOP ||
				   arrow->part == SKINPART_TILE_BOTTOM ))
	{
		if(arrow->offset < 1)
		{
			UINT8 left, top, right, bottom;
			GetSkinImage()->GetSkinElement()->
				GetStretchBorder(&left, &top, &right, &bottom, 0);
			rect.x -= left;
			GetSkinImage()->PointArrow(rect, edit_rect_screen);
		}
	}

	DesktopWindow::Show(TRUE, &rect);
	SetFocus(FOCUS_REASON_ACTIVATE);

	if (m_animate)
		Animate(TRUE);
}

void OpRichMenuWindow::CloseMenu()
{
	if (m_listener)
	{
		// Since close is asynchronous, we need to trig and unref m_listener here.
		m_listener->OnMenuClosed();
		m_listener = NULL;
		m_owner = NULL;
		SetParentInputContext(NULL);
	}

	DesktopWindow::Close();
}

void OpRichMenuWindow::Close(BOOL immediately, BOOL user_initiated, BOOL force)
{
	if (m_animate)
	{
		if (immediately)
		{
			DesktopWindow::Close(TRUE);
			return;
		}

		m_close_on_complete = TRUE;
		Animate(FALSE);
	}
	else
	{
		DesktopWindow::Close(immediately, user_initiated, force);
	}
}

OpWidget* OpRichMenuWindow::GetToolbarWidgetOfType(OpTypedObject::Type type)
{
	for (OpWidget *w = m_toolbar->GetFirstChild(); w != NULL; w = w->GetNextSibling())
	{
		if (w->GetType() == type)
			return w;
	}
	return NULL;
}

void OpRichMenuWindow::Animate(BOOL show)
{
	QuickAnimationWindowObject *anim_win = OP_NEW(QuickAnimationWindowObject, ());
	if (!anim_win || OpStatus::IsError(anim_win->Init(this, NULL)))
	{
		OP_DELETE(anim_win);
		return;
	}

	OpRect start_rect, end_rect;
	GetRect(start_rect);
	end_rect.Set(start_rect);
	if (show)
		start_rect.OffsetBy(0, ANIMATION_OFFSET);
	else
		end_rect.OffsetBy(0, ANIMATION_OFFSET);
	anim_win->animateOpacity(!show, show);
	anim_win->animateHideWhenCompleted(!show);
	anim_win->animateIgnoreMouseInput(TRUE, !show);
	anim_win->animateRect(start_rect, end_rect);
	g_animation_manager->startAnimation(anim_win, 
		show ? ANIM_CURVE_SPEED_UP : ANIM_CURVE_SLOW_DOWN);
}

void OpRichMenuWindow::OnShow(BOOL show)
{
	if (m_animate && !show && m_close_on_complete)
		DesktopWindow::Close();
}

void OpRichMenuWindow::OnLayout()
{
	OpRect rect;
	GetBounds(rect);

	INT32 left, top, bottom, right;
	GetSkinImage()->GetPadding(&left, &top, &right, &bottom);
	rect.x += left;
	rect.y += top;
	rect.width -= left + right;
	rect.height -= top + bottom;

	rect = m_toolbar->LayoutToAvailableRect(rect);
}

void OpRichMenuWindow::OnClose(BOOL user_initiated)
{
	if (m_listener)
	{
		m_listener->OnMenuClosed();
		m_listener = NULL;
		m_owner = NULL;
	}
}
