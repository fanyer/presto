/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/media/src/controls/mediacontrols.h"

#include "modules/display/vis_dev.h"
#include "modules/media/src/controls/mediaduration.h"
#include "modules/media/src/controls/mediaslider.h"
#include "modules/media/src/controls/mediavolume.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/WidgetWindow.h"

/* static */ OP_STATUS
MediaButton::Create(MediaButton*& button)
{
	if ((button = OP_NEW(MediaButton, ())))
	{
		button->GetBorderSkin()->SetImage("Media Button");
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_MEMORY;
}

/* virtual */ void
MediaButton::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpButton::OnPaint(widget_painter, paint_rect);

#ifndef MEDIA_SIMPLE_CONTROLS
	// draw focus rect
	if (IsFocused())
	{
		GetVisualDevice()->SetColor(200, 200, 200);

		OpRect knob_rect(paint_rect.InsetBy(2, 2));
		GetVisualDevice()->RectangleOut(knob_rect.x, knob_rect.y, knob_rect.width, knob_rect.height, CSS_VALUE_dashed);
	}
#endif // !MEDIA_SIMPLE_CONTROLS
}

/* virtual */ void
MediaButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetBorderSkin()->GetSize(w, h);
}

void
MediaButton::SetPaused(BOOL paused)
{
	const char* fg_skin = paused ? "Media Play" : "Media Pause";
	GetForegroundSkin()->SetImage(fg_skin);
}

void
MediaButton::SetVolume(double volume)
{
	OP_ASSERT(volume >= 0 && volume <= 1);

	unsigned part = 0;
	if (volume > 0 && IsEnabled())
	{
		// Map volume to a skin part such that 0.0 is part 0, 1.0 is
		// part (parts-1) and the other parts are spread in between.
		const unsigned parts = g_media_module.GetVolumeSkinCount();
		if (parts >= 2)
		{
			if (volume == 1)
				part = parts - 1;
			else if (volume > 0)
				part = static_cast<unsigned>(volume * (parts - 2)) + 1;
			// else part = 0
		}
	}

	VolumeString volstr;
	GetForegroundSkin()->SetImage(volstr.Get(part));
}

