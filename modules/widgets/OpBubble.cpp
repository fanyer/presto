/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/pi/OpWindow.h"
#include "modules/widgets/OpBubble.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/display/vis_dev.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BUBBLE_ANIM_TIMER_RES 1

#define BUBBLE_ANIM_NORMAL_DURATION 800

#define BUBBLE_ANIM_BOUNCE_DURATION 400
#define BUBBLE_ANIM_BOUNCE_COUNT 1
#define BUBBLE_ANIM_BOUNCE_DROP_DISTANCE 30

#define BUBBLE_ANIM_SHAKE_DURATION 400
#define BUBBLE_ANIM_SHAKE_COUNT 4
#define BUBBLE_ANIM_SHAKE_DISTANCE 20

double GetAnimationDuration(OpBubble::ANIMATION a)
{
	switch (a)
	{
	case OpBubble::ANIMATION_NORMAL:			return BUBBLE_ANIM_NORMAL_DURATION;
	case OpBubble::ANIMATION_BOUNCE:			return BUBBLE_ANIM_BOUNCE_DURATION;
	case OpBubble::ANIMATION_SHAKE:				return BUBBLE_ANIM_SHAKE_DURATION;
	};
	return 0;
}

// == OpBubble =============================================

OpBubble::OpBubble(OpBubbleHandler *handler)
	: m_handler(handler)
	, m_edit(NULL)
	, m_visible(FALSE)
	, m_extra_distance(0)
	, m_extra_shift(0)
{
	m_timer.SetTimerListener(this);
}

OpBubble::~OpBubble()
{
	if (m_handler && m_handler->m_bubble == this)
		m_handler->m_bubble = NULL;
	else if (m_handler && m_handler->m_closed_bubble == this)
		m_handler->m_closed_bubble = NULL;
}

OP_STATUS OpBubble::Init(OpWindow *parent_window)
{
	RETURN_IF_ERROR(WidgetWindow::Init(GetWindowStyle(), parent_window, NULL, OpWindow::EFFECT_TRANSPARENT));
	//SetParentInputContext(g_application->GetInputContext());

	RETURN_IF_ERROR(OpMultilineEdit::Construct(&m_edit));

	OpWidget* root = m_widget_container->GetRoot();

#ifdef SKIN_SUPPORT
	root->GetBorderSkin()->SetImage("Bubble Skin", "Edit Skin");
	root->SetSkinned(TRUE);
#endif
	root->SetHasCssBorder(FALSE);
	root->AddChild(m_edit);

	m_edit->SetJustify(m_edit->GetRTL() ? JUSTIFY_RIGHT : JUSTIFY_LEFT, TRUE);

	// Some OpMultilineEdit functions fail if we don't have a width (like reformatting on font change)
	m_edit->SetRect(OpRect(0, 0, 500, 0), FALSE);
	m_edit->SetWrapping(TRUE);
	m_edit->SetFlatMode();
#ifdef SKIN_SUPPORT
	// Update skin padding (read from skin)
	INT32 left, top, right, bottom;
	m_edit->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
	m_edit->SetPadding(left, top, right, bottom);
#endif

	m_edit->font_info.weight = 5;
	root->SetListener(this);

	return OpStatus::OK;
}

void OpBubble::UpdatePlacement(const OpRect &target_screen_rect)
{
	INT32 margin_left = 0, margin_top = 0, margin_right = 0, margin_bottom = 0;
#ifdef SKIN_SUPPORT
	OpWidget* root = m_widget_container->GetRoot();
	root->GetBorderSkin()->GetMargin(&margin_left, &margin_top, &margin_right, &margin_bottom);
#endif

	PLACEMENT placement = GetDefaultPlacement();
	ALIGNMENT alignment = GetDefaultAlignment();

	INT32 width = 0, height = 0;
	GetRequiredSize(width, height);

	m_placement_rect = GetPlacement(width, height, margin_left, margin_top, margin_right, margin_bottom, target_screen_rect, placement, alignment);
	m_current_placement = placement;

#ifdef SKIN_SUPPORT
	SkinArrow *arrow = root->GetBorderSkin()->GetArrow();
	arrow->Reset();
	if (placement == PLACEMENT_BOTTOM)
		arrow->SetTop();
	else if (placement == PLACEMENT_TOP)
		arrow->SetBottom();
	else if (placement == PLACEMENT_LEFT)
		arrow->SetRight();
	else if (placement == PLACEMENT_RIGHT)
		arrow->SetLeft();

	if (alignment == ALIGNMENT_MIDDLE)
		arrow->SetOffset(50);
	else if (alignment == ALIGNMENT_END)
		arrow->SetOffset(100);
#endif

	if (m_visible)
		Show();
}

