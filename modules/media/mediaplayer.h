/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/doc/frm_doc.h"
#include "modules/pi/OpMediaPlayer.h"

class MediaPlayer;
class MediaStreamResource;
class Window;

class MediaHandle {};

/** Receive buffering and playback notifications. */
class MediaPlayerListener
{
public:
	virtual ~MediaPlayerListener() {}
	// Load progress events

	/** Buffering is progressing normally. */
	virtual void OnProgress(MediaPlayer* player) {}
	/** Buffering has not progressed for some time. */
	virtual void OnStalled(MediaPlayer* player) {}
	/** Buffering has stopped because of a network error. */
	virtual void OnError(MediaPlayer* player) {}
	/** Buffering has stopped because of the buffering strategy. */
	virtual void OnIdle(MediaPlayer* player) {}
	/** Playback has paused due to buffering underrun. */
	virtual void OnPauseForBuffering(MediaPlayer* player) {}
	/** Playback continued after recovering from buffering underrun. */
	virtual void OnPlayAfterBuffering(MediaPlayer* player) {}

	// Playback events

	/** The length of the media resource has become known or changed. */
	virtual void OnDurationChange(MediaPlayer* player) {}
	/** The size of the video has become known or changed. */
	virtual void OnVideoResize(MediaPlayer* player) {}
	/** A new video frame is available. */
	virtual void OnFrameUpdate(MediaPlayer* player) {}
	/** The media resource cannot be decoded. */
	virtual void OnDecodeError(MediaPlayer* player) {}
	/** Playback has reached the end of the media resource. */
	virtual void OnPlaybackEnd(MediaPlayer* player) {}
	/** Seeking has completed. */
	virtual void OnSeekComplete(MediaPlayer* player) {}
	/** Force the playback to pause. Used by backends with limited resources. */
	virtual void OnForcePause(MediaPlayer* player) {}
};

/** Handles playback of a single media source. */
class MediaPlayer
{
public:
	/** Creates a media player for playing URLs.
	 * @param[out] player The created player (if return value is OpStatus::OK).
	 * @param[in] url The resource to play.
	 * @param[in] referer_url The referer to use when requesting the load of |url|.
	 * @param[in] is_video TRUE if the media player is associated with video;
	 *                     FALSE if it is associated with audio.
	 * @param[in] window Optional Window pointer. Required for Gadgets to be able to connect the
	 *                   load to the appropriate gadget and apply correct security. Can be NULL
	 *                   otherwise.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
	static OP_STATUS Create(MediaPlayer*& player, const URL& url, const URL& referer_url, BOOL is_video, Window* window);
#ifdef DOM_STREAM_API_SUPPORT
	/** Creates a media player for playing input from a MediaStreamResource. */
	static OP_STATUS CreateStreamPlayer(MediaPlayer*& player, const URL& origin_url, MediaStreamResource* stream_resource);
#endif // DOM_STREAM_API_SUPPORT
	virtual ~MediaPlayer() {}

	/** Play the media resource. */
	virtual OP_STATUS Play() = 0;
	/** Pause the media resource.
	 *
	 * Pausing a newly created player should cause the first video
	 * frame to be decoded.
	 */
	virtual OP_STATUS Pause() = 0;

	virtual OP_STATUS Suspend() = 0;
	virtual OP_STATUS Resume() = 0;

	/** Get the buffered time ranges.
	 *
	 * @param[out] ranges Set to the time ranges in seconds which are currently
	 *                    buffered and could be played back without network
	 *                    access (unless seeking is required to reach those
	 *                    ranges, in which case it cannot be guaranteed).
	 */
	virtual OP_STATUS GetBufferedTimeRanges(const OpMediaTimeRanges*& ranges) = 0;

	/** Get the seekable time ranges.
	 *
	 * @param[out] ranges Set to the time ranges in seconds for which
	 *                    SetPosition is expected (but not guaranteed)
	 *                    to succeed.
	 */
	virtual OP_STATUS GetSeekableTimeRanges(const OpMediaTimeRanges*& ranges) = 0;