OP_STATUS MediaControls::Create(MediaControls*& controls, MediaControlsOwner* owner)
{
	OpAutoPtr<MediaControls> controls_safe(OP_NEW(MediaControls, (owner)));
	if (controls_safe.get())
	{
		RETURN_IF_ERROR(controls_safe->Init());
		controls = controls_safe.release();
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_MEMORY;
}

MediaControls::MediaControls(MediaControlsOwner* owner) :
	   m_owner(owner),
	   m_play_pause(NULL),
	   m_fader(this),
#ifndef MEDIA_SIMPLE_CONTROLS
	   m_volume(NULL),
	   m_slider(NULL),
	   m_duration(NULL),
	   m_volume_slider_container(NULL),
	   m_volume_fader(this),
	   m_has_keyboard_focus(FALSE),
#endif // !MEDIA_SIMPLE_CONTROLS
	   m_opacity(0),
	   m_hover(FALSE)
{}

OP_STATUS MediaControls::Init()
{
	GetBorderSkin()->SetImage("Media Controls");
	SetSkinned(TRUE);
	SetOnMoveWanted(TRUE);

	// Play/pause button
	RETURN_IF_ERROR(MediaButton::Create(m_play_pause));
	m_play_pause->SetListener(static_cast<MediaButtonListener*>(this));
	m_play_pause->SetParentInputContext(this);
#ifndef MEDIA_SIMPLE_CONTROLS
	m_play_pause->SetTabStop(TRUE);
	m_play_pause->SetHasFocusRect(TRUE);
#endif // !MEDIA_SIMPLE_CONTROLS
	AddChild(m_play_pause);

#ifndef MEDIA_SIMPLE_CONTROLS
	// Progress slider
	RETURN_IF_ERROR(MediaSlider::Create(m_slider, MediaSlider::Progress));
	m_slider->SetParentInputContext(this);
	m_slider->SetListener(static_cast<MediaButtonListener*>(this));
	m_slider->SetTabStop(TRUE);
	m_slider->SetMaxValue(m_owner->GetMediaDuration());
	m_slider->SetValue(m_owner->GetMediaPosition());
	AddChild(m_slider);

	// Volume button
	RETURN_IF_ERROR(MediaButton::Create(m_volume));
	m_volume->SetParentInputContext(this);
	m_volume->SetListener(static_cast<MediaButtonListener*>(this));
	m_volume->SetTabStop(TRUE);
	m_volume->SetHasFocusRect(TRUE);
	AddChild(m_volume);

	// Played/total time
	RETURN_IF_ERROR(MediaDuration::Create(m_duration));
	m_duration->SetDuration(m_owner->GetMediaDuration());
	AddChild(m_duration);
#endif // !MEDIA_SIMPLE_CONTROLS

	// Sync all state
	OnPlayPause();
	OnTimeUpdate(FALSE);
	OnDurationChange();
	OnVolumeChange();

	return OpStatus::OK;
}

MediaControls::~MediaControls()
{
	if (m_play_pause)
	{
		m_play_pause->Remove();
		OP_DELETE(m_play_pause);
	}

#ifndef MEDIA_SIMPLE_CONTROLS
	if (m_volume)
	{
		m_volume->Remove();
		OP_DELETE(m_volume);
	}

	if (m_slider)
	{
		m_slider->Remove();
		OP_DELETE(m_slider);
	}

	OP_DELETE(m_volume_slider_container);
#endif // !MEDIA_SIMPLE_CONTROLS
}

#ifndef MEDIA_SIMPLE_CONTROLS

/* virtual */ BOOL
MediaControls::OnInputAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION)
	{
		OpInputAction* child_action = action->GetChildAction();

		if (child_action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED)
		{
			OpInputContext* context = child_action->GetFirstInputContext();
			OpKey::Code key = child_action->GetActionKeyCode();
			ShiftKeyState shift = child_action->GetShiftKeys();
#if defined(OP_KEY_UP_ENABLED) && defined(OP_KEY_DOWN_ENABLED)
			// volume/muted control:
			if (context == m_volume &&
				(key == OP_KEY_UP || key == OP_KEY_DOWN) &&
				(shift == SHIFTKEY_NONE || shift == SHIFTKEY_CTRL))
			{
				if (shift == SHIFTKEY_NONE)
				{
					// up/down increases/decreases volume
					double v = m_owner->GetMediaVolume();
					if (key == OP_KEY_UP)
						v = MIN(1.0, v + 0.1);
					else
						v = MAX(0.0, v - 0.1);
					// unmute if necessary
					if (v > 0)
						m_owner->SetMediaMuted(FALSE);
					m_owner->SetMediaVolume(v);
				}
				else
				{
					// ctrl+down/up mutes/unmutes
					m_owner->SetMediaMuted(key == OP_KEY_DOWN);
				}
				return TRUE;
			}
#endif // defined(OP_KEY_UP_ENABLED) && defined(OP_KEY_DOWN_ENABLED)
			// seeking control
			if (context == m_slider)
			{
#if defined(OP_KEY_LEFT_ENABLED) && defined(OP_KEY_RIGHT_ENABLED)
				// relative seeking
				if ((key == OP_KEY_LEFT || key == OP_KEY_RIGHT) &&
					(shift == SHIFTKEY_NONE || shift == SHIFTKEY_CTRL))
				{
					double pos = m_owner->GetMediaPosition();
					// left/right seeks 15 seconds
					double step = 15;
					if (shift == SHIFTKEY_CTRL)
					{
						// ctrl+left/right seeks 10%
						double dur = m_owner->GetMediaDuration();
						if (op_isfinite(dur))
							step = dur/10.0;
					}
					if (key == OP_KEY_LEFT)
						pos -= step;
					else
						pos += step;
					m_owner->SetMediaPosition(pos);
					return TRUE;
				}
#endif // defined(OP_KEY_LEFT_ENABLED) && defined(OP_KEY_RIGHT_ENABLED)
#if defined(OP_KEY_HOME_ENABLED) && defined(OP_KEY_END_ENABLED)
				// absolute seeking
				if (shift == SHIFTKEY_NONE)
				{
					if (key == OP_KEY_HOME)
					{
						m_owner->SetMediaPosition(0);
						return TRUE;
					}
					else if (key == OP_KEY_END)
					{
						double dur = m_owner->GetMediaDuration();
						if (op_isfinite(dur))
							m_owner->SetMediaPosition(dur);
						return TRUE;
					}
				}
#endif // defined(OP_KEY_HOME_ENABLED) && defined(OP_KEY_END_ENABLED)
			}
		}
	}

	return OpWidget::OnInputAction(action);
}

