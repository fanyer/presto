/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPMEDIAPLAYER_H
#define OPMEDIAPLAYER_H

#include "modules/util/adt/opvector.h"

/** @short Opaque handle to a media resource in core, used by the
 *  platform to identify the origin via e.g. windowcommander APIs.
 */
typedef class MediaHandle* OpMediaHandle;

class OpMediaPlayer;

/** @short Media manager. Knows what media types and URL:s are
 *  supported and how to create players for them.
 */
class OpMediaManager
{
public:
	/** @short Create a new OpMediaManager object.
	 *
	 * @param[out] manager Set to the new OpMediaManager instance.
	 */
	static OP_STATUS Create(OpMediaManager** manager);

	virtual ~OpMediaManager() {}

	/** @short Test if a given media type with codecs is supported.
	 *
	 * @param type The MIME media type on form "type/subtype", not
	 * including any parameters.
	 * @param codecs The list of codecs used in the resource as given
	 * by the codecs parameter of a full MIME media type.
	 * @return YES if the media type and all codecs are known to be
	 * supported, NO if either the media type or any codec is known to
	 * be unsupported, otherwise MAYBE. For container types such as
	 * Ogg (video/ogg) or MPEG-4 (video/mp4), codecs must be given in
	 * order to return YES, as the codecs used are otherwise unknown.
	 *
	 * Example: for MIME media type "video/ogg; codecs=theora,vorbis",
	 * type is "video/ogg" and codecs is a vector of "theora" and
	 * "vorbis". Implementations should return YES if they can demux
	 * Ogg streams and decode both Theora video and Vorbis audio.
	 */
	virtual BOOL3 CanPlayType(const OpStringC8& type, const OpVector<OpStringC8>& codecs) = 0;

	/** @short Test if a given URL can be handled without core.
	 *
	 * @param url URL of the media resource.
	 *
	 * By returning TRUE, the implementation indicates that it is
	 * capable of downloading the media resource without involvement
	 * from core.
	 *
	 * Example: By returning TRUE for URLs beginning with rtsp://,
	 * the implementation can handle streaming over RTSP.
	 */
	virtual BOOL CanPlayURL(const uni_char* url) = 0;

	/** @short Create a new OpMediaPlayer object.
	 *
	 * This method is called if CanPlayURL returned FALSE.
	 *
	 * The implementation must obtain an OpMediaSource via
	 * windowcommander from which to read data.
	 *
	 * @param[out] player Set to the new OpMediaPlayer instance.
	 * @param handle Handle to the core media instance.
	 */
	virtual OP_STATUS CreatePlayer(OpMediaPlayer** player, OpMediaHandle handle) = 0;

	/** @short Create a new OpMediaPlayer object.
	 *
	 * This method is called if CanPlayURL returned TRUE.
	 *
	 * @param[out] player Set to the new OpMediaPlayer instance.
	 * @param handle Handle to the core media instance.
	 * @param url URL of the media resource.
	 */
	virtual OP_STATUS CreatePlayer(OpMediaPlayer** player, OpMediaHandle handle, const uni_char* url) = 0;

	/** @short Destroy an OpMediaPlayer instance
	 *
	 * @param player Player to destroy.
	 */
	virtual void DestroyPlayer(OpMediaPlayer* player) = 0;

#ifdef PI_VIDEO_LAYER
	/** Get the video layer transparency/chroma color.
	 *
	 * When the video content area is rendered in core, this color is used
	 * instead of the video bitmap.
	 *
	 * param[out] red Red component of the transparency/chroma color.
	 * param[out] green Green component of the transparency/chroma color.
	 * param[out] blue Blue component of the transparency/chroma color.
	 * param[out] alpha Alpha component of the transparency/chroma color.
	 */
	static void GetVideoLayerColor(UINT8* red, UINT8* green, UINT8* blue, UINT8* alpha);
#endif //PI_VIDEO_LAYER
};

