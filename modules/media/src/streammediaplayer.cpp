/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include <core/pch.h>

#ifdef DOM_STREAM_API_SUPPORT

#include "modules/media/src/streammediaplayer.h"

#include "modules/media/camerarecorder.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"

/* static */ OP_STATUS
MediaPlayer::CreateStreamPlayer(MediaPlayer*& new_player, const URL& origin_url, MediaStreamResource* stream_resource)
{
	new_player = OP_NEW(StreamMediaPlayer, (stream_resource));
	RETURN_OOM_IF_NULL(new_player);
	return OpStatus::OK;
}

StreamMediaPlayer::StreamMediaPlayer(MediaStreamResource* stream_resource)
	: m_listener(NULL)
	, m_is_playing(FALSE)
	, m_total_play_time_ms(0.0)
	, m_last_play_time_update(0.0)
	, m_frame_width(0)
	, m_frame_height(0)
#if VIDEO_THROTTLING_FPS_HIGH > 0
	, m_throttler(this)
#endif // VIDEO_THROTTLING_FPS_HIGH > 0
	, m_stream_resource(stream_resource)
{
	OP_ASSERT(m_stream_resource);
	m_stream_resource->IncReferenceCount();
}

StreamMediaPlayer::~StreamMediaPlayer()
{
	if (m_stream_resource)
	{
		Pause();
		m_stream_resource->DecReferenceCount();
	}
}

/* virtual */
OP_STATUS StreamMediaPlayer::Play()
{
	OP_ASSERT(m_stream_resource);
	if (m_is_playing)
		return OpStatus::OK;

	OP_NEW_DBG("StreamMediaPlayer::Play", "camera");

	OP_STATUS attach_status = m_stream_resource->AttachStreamListener(this);
	if (OpStatus::IsError(attach_status))
	{
		OP_DBG(("Cannot attach stream listener to the stream: %d", attach_status));
		return attach_status;
	}

	UpdateFrameDimensions();

	m_last_play_time_update = g_op_time_info->GetRuntimeMS();
	m_is_playing = TRUE;
	return OpStatus::OK;
}

/* virtual */
OP_STATUS StreamMediaPlayer::Pause()
{
	OP_ASSERT(m_stream_resource);
	if (m_is_playing)
	{
		m_stream_resource->DetachStreamListener(this);
		m_is_playing = FALSE;

		UpdateCurrentPosition();
	}
	return OpStatus::OK;
}

/* virtual */
void StreamMediaPlayer::SetListener(MediaPlayerListener* listener)
{
	m_listener = listener;
	if (m_listener)
	{
		// Required for the listener to know what's going on and set
		// appropriate state.
		m_listener->OnDurationChange(this);
		m_listener->OnIdle(this);
	}
}

/* virtual */
OP_STATUS StreamMediaPlayer::GetFrame(OpBitmap*& bitmap)
{
	OP_ASSERT(m_stream_resource);
	if (!m_is_playing)
		return OpStatus::ERR;

	OP_NEW_DBG("StreamMediaPlayer::GetFrame", "camera");
	OP_STATUS get_frame_status = m_stream_resource->GetCurrentFrame(bitmap);
	if (OpStatus::IsError(get_frame_status))
	{
		OP_DBG(("Cannot obtain current frame: %d", get_frame_status));
		return ConvertCameraStatus(get_frame_status);
	}

	return OpStatus::OK;
}

/* virtual */
void StreamMediaPlayer::OnUpdateFrame(MediaStreamResource* stream_resource)
{
	OP_ASSERT(m_stream_resource == stream_resource);
#if VIDEO_THROTTLING_FPS_HIGH > 0
	if (m_throttler.IsFrameUpdateAllowed())
#endif // VIDEO_THROTTLING_FPS_HIGH > 0
	{
		if (m_listener)
			m_listener->OnFrameUpdate(this);
	}
}

/* virtual */
void StreamMediaPlayer::OnFrameResize(MediaStreamResource* stream_resource)
{
	OP_ASSERT(m_stream_resource == stream_resource);
	UpdateFrameDimensions();
}

/* virtual */
void StreamMediaPlayer::OnStreamFinished(MediaStreamResource* stream_resource)
{
	OP_ASSERT(m_stream_resource == stream_resource);
	if (m_listener)
		m_listener->OnPlaybackEnd(this);
	Pause();
}

#if VIDEO_THROTTLING_FPS_HIGH > 0
/* virtual */
void StreamMediaPlayer::OnFrameUpdateAllowed()
{
	if (m_listener)
		m_listener->OnFrameUpdate(this);
}
#endif // VIDEO_THROTTLING_FPS_HIGH > 0

#ifdef PI_VIDEO_LAYER
void
StreamMediaPlayer::GetVideoLayerColor(UINT8* red, UINT8* green, UINT8* blue, UINT8* alpha)
{
	OpMediaManager::GetVideoLayerColor(red, green, blue, alpha);
}
#endif //PI_VIDEO_LAYER

/* virtual */
OP_STATUS StreamMediaPlayer::GetPosition(double &position_seconds)
{
	if (m_is_playing)
		UpdateCurrentPosition();

	position_seconds = m_total_play_time_ms / 1000.0;

	return OpStatus::OK;
}

/* virtual */
URL StreamMediaPlayer::GetOriginURL()
{
	return m_stream_resource->GetOriginURL();
}

OP_STATUS StreamMediaPlayer::UpdateFrameDimensions()
{
	OP_ASSERT(m_stream_resource);
	unsigned new_width;
	unsigned new_height;
	OP_STATUS status = m_stream_resource->GetFrameDimensions(new_width, new_height);
	if (OpStatus::IsError(status))
		return status;

	if (OpStatus::IsSuccess(status) && m_frame_width != new_width && m_frame_height != new_height)
	{
		m_frame_width = new_width;
		m_frame_height = new_height;
		if (m_listener)
			m_listener->OnVideoResize(this);
	}
	return OpStatus::OK;
}

void StreamMediaPlayer::UpdateCurrentPosition()
{
	double old_last_play_time_update = m_last_play_time_update;
	m_last_play_time_update = g_op_time_info->GetRuntimeMS();
	m_total_play_time_ms += (m_last_play_time_update - old_last_play_time_update);
}

OP_STATUS StreamMediaPlayer::ConvertCameraStatus(OP_CAMERA_STATUS camera_status)
{
	switch(camera_status)
	{
	case OpCameraStatus::ERR_NOT_AVAILABLE:
	case OpCameraStatus::ERR_DISABLED:
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	case OpCameraStatus::ERR_OPERATION_ABORTED:
	case OpCameraStatus::ERR_FILE_EXISTS:
	case OpCameraStatus::ERR_RESOLUTION_NOT_SUPPORTED:
		OP_ASSERT(!"Unexpected error status");
		return OpStatus::ERR;
	default:
		// regular OpStatus values
		return camera_status;
	}
}

#endif // DOM_STREAM_API_SUPPORT