void OpBubble::UpdateFontAndColors(OpWidget *source)
{
	WIDGET_FONT_INFO wfi = source->font_info;
	int zoom = source->GetVisualDevice() ? source->GetVisualDevice()->GetScale() : 100;
	wfi.size = wfi.size * zoom / 100;
	m_edit->SetFontInfo(wfi);
}

void OpBubble::Show()
{
	if (!m_visible)
	{
		// Start fade in
		m_timer_start_time = g_op_time_info->GetWallClockMS();
		m_extra_distance = 0;
		m_extra_shift = 0;
		GetWindow()->SetTransparency(0);
		m_timer.Start(BUBBLE_ANIM_TIMER_RES);
	}

	OpRect translated_rect = m_placement_rect;

	if (m_current_placement == PLACEMENT_BOTTOM)
	{
		translated_rect.x += m_extra_shift;
		translated_rect.y += m_extra_distance;
	}
	else if (m_current_placement == PLACEMENT_TOP)
	{
		translated_rect.x += m_extra_shift;
		translated_rect.y -= m_extra_distance;
	}
	else if (m_current_placement == PLACEMENT_LEFT)
	{
		translated_rect.x -= m_extra_distance;
		translated_rect.y += m_extra_shift;
	}
	else if (m_current_placement == PLACEMENT_RIGHT)
	{
		translated_rect.x += m_extra_distance;
		translated_rect.y += m_extra_shift;
	}

	WidgetWindow::Show(TRUE, &translated_rect);
	m_visible = TRUE;
}

void OpBubble::Hide()
{
	WidgetWindow::Show(FALSE);
	m_visible = FALSE;
}

void OpBubble::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_timer)
	{
		double duration = GetAnimationDuration(GetAnimation());
		double now = g_op_time_info->GetWallClockMS();
		double progress = (now - m_timer_start_time) / duration;
		if (progress < 1)
			m_timer.Start(BUBBLE_ANIM_TIMER_RES);
		else
			progress = 1;
		if (GetAnimation() == ANIMATION_BOUNCE)
		{
			m_extra_distance = (int)((op_fabs(op_cos(progress * M_PI * BUBBLE_ANIM_BOUNCE_COUNT) * (1 - progress * progress))) * BUBBLE_ANIM_BOUNCE_DROP_DISTANCE);
			Show();

			// Fade should be twice as fast as the bounce
			progress *= 2;
			progress = MIN(progress, 1);
		}
		else if (GetAnimation() == ANIMATION_SHAKE)
		{
			m_extra_shift = (int)(op_sin(progress * M_PI * BUBBLE_ANIM_SHAKE_COUNT) * BUBBLE_ANIM_SHAKE_DISTANCE * (1 - progress * progress));
			Show();

			// Fade should be four times as fast as the shake
			progress *= 4;
			progress = MIN(progress, 1);
		}
		GetWindow()->SetTransparency((INT32)(progress * 255));
	}
}

OP_STATUS OpBubble::SetText(const OpStringC& text)
{
	return m_edit->SetText(text.CStr());
}

