/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_ELEMENT_H
#define MEDIA_ELEMENT_H

#ifdef MEDIA_HTML_SUPPORT

#include "modules/dom/domenvironment.h"
#include "modules/dom/domeventtypes.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/media/media.h"
#include "modules/media/mediaplayer.h"
#include "modules/media/src/controls/mediacontrols.h"
#include "modules/media/src/mediastate.h"
#include "modules/media/src/mediathread.h"

class ComplexAttr;
class FramesDocument;
class HTML_Element;
class MediaTrack;
class MediaTrackCue;
class MediaTrackList;
class OpBitmap;
class TrackDisplayState;
class URL;
class VisualDevice;

/** Reference to a source element from a MediaElement. */
class SourceElementRef
	: public ElementRef
{
public:
	SourceElementRef(MediaElement* media_element)
		: m_media_element(media_element) {}

	virtual	void OnDelete(FramesDocument* document) { Unreference(); }
	virtual	void OnRemove(FramesDocument* document);

private:
	MediaElement* m_media_element;
};

// DOM interface constants

/** HTMLMediaElement MediaError code. */
enum MediaError
{
	MEDIA_ERR_NONE = 0, // internal use
	MEDIA_ERR_ABORTED,
	MEDIA_ERR_NETWORK,
	MEDIA_ERR_DECODE,
	MEDIA_ERR_SRC_NOT_SUPPORTED
};

/** Implementation of the logic and painting of HTMLMediaElement. */
class MediaElement :
	public Media,
	private MessageObject,
	private ES_ThreadListener,
	private MediaPlayerListener,
	private MediaThreadQueueListener,
	private MediaControlsOwner
#ifdef PI_VIDEO_LAYER
	, private CoreViewScrollListener
