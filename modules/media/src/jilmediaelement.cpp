/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_JIL_PLAYER_SUPPORT

#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/domutils.h"
#include "modules/layout/box/box.h"
#include "modules/media/jilmediaelement.h"
#include "modules/media/src/controls/mediacontrols.h"
#include "modules/style/css_media.h"

JILMediaElement::JILMediaElement(HTML_Element* el) :
	Media(el),
	m_listener(NULL),
	m_frm_doc(NULL),
	m_player(NULL),
	m_controls(NULL),
	m_video_width(0),
	m_video_height(0),
	m_paused(TRUE),
	m_suspended(FALSE),
	m_has_controls(FALSE),
	m_muted(FALSE),
	m_playback_ended(TRUE),
	m_volume(1.0)
{
	m_progress_update_timer.SetTimerListener(this);
}

JILMediaElement::~JILMediaElement()
{
	Cleanup();
}

void JILMediaElement::Cleanup()
{
	m_progress_update_timer.Stop();
	if (m_frm_doc)
		m_frm_doc->RemoveMedia(this);
	m_frm_doc = NULL;
	OP_DELETE(m_player);
	m_player = NULL;
	OP_DELETE(m_controls);
	m_controls = NULL;
}

OP_STATUS JILMediaElement::TakeOver(JILMediaElement* src_element)
{
	OP_ASSERT(src_element != this);
	if (!src_element)
		return OpStatus::OK;
	Cleanup();

	m_listener = src_element->m_listener;
	m_player = src_element->m_player;

	// copy all the attributes
	m_video_width = src_element->m_video_width;
	m_video_height = src_element->m_video_height;
	m_paused = src_element->m_paused;
	m_suspended = src_element->m_suspended;
	m_has_controls = src_element->m_has_controls;
	m_muted = src_element->m_muted;
	m_playback_ended = src_element->m_playback_ended;
	m_volume = src_element->m_volume;

	RETURN_IF_ERROR(EnsureFramesDocument());
	RETURN_IF_ERROR(m_frm_doc->AddMedia(this));
	RETURN_IF_ERROR(ManageMediaControls());
	RETURN_IF_ERROR(HandleTimeUpdate(TRUE));

	// reset all attrs in src element
	src_element->m_listener = NULL;
	src_element->m_player = NULL;

	src_element->m_paused = FALSE;
	src_element->m_suspended = FALSE;
	src_element->m_has_controls = FALSE;
	src_element->m_muted = FALSE;
	src_element->m_playback_ended = FALSE;
	src_element->m_volume = 1.0;
	src_element->Cleanup();

	if (m_player)
		m_player->SetListener(this);
	return OpStatus::OK;
}

UINT32
JILMediaElement::GetIntrinsicWidth()
{
	UINT32 width = 0;
	if (OpStatus::IsSuccess(EnsureFramesDocument()) && m_frm_doc->GetShowImages())
		width = m_video_width;
	return width;
}


UINT32
JILMediaElement::GetIntrinsicHeight()
{
	UINT32 height = 0;
	if (OpStatus::IsSuccess(EnsureFramesDocument()) && m_frm_doc->GetShowImages())
		height = m_video_height;
	return height;
}

OP_STATUS
JILMediaElement::Paint(VisualDevice* visdev, OpRect video, OpRect content)
{
	OpBitmap* bm = NULL;

	if (m_player && IsImageType())
		RETURN_IF_MEMORY_ERROR(m_player->GetFrame(bm));

	if (bm)
		visdev->BitmapOut(bm, OpRect(0, 0, bm->Width(), bm->Height()), video);

	if (m_controls)
		m_controls->Paint(visdev, content);

	return OpStatus::OK;
}