/** Simple interface used for querying which time ranges the player instance has buffered and can seek to. */
class OpMediaTimeRanges
{
public:
	virtual ~OpMediaTimeRanges() { }

	/** @return the number of ranges */
	virtual UINT32 Length() const = 0;

	/** @return the start position of the idx'th range */
	virtual double Start(UINT32 idx) const = 0;

	/** @return the end position of the idx'th range */
	virtual double End(UINT32 idx) const = 0;
};

/** Interface which must be implemented in order to receive playback events from OpMediaPlayer. */
class OpMediaPlayerListener
{
public:
	virtual ~OpMediaPlayerListener() { }

	/** The length of the media resource has become known or changed. */
	virtual void OnDurationChange(OpMediaPlayer* player) = 0;

	/** The size of the video has become known or changed. */
	virtual void OnVideoResize(OpMediaPlayer* player) = 0;

	/** A new video frame is available. */
	virtual void OnFrameUpdate(OpMediaPlayer* player) = 0;

	/** The media resource cannot be decoded. */
	virtual void OnDecodeError(OpMediaPlayer* player) = 0;

	/** Playback has reached the end of the media resource.
	 *
	 * This function may only be called if the duration has become known.
	 * In other words, only call this function if OnDurationChange has
	 * been called at least once.
	 */
	virtual void OnPlaybackEnd(OpMediaPlayer* player) = 0;

	/** Seek has finished. */
	virtual void OnSeekComplete(OpMediaPlayer* player) = 0;

	/** Force the playback to pause. Used by backends with limited resources. */
	virtual void OnForcePause(OpMediaPlayer* player) = 0;

	/** Playback has paused due to buffering underrun. */
	virtual void OnPauseForBuffering(OpMediaPlayer* player) = 0;

	/** Playback continued after recovering from buffering underrun. */
	virtual void OnPlayAfterBuffering(OpMediaPlayer* player) = 0;

	/* The following buffering callbacks should only be used by
	 * instances handling the source internally (see CanPlayURL). */

	/** Buffering is progressing normally. */
	virtual void OnProgress(OpMediaPlayer* player) = 0;

	/** Buffering has not progressed for some time. */
	virtual void OnStalled(OpMediaPlayer* player) = 0;

	/** Buffering has stopped due to network error. */
	virtual void OnError(OpMediaPlayer* player) = 0;

	/** Buffering has stopped due to buffering strategy. */
	virtual void OnIdle(OpMediaPlayer* player) = 0;
};

/** @short A media player for playback of audio and video resources.
 *
 * An implementation of OpMediaPlayer is required to support playback
 * of WAV PCM audio, other supported formats depend on the underlying
 * media framework. An instance of OpMediaPlayer is bound to a single
 * resource, to play another resource a new instance must be created.
 */
class OpMediaPlayer
{
public:

	/** Play the media resource. */
	virtual OP_STATUS Play() = 0;

	/** Pause the media resource.
	 *
	 * Pausing a newly created player should cause the first video frame to be
	 * decoded.
	 */
	virtual OP_STATUS Pause() = 0;

	/** Get buffered time ranges.
	 *
	 * @param[out] ranges Set to the time ranges in seconds which are currently
	 * buffered and could be played back without network access (unless seeking
	 * is required to reach those ranges, in which case it cannot be
	 * guaranteed).
	 */
	virtual OP_STATUS GetBufferedTimeRanges(const OpMediaTimeRanges** ranges) = 0;

	/** Get seekable time ranges.
	 *
	 * @param[out] ranges Set to the time ranges in seconds for which
	 * SetPosition() is expected (but not guaranteed) to succeed.
	 */
	virtual OP_STATUS GetSeekableTimeRanges(const OpMediaTimeRanges** ranges) = 0;

	/** Get current playback position.
	 *
	 * @param[out] position Set to current playback position in seconds
	 */
	virtual OP_STATUS GetPosition(double* position) = 0;

