/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/skin/UnixWidgetPainter.h"

#include "modules/display/vis_dev.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpSlider.h"
#include "platforms/quix/skin/UnixWidgetInfo.h"
#include "platforms/quix/toolkits/ToolkitWidgetPainter.h"


UnixWidgetPainter::UnixWidgetPainter(ToolkitWidgetPainter* toolkit_painter)
  : m_toolkit_painter(toolkit_painter)
  , m_widget_info(0)
  , m_scrollbar(0)
  , m_slider(0)
{
}


UnixWidgetPainter::~UnixWidgetPainter()
{
	OP_DELETE(m_widget_info);
	OP_DELETE(m_scrollbar);
	OP_DELETE(m_slider);
}


BOOL UnixWidgetPainter::DrawSlider(const OpRect& rect, BOOL horizontal, double min, double max, double pos, BOOL highlighted, BOOL pressed_knob, OpRect& out_knob_position, OpPoint& out_start_track, OpPoint& out_end_track)
{
	// Opera 11.50: We are going to use special skinned (non-toolkit) sliders for zoom sliders
	// so we are not drawing with our painter in that case
	if (!widget || widget->GetType() == OpTypedObject::WIDGET_TYPE_ZOOM_SLIDER)
		return FALSE;

	ToolkitSlider* toolkit_slider = GetSliderForWidget(widget);

	if (!toolkit_slider ||
		OpStatus::IsError(DrawWidget(toolkit_slider, rect)))
		return IndpWidgetPainter::DrawSlider(rect, horizontal, min, max, pos, highlighted, pressed_knob, out_knob_position, out_start_track, out_end_track);

	// Update knob rectangle
	toolkit_slider->GetKnobRect(out_knob_position.x, out_knob_position.y, out_knob_position.width, out_knob_position.height);
	// Update track start and end
	toolkit_slider->GetTrackPosition(out_start_track.x, out_start_track.y, out_end_track.x, out_end_track.y);

	double scale = vd->GetScale();
	if (scale != 100.0 && scale > 0.0)
	{
		out_knob_position.x = (double(out_knob_position.x * 100) / scale) + 0.5;
		out_knob_position.y = (double(out_knob_position.y * 100) / scale) + 0.5;
		out_knob_position.width = (double(out_knob_position.width * 100) / scale) + 0.5;
		out_knob_position.height = (double(out_knob_position.height * 100) / scale) + 0.5;

		out_start_track.x = (double(out_start_track.x * 100) / scale) + 0.5;
		out_start_track.y = (double(out_start_track.y * 100) / scale) + 0.5;
		out_end_track.x = (double(out_end_track.x * 100) / scale) + 0.5;
		out_end_track.y = (double(out_end_track.y * 100) / scale) + 0.5;
	}

	return TRUE;
}


BOOL UnixWidgetPainter::DrawScrollbar(const OpRect &drawrect)
{
	ToolkitScrollbar* toolkit_scrollbar = GetScrollbarForWidget(widget);

	if (!toolkit_scrollbar ||
		OpStatus::IsError(DrawWidget(toolkit_scrollbar, drawrect)))
		return IndpWidgetPainter::DrawScrollbar(drawrect);

	// Update the knob rect if necessary
	OpRect knob_rect;
	toolkit_scrollbar->GetKnobRect(knob_rect.x, knob_rect.y, knob_rect.width, knob_rect.height);
	static_cast<OpScrollbar*>(widget)->SetKnobRect(knob_rect);

	return TRUE;
}


ToolkitScrollbar* UnixWidgetPainter::GetScrollbarForWidget(OpWidget* widget)
{
	OP_ASSERT(widget && widget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR);

	if (m_scrollbar)
		OP_DELETE(m_scrollbar);
	m_scrollbar = m_toolkit_painter->CreateScrollbar();
	if (!m_scrollbar)
		return 0;

	OpScrollbar* scrollbar = static_cast<OpScrollbar*>(widget);
	m_scrollbar->SetOrientation(scrollbar->IsHorizontal() ? ToolkitScrollbar::HORIZONTAL : ToolkitScrollbar::VERTICAL);
	m_scrollbar->SetValueAndRange(scrollbar->value, scrollbar->limit_min, scrollbar->limit_max, scrollbar->limit_visible);

	m_scrollbar->SetHitPart(ConvertHitPartToToolkit(scrollbar->GetHitPart()));
	m_scrollbar->SetHoverPart(ConvertHitPartToToolkit(scrollbar->GetHoverPart()));

	return m_scrollbar;
}

