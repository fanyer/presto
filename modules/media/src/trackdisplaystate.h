/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef TRACKDISPLAYSTATE_H
#define TRACKDISPLAYSTATE_H

#ifdef MEDIA_HTML_SUPPORT

class HTML_Element;
class LogicalDocument;

#include "modules/media/mediatrack.h"
#include "modules/util/simset.h"

class TrackDisplayState
{
public:
	TrackDisplayState(int viewport_width, int viewport_height);
	~TrackDisplayState();

	/** Add display state for a cue.
	 *
	 * Called by the track/cue scheduler when a cue becomes active on the
	 * timeline ('active' flag transitions from 'false' to 'true').
	 */
	OP_STATUS AddCue(MediaTrackCue* cue, FramesDocument* frm_doc,
					 HTML_Element* track_root);

	/** Remove display state for a cue.
	 *
	 * Called by the track/cue scheduler when a cue that was
	 * previously active on the timeline, no longer is ('active' flag
	 * transitions from 'true' to 'false').
	 */
	void RemoveCue(MediaTrackCue* cue, FramesDocument* frm_doc);

	/** Add display state for all active cues of a track. */
	OP_STATUS AddCuesForTrack(MediaTrack* track, FramesDocument* frm_doc,
							  HTML_Element* track_root);

	/** Remove display state for all cues of a track.
	 *
	 * Remove all cues that are associated with 'track'.
	 */
	void RemoveCuesForTrack(MediaTrack* track, FramesDocument* frm_doc);

	/** Remove display state for all cues. */
	void Clear(FramesDocument* frm_doc);

	/** Update state for intra-cue timestamps.
	 *
	 * Go through all pending and active cues and reset or advance
	 * (respectively) the intra-cue state.
	 */
	void UpdateIntraCueState(FramesDocument* doc, double current_time);

	/** Get the time of the occurence of the next cue event - if any.
	 *
	 * @return The next time an event will occur - timestamp or end -
	 *         or NaN if none.
	 */
	double NextEventTime();

	/** Attach cue rendering fragments.
	 *
	 * Called by layout during reflow when the box for the
	 * <video-tracks> (the actual track root) has been created. This
	 * means cue fragments should be reparented from the <video> to
	 * the <video-tracks> element.
	 */
	void AttachCues(HTML_Element* track_root);

	/** Called by layout to get the current position of a cue. */
	OpPoint GetCuePosition(HTML_Element* cue_root);
	/** Called by layout when a cues box has finished layouting. */
	OpPoint OnCueLayoutFinished(HTML_Element* cue_root, int height, int first_line_height);

	/** Mark the cue as dirty. */
	void MarkCueDirty(FramesDocument* frm_doc, MediaTrackCue* cue);

	/** Mark the cue as extra dirty. */
	void MarkCueExtraDirty(FramesDocument* frm_doc, MediaTrackCue* cue);

	/** Mark all cues as dirty - in response to the video viewport changing size. */
	void MarkCuesDirty(FramesDocument* frm_doc);

	/** Signal a change in the size of the video content.
	 *
	 * Called by layout when the size of the content box or the
	 * visibility of the controls change.
	 *
	 * @return true if the size changed, false otherwise.
	 */
	bool OnContentSize(int content_width, int content_height);

	/** Signal a change in the visibility of video controls.
	 *
	 * Called by media code when the visibility of the controls
	 * change. This is communicated as a rectangle. If the rectangle
	 * is empty the controls are invisible.
	 *
	 * @return true if the controls area was updated, false otherwise.
	 */
	bool OnControlsVisibility(const OpRect& controls_area);

	/** Invalidate the layout of all cues.
	 *
	 * Called in response to a true return from OnContentSize() or
	 * OnControlsVisibility() - or when the cue layout state needs to
	 * be invalidated for any other reason. Will reset all cues to
	 * their default position.
	 *
	 * @return true if the layout was updated, false otherwise.
	 */
	bool InvalidateCueLayout(FramesDocument* doc, bool mark_dirty = true);

	/** Get the track root to attach cue fragments to. */
	static HTML_Element* GetActualTrackRoot(HTML_Element* video_element);

private:
	/** Find the display state for cue. */
	MediaCueDisplayState* GetDisplayState(MediaTrackCue* cue);

	/** Remove the display state. */
	void RemoveDisplayState(MediaCueDisplayState* cuestate, FramesDocument* frm_doc);

	/** Mark any successors in the active list as dirty. */
	void MarkActiveSuccessorsDirty(FramesDocument* frm_doc,
								   MediaCueDisplayState* stop);

	/** Find the <track-root> element from the <video> element. */
	static HTML_Element* GetTrackRoot(HTML_Element* video_element);

	List<MediaCueDisplayState> m_active_cues;
	List<MediaCueDisplayState> m_pending_cues;

