/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef STREAMMEDIAPLAYER_H
#define STREAMMEDIAPLAYER_H

#ifdef DOM_STREAM_API_SUPPORT

#include "modules/media/mediaplayer.h"

#include "modules/pi/device_api/OpCamera.h"
#include "modules/media/mediastreamresource.h"

#if VIDEO_THROTTLING_FPS_HIGH > 0
#include "modules/media/src/mediathrottler.h"
#endif // VIDEO_THROTTLING_FPS_HIGH > 0

class StreamMediaPlayer
	: public MediaPlayer
	, public MediaHandle
	, public MediaStreamListener
#if VIDEO_THROTTLING_FPS_HIGH > 0
	, public MediaThrottlerListener
#endif // VIDEO_THROTTLING_FPS_HIGH > 0
{
public:
	StreamMediaPlayer(MediaStreamResource* stream_resource);
	~StreamMediaPlayer();

	virtual OP_STATUS Play();
	virtual OP_STATUS Pause();

	virtual OP_STATUS Suspend() { return Pause(); }
	virtual OP_STATUS Resume() { return Play(); }

	virtual OP_STATUS GetBufferedTimeRanges(const OpMediaTimeRanges*& ranges) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetSeekableTimeRanges(const OpMediaTimeRanges*& ranges) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetPlayedTimeRanges(const OpMediaTimeRanges*& ranges) { return OpStatus::ERR_NOT_SUPPORTED; }

	virtual BOOL GetSeeking() const { return FALSE; }

	virtual OP_STATUS GetPosition(double &position_seconds);

	virtual OP_STATUS SetPosition(double position_seconds) { return OpStatus::ERR_NOT_SUPPORTED; }

	virtual OP_STATUS GetDuration(double &duration) { duration = g_stdlib_infinity; return OpStatus::OK; }

	virtual OP_STATUS SetPlaybackRate(double rate) { return OpStatus::ERR_NOT_SUPPORTED; }

	virtual OP_STATUS SetVolume(double volume) { return OpStatus::ERR_NOT_SUPPORTED; }

	virtual OP_STATUS SetPreload(BOOL preload) { return OpStatus::ERR_NOT_SUPPORTED; }

	virtual void SetListener(MediaPlayerListener* listener);

	/** Get the aspect ratio-adjusted size of the video frame.
	 *
	 * @param[out] width The aspect ratio-adjusted width, or 0 if it
	 *                   is still unknown.
	 * @param[out] height The aspect ratio-adjusted height, or 0 if it
	 *                    is still unknown.
	 *
	 * Note: for stream input a 1:1 pixel aspect ratio is assumed
	 * (i.e. the pixels are square) so the provided size is the same
	 * as the frame size in pixels.
	 */
	virtual void GetVideoSize(UINT32& width, UINT32& height) { width = m_frame_width; height = m_frame_height; }

	/** Get the last frame as an OpBitmap. */
	virtual OP_STATUS GetFrame(OpBitmap*& bitmap);

	// From OpCameraPreviewListener
	virtual void OnUpdateFrame(MediaStreamResource* stream_resource);
	virtual void OnFrameResize(MediaStreamResource* stream_resource);
	virtual void OnStreamFinished(MediaStreamResource* stream_resource);
#if VIDEO_THROTTLING_FPS_HIGH > 0
	virtual void OnFrameUpdateAllowed();
#endif // VIDEO_THROTTLING_FPS_HIGH > 0

	/** Get the OpMediaHandle associated with this player. */
	virtual OpMediaHandle GetHandle() { return this; }

	virtual URL GetOriginURL();

#ifdef PI_VIDEO_LAYER
	virtual BOOL SupportsVideoLayer() { return FALSE; }
	virtual void GetVideoLayerColor(UINT8* red, UINT8* green, UINT8* blue, UINT8* alpha);
	virtual void SetVideoLayerRect(const OpRect& rect, const OpRect& clipRect) {}
#endif // PI_VIDEO_LAYER

private:
	/** Get new frame dimensions from camera.
	 *
	 *  If the new dimensions differ from the ones currently used,
	 *  OnVideoResize is called on the media player listener.
	 */
	OP_STATUS UpdateFrameDimensions();

	/** Converts some OP_CAMERA_STATUS values to OP_STATUS.
	 *
	 * It's not a general purpose function, it assumes the status
	 * values come from live-preview related operations.
	 */
	OP_STATUS ConvertCameraStatus(OP_CAMERA_STATUS status);

	/** Updates the current playing position. */
	void UpdateCurrentPosition();

	MediaPlayerListener* m_listener;
	BOOL m_is_playing;
	double m_total_play_time_ms;
	double m_last_play_time_update;

	UINT32 m_frame_width;
	UINT32 m_frame_height;

#if VIDEO_THROTTLING_FPS_HIGH > 0
	MediaThrottler m_throttler;
#endif // VIDEO_THROTTLING_FPS_HIGH > 0

	MediaStreamResource* m_stream_resource;
};

#endif // DOM_STREAM_API_SUPPORT

#endif // STREAMMEDIAPLAYER_H