OP_STATUS
JILMediaElement::Open(const URL &url)
{
	RETURN_IF_ERROR(EnsureFramesDocument());

	if (url.IsEmpty())
		return OpStatus::ERR;

	// Cancel loading/playing resource.
	OP_DELETE(m_player);
	m_player = NULL;

	OP_ASSERT(m_frm_doc);

	RETURN_IF_ERROR(MediaPlayer::Create(m_player, url, URL(), IsImageType(), m_frm_doc->GetWindow()));
	m_player->SetListener(this);


	// possibly set via DOM before the player was created
	OpStatus::Ignore(m_player->SetPlaybackRate(1.0));
	OpStatus::Ignore(m_player->SetVolume(m_muted ? 0 : m_volume));

	m_paused = TRUE;
	m_playback_ended = TRUE;

	//stop progress updates
	m_progress_update_timer.Stop();

	//this will start buffering media content
	OpStatus::Ignore(m_player->Pause());
	return OpStatus::OK;
}

OP_STATUS
JILMediaElement::Play(BOOL resume_playback)
{
	if (!resume_playback || m_playback_ended)
		RETURN_IF_ERROR(SetPosition(0));

	if (m_paused)
	{
		if (m_player)
			m_player->Play();

		if (m_playback_ended && m_listener)
			m_listener->OnPlaybackStart();

		m_paused = FALSE;
		m_playback_ended = FALSE;

		RETURN_IF_ERROR(HandleTimeUpdate(TRUE));
	}

	return OpStatus::OK;
}

OP_STATUS
JILMediaElement::Pause()
{
	if (!m_paused)
	{
		m_paused = TRUE;

		RETURN_IF_ERROR(HandleTimeUpdate(FALSE));

		if (m_player)
			m_player->Pause();

		if (m_listener)
			m_listener->OnPlaybackPause();
	}

	return OpStatus::OK;
}

OP_STATUS
JILMediaElement::Stop()
{
	if (m_player)
		m_player->Pause();

	if (m_listener)
		m_listener->OnPlaybackStop();

	m_playback_ended = TRUE;
	m_paused = FALSE;

	RETURN_IF_ERROR(SetPosition(0));
	RETURN_IF_ERROR(HandleTimeUpdate(FALSE));

	return OpStatus::OK;
}

OP_STATUS
JILMediaElement::SetPosition(const double position)
{
	OP_ASSERT(position >= 0 && position <= GetDuration());

	if (m_player)
		OpStatus::Ignore(m_player->SetPosition(position));

	RETURN_IF_ERROR(HandleTimeUpdate(!m_paused && !m_suspended));

	return OpStatus::OK;
}

double
JILMediaElement::GetDuration() const
{
	double duration = op_nan(NULL);
	if (m_player)
		OpStatus::Ignore(m_player->GetDuration(duration));

	return duration;
}

void
JILMediaElement::OnMouseEvent(DOM_EventType event_type, MouseButton button, int x, int y)
{
	if (m_controls)
	{
		// Element wide events
		switch(event_type)
		{
		case ONMOUSEOVER:
			m_controls->OnElementMouseOver();
			break;
		case ONMOUSEOUT:
			m_controls->OnElementMouseOut();
			break;
		default:
			break;
		}

		// Controls events
		Box* layout_box = m_element->GetLayoutBox();
		if (!layout_box)
			return;

		// Translate mouse position to controls coordinate space.
		RECT rect;
		AffinePos rect_pos;
		if (!layout_box->GetRect(m_frm_doc, CONTENT_BOX, rect_pos, rect))
			return;
		OpRect content = OpRect(0, 0, rect.right - rect.left, rect.bottom - rect.top);
		OpRect controls = m_controls->GetControlArea(content);
		OpPoint wpoint(x - controls.x, y - controls.y);

		if (m_controls->GetBounds().Contains(wpoint))
		{
			switch (event_type)
			{
			case ONMOUSEDOWN:
				m_controls->GenerateOnMouseDown(wpoint, button, 1);
				break;

			//case ONMOUSEUP:
				//Not needed. Hooked widgets gets mouseup in HTML_Document::MouseAction.

			case ONMOUSEMOVE:
				// Hooked mousemove events is given to the widget from HTML_Document::MouseAction
				if (!OpWidget::hooked_widget)
					m_controls->GenerateOnMouseMove(wpoint);
				break;

			default:
				break;
			}
		}
	}
}