	MediaCueDisplayState m_controls; ///< Dummy cue state for controls.

	int m_viewport_width;	///< Width of viewport (== video content).
	int m_viewport_height;	///< Height of viewport (== video content).
};

class DOM_Environment;

/** Helper class for handling track updates ("ticks").
 *
 * Tracks the generated events, sorts and dispatches them as specified
 * in:
 *
 * http://www.whatwg.org/html#playing-the-media-resource
 */
class TrackUpdateState
{
public:
	TrackUpdateState() :
		m_events(m_stack_events),
		m_event_count(0),
		m_size(ARRAY_SIZE(m_stack_events)),
		m_seen_pause_on_exit(false) {}
	~TrackUpdateState() { FreeEvents(); }

	/** Signal a cue started. */
	inline OP_STATUS Started(MediaTrackCue* cue);

	/** Signal a cue ended. */
	inline OP_STATUS Ended(MediaTrackCue* cue);

	/** Signal a cue was missed (skipped). */
	inline OP_STATUS Missed(MediaTrackCue* cue);

	/** Make sure events are in the specified order. */
	void SortEvents();

	/** Dispatch 'enter' and 'exit' events to cues. */
	void DispatchCueEvents(DOM_Environment* environment);

	/** Update the active state ('flag') of the cues.
	 *
	 * @param track_state Track display state if there are visual
	 *                    tracks associated with the MediaElement
	 *                    (the caller). NULL if not.
	 * @param frm_doc The FramesDocument of the MediaElement.
	 * @param track_root The HTML_Element root to which cues will be
	 *                   added. Will be the MediaElement's
	 *                   HTML_Element. Not used if track_state is
	 *                   NULL.
	 */
	void UpdateActive(TrackDisplayState* track_state, FramesDocument* frm_doc,
					  HTML_Element* track_root);

	/** Did any of the cues that 'exit'ed signal a pause? */
	bool HasPauseOnExit() const { return m_seen_pause_on_exit; }

	/** Were there any events at all? */
	bool HasCueEvents() const { return m_event_count != 0; }

	/** How many events have been emitted? */
	unsigned EventCount() const { return m_event_count; }

private:
	class Event
	{
	public:
		Event() {}
		Event(MediaTrackCue* c, bool is_start, bool is_missed)
		{
			event_data = reinterpret_cast<UINTPTR>(c);
			event_data |= is_start ? IS_START : 0;
			event_data |= is_missed ? CUE_MISSED : 0;
		}

		MediaTrackCue* GetCue() const
		{
			return reinterpret_cast<MediaTrackCue*>(event_data & ~(UINTPTR)ALL_BITS);
		}
		double GetTime() const
		{
			MediaTrackCue* cue = GetCue();
			return IsStart() ? cue->GetStartTime() : cue->GetEndTime();
		}
		DOM_EventType GetEventType() const
		{
			return IsStart() ? MEDIACUEENTER : MEDIACUEEXIT;
		}

		bool IsStart() const { return (event_data & IS_START) != 0; }
		bool IsMissed() const { return (event_data & CUE_MISSED) != 0; }

	private:
		// The below are "pointer tags" stored in the low 2 bits of
		// the event_data member. They are used for event attributes
		// to save space.
		enum
		{
			IS_START	= (1 << 0),
			CUE_MISSED	= (1 << 1),

			ALL_BITS	= IS_START | CUE_MISSED
		};

		UINTPTR event_data;
	};

	Event* NewEvent();
	void FreeEvents()
	{
		if (m_events != m_stack_events)
			OP_DELETEA(m_events);
	}
	OP_STATUS EmitEvent(MediaTrackCue* cue, bool is_start, bool is_missed = false);

	static int OrderEvents(const void* a, const void* b);

	// Keep a small number of pre-allocated Events to avoid allocating
	// memory in the more common cases (assuming a small number of
	// cues need updating each tick).
	Event m_stack_events[8]; // ARRAY OK 2012-02-27 fs
	Event* m_events;
	unsigned m_event_count;
	unsigned m_size;
	bool m_seen_pause_on_exit;
};

inline OP_STATUS
TrackUpdateState::Started(MediaTrackCue* cue)
{
	return EmitEvent(cue, true /* start event */);
}

inline OP_STATUS
TrackUpdateState::Ended(MediaTrackCue* cue)
{
	return EmitEvent(cue, false /* end event */);
}

inline OP_STATUS
TrackUpdateState::Missed(MediaTrackCue* cue)
{
	RETURN_IF_ERROR(EmitEvent(cue, true /* start */, true /* missed */));
	return EmitEvent(cue, false /* end */, true /* missed */);
}

#endif // MEDIA_HTML_SUPPORT
#endif // !TRACKDISPLAYSTATE_H