void OpBubble::GetRequiredSize(INT32 &width, INT32 &height)
{
	// Set a rect so the wrapping will be done there before asking GetRequiredSize
	OpRect old_rect = m_edit->GetRect();
	m_edit->SetRect(OpRect(0, 0, 250, 20), FALSE);

	INT32 padding_left = 0, padding_top = 0, padding_right = 0, padding_bottom = 0;
#ifdef SKIN_SUPPORT
	m_edit->GetBorderSkin()->GetPadding(&padding_left, &padding_top, &padding_right, &padding_bottom);
#endif
	/** 
	 * m_edit->GetContentWidth() is affected by text justification and hence
	 * would give a value much larger than the width of the text block
	 * when text is not left justified resulting in too wide a bubble.
	 * Hence m_edit->GetMaxBlockWidth() is used
	 */
	width = m_edit->GetMaxBlockWidth()  + padding_left + padding_right;
	height = m_edit->GetContentHeight() + padding_top + padding_bottom;

#ifdef SKIN_SUPPORT
	OpWidget* root = m_widget_container->GetRoot();
	root->GetBorderSkin()->GetPadding(&padding_left, &padding_top, &padding_right, &padding_bottom);
#endif
	width += padding_left + padding_right;
	height += padding_top + padding_bottom;

	// Some sanity checking. We don't want to get too large if the content happens to be specified by some evil webpage.
	if (width > 1000)
		width = 1000;
	if (height > 1000)
		height = 1000;

	// Set back rect to what it was
	m_edit->SetRect(old_rect, FALSE);
}

void OpBubble::OnResize(UINT32 width, UINT32 height)
{
	WidgetWindow::OnResize(width, height);

	OpWidget* root = m_widget_container->GetRoot();

	INT32 padding_left = 0, padding_top = 0, padding_right = 0, padding_bottom = 0;
#ifdef SKIN_SUPPORT
	root->GetBorderSkin()->GetPadding(&padding_left, &padding_top, &padding_right, &padding_bottom);
#endif
	OpRect rect = root->GetBounds();
	rect.x += padding_left;
	rect.y += padding_top;
	rect.width -= padding_left + padding_right;
	rect.height -= padding_top + padding_bottom;
	m_edit->SetRect(rect);
}

BOOL OpBubble::OnIsCloseAllowed()
{
	if (WidgetWindow::OnIsCloseAllowed())
	{
		BOOL allow_close = !m_edit->IsInUseByExternalCode();
		OP_ASSERT(allow_close); // If this happen the window might stay forever i think
		return allow_close;
	}
	return FALSE;
}

INT32 Align(int length, int range_start, int range_length, OpBubble::ALIGNMENT alignment)
{
	if (alignment == OpBubble::ALIGNMENT_BEGINNING)
		return range_start;
	else if (alignment == OpBubble::ALIGNMENT_MIDDLE)
		return range_start + (range_length - length) / 2;
	return range_start + range_length - length;
}


