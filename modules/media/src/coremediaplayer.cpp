/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/media/src/coremediaplayer.h"
#include "modules/media/src/mediasource.h"

/* static */ OP_STATUS
MediaPlayer::Create(MediaPlayer*& player, const URL& url, const URL& referer_url, BOOL is_video, Window* window)
{
	RETURN_IF_ERROR(g_media_module.EnsureOpMediaManager());

	// Find a good MessageHandler for the MediaPlayer to use. There are two requirements:
	// * The MessageHandler must outlive the MediaPlayer.
	// * MessageHandler::GetWindow() must work for Gadget Media to load correctly.
	// There is no known MessageHandler that does both of those so try the best mix possible.
	MessageHandler* message_handler = g_main_message_handler;
#ifdef GADGET_SUPPORT
	if (window && window->GetType() == WIN_TYPE_GADGET)
		// Might crash if element is abused. See CORE-38309.
		message_handler = window->GetMessageHandler();
#endif // GADGET_SUPPORT

	// Don't ever play in a thumbnail
	BOOL force_paused = window && window->IsThumbnailWindow();

	player = OP_NEW(CoreMediaPlayer, (url, referer_url, is_video, g_op_media_manager, g_media_source_manager, message_handler, force_paused));
	if (!player)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

#ifndef SELFTEST
# define m_source_manager g_media_source_manager
# define m_pi_manager g_op_media_manager
#endif // !SELFTEST

CoreMediaPlayer::CoreMediaPlayer(const URL& url, const URL& origin_url, BOOL is_video,
								 OpMediaManager* pi_manager,
								 MediaSourceManager* source_manager,
								 MessageHandler* message_handler,
								 BOOL force_paused)
	: m_pi_player(NULL),
	  m_listener(NULL),
	  m_source(NULL),
	  m_url(url),
	  m_origin_url(origin_url),
	  m_position(0.0),
	  m_rate(1.0),
	  m_volume(1.0),
	  m_preload(g_stdlib_infinity),
	  m_message_handler(message_handler),
	  m_played_ranges(NULL),
	  m_played_range_start(-1),
	  m_paused(TRUE),
	  m_force_paused(force_paused),
	  m_suspend(FALSE),
	  m_seeking(FALSE)
#ifdef DEBUG_ENABLE_OPASSERT
	  , m_network_err(FALSE)
	  , m_decode_err(FALSE)
	  , m_duration_known(FALSE)
#endif // DEBUG_ENABLE_OPASSERT
	  , m_is_video(is_video)
#ifdef SELFTEST
	  , m_pi_manager(pi_manager)
	  , m_source_manager(source_manager)
#endif // SELFTEST
#if VIDEO_THROTTLING_FPS_HIGH > 0
	  , m_throttler(this)
#endif // VIDEO_THROTTLING_FPS_HIGH
{
}

/* virtual */
CoreMediaPlayer::~CoreMediaPlayer()
{
	DestroyPlatformPlayer();
	if (m_source)
		ReleaseSource();
	OP_DELETE(m_played_ranges);
}

OP_STATUS
CoreMediaPlayer::Init()
{
	if (!m_pi_player && !m_source)
	{
		// Ask OpMediaManager::CanPlayURL about how to handle this
		// URL. Don't ask about data: URLs as that requires copying
		// the (potentially huge) data just to ask.
		if (m_url.Type() != URL_DATA)
		{
			OpString url_str;
			RETURN_IF_ERROR(m_url.GetAttribute(URL::KUniName_For_Data, 0, url_str));
			if (m_pi_manager->CanPlayURL(url_str.CStr()))
			{
				// OpMediaPlayer wants to handle this URL without help
				// from core. Simply create and initialize the player.
				InitPlatformPlayer(m_pi_manager->CreatePlayer(&m_pi_player, GetHandle(), url_str.CStr()));
				return OpStatus::OK;
			}
		}

		// OpMediaPlayer will need to use OpMediaSource. Create a
		// MediaSource and begin buffering. OpMediaPlayer is
		// created and initalized in OnProgress or OnIdle.
		OP_ASSERT(m_source == NULL);
		RETURN_IF_ERROR(m_source_manager->GetSource(m_source, m_url, m_origin_url, m_message_handler));
		RETURN_IF_ERROR(m_source->AddProgressListener(this));
		m_source->HandleClientChange();
	}

	if (!m_played_ranges)
	{
		m_played_ranges = OP_NEW(MediaTimeRanges, ());
		if (!m_played_ranges)
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::Play()
{
	if (m_force_paused)
		return Pause();

	RETURN_IF_ERROR(Init());
	m_paused = FALSE;
	if (m_played_range_start < 0)
		GetPosition(m_played_range_start);
	if (m_pi_player)
		return m_pi_player->Play();
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::Pause()
{
	RETURN_IF_ERROR(Init());
	m_paused = TRUE;
#if VIDEO_THROTTLING_FPS_HIGH > 0
	m_throttler.Break();
#endif // VIDEO_THROTTLING_FPS_HIGH > 0
	if (m_pi_player)
		return m_pi_player->Pause();
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::Suspend()
{
	// Temporary fix for CORE-26421. Ignores callback from the backend while
	// the media element is suspended. These callback will need to be handled
	// somehow for suspend/resume to work properly. See CORE-27035.
	m_suspend = TRUE;
	DestroyPlatformPlayer();
#if VIDEO_THROTTLING_FPS_HIGH > 0
	m_throttler.Break();
#endif //VIDEO_THROTTLING_FPS_HIGH > 0
	if (m_source)
	{
		ReleaseSource();
		m_source = NULL;

#ifdef DEBUG_ENABLE_OPASSERT
		m_network_err = FALSE;
		m_decode_err = FALSE;
#endif // DEBUG_ENABLE_OPASSERT
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::Resume()
{
	m_suspend = FALSE;
	return Init();
}

#ifdef _DEBUG
static Debug&
operator<<(Debug& d, const OpMediaTimeRanges* ranges)
{
	UINT32 length = ranges->Length();
	for (UINT32 i = 0; i < length; i++)
	{
		if (i > 0)
			d << ", ";
		d << "[";
		d << ranges->Start(i);
		d << ", ";
		d << ranges->End(i);
		d << ")";
	}
	return d;
}
#endif

/* virtual */ OP_STATUS
CoreMediaPlayer::GetBufferedTimeRanges(const OpMediaTimeRanges*& ranges)
{
	RETURN_VALUE_IF_NULL(m_pi_player, OpStatus::ERR);
	RETURN_IF_ERROR(m_pi_player->GetBufferedTimeRanges(&ranges));

#ifdef DEBUG_ENABLE_OPASSERT
	// Verify that the ranges are normalized.
	double duration;
	OpStatus::Ignore(GetDuration(duration));
	double prev_end = op_nan(NULL);
	for (UINT32 i = 0; i < ranges->Length(); i++)
	{
		double start = ranges->Start(i);
		double end = ranges->End(i);
		OP_ASSERT(start >= 0 && op_isfinite(start));
		OP_ASSERT(end > 0 && op_isfinite(end));
		OP_ASSERT(op_isnan(prev_end) || prev_end < start);
		OP_ASSERT(start < end);
		OP_ASSERT(!op_isfinite(duration) || duration >= end);
		prev_end = end;
	}
	OP_NEW_DBG("GetBufferedTimeRanges", "MediaPlayer");
	OP_DBG(("duration: %f; buffered: ", duration) << ranges);
#endif // DEBUG_ENABLE_OPASSERT

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::GetSeekableTimeRanges(const OpMediaTimeRanges*& ranges)
{
	if (m_pi_player)
		return m_pi_player->GetSeekableTimeRanges(&ranges);
	return OpStatus::ERR;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::GetPlayedTimeRanges(const OpMediaTimeRanges*& ranges)
{
	RETURN_VALUE_IF_NULL(m_played_ranges, OpStatus::ERR);
	if (m_played_range_start >= 0)
	{
		double end;
		if (GetPosition(end) == OpStatus::OK)
			OpStatus::Ignore(m_played_ranges->AddRange(m_played_range_start, end));
	}
	ranges = m_played_ranges;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::GetPosition(double& position)
{
	if (m_seeking)
	{
		position = m_position;
		return OpStatus::OK;
	}
	if (m_pi_player)
		return m_pi_player->GetPosition(&position);
	position = 0.0;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::SetPosition(double position)
{
	if (position < 0 || !op_isfinite(position))
		return OpStatus::ERR_OUT_OF_RANGE;
	if (m_played_ranges && m_played_range_start >= 0)
	{
		double end;
		if (GetPosition(end) == OpStatus::OK)
			OpStatus::Ignore(m_played_ranges->AddRange(m_played_range_start, end));
	}
	m_position = position;
	if (m_pi_player)
	{
		RETURN_IF_ERROR(m_pi_player->SetPosition(position));
		m_seeking = TRUE;
	}
	m_played_range_start = position;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::GetDuration(double& duration)
{
	duration = op_nan(NULL);
	if (m_pi_player)
		return m_pi_player->GetDuration(&duration);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::SetPlaybackRate(double rate)
{
	if (m_pi_player)
		return m_pi_player->SetPlaybackRate(rate);
	m_rate = rate;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::SetVolume(double volume)
{
	if (m_pi_player)
		return m_pi_player->SetVolume(volume);
	m_volume = volume;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
CoreMediaPlayer::SetPreload(BOOL preload)
{
	m_preload = preload ? g_stdlib_infinity : MEDIA_PRELOAD_DURATION;
	if (m_pi_player)
		return m_pi_player->SetPreload(m_preload);
	return OpStatus::OK;
}

/* virtual */ void
CoreMediaPlayer::GetVideoSize(UINT32& width, UINT32& height)
{
	double pixel_ratio;
	if (m_pi_player && OpStatus::IsSuccess(m_pi_player->GetNativeSize(&width, &height, &pixel_ratio)))
	{
		// Increase one dimension if needed to preserve aspect ratio.
		if (pixel_ratio > 1.0)
			width = (UINT32)(width*pixel_ratio + 0.5);
		else if (pixel_ratio > 0.0  && pixel_ratio < 1.0)
			height = (UINT32)(height/pixel_ratio + 0.5);
	}
	else
	{
		// OpMediaPlayer isn't required to not modify width/height
		// when failing, so...
		width = height = 0;
	}
}

/* virtual */ OP_STATUS
CoreMediaPlayer::GetFrame(OpBitmap*& bitmap)
{
	if (m_pi_player)
	{
		OP_STATUS status = m_pi_player->GetFrame(&bitmap);
		if (OpStatus::IsSuccess(status) && !bitmap)
			return OpStatus::ERR;
		return status;
	}
	return OpStatus::ERR;
}

/* virtual */ void
CoreMediaPlayer::OnDurationChange(OpMediaPlayer* player)
{
#ifdef DEBUG_ENABLE_OPASSERT
	m_duration_known = TRUE;
#endif // DEBUG_ENABLE_OPASSERT

	if (m_listener && !m_suspend)
		m_listener->OnDurationChange(this);
}

/* virtual */ void
CoreMediaPlayer::OnVideoResize(OpMediaPlayer* player)
{
	if (m_listener && !m_suspend)
		m_listener->OnVideoResize(this);
}

/* virtual */ void
CoreMediaPlayer::OnFrameUpdate(OpMediaPlayer* player)
{
#if VIDEO_THROTTLING_FPS_HIGH > 0
	if (m_throttler.IsFrameUpdateAllowed())
#endif // VIDEO_THROTTLING_FPS_HIGH > 0
	{
		if (m_listener && !m_suspend)
			m_listener->OnFrameUpdate(this);
	}
}

/* virtual */ void
CoreMediaPlayer::OnDecodeError(OpMediaPlayer* player)
{
#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(!m_decode_err || !"a decode error is fatal and cannot happen twice");
	m_decode_err = TRUE;
#endif // DEBUG_ENABLE_OPASSERT
	if (m_listener && !m_suspend)
		m_listener->OnDecodeError(this);
}

/* virtual */ void
CoreMediaPlayer::OnPlaybackEnd(OpMediaPlayer* player)
{
	OP_ASSERT(m_duration_known || !"Playback ended before duration was known.");

	if (m_listener && !m_suspend)
		m_listener->OnPlaybackEnd(this);

#if VIDEO_THROTTLING_FPS_HIGH > 0
	m_throttler.Break();
#endif // VIDEO_THROTTLING_FPS_HIGH > 0
}

/* virtual */ void
CoreMediaPlayer::OnForcePause(OpMediaPlayer* player)
{
	if (m_listener)
		m_listener->OnForcePause(this);
}

/* virtual */ void
CoreMediaPlayer::OnSeekComplete(OpMediaPlayer* player)
{
	if (m_listener && !m_suspend && m_seeking)
		m_listener->OnSeekComplete(this);
	m_seeking = FALSE;
}

/* virtual */ void
CoreMediaPlayer::OnStalled(OpMediaPlayer* player)
{
	if (m_listener && !m_suspend)
		m_listener->OnStalled(this);
}

/* virtual */ void
CoreMediaPlayer::OnProgress(MediaSource* source)
{
	OP_ASSERT(source == m_source);

	if (m_listener && !m_suspend)
		m_listener->OnProgress(this);

	// When using a core MediaSource, initialize platform player.
	if (!m_pi_player && source)
		InitPlatformPlayer(m_pi_manager->CreatePlayer(&m_pi_player, GetHandle()));
}

/* virtual */ void
CoreMediaPlayer::OnError(MediaSource* source)
{
	OP_ASSERT(source == m_source);
#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(!m_network_err);
	m_network_err = TRUE;
#endif // DEBUG_ENABLE_OPASSERT
	if (m_listener && !m_suspend)
		m_listener->OnError(this);
}

/* virtual */ void
CoreMediaPlayer::OnIdle(MediaSource* source)
{
	OP_ASSERT(source == m_source);

	if (m_listener && !m_suspend)
		m_listener->OnIdle(this);

	// There's no guarantee that OnProgress is called at all (e.g. in
	// the case of a cached resource), so create player here too.
	if (!m_pi_player && source)
		InitPlatformPlayer(m_pi_manager->CreatePlayer(&m_pi_player, GetHandle()));
}

/* virtual */ void
CoreMediaPlayer::OnPauseForBuffering(OpMediaPlayer* player)
{
	if (m_listener)
		m_listener->OnPauseForBuffering(this);
}

/* virtual */ void
CoreMediaPlayer::OnPlayAfterBuffering(OpMediaPlayer* player)
{
	if (m_listener)
		m_listener->OnPlayAfterBuffering(this);
}

/* virtual */ void
CoreMediaPlayer::OnClientCollision(MediaSource* source)
{
	// Release source and get a new one in Init()
	OP_ASSERT(source == m_source);
	ReleaseSource();
	m_source = NULL;
	RAISE_IF_MEMORY_ERROR(Init());
}

void
CoreMediaPlayer::ReleaseSource()
{
	OP_ASSERT(m_source);
	m_source->RemoveProgressListener(this);
	m_source_manager->PutSource(m_source);
}

void
CoreMediaPlayer::InitPlatformPlayer(OP_STATUS status)
{
	if (OpStatus::IsSuccess(status))
	{
		m_pi_player->SetListener(this);

		// Initialize state set before platform player was created.

		if (m_paused)
			m_pi_player->Pause();
		else
			m_pi_player->Play();

		m_pi_player->SetPosition(m_position);
		m_pi_player->SetPlaybackRate(m_rate);
		m_pi_player->SetVolume(m_volume);
		m_pi_player->SetPreload(m_preload);
	}
	else
	{
		OP_ASSERT(m_pi_player == NULL);
		RAISE_IF_MEMORY_ERROR(status);
		// Treat all errors as decode errors.
		if (m_listener && !m_suspend)
			m_listener->OnDecodeError(this);
	}
}

void
CoreMediaPlayer::DestroyPlatformPlayer()
{
	m_pi_manager->DestroyPlayer(m_pi_player);
	m_pi_player = NULL;
}

#if VIDEO_THROTTLING_FPS_HIGH > 0
void
CoreMediaPlayer::OnFrameUpdateAllowed()
{
	if (m_listener)
		m_listener->OnFrameUpdate(this);
}
#endif // VIDEO_THROTTLING_FPS_HIGH > 0

#ifdef PI_VIDEO_LAYER
void
CoreMediaPlayer::GetVideoLayerColor(UINT8* red, UINT8* green, UINT8* blue, UINT8* alpha)
{
	OpMediaManager::GetVideoLayerColor(red, green, blue, alpha);
}

void
CoreMediaPlayer::SetVideoLayerRect(const OpRect& rect, const OpRect& clipRect)
{
	if (m_pi_player)
	{
		// Notify player only if there is a change in video window
		if (!rect.Equals(m_last_screen_rect) || !clipRect.Equals(m_last_clip_rect))
		{
			m_last_screen_rect = rect;
			m_last_clip_rect = clipRect;
			m_pi_player->SetVideoLayerRect(rect, clipRect);
		}
	}
}
#endif //PI_VIDEO_LAYER
#endif // MEDIA_PLAYER_SUPPORT
