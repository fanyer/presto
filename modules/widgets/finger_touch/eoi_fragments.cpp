/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/widgets/finger_touch/eoi_fragments.h"

#include "modules/display/vis_dev.h"
#include "modules/widgets/finger_touch/element_expander_impl.h"
#include "modules/widgets/finger_touch/element_of_interest.h"
#include "modules/widgets/finger_touch/fingertouch_config.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpWidgetStringDrawer.h"

void TextAnchorFragment::SetRect(const OpRect& r, int internal_y)
{
	AnchorFragment::SetRect(r, internal_y);
	if (multi_edit && internal_y)
	{
		OpRect multi_edit_rect = multi_edit->GetRect();
		multi_edit->SetRect(OpRect(LAYER_PADDING, LAYER_PADDING + internal_y, multi_edit_rect.width, multi_edit_rect.height));
	}
}

void TextAnchorFragment::OffsetPosBy(int dx, int dy)
{
	AnchorFragment::OffsetPosBy(dx, dy);
	if(multi_edit)
	{
		OpRect multi_edit_rect = multi_edit->GetRect();
		multi_edit_rect.OffsetBy(dx, dy);
		multi_edit->SetRect(multi_edit_rect);
	}
}

int TextAnchorFragment::GetWidth()
{
	if (multi_edit)
		return multi_edit->GetBounds().width;
	return max_width ? MIN(max_width, widgetstring.GetWidth()) : widgetstring.GetWidth();
}

int TextAnchorFragment::GetHeight()
{
	if (multi_edit)
		return multi_edit->GetBounds().height;
	return widgetstring.GetHeight();
}

OP_STATUS TextAnchorFragment::UpdateWidgetString(OpWidget *widget, int max_width, BOOL wrap)
{
	this->max_width = max_width;
	if (!multi_edit && wrap)
	{
		RETURN_IF_ERROR(OpMultilineEdit::Construct(&multi_edit));
		widget->AddChild(multi_edit);
		multi_edit->SetLabelMode();
		multi_edit->SetFontInfo(widget->font_info.font_info,
			widget->font_info.size, widget->font_info.italic,
			widget->font_info.weight, widget->font_info.justify,
			widget->font_info.char_spacing_extra);
		multi_edit->SetIgnoresMouse(TRUE);

		multi_edit->SetRect(OpRect(LAYER_PADDING, LAYER_PADDING, max_width, 10));
		RETURN_IF_ERROR(multi_edit->SetText(text.CStr()));
		multi_edit->SetRect(OpRect(LAYER_PADDING, LAYER_PADDING, multi_edit->GetContentWidth() + multi_edit->GetPaddingLeft() + multi_edit->GetPaddingRight(),
			multi_edit->GetContentHeight() + multi_edit->GetPaddingTop() + multi_edit->GetPaddingBottom()));

		// Update fragments size according to the textarea
		rect.width = multi_edit->GetBounds().width;
		rect.height = multi_edit->GetBounds().height;
	}
	else if (!wrap)
	{
		if (!widgetstring.GetWidth())
			return widgetstring.Set(text.CStr(), text.Length(), widget);
	}
	//widgetstring.Reset(widget);
	return OpStatus::OK;
}

OP_STATUS TextAnchorFragment::Paint(EoiPaintInfo& info)
{
	if (multi_edit)
		multi_edit->SetForegroundColor(info.text_color);
	if (widgetstring.Get())
	{
		BOOL underline = FALSE;
		OpWidgetStringDrawer drawer;
		drawer.SetEllipsisPosition(ELLIPSIS_END);
		drawer.SetUnderline(underline);
		drawer.Draw(&widgetstring, OpRect(x, y, max_width ? max_width : 10000, widgetstring.GetHeight()),
			info.vis_dev, info.text_color);
	}
	return OpStatus::OK;
}

// == ImageAnchorFragment ===========================================

OP_STATUS ImageAnchorFragment::Paint(EoiPaintInfo& info)
{
	if (img.Width() && img.Height())
		info.vis_dev->ImageOut(img,
						  OpRect(0, 0, img.Width(), img.Height()),
						  OpRect(x + LAYER_PADDING, y + LAYER_PADDING, rect.width, rect.height));

	return OpStatus::OK;
}

#endif // NEARBY_ELEMENT_DETECTION