OpRect OpBubble::GetPlacement(INT32 width, INT32 height, INT32 margin_left, INT32 margin_top, INT32 margin_right, INT32 margin_bottom, const OpRect &ref_screen_rect, PLACEMENT &placement, ALIGNMENT alignment)
{
	OpScreenProperties screen_props;
	OpPoint point(ref_screen_rect.Center());

	OpWindow* topwindow = GetWindow();
	while (topwindow && topwindow->GetParentWindow())
		topwindow = topwindow->GetParentWindow();

	g_op_screen_info->GetProperties(&screen_props, topwindow, &point);

	OpRect screen(screen_props.screen_rect);

	OpRect rect(0, 0, width, height);

	// Align to ref_screen_rect
	if (placement == PLACEMENT_LEFT || placement == PLACEMENT_RIGHT)
		rect.y = Align(rect.height, ref_screen_rect.y + margin_top, ref_screen_rect.height - margin_top - margin_bottom, alignment);
	else
		rect.x = Align(rect.width, ref_screen_rect.x + margin_left, ref_screen_rect.width - margin_left - margin_right, alignment);

	// Move so it's inside screen
	int attempt = 0;
	while (attempt++ < 2)
	{
		if (placement == PLACEMENT_RIGHT)
		{
			rect.x = ref_screen_rect.x + ref_screen_rect.width;
			rect.x += margin_left;
		}
		else if (placement == PLACEMENT_BOTTOM)
		{
			rect.y = ref_screen_rect.y + ref_screen_rect.height;
			rect.y += margin_top;
		}
		else if (placement == PLACEMENT_LEFT)
		{
			rect.x = ref_screen_rect.x - rect.width;
			rect.x -= margin_right;
		}
		else if (placement == PLACEMENT_TOP)
		{
			rect.y = ref_screen_rect.y - rect.height;
			rect.y -= margin_bottom;
		}

		// If outside, try again using the opposite side
		//if (screen.Contains(rect))
		//	break;
		if ((placement == PLACEMENT_LEFT ||
			placement == PLACEMENT_RIGHT) &&
			rect.x >= screen.x &&
			rect.x < screen.x + screen.width - rect.width)
			break;
		if ((placement == PLACEMENT_TOP ||
			placement == PLACEMENT_BOTTOM) &&
			rect.y >= screen.y &&
			rect.y < screen.y + screen.height - rect.height)
			break;

		switch (placement)
		{
		case PLACEMENT_LEFT:
			placement = PLACEMENT_RIGHT; break;
		case PLACEMENT_TOP:
			placement = PLACEMENT_BOTTOM; break;
		case PLACEMENT_RIGHT:
			placement = PLACEMENT_LEFT; break;
		case PLACEMENT_BOTTOM:
			placement = PLACEMENT_TOP; break;
		};
	}

	// Adjust the rect so it really is inside the screen.
	rect.x = MAX(rect.x, screen.x);
	rect.x = MIN(rect.x, screen.x + screen.width - rect.width);
	rect.y = MAX(rect.y, screen.y);
	rect.y = MIN(rect.y, screen.y + screen.height - rect.height);
	return rect;
}

// == OpBubbleHandler =============================================

OpBubbleHandler::OpBubbleHandler()
	: m_bubble(NULL)
	, m_closed_bubble(NULL)
{
}

OpBubbleHandler::~OpBubbleHandler()
{
	if (m_bubble)
		m_bubble->Close(TRUE);

	// If we have a pending closed bubble (the async message still hasn't arrived), we must
	// ensure it is closed immediately as we might be shutting down (or leak checks might trig because it's would get deleted in time).
	if (m_closed_bubble)
		m_closed_bubble->Close(TRUE);
}

void OpBubbleHandler::SetBubble(OpBubble *bubble)
{
	if (m_bubble)
	{
		// Trig a asynchronous close of the bubble and store the pointer in m_closed_bubble so we can
		// close it immediately if we happen to be deallocated ourselfs.

		// Any pending closed bubble can be immediately closed now (and should, since we'll use the pointer for the next one)
		if (m_closed_bubble)
			m_closed_bubble->Close(TRUE);

		m_closed_bubble = m_bubble;
		m_bubble->Hide();
		m_bubble->Close();
	}
	m_bubble = bubble;
}

OP_STATUS OpBubbleHandler::CreateBubble(OpWindow *parent_window, const uni_char *text)
{
	OpBubble *b = OP_NEW(OpBubble, (this));
	if (!b)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS status = b->Init(parent_window);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(b);
		return status;
	}

	status = b->SetText(text);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(b);
		return status;
	}
	SetBubble(b);
	return OpStatus::OK;
}

void OpBubbleHandler::UpdatePlacement(const OpRect &target_screen_rect)
{
	if (m_bubble)
		m_bubble->UpdatePlacement(target_screen_rect);
}

void OpBubbleHandler::UpdateFontAndColors(OpWidget *source)
{
	if (m_bubble)
		m_bubble->UpdateFontAndColors(source);
}

void OpBubbleHandler::Show()
{
	if (m_bubble)
		m_bubble->Show();
}

void OpBubbleHandler::Close()
{
	// Just remove the current bubble for now (never reuse it)
	SetBubble(NULL);
}