/* virtual */ void
MediaControls::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if (reason == FOCUS_REASON_KEYBOARD)
	{
		OpWidget* child = GetFirstChild();
		while (child)
		{
			if (child->IsTabStop() && child == new_input_context)
				m_has_keyboard_focus = TRUE; // One of our children focus

			child = (OpWidget*)child->Suc();
		}
	}

	m_owner->HandleFocusGained();

	EnsureVisibility();

	// Show volume slider if the volume button gains focus
	if (new_input_context == m_volume)
		Show(m_volume_fader);
	else
		Hide(m_volume_fader);
}

/* virtual */ void
MediaControls::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	BOOL focus_lost = TRUE;
	OpWidget* child = GetFirstChild();
	while (child)
	{
		if (child->IsTabStop() && child == new_input_context)
			focus_lost = FALSE; // We still have focus on one of the children.

		child = (OpWidget*)child->Suc();
	}

	if (m_volume_slider_container && m_volume_slider_container->Contains(new_input_context))
	{
		focus_lost = FALSE;
	}

	m_has_keyboard_focus = !focus_lost;

	EnsureVisibility();

	if (new_input_context != m_volume)
		Hide(m_volume_fader);
}

#endif // !MEDIA_SIMPLE_CONTROLS

/** Get the width of a widget.
 *
 * Falls back to container height (for square buttons) if there
 * is no preferred width. Never exceeds container width.
 */
static INT32 WidgetWidth(OpWidget* widget, OpRect& container)
{
	INT32 width, height;
	widget->GetPreferedSize(&width, &height, 0, 0);
	if (width <= 0)
		width = container.height;
	return MIN(width, container.width);
}

/** Layout the widget to the left in the container. */
static void LayoutLeft(OpWidget* widget, OpRect& container)
{
	INT32 width = WidgetWidth(widget, container);
	OpRect rect(container.x, container.y, width, container.height);
	widget->SetRect(rect);
	// shrink container
	container.x += rect.width;
	container.width -= rect.width;
}

#ifndef MEDIA_SIMPLE_CONTROLS
/** Layout the widget to the right in the container. */
static void LayoutRight(OpWidget* widget, OpRect& container)
{
	if (!widget->IsVisible())
		return;

	INT32 width = WidgetWidth(widget, container);
	OpRect rect(container.Right() - width, container.y, width, container.height);
	widget->SetRect(rect);
	// shrink container
	container.width -= rect.width;
}
#endif // !MEDIA_SIMPLE_CONTROLS

/* virtual */
void MediaControls::OnResize(INT32* new_w, INT32* new_h)
{
	OpRect container(0, 0, *new_w, *new_h);
	if (!container.IsEmpty())
	{
		LayoutLeft(m_play_pause, container);
#ifndef MEDIA_SIMPLE_CONTROLS
		LayoutRight(m_volume, container);
		LayoutRight(m_duration, container);
		// fill the rest with the slider
		m_slider->SetRect(container);
#endif // !MEDIA_SIMPLE_CONTROLS
	}
}

/* virtual */ void
MediaControls::OnClick(OpWidget* widget, UINT32 id /* = 0 */)
{
	if (widget == m_play_pause)
	{
		if (m_owner->GetMediaPaused())
			m_owner->MediaPlay();
		else
			m_owner->MediaPause();
	}
#ifndef MEDIA_SIMPLE_CONTROLS
	else if (widget == m_volume)
	{
		m_owner->SetMediaMuted(!m_owner->GetMediaMuted());
	}
#endif // !MEDIA_SIMPLE_CONTROLS
}

/* virtual */ void
MediaControls::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetBorderSkin()->GetSize(w, h);
}

#ifndef MEDIA_SIMPLE_CONTROLS

