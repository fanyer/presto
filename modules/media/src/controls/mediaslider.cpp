/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_PLAYER_SUPPORT
#ifndef MEDIA_SIMPLE_CONTROLS

#include "modules/media/src/controls/mediacontrols.h"
#include "modules/media/src/controls/mediaslider.h"

#include "modules/display/vis_dev.h"
#include "modules/skin/OpSkinManager.h"

/* static */ OP_STATUS
MediaSlider::Create(MediaSlider*& slider, MediaSliderType slider_type)
{
	if ((slider = OP_NEW(MediaSlider, (slider_type))))
		return OpStatus::OK;
	return OpStatus::ERR_NO_MEMORY;
}

MediaSlider::MediaSlider(MediaSliderType slider_type)
	: m_slider_type(slider_type),
	  m_min(0),
	  m_max(0),
	  m_current(0),
	  m_is_dragging(FALSE)
{}

/* virtual */ BOOL
MediaSlider::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
#ifdef ACTION_MEDIA_SEEK_FORWARD_ENABLED
		case OpInputAction::ACTION_MEDIA_SEEK_FORWARD:
		{
			SetValue(m_current + action->GetActionData());
			HandleOnChange(FALSE);
			return TRUE;
		}
#endif // ACTION_MEDIA_SEEK_FORWARD_ENABLED
#ifdef ACTION_MEDIA_SEEK_BACKWARD_ENABLED
		case OpInputAction::ACTION_MEDIA_SEEK_BACKWARD:
		{
			SetValue(m_current - action->GetActionData());
			HandleOnChange(FALSE);
			return TRUE;
		}
#endif // ACTION_MEDIA_SEEK_BACKWARD_ENABLED
	}
	return FALSE;
}

/* virtual */ void
MediaSlider::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	// Abuse the skin system to draw a background color for the
	// unbuffered ranges, then the buffered ranges and finally the
	// actual seek track and knob on top of that.

	VisualDevice* visdev = GetVisualDevice();

	UINT32 color = 0;
	OpStatus::Ignore(g_skin_manager->GetBackgroundColor("Media Seek Background", &color));
	visdev->SetColor(color);
	visdev->FillRect(paint_rect);

	PaintBufferedRanges(paint_rect);

	g_skin_manager->DrawElement(visdev, GetTrackSkin(), paint_rect);

	OpRect knob_rect;
	if (CalcKnobRect(knob_rect))
	{
		g_skin_manager->DrawElement(visdev, GetKnobSkin(), knob_rect);

		if (IsFocused())
		{
			GetVisualDevice()->SetColor(200, 200, 200);
			GetVisualDevice()->RectangleOut(knob_rect.x, knob_rect.y, knob_rect.width, knob_rect.height, CSS_VALUE_dashed);
		}
	}
}

/* virtual */ void
MediaSlider::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	g_skin_manager->GetSize(GetTrackSkin(), w, h);
}

/* virtual */ void
MediaSlider::OnMouseMove(const OpPoint &point)
{
	if (m_is_dragging)
	{
		SetValue(PointToOffset(point));
		HandleOnChange(TRUE);
	}
}

/* virtual */ void
MediaSlider::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1)
	{
		m_is_dragging = TRUE;
		if (!IsPointInKnob(point))
		{
			SetValue(PointToOffset(point));
			HandleOnChange(TRUE);
		}
	}
}

/* virtual */ void
MediaSlider::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (m_is_dragging)
	{
		m_is_dragging = FALSE;
		HandleOnChange(TRUE);
	}
}

BOOL
MediaSlider::IsPointInKnob(const OpPoint &point) const
{
	OpRect rect;
	if (CalcKnobRect(rect))
		return rect.Contains(point);
	return FALSE;
}

bool
MediaSlider::CalcKnobRect(OpRect& res) const
{
	if (!op_isfinite(m_max) || m_max <= 0)
		return false;

	OpSkinElement* element = g_skin_manager->GetSkinElement(GetKnobSkin());
	if (!element)
		return false;

	INT32 width, height, pad_left, pad_right, pad_top, pad_bottom;
	if (OpStatus::IsError(element->GetSize(&width, &height, 0)) ||
		OpStatus::IsError(element->GetPadding(&pad_left, &pad_top, &pad_right, &pad_bottom, 0)))
		return false;

	res.width = width;
	res.height = height;

	const OpRect r = const_cast<MediaSlider*>(this)->GetBounds();
	const double relative_offset = m_current / (m_max - m_min);
	if (IsHorizontal())
	{
		res.x = pad_left + (INT32)(relative_offset * (r.width - pad_left - pad_right)) - res.width/2;
		res.y = pad_top + r.height / 2 - res.height;
	}
	else
	{
		res.x = pad_left + r.width / 2 - res.width;
		res.y = pad_top + (INT32)((1.0 - relative_offset) * (r.height - pad_top - pad_bottom)) - res.height/2;
	}

	return true;
}

OpRect
MediaSlider::CalcRangeRect(const OpRect& bounds, double start, double end) const
{
	const double size = m_max - m_min;

	// Prevent division by zero and negative values.
	if (size <= 0)
		return OpRect();

	// Clamp values to [m_min, m_max].
	start = MAX(start, m_min);
	end = MIN(end, m_max);

	const double start_offset = (start - m_min) / size;
	const double end_offset = (end - m_min) / size;

	// Round towards the center of the interval. (See documentation of
	// function declaration).
	const INT32 x0 = (INT32)op_ceil(start_offset * bounds.width);
	const INT32 x1 = (INT32)op_floor(end_offset * bounds.width);

	// Due to rounding, very small ranges can end up producing negative
	// rectangle width. Don't allow that.
	const INT32 width = MAX(0, x1 - x0);

	return OpRect(bounds.x + x0, bounds.y, width, bounds.height);
}

