/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/widgets/finger_touch/anchor_element_of_interest.h"

#include "modules/logdoc/htm_lex.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/widgets/finger_touch/element_expander_container.h"
#include "modules/widgets/finger_touch/fingertouch_config.h"
#include "modules/widgets/finger_touch/element_expander_impl.h"

void AnchorElementOfInterest::GenerateInitialDestRects(float scale, unsigned int max_elm_size, const OpPoint& click_origin)
{
	dest_rect.Empty();

	for (AnchorFragment* fragment = (AnchorFragment*) region.First(); fragment; fragment = fragment->Suc())
	{
		OpRect rect = fragment->GetRect();

		if ((unsigned int) rect.width > max_elm_size && (unsigned int) rect.height > max_elm_size)
		{
			// What should happen with too large elements?
		}
		else
			rect = ScaleRectFromCenter(rect, scale, click_origin);

		dest_rect.UnionWith(rect);
		fragment->SetRect(rect, 0);
	}
}

BOOL AnchorElementOfInterest::DoIsSuitable(unsigned int max_elm_size)
{
	for (AnchorFragment* fragment = (AnchorFragment*) region.First(); fragment; fragment = fragment->Suc())
	{
		OpRect rect = fragment->GetRect();

		if ((unsigned int) rect.width <= max_elm_size || (unsigned int) rect.height <= max_elm_size)
			return TRUE;
	}

	return FALSE;
}

void AnchorElementOfInterest::DoActivate()
{
	// should be positioned in the center of the screen ?
	OpRect bigger_dest(dest_rect.x - dest_rect.width * 2, dest_rect.y - dest_rect.height * 2, dest_rect.width *4 , dest_rect.height * 4);
	ScheduleAnimation(dest_rect, 1.0f, bigger_dest, 0.0f, ACTIVATION_ANIMATION_LENGTH_MS);
}

void AnchorElementOfInterest::TranslateDestination(int offset_x, int offset_y)
{
	dest_rect.OffsetBy(offset_x, offset_y);

	for (AnchorFragment* fragment = region.First(); fragment; fragment = fragment->Suc())
	{
		OpRect rect = fragment->GetRect();

		rect.OffsetBy(offset_x, offset_y);
		dest_rect.UnionWith(rect);
		fragment->SetRect(rect, 0);
	}
}

void AnchorElementOfInterest::LayoutMultilineElements()
{
	// Recalculate dest_rect below.

	dest_rect.Empty();

	// Put each fragment on its own line.

	const OpRect* prev_rect = &region.First()->GetRect();

	dest_rect = *prev_rect;

	for (AnchorFragment* fragment = region.First()->Suc(); fragment; fragment = fragment->Suc())
	{
		/* Use 1/5 of the height of the previous line (fragment) as
		   inter-line spacing (~ line-height:1.2em) */

		const OpRect& target_rect = fragment->GetRect();
		OpRect new_rect(prev_rect->x, prev_rect->Bottom() + prev_rect->height / 5, target_rect.width, target_rect.height);

		dest_rect.UnionWith(new_rect);

		fragment->SetRect(new_rect, new_rect.y - region.First()->GetRect().y);
		fragment->SetPos(0, new_rect.y - region.First()->GetRect().y);
		prev_rect = &fragment->GetRect();
	}

	int minimum_size = GetElementExpander()->GetElementMinimumSize();
	int old_width = dest_rect.width;
	int old_height = dest_rect.height;
	if (dest_rect.width < minimum_size)
	{
		dest_rect.width = minimum_size;
	}
	if (dest_rect.height < minimum_size)
	{
		dest_rect.height = minimum_size;
	}
	int delta_x = (dest_rect.width - old_width) / 2;
	int delta_y = (dest_rect.height - old_height) / 2;
	if (old_width != dest_rect.width || old_height != dest_rect.height)
	{
		dest_rect.x = dest_rect.x - delta_x;
		dest_rect.y = dest_rect.y - delta_y;
		for (AnchorFragment* fragment = region.First(); fragment; fragment = fragment->Suc())
		{
			fragment->OffsetPosBy(delta_x,delta_y);
		}
	}

	AnchorFragment* fragment = region.First(); if(!fragment) return;
	if(fragment->IsImageFragment() && !fragment->Suc())
	{
		OpRect rect = fragment->GetRect();
		fragment->OffsetPosBy(-delta_x, -delta_y);
		rect.width += delta_x * 2;
		rect.height += delta_y * 2;
		fragment->SetRect(rect, 0);
	}
}

OP_STATUS AnchorElementOfInterest::InitContent()
{
#ifdef SKIN_SUPPORT
	COLORREF bg_col = HTM_Lex::GetColValByIndex(background_color);
	int bg_brightness = (GetRValue(bg_col) * 299 + GetGValue(bg_col) * 587 + GetBValue(bg_col) * 114) / 1000;
	if (bg_brightness < 64)
		skin.SetImage(LAYER_ELEMENT_NAME_BRIGHT);
#endif

	for (AnchorFragment* fragment = (AnchorFragment*) region.First(); fragment; fragment = fragment->Suc())
	{
		TextAnchorFragment *text_anchor_frag = fragment->GetTextAnchorFragment();
		if (text_anchor_frag)
		{
			OpWidget *root = widget_container->GetRoot();
			RETURN_IF_ERROR(text_anchor_frag->UpdateWidgetString(root, GetMaxWidth(), TRUE));
		}
	}
	LayoutMultilineElements();
	return OpStatus::OK;
}

void AnchorElementOfInterest::OnClick(OpWidget *widget, UINT32 id)
{
	// Follow link if popup menu hasn't had time to open yet
	if (!popupMenuOpen)
	{
		popupMenuTimer.Stop();
		// Follow link
		GetElementExpander()->Activate(this);
	}
}

void AnchorElementOfInterest::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	GetElementExpander()->ExtendInactivityTimer();

#if 0 // Where is POPUP_MOUSE_DELAY defined?
	// Start the popup menu timer
	if (button == MOUSE_BUTTON_1)
	{
		if (down)
		{
			popupMenuOpen = FALSE;
			popupMenuTimer.SetTimerListener(this);
			popupMenuTimer.Start(POPUP_MOUSE_DELAY);
		}
	}
#endif
}

void AnchorElementOfInterest::OnTimeOut(OpTimer* timer)
{
	GetElementExpander()->StopInactivityTimer();

	popupMenuOpen = TRUE;
#if 0
	FramesDocument* doc = GetElementExpander()->GetDocument();
	WindowCommander* win_com = doc->GetWindow()->GetWindowCommander();
	win_com->ShowPopupMenu(GetHtmlElement(), doc, dest_rect.Center(), orig_rect.Center());
#endif
// FIXME: show pop-up menu?
}

void AnchorElementOfInterest::OnPaint(OpWidget *widget, const OpRect &paint_rect)
{
	OpWidget *root = widget_container->GetRoot();
	if (widget == root)
		PaintFragments(paint_rect, widget->GetVisualDevice()); // FIXME: OOM
}

#endif // NEARBY_ELEMENT_DETECTION
