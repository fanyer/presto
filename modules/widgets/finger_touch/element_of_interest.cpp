/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/widgets/finger_touch/element_of_interest.h"

#include "modules/display/vis_dev.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/widgets/finger_touch/fingertouch_config.h"
#include "modules/widgets/finger_touch/element_expander_impl.h"
#include "modules/widgets/finger_touch/element_expander_container.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/skin/OpSkinManager.h"

/* static */ OpRect ElementOfInterest::ScaleRectFromCenter(const OpRect& orig_rect, float scale, const OpPoint& click_origin)
{
	OpPoint orig_rect_center = orig_rect.Center();

	// Find the distance from the click origin the center of the rectangle.

	int x_center_dist = orig_rect_center.x - click_origin.x;
	int y_center_dist = orig_rect_center.y - click_origin.y;

	// Scale width and height, and center the rectangle around the click origin.

	OpRect new_rect;

	new_rect.width = (INT32) op_ceil(orig_rect.width * scale);
	new_rect.height = (INT32) op_ceil(orig_rect.height * scale);
	new_rect.x = click_origin.x - new_rect.width / 2;
	new_rect.y = click_origin.y - new_rect.height / 2;

	// Scale distance between click origin rectangle center by zoom factor.

	new_rect.x += (INT32) ((float) x_center_dist * scale);
	new_rect.y += (INT32) ((float) y_center_dist * scale);

	return new_rect;
}

ElementOfInterest::~ElementOfInterest()
{
	Out();
	OP_DELETE(widget_container);
	OP_DELETE(window);
}

void ElementOfInterest::Activate()
{
	active = TRUE;
	DoActivate();
}

void ElementOfInterest::SetFont(const FontAtt& f)
{
	int min_font_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MinFontSize);

	font = f;

	if (font.GetSize() < min_font_size)
		font.SetSize(min_font_size);
}

OP_STATUS ElementOfInterest::PrepareForLayout(OpWindow* parent_window)
{
	OP_ASSERT(!window);

	RETURN_IF_ERROR(OpWindow::Create(&window));
	RETURN_IF_ERROR(window->Init(OpWindow::STYLE_ANIMATED, OpTypedObject::WINDOW_TYPE_UNKNOWN, parent_window, NULL));

	window->SetWindowAnimationListener(this);

	widget_container = OP_NEW(ElementExpanderContainer, (GetElementExpander()));
	if (!widget_container)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(widget_container->Init(OpRect(0, 0, 0, 0), window));
	OpWidget *root = widget_container->GetRoot();
	root->SetListener(this);

#ifdef SKIN_SUPPORT
	skin.SetImage(LAYER_ELEMENT_NAME);
#endif

#ifdef SUPPORT_TEXT_DIRECTION
	root->SetRTL(is_rtl);
#endif

	root->SetFontInfo(styleManager->GetFontInfo(font.GetFontNumber()),
					  (INT32) (font.GetSize() * GetElementExpander()->GetScaleFactor()),
					  font.GetItalic(),
					  font.GetWeight(),
					  root->font_info.justify,
					  root->font_info.char_spacing_extra);

	RETURN_IF_ERROR(InitContent());

	return OpStatus::OK;
}

OP_STATUS ElementOfInterest::PrepareForDisplay()
{
	AdjustSize();
	window->Show(TRUE);
	return OpStatus::OK;
}

void ElementOfInterest::Expand()
{
	ScheduleAnimation(orig_rect, 0.3f, dest_rect, 1.0f, HIDE_ANIMATION_LENGTH_MS);
}

void ElementOfInterest::DoGenerateInitialDestRect(float scale, unsigned int max_elm_size, const OpPoint& click_origin)
{
	dest_rect = ScaleRectFromCenter(orig_rect, scale, click_origin);
}

void ElementOfInterest::DoTranslateDestination(int offset_x, int offset_y)
{
	dest_rect.OffsetBy(offset_x, offset_y);
}

void ElementOfInterest::Move(int dx, int dy)
{
}

void ElementOfInterest::SetDestinationPadding(unsigned int padding)
{
	int diff = padding - this->padding;

	this->padding = padding;
	dest_rect = dest_rect.InsetBy(-diff);
}

void ElementOfInterest::AdjustSize()
{
	widget_container->SetPos(AffinePos());
	widget_container->SetSize(dest_rect.width, dest_rect.height);
	window->SetMaximumInnerSize(dest_rect.width, dest_rect.height);
	window->SetInnerPos(0, 0);  // window cannot be transparent, set  its size to 0
	window->SetInnerSize(dest_rect.width, dest_rect.height);
}

int ElementOfInterest::GetMaxWidth()
{
	OpRect avail_rect;
	GetElementExpander()->GetAvailableRect(avail_rect);
	int available_width = avail_rect.width - LAYER_PADDING - LAYER_SPACING;
	int widest_rect_width = MAX(orig_rect.width, dest_rect.width);
	widest_rect_width = (int) (widest_rect_width * (GetElementExpander()->GetScaleFactor() + 0.1f));
	return MIN(widest_rect_width, available_width);
}

void ElementOfInterest::DoHide()
{
	hidden = TRUE;
	ScheduleAnimation(dest_rect, 1.0f, orig_rect, 0.0f, HIDE_ANIMATION_LENGTH_MS);
}

void ElementOfInterest::ScheduleAnimation(const OpRect& origin, float origin_opacity, const OpRect& dest, float dest_opacity, int duration_in_ms)
{
	animation = OpWindowAnimation(
		OpAnimatedWindowState(origin, origin_opacity),
		OpAnimatedWindowState(dest, dest_opacity),
		duration_in_ms);

	animation_scheduled = TRUE;
	GetElementExpander()->ScheduleAnimation(this);
}

void ElementOfInterest::StartAnimation()
{
	if (animation_scheduled)
	{
		animation_running = TRUE;
		animation_scheduled = FALSE;
		window->AnimateWindow(animation);
	}
}

void ElementOfInterest::OnAnimationFinished(OpWindow* window)
{
	animation_running = FALSE;
	GetElementExpander()->OnAnimationEnded(this);
}

OP_STATUS ElementOfInterest::PaintFragments(const OpRect& paint_rect, VisualDevice* vis_dev)
{
	vis_dev->SetColor(background_color);
	vis_dev->FillRect(OpRect(LAYER_BG_PADDING, LAYER_BG_PADDING, dest_rect.width - LAYER_BG_PADDING * 2, dest_rect.height - LAYER_BG_PADDING * 2));

#ifdef SKIN_SUPPORT
	skin.Draw(vis_dev, OpRect(0, 0, dest_rect.width, dest_rect.height), &paint_rect, 0);
#endif // SKIN_SUPPORT

	OpWidget *root = widget_container->GetRoot();

/*#ifdef SKIN_SUPPORT
	// Get text color from skin instead of webpage.
	UINT32 tmp;
	g_skin_manager->GetTextColor(LAYER_ELEMENT_NAME, &tmp);
	text_color = tmp;
#endif*/
	text_color = vis_dev->GetVisibleColorOnBgColor(text_color, background_color);

	vis_dev->SetColor(text_color);
	vis_dev->SetFont(font);
	EoiPaintInfo paint_info(root, text_color, vis_dev, OpPoint(dest_rect.x, dest_rect.y), paint_rect);

	for (AnchorFragment* fragment = region.First(); fragment; fragment = fragment->Suc())
		RETURN_IF_ERROR(fragment->Paint(paint_info));

	return OpStatus::OK;
}

#endif // NEARBY_ELEMENT_DETECTION