#endif // PI_VIDEO_LAYER
{
public:
	MediaElement(HTML_Element* elm);
	virtual ~MediaElement();

	/** The aspect ratio adjusted width of the video. */
	virtual UINT32 GetIntrinsicWidth();

	/** The aspect ratio adjusted height of the video. */
	virtual UINT32 GetIntrinsicHeight();

	/** Is this image-like content? */
	virtual BOOL IsImageType() const { return GetElementType() == Markup::HTE_VIDEO; }

	/** Paint the current video frame, if any. */
	virtual OP_STATUS Paint(VisualDevice* visdev, OpRect video, OpRect content);

	/** Paint any controls currently active for this media element.
	 *
	 *  @param visdev  the current visual device.
	 *  @param content position and size of the content box.
	 */
	void PaintControls(VisualDevice* visdev, const OpRect& content);

	/* Handle mouse event */
	virtual void OnMouseEvent(DOM_EventType event_type, MouseButton button, int x, int y);

	/** Stop playback and free as many resources as possible. */
	virtual void Suspend(BOOL removed);

	/** Resume state from before Suspend was called. */
	virtual OP_STATUS Resume();

	/** An element was inserted or removed.
	 *
	 * This can be caused by parser, script or documentedit.
	 */
	OP_STATUS HandleElementChange(HTML_Element* element, BOOL inserted, ES_Thread* thread);

	/** The element was closed.
	 *
	 * Triggered by parser, including when setting innerHTML.
	 */
	void HandleEndElement() { SelectTracks(); }

	/** An attribute was added or modified.
	 *
	 * This can be caused by parser or script.
	 */
	OP_STATUS HandleAttributeChange(HTML_Element* element, short attr, ES_Thread* thread);

	/** Handle controls keyboard focus. */
	virtual void HandleFocusGained();

	/** Is the media element focusable? */
	virtual BOOL IsFocusable() const;

	/** Focus next internal media control */
	virtual BOOL FocusNextInternalTabStop(BOOL forward);

	/** Focus first internal media control when tab navigation enters element */
	virtual void FocusInternalEdge(BOOL forward);

	/** @return TRUE if the element is playing or otherwise in a state
	 *  where it should not yet be garbage collected. */
	BOOL GCIsPlaying() const;

	/** Can we play a media resource of the requested type?
	 *
	 * @param type the MIME type of a media resource, e.g.
	 *             "application/ogg; codecs=theora,vorbis"
	 * @return "no", "maybe" or "probably".
	 */
	const uni_char* CanPlayType(const uni_char* type);

	/** The media element load algorithm.
	 *
	 * http://www.whatwg.org/html#media-element-load-algorithm
	 *
	 * @param thread Current thread, can be NULL.
	 */
	OP_STATUS Load(ES_Thread* thread = NULL);

	/** Stops the playback and unloads the current resource.
	 *
	 * Playback is not possible until a new resource is loaded.
	 */
	void Unload() { DestroyPlayer(); MarkDirty(); }

#ifdef MEDIA_CAMERA_SUPPORT
	/** Set stream resource information.
	 *
	 *  A stream resource is a resource not associated with a URL but with
	 *  a Stream DOM object.
	 *  When a Stream object is assigned to HTML media element's src attribute,
	 *  the actual attribute must be set to a reserved "about:streamurl".
	 *
	 *  This function is used to set the media element to play a stream
	 *  resource.
	 *
	 *  @param stream_resource The stream resource to set.
	 */
	void SetStreamResource(MediaStreamResource* stream_resource);

	/** Clears the information about streaming resource. */
	void ClearStreamResource() { ClearStreamResource(TRUE); }
#endif // MEDIA_CAMERA_SUPPORT

	/** HTMLMediaElement play().
	 *
	 * @param thread Current thread, can be NULL.
	 */
	OP_STATUS Play(ES_Thread* thread = NULL);

	/** HTMLMediaElement pause().
	 *
	 * @param thread Current thread, can be NULL.
	 */
	OP_STATUS Pause(ES_Thread* thread = NULL);

	/** HTMLMediaElement MediaError code. */
    MediaError GetError() const { return m_error; }

	/** HTMLMediaElement currentSrc.
	 *
	 * @return the current source URL (never NULL).
	 */
	const URL* GetCurrentSrc() const;
	/** HTMLMediaElement networkState. */
	MediaNetwork GetNetworkState() const { return m_state.GetNetwork(); }
	/** HTMLMediaElement preload. */
	MediaPreload GetPreloadAttr() const;
	/** HTMLMediaElement buffered. */
	OP_STATUS GetBufferedTimeRanges(const OpMediaTimeRanges*& ranges);
	/** HTMLMediaElement readyState. */
	MediaReady GetReadyState() const { return m_state.GetReady(); }
	/** HTMLMediaElement seeking. */
	BOOL GetSeeking() const;
	/** HTMLMediaElement currentTime. */
	double GetPosition();
	/** HTMLMediaElement currentTime. */
	OP_STATUS SetPosition(const double position);
	/** HTMLMediaElement duration. */
	double GetDuration() const;
	/** HTMLMediaElement paused. */
	BOOL GetPaused() const { return m_paused; }
	/** HTMLMediaElement defaultPlaybackRate. */
	double GetDefaultPlaybackRate() const { return m_default_playback_rate; }
	/** HTMLMediaElement defaultPlaybackRate. */
	OP_STATUS SetDefaultPlaybackRate(const double rate);
	/** HTMLMediaElement playbackRate. */
	double GetPlaybackRate() const { return m_playback_rate; }
	/** HTMLMediaElement playbackRate. */
	OP_STATUS SetPlaybackRate(const double rate);
	/** HTMLMediaElement played. */
	OP_STATUS GetPlayedTimeRanges(const OpMediaTimeRanges*& ranges);
	/** HTMLMediaElement seekable. */
	OP_STATUS GetSeekableTimeRanges(const OpMediaTimeRanges*& ranges);
	/** HTMLMediaElement ended. */
	BOOL GetPlaybackEnded() const { return m_playback_ended; }
	/** HTMLMediaElement volume. */
	double GetVolume() const { return m_volume; }
	/** HTMLMediaElement volume. */
	OP_STATUS SetVolume(const double volume);
	/** HTMLMediaElement muted. */
	BOOL GetMuted() const { return m_muted; }
	/** HTMLMediaElement muted. */
	OP_STATUS SetMuted(const BOOL muted);
	/** HTMLVideoElement videoWidth. */
	inline UINT32 GetVideoWidth() { return m_video_width; }
	/** HTMLVideoElement videoHeight. */
	inline UINT32 GetVideoHeight() { return m_video_height; }

	/** Is the media element's delaying-the-load-event flag set?
	 *
	 * http://www.whatwg.org/html#delaying-the-load-event-flag
	 */
	virtual BOOL GetDelayingLoad() const;
	/** Get the internal MediaPlayer (may be NULL). */
	MediaPlayer* GetPlayer() const;
	/** Get the element type (audio or video) */
	HTML_ElementType GetElementType() const;

	/** Should the poster image be displayed? */
	BOOL DisplayPoster() const { return !m_poster_failed && (!m_have_played || !m_have_frame) && m_element->HasAttr(Markup::HA_POSTER); }
	/** The poster image is broken, fall back to the video itself. */
	void PosterFailed();

	/** Should controls be displayed? */
	virtual BOOL ControlsNeeded() const;

	/** Attach cues node fragments. */
	void AttachCues(HTML_Element* track_root);

	/** Get the position for the cue rooted in the HTML_Element passed in.
	 *
	 * @return The position of the cue.
	 */
	OpPoint GetCuePosition(HTML_Element* cue_root);

	/** Callback for when the layout engine has finished layouting a cue.
	 *
	 * @return The position of the cue.
	 */
	OpPoint OnCueLayoutFinished(HTML_Element* cue_root, int height, int first_line_height);

	enum TrackChangeReason
	{
		TRACK_CLEARED,
		/**< The cues of the track will be cleared. */

		TRACK_VISIBILITY,
		/**< The visibility of the track changed. */

		TRACK_READY
		/**< A track has finished loading and is ready for use. */
	};

	/** Signal that a change occured on the track 'track'. */
	void HandleTrackChange(MediaTrack* track, TrackChangeReason reason,
						   ES_Thread* thread = NULL);

	enum CueChangeReason
	{
		CUE_CONTENT_CHANGED,
		/**< The contents (node structure) of the cue changed. */

		CUE_TIME_CHANGED,
		/**< The start/end time of the cue changed. */

		CUE_LAYOUT_CHANGED,
		/**< The layout properties of the cue changed. */

		CUE_ADDED,
		/**< The cue was added to a track. */

		CUE_REMOVED
		/**< The cue was removed from a track. */
	};

	/** Signal that a change occurred on the cue 'cue'.
	 *
	 * The cue must be associated with a track.
	 */
	void HandleCueChange(MediaTrackCue* cue, CueChangeReason reason,
						 ES_Thread* thread);

	/** Callback for when the layout box size has changed.
	 *
	 * @param content_width The video viewport width.
	 * @param content_height The video viewport height.
	 * @return true if the change in size affected any cues.
	 */
	bool OnContentSize(int content_width, int content_height);

	/** Mark any cues as dirty.
	 *
	 * If there are showing tracks and those tracks are currently
	 * showing cues, mark the rendering representation of those cues
	 * (their HTML fragments) as dirty - including their properties -
	 * and recalculate their default positions and sizes.
	 *
	 * This is called when the video viewport size changes.
	 */
	void MarkCuesDirty();

	/** Read the dimension report by layout (via OnContentSize). */
	void GetContentSize(int& content_width, int& content_height);

	/** Get the list of tracks.
	 *
	 * The track list is created if none exists.
	 *
	 * @return track list, or NULL on OOM.
	 */
	MediaTrackList* GetTrackList();

	/** Get the 'visual' position of a track in the track list.
	 *
	 * Determines the position of a track by counting the number of
	 * preceding tracks that are 'showing*'.
	 * The track should be associated with this MediaElement.
	 */
	int GetVisualTrackPosition(MediaTrack* track);

	/** Get the position (index) of a track in the track list.
	 *
	 * The track should be associated with this MediaElement.
	 */
	int GetTrackListPosition(MediaTrack* track);

	/** Add a track created via HTMLMediaElement.addTextTrack(). */
	OP_STATUS DOMAddTextTrack(MediaTrack* track, DOM_Object* domtrack);

	/** Notify the MediaElement that a <track> was removed. */
	void NotifyTrackRemoved(HTML_Element* track_element) { HandleTrackRemoved(track_element); }

	/** Notify the MediaElement that a <source> was removed. */
	void NotifySourceRemoved(HTML_Element* source_element);

private:
	// MediaPlayerListener
	virtual void OnDurationChange(MediaPlayer* player);
	virtual void OnVideoResize(MediaPlayer* player);
	virtual void OnFrameUpdate(MediaPlayer* player);
	virtual void OnDecodeError(MediaPlayer* player);
	virtual void OnPlaybackEnd(MediaPlayer* player);
	virtual void OnSeekComplete(MediaPlayer* player);
	virtual void OnForcePause(MediaPlayer* player);
	virtual void OnProgress(MediaPlayer* player);
	virtual void OnStalled(MediaPlayer* player);
	virtual void OnError(MediaPlayer* player);
	virtual void OnIdle(MediaPlayer* player);
	virtual void OnPauseForBuffering(MediaPlayer* player);
	virtual void OnPlayAfterBuffering(MediaPlayer* player);

	// ComplexAttr // FIXME: implement properly
	virtual BOOL IsA(int type) { return type == T_UNKNOWN; }
	virtual OP_STATUS CreateCopy(ComplexAttr** copy_to) { return OpStatus::ERR_NOT_SUPPORTED; /* It will be created on demand in HTML_Element::GetMediaElement() */ }
	virtual BOOL MoveToOtherDocument(FramesDocument* old_document, FramesDocument* new_document) { return FALSE; }

	// MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// MediaThreadQueueListener
	virtual void OnTimeUpdateThreadsFinished();

	// MediaControlsOwner
	virtual double GetMediaDuration() const { return GetDuration(); }
	virtual double GetMediaPosition() { return GetPosition(); }
	virtual OP_STATUS SetMediaPosition(const double position) { return SetPosition(position); }
	virtual double GetMediaVolume() const { return GetVolume(); }
	virtual OP_STATUS SetMediaVolume(const double volume) { return SetVolume(volume); }
	virtual BOOL GetMediaMuted() const { return GetMuted(); }
	virtual OP_STATUS SetMediaMuted(const BOOL muted) { return SetMuted(muted); }
	virtual BOOL GetMediaPaused() const { return GetPaused() || GetPlaybackEnded(); }
	virtual OP_STATUS MediaPlay() { return Play(); }
	virtual OP_STATUS MediaPause() { return Pause(); }
	virtual const OpMediaTimeRanges* GetMediaBufferedTimeRanges();
	virtual BOOL UseControlsOpacity() const
	{
		return IsImageType()
#ifdef PI_VIDEO_LAYER
			&& (!m_use_video_layer || m_use_video_layer_opacity)
#endif // PI_VIDEO_LAYER
			;
	}
	virtual BOOL UseControlsFading() const { return IsImageType() && IsPotentiallyPlaying(); }
	virtual void SetControlsVisible(bool visible);

	// Helpers for above
	OP_STATUS SetCallbacks();
	void UnsetCallbacks();

	// ES_ThreadListener
	virtual OP_STATUS Signal(ES_Thread* thread, ES_ThreadSignal signal);

	/** Is this element potentially playing?
	 *
	 * http://www.whatwg.org/html#potentially-playing
	 */
	BOOL IsPotentiallyPlaying() const;

	/** Update delaying load state. */
	void SetDelayingLoad(BOOL delaying_load);

	/** Ensure that we have a FramesDocument and that it has us.
	 *
	 * A media element can be created by logdoc or via DOM. Try to
	 * find the owner FramesDocument via one of those, register with
	 * FramesDocument::AddMedia and cache a reference in m_frm_doc.
	 * OOM is handled internally.
	 *
	 * @return true if m_frm_doc is set (not NULL).
	 */
	bool EnsureFramesDocument();

	/** Get the associated DOM_Environment.
	 *
	 * If no environment exists one is constructed.
	 *
	 * @param[out] environment Set to the current DOM_Environment.
	 * @return OK if and only if the DOM_Environment could be found or
	 *         constructed successfully.
	 */
	OP_STATUS GetDOMEnvironment(DOM_Environment*& environment);

	/** Queue an event to be fired at an element.
	 *
	 *  @param target The HTML_Element on which to fire the event.
	 */
	void QueueDOMEvent(HTML_Element* target, DOM_EventType type);

	/** Queue a 'progress' event to be fired on the specified element.
	 *
	 *  This function updates the controls with the new buffered time ranges,
	 *  and then queues a DOM 'progress' event on the target element.
	 *
	 *  @param target The HTML_Element on which to fire the event.
	 */
	void QueueProgressEvent(HTML_Element* target);

	/** Invalidate the element for repainting. */
	void Update();

	/* The resource selection algorithm.
	 *
	 * http://www.whatwg.org/html#concept-media-load-algorithm
	 */
	enum SelectResourceState
	{
		SELECT_NULL,
		SELECT_LOADSTART,
		SELECT_FETCH_SRC,
		SELECT_FETCH_SRC_FAIL,
		SELECT_FETCH_SOURCE,
		SELECT_FETCH_SOURCE_FAIL,
		SELECT_WAIT_FOR_SOURCE
	};
	void SelectResourceInvoke(ES_Thread* thread);
	void SelectResource(ES_Thread* thread);
	// called from the fetch algorithm
	void SelectResourceReturn();

	/* The resource fetch algorithm.
	 *
	 * http://www.whatwg.org/html#concept-media-load-resource
	 */
	void FetchResourceInvoke(const URL& url);
	void FetchResource();
	/** @return TRUE if the resource fetch algorithm is running */
	BOOL InFetchResource() { return m_select_state == SELECT_FETCH_SRC || m_select_state == SELECT_FETCH_SOURCE; }

	/** Create the MediaPlayer.
	 *
	 * This will also cause the resource to start loading.
	 */
	OP_STATUS CreatePlayer();

	/** Destroy the MediaPlayer and clear related flags.
	 *
	 * This will also cause the resource to stop loading.
	 */
	void DestroyPlayer();

	/** Update preload state.
	 *
	 * Any resulting changes in buffering strategy are applied to
	 * MediaPlayer.
	 */
	void SetPreloadState(MediaPreload state) { m_state.SetPreload(state); HandleStateTransitions(); }

	/** Update the effective preload on MediaPlayer. */
	void UpdatePlayerPreload();

	/** Update network state and handle state transitions. */
	void SetNetworkState(MediaNetwork state) { m_state.SetNetwork(state); HandleStateTransitions(); }

	/** Perform ready state transition. */
	void SetReadyState(MediaReady state) { m_state.SetReady(state); HandleStateTransitions(); }

	/** Set playback ended flag and invalidate dependent state. */
	void SetPlaybackEnded(BOOL ended);

	/** Update the pending ready state based on preload and ended.
	 *
	 * Must be called from HandleStateTransitions or followed by a
	 * call to HandleStateTransitions.
	 */
	void UpdatePendingReady();

	/** Handle preload, network and ready state transitions. */
	void HandleStateTransitions();

	void HandleReadyStateTransition(MediaReady old_state, MediaReady new_state);

	// helper for source element iteration
	HTML_Element* SucSourceElement(HTML_Element* elem);

	// <track> element handling
	void HandleTrackAdded(HTML_Element* track_element, ES_Thread* thread);
	void HandleTrackRemoved(HTML_Element* track_element);

	OP_STATUS EnsureTrackList();
	void EnsureTrackState();
	void NotifyTrackAdded(MediaTrack* track);
	void SynchronizeTrackClock();
	void SynchronizeTracks(ES_Thread* thread = NULL);
	void SynchronizeTrack(MediaTrack* track, ES_Thread* thread = NULL);
	void SynchronizeCue(MediaTrackCue* cue, ES_Thread* thread = NULL);
	void AdvanceTracks(ES_Thread* thread = NULL);
	void UpdateTracks(bool seeking, ES_Thread* thread = NULL);
	void SelectTracks();

	void ScheduleTrackUpdate(double next_update);
	void CancelTrackUpdate();
	void ClearTrackTimeline();

	void InvalidatePosition(MediaTrack* track, bool invalidate_listpos = true);

	/** Fire progress event and set timeout for the next. */
	void HandleProgress();

	/** Used as parameter to HandleTimeUpdate() to specify if the event should
	    be queued unconditionally, or whether it can be skipped under some
	    circumstances. */
	enum TimeUpdateType
	{
		/** The event must be queued. */
		TIMEUPDATE_UNCONDITIONAL,
		/** The event can be skipped, if an event is already queued. */
		TIMEUPDATE_SKIPPABLE,
		/** The event must only be queued if we have previously skipped a
		    'timeupdate' event. Otherwise, it must be ignored. */
		TIMEUPDATE_DELAYED
	};

	/** Fire timeupdate event and start/stop periodic firing. */
	void HandleTimeUpdate(TimeUpdateType type = TIMEUPDATE_UNCONDITIONAL);

	/** Enable or disable media controls as needed.
	 *
	 * Controls should be enabled if the controls attribute is present
	 * or if scripts are disabled. OOM is handled internally.
	 *
	 * @return true if m_controls is set (not NULL).
	 */
	bool EnsureMediaControls();

	/** Get the area occupied by the controls (if any).
	 *
	 * @return The area within the content area that the controls
	 *         occupy, or an empty rectangle if the controls are
	 *         available but invisible, or unavailable.
	 */
	OpRect GetControlsArea();

	/** @return TRUE if media should autoplay */
	BOOL UseAutoplay() const;

	/** Mark the element as dirty after a (potential) resize. */
	void MarkDirty();

#ifdef MEDIA_CAMERA_SUPPORT
	/** Checks if the resource is a stream resource.
	 *
	 * @see SetStreamResource
	 */
	BOOL IsStreamResource() { return m_stream_resource != NULL; }

	/** Clears the information about streaming resource.
	 *
	 * @param repaint Whether to trigger a repaint.
	 */
	void ClearStreamResource(BOOL repaint);
#endif // MEDIA_CAMERA_SUPPORT

	URL m_url;

	/** Cached reference to owning FramesDocument.
	 *
	 * Non-NULL if and only if this MediaElement is added to the
	 * FramesDocument's collection of media elements.
	 */
	FramesDocument* m_frm_doc;
	MediaPlayer* m_player;
	MediaControls* m_controls;

	TrackDisplayState* m_track_state;

	UINT32 m_video_width, m_video_height;
	int m_content_width, m_content_height;

	MediaState m_state; // preload, network and ready states
	MediaError m_error;
	double m_default_playback_rate;
	double m_playback_rate;
	double m_volume;

	MediaThreadQueue m_thread_queue;

	SelectResourceState m_select_state;
	SourceElementRef m_select_pointer;
	SourceElementRef m_select_candidate;

#ifdef MEDIA_CAMERA_SUPPORT
	MediaStreamResource* m_stream_resource;
#endif // MEDIA_CAMERA_SUPPORT

	MediaTrackList* m_track_list;
	double m_last_track_time;
	double m_last_track_realtime;

	unsigned m_paused:1;
	unsigned m_playback_ended:1;
	unsigned m_muted:1;
	unsigned m_flag_autoplaying:1;
	unsigned m_flag_delaying_load:1;
	unsigned m_has_set_callbacks:1;
	unsigned m_fired_loadeddata:1;
	unsigned m_suspended:1;
	unsigned m_poster_failed:1;
	unsigned m_have_played:1; // Has played or a seek has been performed.
	unsigned m_have_frame:1;
	unsigned m_invalidate_controls:1;
	unsigned m_select_resource_pending:1;
	unsigned m_select_tracks_pending:1;
	unsigned m_track_update_pending:1;
	unsigned m_track_sync_pending:1;
	unsigned m_track_sync_seeking:1;
	unsigned m_cached_linepositions_valid:1;
	unsigned m_cached_listpositions_valid:1;
	unsigned m_progress_state:2; // See MediaProgressState.
	unsigned m_timeupdate_state:1; // See MediaTimeUpdateState.
#ifdef MEDIA_CAMERA_SUPPORT
	unsigned m_is_stream_resource:1;
#endif // MEDIA_CAMERA_SUPPORT
#ifdef DEBUG_ENABLE_OPASSERT
	unsigned m_paused_for_buffering:1; // See OnPauseForBuffering/OnPlayAfterBuffering
	unsigned m_in_state_transitions:1; // See HandleStateTransitions.
#endif // DEBUG_ENABLE_OPASSERT

#ifdef PI_VIDEO_LAYER
	/** Update the position and size of the platform's video layer. */
	void UpdateVideoLayerRect(VisualDevice* visdev = NULL);
	// CoreViewScrollListener
	virtual void OnScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason);
	virtual void OnParentScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason);
	OpRect m_doc_rect;
	unsigned m_use_video_layer:1;
	unsigned m_use_video_layer_opacity:1;
#endif // PI_VIDEO_LAYER
};

#endif // MEDIA_HTML_SUPPORT

#endif // MEDIA_ELEMENT_H