void
MediaControls::OnChange(OpWidget* widget, BOOL changed_by_mouse)
{
	if (widget == m_slider)
	{
		if (m_slider->IsDragging())
			m_duration->SetPosition(m_slider->GetLastFixedValue());
		else
			m_owner->SetMediaPosition(m_slider->GetLastFixedValue());
	}
	else if (m_volume_slider_container && m_volume_slider_container->Contains(widget))
	{
		double v = m_volume_slider_container->GetCurrentValue();
		// unmute if necessary
		if (v > 0)
			m_owner->SetMediaMuted(FALSE);
		m_owner->SetMediaVolume(v);
	}
}

/* virtual */ void
MediaControls::OnMouseMove(OpWidget* widget, const OpPoint &point)
{
	if (widget == m_volume || (m_volume_slider_container && m_volume_slider_container->Contains(widget)))
	{
		Show(m_volume_fader);
	}
}

/* virtual */ void
MediaControls::OnMouseLeave(OpWidget* widget)
{
	if (widget == m_volume || (m_volume_slider_container && m_volume_slider_container->Contains(widget)))
	{
		Hide(m_volume_fader);
	}
}

/* virtual */ void
MediaControls::OnMove()
{
	if (m_volume_slider_container)
		m_volume_slider_container->UpdatePosition();
}

#endif // !MEDIA_SIMPLE_CONTROLS

void
MediaControls::Paint(VisualDevice* visdev, const OpRect& content)
{
	if (GetVisualDevice() != visdev)
	{
		SetVisualDevice(visdev);
		SetParentInputContext(visdev);
	}

	const OpRect controls = GetControlArea(content);
	visdev->Translate(controls.x, controls.y);
	const OpRect paint_rect = OpRect(0, 0, controls.width, controls.height);
	SetRect(paint_rect, FALSE);
	SetPosInDocument(visdev->GetCTM());
	PaintInternal(visdev, paint_rect);
	visdev->Translate(-controls.x, -controls.y);
}

void
MediaControls::PaintInternal(VisualDevice* visdev, const OpRect& paint_rect)
{
	OP_ASSERT(paint_rect.Equals(GetBounds()));

	if (m_opacity == 0)
		return;

#ifndef MEDIA_SIMPLE_CONTROLS
	if (m_volume_slider_container)
		m_volume_slider_container->UpdatePosition();
#endif // !MEDIA_SIMPLE_CONTROLS

	OP_STATUS err = OpStatus::OK;
	BOOL use_opacity = m_owner->UseControlsOpacity() && m_opacity < 255;

	if (use_opacity)
		err = visdev->BeginOpacity(paint_rect, m_opacity);

	GenerateOnPaint(paint_rect);

	if (use_opacity && OpStatus::IsSuccess(err))
		visdev->EndOpacity();
}

OpRect
MediaControls::GetControlArea(const OpRect& content)
{
	OpRect controls = content;
	INT32 width, height;
	GetPreferedSize(&width, &height, 0, 0);
#ifdef MEDIA_SIMPLE_CONTROLS
	// Center controls in content rect
	if (content.width > width)
	{
		controls.x += (content.width - width) / 2;
		controls.width = width;
	}
	if (content.height > height)
	{
		controls.y += (content.height - height) / 2;
		controls.height = height;
	}
#else
	// Position controls at bottom of content rect
	if (content.height > height)
	{
		controls.y += content.height - height;
		controls.height = height;
	}
#endif // MEDIA_SIMPLE_CONTROLS
	return controls;
}

void
MediaControls::OnOpacityChange(MediaControlsFader* fader, UINT8 opacity)
{
	if (fader == &m_fader)
	{
		m_opacity = opacity;
		InvalidateAll();
	}
#ifndef MEDIA_SIMPLE_CONTROLS
	else
	{
		if (m_volume_slider_container)
			m_volume_slider_container->SetOpacity(opacity);
	}
#endif // MEDIA_SIMPLE_CONTROLS
}