	/** Get the played time ranges.
	 *
	 * @param[out] ranges Set to the normalized time ranges in seconds which have
	 *                    been played by the media player, at the time of the
	 *                    function call.
	 */
	virtual OP_STATUS GetPlayedTimeRanges(const OpMediaTimeRanges*& ranges) = 0;

	/** Is the media player currently seeking? */
	virtual BOOL GetSeeking() const = 0;

	/** Get the current playback position in seconds. */
	virtual OP_STATUS GetPosition(double &position) = 0;

	/** Set the current playback position in seconds. */
	virtual OP_STATUS SetPosition(double position) = 0;

	/** Get the duration of the resource in seconds.
	 *
	 * @param[out] duration Set to a non-negative number if the
	 *                      duration is known, NaN if the duration is
	 *                      unknown and Infinity if duration is
	 *                      unbounded (e.g. for web radio).
	 */
	virtual OP_STATUS GetDuration(double &duration) = 0;

	/** Set the playback rate.
	 *
	 * @param rate The new playback rate, MUST not be 0.
	 * @return OpStatus::OK if playback rate changed.
	 */
	virtual OP_STATUS SetPlaybackRate(double rate) = 0;

	/** Set the audio volume.
	 *
	 * @param volume The new volume, MUST be between 0 and 1 inclusive.
	 * @return OpStatus::OK or OpStatus::ERR_OUT_OF_RANGE.
	 */
	virtual OP_STATUS SetVolume(double volume) = 0;

	/** Set preload hint.
	 *
	 * @param preload If TRUE, preload as much of the resource as
	 *                possible. If FALSE, be conservative with
	 *                preloading data.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NOT_SUPPORTED.
	 */
	virtual OP_STATUS SetPreload(BOOL preload) = 0;

	/** Set a listener that receives player callbacks. */
	virtual void SetListener(MediaPlayerListener* listener) = 0;

	/** Get the aspect ratio-adjusted size of the video frame.
	 *
	 * @param[out] width The aspect ratio-adjusted width, or 0 if it
	 *                   is still unknown.
	 * @param[out] height The aspect ratio-adjusted height, or 0 if it
	 *                    is still unknown.
	 *
	 * Note: The size may be different from the physical size in
	 * pixels of the video frame.
	 */
	virtual void GetVideoSize(UINT32& width, UINT32& height) = 0;

	/** Get the last frame as an OpBitmap.
	 *
	 * @param bitmap set to an OpBitmap. The caller MUST NOT keep a
	 *               reference to this.
	 * @return OpStatus::OK, OpStatus::ERR if no frame is available
     *         yet, or OpStatus::ERR_NO_MEMORY.
	 */
	virtual OP_STATUS GetFrame(OpBitmap*& bitmap) = 0;

	/** Get the OpMediaHandle associated with this player.
	 *
	 * This is used for communicating with the platform.
	 */
	virtual OpMediaHandle GetHandle() = 0;

	/** Get the original URL for security purposes.
	 *
	 * @return the origin URL. An empty URL indicates unknown origin
	 *         and should be treated as unsafe.
	 */
	virtual URL GetOriginURL() = 0;

#ifdef PI_VIDEO_LAYER
	/** Does this player support using a platform video layer? */
	virtual BOOL SupportsVideoLayer() = 0;

	/** See OpMediaManager::GetVideoLayerColor. */
	virtual void GetVideoLayerColor(UINT8* red, UINT8* green, UINT8* blue, UINT8* alpha) = 0;

	/** See OpMediaPlayer::SetVideoLayerRect. */
	virtual void SetVideoLayerRect(const OpRect& rect, const OpRect& clipRect) = 0;
#endif //PI_VIDEO_LAYER
};

#endif // MEDIA_PLAYER_SUPPORT

#endif // OPMEDIAPLAYER_H