double
MediaSlider::PointToOffset(const OpPoint& point)
{
	INT32 pad_left, pad_right, pad_top, pad_bottom;
	OpStatus::Ignore(g_skin_manager->GetPadding(GetTrackSkin(), &pad_left, &pad_top, &pad_right, &pad_bottom));

	double offset = 0;
	if (IsHorizontal())
	{
		offset = (double)(point.x - pad_left) / (double)(GetBounds().width - pad_left - pad_right);
	}
	else
	{
		offset = 1.0 - (double)(point.y - pad_bottom) / (double)(GetBounds().height - pad_top - pad_bottom);
	}

	// Clamp to [0,1]
	if (offset < 0)
		offset = 0;
	else if (offset > 1)
		offset = 1;

	return (m_max - m_min) * offset;
}

void
MediaSlider::SetValue(double val, BOOL invalidate)
{
	if (m_current != val)
	{
		m_current = val;
		if (invalidate)
			InvalidateAll();
	}
}

void
MediaSlider::HandleOnChange(BOOL changed_by_mouse)
{
	if (listener)
		listener->OnChange(this, changed_by_mouse);
}

void
MediaSlider::PaintBufferedRanges(const OpRect& paint_rect)
{
	VisualDevice* visdev = GetVisualDevice();

	if (!IsHorizontal())
		return;

	UINT32 color = 0;
	OpStatus::Ignore(g_skin_manager->GetBackgroundColor("Media Seek Buffered", &color));
	visdev->SetColor(color);

	UINT32 length = m_buffered_ranges.Length();
	for (UINT32 i = 0; i < length; i++)
	{
		double start = m_buffered_ranges.Start(i);
		double end = m_buffered_ranges.End(i);

		OpRect progress = CalcRangeRect(paint_rect, start, end);
		visdev->FillRect(progress);
	}
}

OpRect
VolumeSliderContainer::CalculateRect()
{
	VisualDevice* vd = m_controls->GetVisualDevice();

	GetWidgetContainer()->GetVisualDevice()->SetScale(vd->GetScale(), FALSE);

	INT32 w, h;
	m_slider->GetPreferedSize(&w, &h, 0, 0);

	AffinePos p = m_controls->GetPosInDocument();

	OpPoint preferred_pos(m_controls->GetWidth() - w, 0);
	p.Apply(preferred_pos);

	OpRect preferred_rect(preferred_pos.x, preferred_pos.y - h, w, h);

	// Translate from document to view coordinates
	preferred_rect.OffsetBy(-vd->GetRenderingViewX(), -vd->GetRenderingViewY());

	// Scale to screen
	preferred_rect = vd->ScaleToScreen(preferred_rect);

	// Get position of upper left corner of the view
	OpPoint pos = m_controls->GetVisualDevice()->view->ConvertToScreen(OpPoint(0, 0));

	preferred_rect.OffsetBy(pos);

	if (preferred_rect.y < pos.y)
		preferred_rect.y = pos.y;

	return preferred_rect;
}

void
VolumeSliderContainer::UpdatePosition()
{
	int m_scale = m_slider->GetVisualDevice()->GetScale();
	GetWidgetContainer()->GetRoot()->GetVisualDevice()->SetScale(m_scale, FALSE);

	OpRect rect = CalculateRect();
	m_window->SetOuterPos(rect.x, rect.y);
	m_window->SetInnerSize(rect.width, rect.height);
}

void
VolumeSliderContainer::OnResize(UINT32 width, UINT32 height)
{
	WidgetWindow::OnResize(width, height);

	int m_scale = m_controls->GetVisualDevice()->GetScale();
	m_slider->SetRect(OpRect(0, 0,
							width * 100 / m_scale,
							height * 100 / m_scale));
}

OP_STATUS
VolumeSliderContainer::Init()
{
	RETURN_IF_ERROR(WidgetWindow::Init(OpWindow::STYLE_POPUP,
									   m_controls->GetParentOpWindow(),
									   0, OpWindow::EFFECT_TRANSPARENT));

	RETURN_IF_ERROR(MediaSlider::Create(m_slider, MediaSlider::Volume));
	GetWidgetContainer()->GetRoot()->GetVisualDevice()->SetScale(100);
	GetWidgetContainer()->GetRoot()->AddChild(m_slider);

	m_slider->SetParentInputContext(m_controls);
	m_slider->SetListener(static_cast<MediaButtonListener*>(m_controls));
	m_slider->SetMaxValue(1.0);

	return OpStatus::OK;
}

/* static */ OP_STATUS
VolumeSliderContainer::Create(VolumeSliderContainer*& container, MediaControls* controls)
{
	OpAutoPtr<VolumeSliderContainer> container_safe(OP_NEW(VolumeSliderContainer, (controls)));
	if (container_safe.get())
	{
		RETURN_IF_ERROR(container_safe->Init());
		container = container_safe.release();
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_MEMORY;
}

VolumeSliderContainer::~VolumeSliderContainer()
{
	m_controls->DetachVolumeSlider();
}

#endif // !MEDIA_SIMPLE_CONTROLS
#endif // MEDIA_PLAYER_SUPPORT