BOOL
JILMediaElement::IsFocusable() const
{
	return m_controls != NULL;
}

BOOL
JILMediaElement::FocusNextInternalTabStop(BOOL forward)
{
	if (m_controls)
	{
		OpWidget* next = m_controls->GetNextInternalTabStop(forward);
		if (next)
		{
			next->SetFocus(FOCUS_REASON_KEYBOARD);
			return TRUE;
		}
	}

	return FALSE;
}

void
JILMediaElement::FocusInternalEdge(BOOL forward)
{
	if (m_controls)
	{
		OpWidget* widget = m_controls->GetFocusableInternalEdge(forward);
		if (widget)
			widget->SetFocus(FOCUS_REASON_KEYBOARD);
	}
}

void
JILMediaElement::HandleFocusGained()
{
	if (m_frm_doc && m_frm_doc->GetHtmlDocument())
		m_frm_doc->GetHtmlDocument()->SetFocusedElement(m_element);
}

void
JILMediaElement::Update(BOOL just_control_area)
{
	VisualDevice* visdev;
	if (m_element->GetLayoutBox() &&
	   OpStatus::IsSuccess(EnsureFramesDocument()) &&
	   (visdev = m_frm_doc->GetVisualDevice()))
	{
		OpRect update_rect;

		if (just_control_area)
		{
			// Update only controls area.
			if (!m_controls)
				return; // do nothing
			RECT content_box;
			m_element->GetLayoutBox()->GetRect(m_frm_doc, CONTENT_BOX, content_box);
			const OpRect content = OpRect(&content_box);
			update_rect = m_controls->GetControlArea(content);
		}
		else
		{
			// Update entire video area.
			RECT bounding_box;
			m_element->GetLayoutBox()->GetRect(m_frm_doc, BOUNDING_BOX, bounding_box);
			update_rect = OpRect(&bounding_box);
		}

		visdev->Update(update_rect.x, update_rect.y, update_rect.width, update_rect.height);
	}
}

OP_STATUS
JILMediaElement::HandleTimeUpdate(BOOL schedule_next_update)
{

	OP_ASSERT(!m_suspended);

	if (m_controls)
		m_controls->OnTimeUpdate(TRUE);

	if (schedule_next_update)
		m_progress_update_timer.Start(1000);

	return OpStatus::OK;
}

OP_STATUS
JILMediaElement::EnsureFramesDocument()
{
	OP_ASSERT(!m_suspended);
	if (!m_frm_doc)
	{
		/* The media element might be created by logdoc or via DOM, in
		 * either case we need to find its FramesDocument.
		 */
		if (LogicalDocument* log_doc = m_element->GetLogicalDocument())
			m_frm_doc = log_doc->GetFramesDocument();
		else if (DOM_Environment* dom_env = DOM_Utils::GetDOM_Environment(m_element->GetESElement()))
			m_frm_doc = dom_env->GetFramesDocument();

		if (m_frm_doc)
		{
			OP_STATUS status = m_frm_doc->AddMedia(this);
			if (OpStatus::IsError(status))
			{
				m_frm_doc = NULL;
				return status;
			}
		}
	}

	return m_frm_doc ? OpStatus::OK : OpStatus::ERR;
}

void
JILMediaElement::Suspend(BOOL removed)
{
	if (!m_suspended)
	{
		m_suspended = TRUE;
		if (m_player)
			OpStatus::Ignore(m_player->Suspend());
	}

	if (removed)
		m_frm_doc = NULL;

	m_progress_update_timer.Stop();

	OP_DELETE(m_controls);
	m_controls = NULL;
}

OP_STATUS
JILMediaElement::Resume()
{
	if (m_suspended)
	{
		m_suspended = FALSE;

		if (m_player)
			RETURN_IF_ERROR(m_player->Resume());

		RETURN_IF_ERROR(ManageMediaControls());
	}
	return OpStatus::OK;
}