	/** Set playback position.
	 *
	 * @param position The new playback position in seconds
	 * @return OK if seeking to the position has begun, or
	 * ERR_OUT_OF_RANGE if the resource is not seekable.
	 */
	virtual OP_STATUS SetPosition(double position) = 0;

	/** Get duration of stream.
	 *
	 * @param[out] duration Set to stream duration in seconds. Will be a
	 * non-negative number if the duration is known, NaN if the duration is
	 * unknown and Infinity if duration is unbounded (e.g. for web radio).
	 */
	virtual OP_STATUS GetDuration(double* duration) = 0;

	/** Set playback rate.
	 *
	 * @param rate The new playback rate, where 1.0 is the default
	 * rate. Implementations are encouraged but not required to
	 * support other playbacks rate than 1.0.
	 * @return OK if playback rate changed or ERR_NOT_SUPPORTED if the
	 * requested playback rate cannot be achieved (likely for negative
	 * values).
	 */
	virtual OP_STATUS SetPlaybackRate(double rate) = 0;

	/** Set sound volume.
	 *
	 * @param volume The new volume. MUST be between 0 and 1 inclusive.
	 * @return OK or ERR_OUT_OF_RANGE
	 */
	virtual OP_STATUS SetVolume(double volume) = 0;

	/** Set the preload duration in seconds.
	 *
	 * @param duration A positive number of seconds worth of data to
	 * preload, even if it is not immediately needed for decoding.
	 * Infinity is allowed and indicates that as much data as possible
	 * should be preloaded.
	 *
	 * @return OK or ERR_NOT_SUPPORTED
	 */
	virtual OP_STATUS SetPreload(double duration) = 0;

	/** Set listener.
	 *
	 * @param listener the listener which is to receive callbacks from this
	 * player
	 */
	virtual void SetListener(OpMediaPlayerListener* listener) = 0;

	/** Get the last decoded frame as an OpBitmap.
	 *
	 * @param[out] bitmap set to an OpBitmap. The caller MUST NOT keep a
	 * reference to this.
	 * @return OK or ERR_NO_MEMORY
	 */
	virtual OP_STATUS GetFrame(OpBitmap** bitmap) = 0;

	/** Get the native video dimensions.
	 *
	 * The display size of the video is calculated based on the native
	 * size, which may be different from the size of the OpBitmap
	 * returned by GetFrame().
	 *
	 * @param[out] width The width of the video in pixels.
	 * @param[out] height The height of the video in pixels.
	 * @param[out] pixel_aspect The native pixel aspect ratio of the
	 * video. This is usually 1.0 (1:1), but for other values the
	 * video will be stretched by increasing (and rounding to integer)
	 * either width or height on playback. For example, a 960x720
	 * video with pixel aspect ratio 1.333 (4:3) will be stretched
	 * horizontally to 1280x720 (a.k.a. anamorphic widescreen).
	 */
	virtual OP_STATUS GetNativeSize(UINT32* width, UINT32* height, double* pixel_aspect) = 0;

#ifdef PI_VIDEO_LAYER
	/** Set the position and size of the video layer.
	 *
	 * This is called when the video becomes visible, moves or is hidden. The
	 * coordinates are relative to the upper left corner of the containing
	 * Opera window. Passing an empty rect means that there's nothing for the
	 * platform to paint.
	 *
	 * @param rect Coordinates of the display rectangle.
	 * @param clipRect Coordinates of the clip rectangle.
	 */
	virtual void SetVideoLayerRect(const OpRect& rect, const OpRect& clipRect) = 0;
#endif //PI_VIDEO_LAYER

protected:
	/** Destructor can not be called directly. Use OpMediaManager::DestroyPlayer
	 *  to destroy OpMediaPlayer instances.
	 */
	virtual ~OpMediaPlayer() {}
};

#endif // OPMEDIAPLAYER_H
