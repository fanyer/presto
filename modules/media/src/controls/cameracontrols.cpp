/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_CAMERA_SUPPORT

#include "modules/media/src/controls/cameracontrols.h"
#include "modules/media/src/controls/mediaduration.h"
#include "modules/display/vis_dev.h"
#include "modules/skin/OpSkinManager.h"

/** Camera Button */

CameraButton::CameraButton(OpInputContext* parent_ic, OpWidgetListener* listener)
	: OpButton(TYPE_CUSTOM, STYLE_IMAGE)
{
	GetBorderSkin()->SetImage("Media Play Button");
	SetParentInputContext(parent_ic);
	SetTabStop(TRUE);
	SetHasFocusRect(TRUE);
	SetListener(listener);
};

/* virtual */ void
CameraButton::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpButton::OnPaint(widget_painter, paint_rect);

	// highlight
	if (IsFocused())
	{
		GetVisualDevice()->SetColor(200, 200, 200);

		OpRect highlight_rect(paint_rect.InsetBy(2, 2));
		GetVisualDevice()->RectangleOut(highlight_rect.x, highlight_rect.y, highlight_rect.width, highlight_rect.height, CSS_VALUE_dashed);
	}
}

/* virtual */ void
CameraButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetBorderSkin()->GetSize(w, h);
}

CameraDurationWidget::CameraDurationWidget()
	: OpButton()
	, m_recording_start(0)
	, m_recording_time(0)
	, m_time_elapsed(0)
{
	m_update_timer.SetTimerListener(this);
}

CameraDurationWidget::~CameraDurationWidget()
{
	m_update_timer.Stop();
}

/* virtual */ void
CameraDurationWidget::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	OpButton::GetPreferedSize(w, h, cols, rows);

	if (VisualDevice* visdev = GetVisualDevice())
	{
		TempBuffer time_str;
		RETURN_VOID_IF_ERROR(MediaTimeHelper::GenerateTimeString(time_str, 0, op_nan(NULL), TRUE));

		*w = visdev->GetTxtExtent(time_str.GetStorage(), time_str.Length());
	}
}

/* virtual */ void
CameraDurationWidget::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	// Background skin
	g_skin_manager->DrawElement(GetVisualDevice(), "Media Duration", paint_rect);

	TempBuffer time_str;
	RETURN_VOID_IF_ERROR(MediaTimeHelper::GenerateTimeString(time_str, (double)(m_recording_time + m_time_elapsed), op_nan(NULL)));
	RETURN_VOID_IF_ERROR(string.Set(time_str.GetStorage(), this));

	// Adjust for padding
	OpRect rect(paint_rect);
	INT32 pad_left, pad_top, pad_right, pad_bottom;
	GetBorderSkin()->GetPadding(&pad_left, &pad_top, &pad_right, &pad_bottom);
	rect.x += pad_left;
	rect.width -= pad_left + pad_right;

	UINT32 color;
	g_skin_manager->GetTextColor("Media Duration", &color);
	string.Draw(rect, GetVisualDevice(), color);
}

/* virtual */ void
CameraDurationWidget::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_update_timer)
	{
		m_time_elapsed = op_time(NULL) - m_recording_start;
		InvalidateAll();
		m_update_timer.Start(1000);
		return;
	}

	OpButton::OnTimeOut(timer);
}

void
CameraDurationWidget::StartRecordingTime()
{
	m_recording_start = op_time(NULL);
	m_time_elapsed = 0;
	m_update_timer.Start(1000);
}

void
CameraDurationWidget::PauseRecordingTime()
{
	m_recording_time += op_time(NULL) - m_recording_start;
	m_recording_start = 0;
	m_time_elapsed = 0;
	m_update_timer.Stop();
}

/** CameraControls */

CameraControls::CameraControls(CameraControlsOwner* owner)
	: OpWidget()
	, m_owner(owner)
	, m_play_pause_button(NULL)
	, m_stop_button(NULL)
	, m_duration_widget(NULL)
{
	m_camera_can_pause = owner->GetCameraSupportsPause();
}

CameraControls::~CameraControls()
{
	if (m_play_pause_button)
		m_play_pause_button->Remove();

	if (m_stop_button)
		m_stop_button->Remove();

	if (m_duration_widget)
		m_duration_widget->Remove();

	OP_DELETE(m_play_pause_button);
	OP_DELETE(m_stop_button);
	OP_DELETE(m_duration_widget);
}