ToolkitSlider* UnixWidgetPainter::GetSliderForWidget(OpWidget* widget)
{
	OP_ASSERT(widget && (widget->GetType() == OpTypedObject::WIDGET_TYPE_ZOOM_SLIDER || widget->GetType() == OpTypedObject::WIDGET_TYPE_SLIDER));

	if (m_slider)
		OP_DELETE(m_slider);
	m_slider = m_toolkit_painter->CreateSlider();
	if (!m_slider)
		return 0;

	OpSlider* slider = static_cast<OpSlider*>(widget);

	m_slider->SetOrientation(slider->IsHorizontal() ? ToolkitSlider::HORIZONTAL : ToolkitSlider::VERTICAL);

	int value = (int)(slider->GetNumberValue()*100);
	int min = (int)(slider->GetMin()*100);
	int max = (int)(slider->GetMax()*100);

	// Opera assumes a vertical slider to have the lowest value at the bottom (from html5)
	// TODO: I assume there is property to reverse it for horizontal mode.
	if (!slider->IsHorizontal())
		value = max - value;

	m_slider->SetValueAndRange(value, min, max, slider->GetNumTickValues());

	int state = 0;
	if (slider->IsEnabled())
		state |= ToolkitSlider::ENABLED;
	if (slider->IsFocused())
		state |= ToolkitSlider::FOCUSED;
	if (slider->GetRTL())
		state |= ToolkitSlider::RTL;
	m_slider->SetState(state);

	m_slider->SetHoverPart(slider->IsHoveringKnob() ? ToolkitSlider::KNOB : ToolkitSlider::NONE);

	return m_slider;
}


ToolkitScrollbar::HitPart UnixWidgetPainter::ConvertHitPartToToolkit(SCROLLBAR_PART_CODE part)
{
	switch (part)
	{
	case LEFT_OUTSIDE_ARROW:
		return ToolkitScrollbar::ARROW_SUBTRACT;
	case RIGHT_OUTSIDE_ARROW:
		return ToolkitScrollbar::ARROW_ADD;
	case LEFT_TRACK:
		return ToolkitScrollbar::TRACK_SUBTRACT;
	case KNOB:
		return ToolkitScrollbar::KNOB;
	}

	return ToolkitScrollbar::NONE;
}


OP_STATUS UnixWidgetPainter::DrawWidget(ToolkitWidget* toolkit_widget, const OpRect& cliprect)
{
	OpRect rect(widget->GetBounds());

	rect.x += vd->GetTranslationX() - vd->GetRenderingViewX();
	rect.y += vd->GetTranslationY() - vd->GetRenderingViewY();
	rect = vd->ScaleToScreen(rect);

	OpAutoArray<UINT32> toolkit_bitmap(OP_NEWA(UINT32, rect.width * rect.height));
	if (!toolkit_bitmap.get())
		return OpStatus::ERR_NO_MEMORY;

	if (toolkit_widget->RequireBackground())
	{
		OpBitmap* background = vd->painter->CreateBitmapFromBackground(rect);
		if (!background)
			return OpStatus::ERR_NO_MEMORY;

		for (int y = 0; y < rect.height; y++)
			background->GetLineData(toolkit_bitmap.get() + y * rect.width, y);
		OP_DELETE(background);
	}

	toolkit_widget->Draw(toolkit_bitmap.get(), rect.width, rect.height);

	OpBitmap* bitmap_tmp;
	RETURN_IF_ERROR(OpBitmap::Create(&bitmap_tmp, rect.width, rect.height));
	OpAutoPtr<OpBitmap> bitmap(bitmap_tmp);

	for (int y = 0; y < rect.height; y++)
		RETURN_IF_ERROR(bitmap->AddLine(toolkit_bitmap.get() + y * rect.width, y));

	widget->SetClipRect(cliprect);
	RETURN_IF_ERROR(vd->BlitImage(bitmap.get(), OpRect(0, 0, rect.width, rect.height), rect));
	widget->RemoveClipRect();

	return OpStatus::OK;
}


OpWidgetInfo* UnixWidgetPainter::GetInfo(OpWidget* widget)
{
	if (!m_widget_info)
	{
		m_widget_info = OP_NEW(UnixWidgetInfo, (this));
		IndpWidgetPainter::GetInfo(widget);
	}

	return m_widget_info;
}