void
MediaControls::OnVisibilityChange(MediaControlsFader* fader, BOOL visible)
{
#ifndef MEDIA_SIMPLE_CONTROLS
	if (fader == &m_volume_fader)
	{
		if (visible)
		{
			if (!m_volume_slider_container)
			{
				RETURN_VOID_IF_ERROR(VolumeSliderContainer::Create(m_volume_slider_container, this));
				m_volume_slider_container->SetVolume(m_owner->GetMediaMuted() ? 0 : m_owner->GetMediaVolume());
			}

			OpRect rect = m_volume_slider_container->CalculateRect();
			m_volume_slider_container->Show(TRUE, &rect);
		}
		else
		{
			if (m_volume_slider_container)
			{
				m_volume_slider_container->Close(FALSE, TRUE);
				m_volume_slider_container = NULL;
			}
		}
	}
	else if (fader == &m_fader)
	{
		SetVisibility(visible);
		m_owner->SetControlsVisible(!!visible);
	}
#endif // MEDIA_SIMPLE_CONTROLS
}

void
MediaControls::OnElementMouseOut()
{
	m_hover = FALSE;
	EnsureVisibility();
}

void
MediaControls::OnElementMouseOver()
{
	m_hover = TRUE;
	EnsureVisibility();
}

void
MediaControls::OnPlayPause()
{
	m_play_pause->SetPaused(m_owner->GetMediaPaused());
	m_play_pause->InvalidateAll();
}

void
MediaControls::OnDurationChange()
{
#ifndef MEDIA_SIMPLE_CONTROLS
	m_slider->SetMaxValue(m_owner->GetMediaDuration());
	m_duration->SetDuration(m_owner->GetMediaDuration());
	OpRect rect = GetBounds();
	OnResize(&rect.width, &rect.height);
	InvalidateAll();
#endif // MEDIA_SIMPLE_CONTROLS
}

void
MediaControls::OnTimeUpdate(BOOL invalidate)
{
#ifndef MEDIA_SIMPLE_CONTROLS
	if (!m_slider->IsDragging())
	{
		m_slider->SetValue(m_owner->GetMediaPosition(), m_opacity > 0);
		m_duration->SetPosition(m_owner->GetMediaPosition(), m_opacity > 0);
		if (invalidate && IsShowing())
			InvalidateAll();
	}
#endif // MEDIA_SIMPLE_CONTROLS
}

void
MediaControls::OnVolumeChange()
{
#ifndef MEDIA_SIMPLE_CONTROLS
	double volume = m_owner->GetMediaMuted() ? 0 : m_owner->GetMediaVolume();
	m_volume->SetVolume(volume);
	m_volume->InvalidateAll();
	if (m_volume_slider_container)
	{
		m_volume_slider_container->SetVolume(volume);
		m_volume_slider_container->Invalidate();
	}
#endif // MEDIA_SIMPLE_CONTROLS
}

void
MediaControls::OnBufferedRangesChange()
{
#ifndef MEDIA_SIMPLE_CONTROLS
	const OpMediaTimeRanges* ranges = m_owner->GetMediaBufferedTimeRanges();

	if (ranges)
	{
		m_slider->SetBufferedRanges(ranges);
		m_slider->InvalidateAll();
	}
#endif // MEDIA_SIMPLE_CONTROLS
}

bool
MediaControls::IsShowing()
{
#ifdef MEDIA_FORCE_VIDEO_CONTROLS
	return true;
#else
	return m_hover ||
#ifndef MEDIA_SIMPLE_CONTROLS
		m_has_keyboard_focus ||
		(m_slider && m_slider->IsDragging()) ||
		(m_volume_slider_container &&
		 m_volume_slider_container->IsDragging()) ||
#endif // MEDIA_SIMPLE_CONTROLS
		!m_owner->UseControlsFading();
#endif // MEDIA_FORCE_VIDEO_CONTROLS
}

void
MediaControls::EnsureVisibility()
{
	if (IsShowing())
		Show(m_fader);
	else
		Hide(m_fader);
}

void
MediaControls::ForceVisibility()
{
	if (IsShowing())
		m_fader.Show();
}

void
MediaControls::Show(MediaControlsFader& fader)
{
	if (m_owner->UseControlsOpacity())
		fader.FadeIn();
	else
		fader.Show();
}

void
MediaControls::Hide(MediaControlsFader& fader)
{
	if (m_owner->UseControlsOpacity())
		fader.FadeOut();
	else
		fader.DelayedHide();
}

#endif // MEDIA_PLAYER_SUPPORT