BOOL
JILMediaElement::ControlsNeeded() const
{
	return m_has_controls;
}

OP_STATUS
JILMediaElement::ManageMediaControls()
{
	if (ControlsNeeded())
	{
		RETURN_IF_ERROR(EnsureFramesDocument());

		if (!m_controls)
		{
			RETURN_IF_ERROR(MediaControls::Create(m_controls, this));

			// Don't fade in the controls when just created.
			m_controls->ForceVisibility();
		}
		else
			m_controls->EnsureVisibility();
	}
	else
	{
		OP_DELETE(m_controls);
		m_controls = NULL;
	}

	return OpStatus::OK;
}

OP_STATUS
JILMediaElement::EnableControls(BOOL enable)
{
	m_has_controls = enable;

	return ManageMediaControls();
}

void JILMediaElement::OnDurationChange(MediaPlayer* player)
{
	OP_ASSERT(player == m_player);

	if (m_listener)
		m_listener->OnOpen(FALSE);

	// Invalidate controls and repaint
	if (m_controls)
		m_controls->OnDurationChange();
}

void JILMediaElement::OnVideoResize(MediaPlayer* player)
{
	OP_ASSERT(player == m_player);

	UINT32 video_width, video_height;
	m_player->GetVideoSize(video_width, video_height);
	if (m_video_width != video_width || m_video_height != video_height)
	{
		m_video_width = video_width;
		m_video_height = video_height;
		if (OpStatus::IsSuccess(EnsureFramesDocument()))
			m_element->MarkDirty(m_frm_doc);
	}
}

void JILMediaElement::OnFrameUpdate(MediaPlayer* player)
{
	OP_ASSERT(player == m_player);

	Update();
}

void JILMediaElement::OnDecodeError(MediaPlayer* player)
{
	if (m_listener)
		m_listener->OnOpen(TRUE);
}

void JILMediaElement::OnPlaybackEnd(MediaPlayer* player)
{
	OP_ASSERT(player == m_player);

	m_paused = TRUE;

	// OnPlaybackEnd may start playing again
	if (m_listener)
		m_listener->OnPlaybackEnd();

	if (m_paused)
		m_playback_ended = TRUE;

	//update progress bar but don't schedule next update
	RAISE_IF_MEMORY_ERROR(HandleTimeUpdate(FALSE));

	// Update controls when playback ends
	Update(TRUE);
}

void JILMediaElement::OnError(MediaPlayer* player)
{
	if (m_listener)
		m_listener->OnOpen(TRUE);
}

double JILMediaElement::GetMediaDuration() const
{
	double duration = 0;
	if (m_player)
		m_player->GetDuration(duration);

	return duration;
}

double JILMediaElement::GetMediaPosition()
{
	double position = 0;
	if (m_player)
		m_player->GetPosition(position);

	return position;
}

OP_STATUS JILMediaElement::SetMediaVolume(const double volume)
{
	if (m_volume != volume)
	{
		if (m_player && !m_muted)
			m_player->SetVolume(volume);
		m_volume = volume;
	}
	return OpStatus::OK;
}

OP_STATUS JILMediaElement::SetMediaMuted(const BOOL muted)
{
	if ((BOOL)m_muted != muted)
	{
		m_muted = muted;
		if (m_player)
			m_player->SetVolume(m_muted ? 0 : m_volume);
	}
	return OpStatus::OK;
}

OP_STATUS JILMediaElement::SetMediaPosition(const double position)
{
	return m_player ? m_player->SetPosition(position) : OpStatus::ERR;
}

/* virtual */ const OpMediaTimeRanges*
JILMediaElement::GetMediaBufferedTimeRanges()
{
	const OpMediaTimeRanges* ranges = NULL;
	if (m_player)
		RAISE_IF_MEMORY_ERROR(m_player->GetBufferedTimeRanges(ranges));
	return ranges;
}

void JILMediaElement::OnTimeOut(OpTimer* timer)
{
	HandleTimeUpdate(TRUE);
}

#endif // MEDIA_JIL_PLAYER_SUPPORT
