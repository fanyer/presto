/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef JILMEDIAELEMENT_H
#define JILMEDIAELEMENT_H

#ifdef MEDIA_JIL_PLAYER_SUPPORT

#include "modules/logdoc/complex_attr.h"
#include "modules/dom/domeventtypes.h"
#include "modules/media/mediaplayer.h"
#include "modules/media/media.h"
#include "modules/media/src/controls/mediacontrols.h"

class JILMediaListener
{
public:
	/** Callback invoked when requested media resource was either
		successfully opened or error occured */
	virtual void OnOpen(BOOL error) = 0;

	/** Callback invoked after playback was sucessfully started
	(or resumed after being paused) */
	virtual void OnPlaybackStart() = 0;

	/** Callback invoked after playback was stopped */
	virtual void OnPlaybackStop() = 0;

	/** Callback invoked after playback was paused */
	virtual void OnPlaybackPause() = 0;

	/** Callback invoked on playback end */
	virtual void OnPlaybackEnd() = 0;
};

class MediaPlayerListener;
class VisualDevice;
class HTML_Element;
class FramesDocument;
class URL;
class OpBitmap;
class MediaControls;

class JILMediaElement :
	public Media,
	private MediaPlayerListener,
	private MediaControlsOwner,
	private OpTimerListener
{
public:
	JILMediaElement(HTML_Element* el);
	virtual ~JILMediaElement();

	void SetListener(JILMediaListener* listener) { m_listener = listener; }

	/** The aspect ratio adjusted width of the video. */
	virtual UINT32 GetIntrinsicWidth();

	/** The aspect ratio adjusted height of the video. */
	virtual UINT32 GetIntrinsicHeight();

	/** Is this image-like content? */
	virtual BOOL IsImageType() const { return TRUE; }

	/** Paint the current video frame, if any. */
	virtual OP_STATUS Paint(VisualDevice* vis_dev, OpRect video, OpRect content);

	/* Handle mouse event */
	virtual void OnMouseEvent(DOM_EventType event_type, MouseButton button, int x, int y);

	/** Called when media controls gain keyboard focus */
	virtual void HandleFocusGained();

	/** Is the media element focusable? */
	virtual BOOL IsFocusable() const;

	/** Focus next internal media control */
	virtual BOOL FocusNextInternalTabStop(BOOL forward);

	/** Focus first internal media control when tab navigation enters element */
	virtual void FocusInternalEdge(BOOL forward);

	/** Stop playback and free as many resources as possible. */
	virtual void Suspend(BOOL removed);

	/** Resume state from before Suspend was called. */
	virtual OP_STATUS Resume();

	/** Is the media element's delaying-the-load-event flag set? */
	virtual BOOL GetDelayingLoad() const { return FALSE; }

	//Opens a media content
	OP_STATUS Open(const URL &url);

	//Starts playback
	OP_STATUS Play(BOOL resume_playback);

	//Pauses playback
	OP_STATUS Pause();

	//Stops playback
	OP_STATUS Stop();

	OP_STATUS EnableControls(BOOL enable);

	/* Takes overs the player and the internal state of src_element
	 * If this operation fails then the src_element remains unchanged and
	 * the state of the object on which the method was called is undefined
	 * so the object should be discarded.
	 */
	OP_STATUS TakeOver(JILMediaElement* src_element);

private:
	// MediaPlayerListener
	virtual void OnDurationChange(MediaPlayer* player);
	virtual void OnVideoResize(MediaPlayer* player);
	virtual void OnFrameUpdate(MediaPlayer* player);
	virtual void OnDecodeError(MediaPlayer* player);
	virtual void OnPlaybackEnd(MediaPlayer* player);
	virtual void OnError(MediaPlayer* player);

	// MediaControlsOwner
	virtual double GetMediaDuration() const;
	virtual double GetMediaPosition();
	virtual OP_STATUS SetMediaPosition(const double position);
	virtual double GetMediaVolume() const { return m_volume; }
	virtual OP_STATUS SetMediaVolume(const double volume);
	virtual BOOL GetMediaMuted() const { return m_muted; }
	virtual OP_STATUS SetMediaMuted(const BOOL muted);
	virtual BOOL GetMediaPaused() const { return m_paused || m_playback_ended; }
	virtual OP_STATUS MediaPlay() { return Play(TRUE); }
	virtual OP_STATUS MediaPause() { return Pause(); }
	virtual const OpMediaTimeRanges* GetMediaBufferedTimeRanges();
	virtual BOOL UseControlsOpacity() const { return TRUE; }
	virtual BOOL UseControlsFading() const { return TRUE; }
	virtual void SetControlsVisible(bool visible) {}

	//Timer for progress bar updates
	virtual void OnTimeOut(OpTimer* timer);

	/** Find the owning FramesDocument and cache it in m_frm_doc. */
	OP_STATUS EnsureFramesDocument();

	/** Should controls be displayed? */
	virtual BOOL ControlsNeeded() const;

	/** Call when the element has been updated and need to be
		repainted. */
	void Update(BOOL just_control_area = FALSE);

	// fire timeupdate event and start/stop periodic firing
	OP_STATUS HandleTimeUpdate(BOOL schedule_next_update);

	//Sets playback position
	OP_STATUS SetPosition(const double position);

	/* Control media controls depending on element state */
	OP_STATUS ManageMediaControls();

	/* Frees the player and controls and disables timers and unregisters from frame document. */
	void Cleanup();
	double GetDuration() const;

	JILMediaListener* m_listener;
	OpTimer m_progress_update_timer;

	FramesDocument* m_frm_doc;
	MediaPlayer* m_player;
	MediaControls* m_controls;

	UINT32 m_video_width, m_video_height;

	unsigned m_paused:1;			// TRUE always when not playing
	unsigned m_suspended:1;
	unsigned m_has_controls:1;
	unsigned m_muted:1;
	unsigned m_playback_ended:1;	// FALSE when playing

	double m_volume;
};

#endif // MEDIA_JIL_PLAYER_SUPPORT

#endif //JILMEDIAELEMENT_H