OP_STATUS
CameraControls::Init()
{
	// The casts below are needed to resolve base class ambiguity in Desktop,
	// where OpWidget might inherit OpWidgetListener.
	OpAutoPtr<CameraButton>
	    pp_button  (OP_NEW(CameraButton, (this, static_cast<CameraButtonListener*>(this)))),
	    stop_button(OP_NEW(CameraButton, (this, static_cast<CameraButtonListener*>(this))));
	OpAutoPtr<CameraDurationWidget> duration_widget(OP_NEW(CameraDurationWidget, ()));

	if (pp_button.get() && stop_button.get() && duration_widget.get())
	{
		OpInputAction* pp_action = NULL;
		OpInputAction* stop_action = NULL;

		OpInputAction::CreateInputActionsFromString(UNI_L("Camera Record | Camera Pause"), pp_action);
		OpInputAction::CreateInputActionsFromString(UNI_L("Camera Stop"), stop_action);

		if (!pp_action || !stop_action)
		{
			OP_DELETE(pp_action);
			OP_DELETE(stop_action);
			return OpStatus::ERR_NOT_SUPPORTED;
		}

		m_play_pause_button = pp_button.release();
		m_stop_button = stop_button.release();
		m_duration_widget = duration_widget.release();

		m_play_pause_button->SetAction(pp_action);
		m_stop_button->SetAction(stop_action);

		AddChild(m_play_pause_button);
		AddChild(m_stop_button);

		m_duration_widget->SetTabStop(FALSE);
		AddChild(m_duration_widget);

		return OpStatus::OK;
	}

	return OpStatus::ERR_NO_MEMORY;
}

void
CameraControls::SetWidgetPosition(VisualDevice* vis_dev, const AffinePos& doc_ctm, const OpRect& rect)
{
	SetVisualDevice(vis_dev);
	SetRect(rect, FALSE);

	SetPosInDocument(doc_ctm);
}

void
CameraControls::Paint(VisualDevice* visdev, const OpRect& paint_rect)
{
	GenerateOnPaint(paint_rect);
}

void
CameraControls::GetControlArea(INT32 width, INT32 height, OpRect& area)
{
	area.x = 0;
	area.y = height - GetRect().height;
	area.width = width;
	area.height = GetRect().height;
}

/* virtual */ void
CameraControls::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	m_play_pause_button->GetPreferedSize(w, h, rows, cols);
}

/* virtual */ void
CameraControls::OnResize(INT32* new_w, INT32* new_h)
{
	// Record/Pause button
	INT32 rec_pause_width, rec_pause_height;
	m_play_pause_button->GetPreferedSize(&rec_pause_width, &rec_pause_height, 0, 0);
	m_play_pause_button->SetRect(OpRect(0, *new_h - rec_pause_height, rec_pause_width, rec_pause_height));

	INT32 stop_width = 0;
	INT32 stop_height = 0;
	// Stop button
	m_stop_button->GetPreferedSize(&stop_width, &stop_height, 0, 0);
	if (rec_pause_width + stop_width <= *new_w)
	{
		m_stop_button->SetRect(OpRect(rec_pause_width, *new_h - stop_height, stop_width, stop_height));

		INT32 duration_width = 0, dummy;
		m_duration_widget->GetPreferedSize(&duration_width, &dummy, 0,0);
		if (rec_pause_width + stop_width + duration_width <= *new_w)
			m_duration_widget->SetRect(OpRect(rec_pause_width + stop_width, *new_h - stop_height, *new_w - rec_pause_width - stop_width, stop_height));
		else
			m_duration_widget->SetVisibility(FALSE);
	}
	else
	{
		m_stop_button->SetVisibility(FALSE);	// button does not fit into preview window
		m_duration_widget->SetVisibility(FALSE);
	}
}

/* virtual */ void
CameraControls::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	// Background skin
	g_skin_manager->DrawElement(GetVisualDevice(), "Media Duration", paint_rect);
}

/* virtual */ void
CameraControls::OnClick(OpWidget* widget, UINT32 id /* = 0 */)
{
	if (widget == m_play_pause_button)
	{
		m_owner->OnMultiFuncButton();
	}
	else if (widget == m_stop_button)
	{
		m_owner->OnCameraStop();
	}
}

/* virtual */ void
CameraControls::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	m_owner->HandleFocusGained();
}

void
CameraControls::SetPaused()
{
	m_play_pause_button->ToggleAction();
	m_duration_widget->PauseRecordingTime();
}

void
CameraControls::SetRecording()
{
	if (m_camera_can_pause)
		m_play_pause_button->ToggleAction();
	else
		m_play_pause_button->SetEnabled(FALSE);

	m_duration_widget->StartRecordingTime();
}


#endif // MEDIA_CAMERA_SUPPORT
