/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef CAMERA_CONTROLS_H
#define CAMERA_CONTROLS_H

#ifdef MEDIA_CAMERA_SUPPORT

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpButton.h"

class CameraControlsOwner
{
public:
	virtual void OnMultiFuncButton() = 0;
	virtual void OnCameraStop() = 0;
	virtual void HandleFocusGained() = 0;
	virtual BOOL GetCameraSupportsPause() = 0;
};

class CameraButton : public OpButton
{
public:
	CameraButton(OpInputContext* parent_ic, OpWidgetListener* listener);

	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	virtual void SetAction(OpInputAction* action) { OpButton::SetAction(action); OnUpdateActionState(); }
	virtual void OnUpdateActionState()
	{
		GetForegroundSkin()->SetImage(m_action_to_use->GetActionImage());
	}

	virtual void ToggleAction()
	{
		OP_ASSERT(GetAction());

		OpInputAction* first = GetAction();
		OpInputAction* second = first->GetNextInputAction();

		if (second)
			m_action_to_use = m_action_to_use != first ? first  : second;

		OnUpdateActionState();
	}
};

class CameraDurationWidget : public OpButton
{
public:
	CameraDurationWidget();
	~CameraDurationWidget();
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void OnTimeOut(OpTimer* timer);
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	void StartRecordingTime();
	void PauseRecordingTime();

private:
	time_t		m_recording_start;
	time_t		m_recording_time;
	time_t		m_time_elapsed;

	OpTimer		m_update_timer;

	OpWidgetString string;
};

/** Dummy wrapper to avoid ambiguity when OpWidget itself inherits from
 *  OpWidgetListener, as it does on Desktop (evenes). */
class CameraButtonListener : public OpWidgetListener
{
public:
	virtual ~CameraButtonListener() {}
};

class CameraControls : public OpWidget, public CameraButtonListener
{
public:
	CameraControls(CameraControlsOwner* owner);
	~CameraControls();

	OP_STATUS Init();
	void SetWidgetPosition(VisualDevice* vis_dev, const AffinePos& doc_ctm, const OpRect& rect);
	void Paint(VisualDevice* visdev, const OpRect& paint_rect);
	void GetControlArea(INT32 width, INT32 height, OpRect& area);

	// OpWidget overrides
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	virtual void OnResize(INT32* new_w, INT32* new_h);
	virtual void OnClick(OpWidget* widget, UINT32 id = 0);
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

	// OpInputContext overrides
	virtual const char*	GetInputContextName() { return "Camera Controls"; }
	virtual void OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

	// OpWidgetListener overrides
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}

	void SetPaused();
	void SetRecording();

private:
	CameraControlsOwner* m_owner;

	CameraButton			*m_play_pause_button;
	CameraButton			*m_stop_button;
	CameraDurationWidget	*m_duration_widget;

	BOOL					m_camera_can_pause;
	//We may optionaly require also following buttons: zoom_in, zoom_out, resolution, light
	//TODO: add optional buttons here (if needed)
};

#endif // MEDIA_CAMERA_SUPPORT

#endif // CAMERA_CONTROLS_H
