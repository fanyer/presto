/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef COREMEDIAPLAYER_H
#define COREMEDIAPLAYER_H

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/media/mediaplayer.h"
#include "modules/media/mediatimeranges.h"
#include "modules/media/src/mediasource.h"

#if VIDEO_THROTTLING_FPS_HIGH > 0
#include "modules/media/src/mediathrottler.h"
#endif // VIDEO_THROTTLING_FPS_HIGH > 0

/** A media player that plays media from URLs. */
class CoreMediaPlayer
	: public MediaPlayer,
	  public MediaHandle,
	  public OpMediaPlayerListener,
	  public MediaProgressListener
#if VIDEO_THROTTLING_FPS_HIGH > 0
	  , public MediaThrottlerListener
#endif // VIDEO_THROTTLING_FPS_HIGH > 0
{
public:
	CoreMediaPlayer(const URL& url, const URL& origin_url, BOOL is_video,
					OpMediaManager* pi_manager,
					MediaSourceManager* source_manager,
					MessageHandler* message_handler,
					BOOL force_paused);
	virtual ~CoreMediaPlayer();

	virtual OP_STATUS Play();
	virtual OP_STATUS Pause();

	virtual OP_STATUS Suspend();
	virtual OP_STATUS Resume();

	virtual OP_STATUS GetBufferedTimeRanges(const OpMediaTimeRanges*& ranges);
	virtual OP_STATUS GetSeekableTimeRanges(const OpMediaTimeRanges*& ranges);
	virtual OP_STATUS GetPlayedTimeRanges(const OpMediaTimeRanges*& ranges);
	virtual BOOL GetSeeking() const { return m_seeking; }
	virtual BOOL IsAssociatedWithVideo() const { return m_is_video; }

	virtual OP_STATUS GetPosition(double& position);
	virtual OP_STATUS SetPosition(double position);
	virtual OP_STATUS GetDuration(double& duration);
	virtual OP_STATUS SetPlaybackRate(double rate);
	virtual OP_STATUS SetVolume(double volume);
	virtual OP_STATUS SetPreload(BOOL preload);

	virtual void SetListener(MediaPlayerListener* listener) { m_listener = listener; }

	virtual void GetVideoSize(UINT32& width, UINT32& height);
	virtual OP_STATUS GetFrame(OpBitmap*& bitmap);

	// The OpMediaHandle given to the platform is just the
	// CoreMediaPlayer pointer.
	virtual OpMediaHandle GetHandle() { return this; }

	virtual URL GetOriginURL() { return m_source ? m_source->GetOriginURL() : URL(); }

#ifdef PI_VIDEO_LAYER
	virtual BOOL SupportsVideoLayer() { return TRUE; }
#endif // PI_VIDEO_LAYER

	// OpMediaPlayerListener
	virtual void OnDurationChange(OpMediaPlayer* player);
	virtual void OnVideoResize(OpMediaPlayer* player);
	virtual void OnFrameUpdate(OpMediaPlayer* player);
	virtual void OnDecodeError(OpMediaPlayer* player);
	virtual void OnPlaybackEnd(OpMediaPlayer* player);
	virtual void OnSeekComplete(OpMediaPlayer* player);
	virtual void OnForcePause(OpMediaPlayer* player);
	virtual void OnPauseForBuffering(OpMediaPlayer* player);
	virtual void OnPlayAfterBuffering(OpMediaPlayer* player);

	virtual void OnProgress(OpMediaPlayer* player) { OP_ASSERT(m_source == NULL); OnProgress(m_source); }
	virtual void OnStalled(OpMediaPlayer* player); // not on MediaProgressListener (CORE-38614)
	virtual void OnError(OpMediaPlayer* player) { OP_ASSERT(m_source == NULL); OnError(m_source); }
	virtual void OnIdle(OpMediaPlayer* player) { OP_ASSERT(m_source == NULL); OnIdle(m_source); }

	// MediaProgressListener
	virtual void OnProgress(MediaSource* source);
	virtual void OnError(MediaSource* source);
	virtual void OnIdle(MediaSource* source);
	virtual void OnClientCollision(MediaSource* source);

#if VIDEO_THROTTLING_FPS_HIGH > 0
	virtual void OnFrameUpdateAllowed();
#endif // VIDEO_THROTTLING_FPS_HIGH > 0

	// Only for MediaSourceManager!
	MediaSource* GetSource() { OP_ASSERT(m_source); return m_source; }

#ifdef PI_VIDEO_LAYER
	virtual void GetVideoLayerColor(UINT8* red, UINT8* green, UINT8* blue, UINT8* alpha);
	virtual void SetVideoLayerRect(const OpRect& rect, const OpRect& clipRect);
#endif //PI_VIDEO_LAYER

private:
	/** Initialize the player.
	 *
	 * This causes MediaSource and OpMediaPlayer to (eventually) be
	 * created as needed.
	 */
	OP_STATUS Init();

	/** Release the MediaSource instance. */
	void ReleaseSource();

	/** Initialize the platform player.
	 *
	 * This must be called directly after creating (or failing to
	 * create) the platform player.
	 *
	 * @param status return value from OpMediaManager::CreatePlayer.
	 */
	void InitPlatformPlayer(OP_STATUS status);
	void DestroyPlatformPlayer();

	OpMediaPlayer* m_pi_player;
	MediaPlayerListener* m_listener;
	MediaSource* m_source;
	URL m_url;
	URL m_origin_url;
	double m_position;
	double m_rate;
	double m_volume;
	double m_preload;
	MessageHandler* m_message_handler;
	MediaTimeRanges* m_played_ranges;
	double m_played_range_start; // Negative if no start value is set.
	unsigned m_paused:1;
	unsigned m_force_paused:1;
	unsigned m_suspend:1;
	unsigned m_seeking:1;

#ifdef DEBUG_ENABLE_OPASSERT
	unsigned m_network_err:1; // TRUE if a network error has occurred.
	unsigned m_decode_err:1; // TRUE if a decode error has occurred.
	unsigned m_duration_known:1; // TRUE if duration has been provided.
#endif // DEBUG_ENABLE_OPASSERT

	unsigned m_is_video:1;  // false for audio

#ifdef SELFTEST
	OpMediaManager* m_pi_manager;
	MediaSourceManager* m_source_manager;
#endif // SELFTEST
#if VIDEO_THROTTLING_FPS_HIGH > 0
	MediaThrottler m_throttler;
#endif // VIDEO_THROTTLING_FPS_HIGH

#ifdef PI_VIDEO_LAYER
	OpRect m_last_screen_rect;
	OpRect m_last_clip_rect;
#endif //PI_VIDEO_LAYER
};

#endif //MEDIA_PLAYER_SUPPORT

#endif // COREMEDIAPLAYER_H
