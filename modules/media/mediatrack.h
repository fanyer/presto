/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_TRACK_H
#define MEDIA_TRACK_H

#ifdef MEDIA_HTML_SUPPORT

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/logdoc/complex_attr.h"
#include "modules/media/src/trackloader.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/tempbuf.h"
#include "modules/windowcommander/WritingSystem.h"

class DOM_Environment;
class DOM_Object;
class MediaElement;

/** Base class for track resources that are exposed in the DOM.
 *
 * This class is meant to carry all the DOM mechanics for track
 * resources such as cues, tracks and lists of these.
 */
class MediaDOMItem
{
public:
	MediaDOMItem() : m_dom_object(NULL) {}
	virtual ~MediaDOMItem() {}

	DOM_Object* GetDOMObject() const { return m_dom_object; }
	void SetDOMObject(DOM_Object* dom_object) { m_dom_object = dom_object; }

	static void DetachOrDestroy(MediaDOMItem* domitem);

protected:
	DOM_Object* m_dom_object;
};

enum MediaTrackCueDirection
{
	CUE_DIRECTION_HORIZONTAL,
	CUE_DIRECTION_VERTICAL,
	CUE_DIRECTION_VERTICAL_LR
};

enum MediaTrackCueAlignment
{
	CUE_ALIGNMENT_START,
	CUE_ALIGNMENT_MIDDLE,
	CUE_ALIGNMENT_END
};

class MediaTrack;
class WVTT_Node;

/** Wrapper for a const uni_char string with known length.
 *
 * The string may contain null characters and is not required to be
 * null-terminated.
 */
class StringWithLength
{
public:
	StringWithLength(const uni_char* string, unsigned length) :
		string(string), length(length) {}

	explicit StringWithLength(const TempBuffer& buf) :
		string(buf.GetStorage()), length(buf.Length()) {}

	explicit StringWithLength(const ES_ValueString& val) :
		string(val.string), length(val.length) {}

	bool operator == (const StringWithLength& other) const
	{
		return length == other.length &&
			uni_strncmp(string, other.string, length) == 0;
	}

	const uni_char *string;
	unsigned length;
};

/** Representation of a cue.
 *
 * Closely resembles TextTrackCue in the specification, and is
 * essentially a 'blob of data'.
 * Generally stored in a MediaTrackCueList, but can also be kept
 * separately (almost exclusively in the context of the DOM).
 */
class MediaTrackCue :
	public MediaDOMItem,
	public ListElement<MediaTrackCue>
{
public:
	MediaTrackCue() :
		m_start_time(0.0),
		m_end_time(0.0),
		m_track(NULL),
		m_identifier(NULL, 0),
		m_text(NULL, 0),
		m_cue_nodes(NULL),
		m_order(UINT_MAX),
		m_line_pos(0),
		m_text_pos(50),
		m_size(100),
		m_snap_to_lines(true),
		m_pause_on_exit(false),
		m_line_pos_is_auto(true),
		m_alignment(CUE_ALIGNMENT_MIDDLE),
		m_direction(CUE_DIRECTION_HORIZONTAL) {}
	virtual ~MediaTrackCue();

	static OP_STATUS DOMCreate(MediaTrackCue*& cue,
							   double start_time, double end_time,
							   const StringWithLength& text);

	/** Create a DocumentFragment for this cue.
	 *
	 * As described by:
	 *  http://dev.w3.org/html5/webvtt/#webvtt-cue-text-dom-construction-rules
	 *
	 * The result is attached to the root element passed.
	 */
	OP_STATUS DOMGetAsHTML(HLDocProfile* hld_profile, HTML_Element* root);

	/** Set/get the track which owns this cue.
	 *
	 * If this cue is not currently owned by any track this should be
	 * NULL. It is the responsibility of the code that adds/removes
	 * from a cuelist to update this.
	 */
	void SetOwnerTrack(MediaTrack* track) { m_track = track; }
	MediaTrack* GetOwnerTrack() const { return m_track; }

	/** Set/get a value indicating insertion order of the cue. */
	void SetOrder(unsigned int order) { m_order = order; }
	unsigned int GetOrder() const { return m_order; }

	/** Get the MediaElement this cue is associated with, if any. */
	MediaElement* GetMediaElement() const;

	OP_STATUS SetIdentifier(const StringWithLength& identifier)
	{
		return SetNonEmptyString(m_identifier, identifier);
	}
	const StringWithLength& GetIdentifier() const { return m_identifier; }

	OP_STATUS SetText(const StringWithLength& text)
	{
		return SetNonEmptyString(m_text, text);
	}
	const StringWithLength& GetText() const { return m_text; }

	void SetStartTime(double start_time) { m_start_time = start_time; }
	double GetStartTime() const { return m_start_time; }

	void SetEndTime(double end_time) { m_end_time = end_time; }
	double GetEndTime() const { return m_end_time; }

	void SetPauseOnExit(bool pause_on_exit) { m_pause_on_exit = pause_on_exit; }
	bool GetPauseOnExit() const { return !!m_pause_on_exit; }

	void SetDirection(MediaTrackCueDirection dir) { m_direction = dir; }
	MediaTrackCueDirection GetDirection() const { return (MediaTrackCueDirection)m_direction; }

	void SetSnapToLines(bool snap_to_lines) { m_snap_to_lines = snap_to_lines; }
	bool GetSnapToLines() const { return !!m_snap_to_lines; }

	void SetLinePosition(int line_pos)
	{
		OP_ASSERT(m_snap_to_lines || line_pos >= -100 && line_pos <= 100);
		m_line_pos = line_pos;
		m_line_pos_is_auto = false;
	}
	int GetLinePosition() const { return m_line_pos; }
	int GetComputedLinePosition() const;
	int DOMGetLinePosition() const { return GetComputedLinePosition(); }

	bool IsLinePositionAuto() const { return !!m_line_pos_is_auto; }

	void SetTextPosition(unsigned int text_pos)
	{
		OP_ASSERT(text_pos <= 100);
		m_text_pos = text_pos;
	}
	unsigned int GetTextPosition() const { return m_text_pos; }

	void SetSize(unsigned int size)
	{
		OP_ASSERT(size <= 100);
		m_size = size;
	}
	unsigned int GetSize() const { return m_size; }

	void SetAlignment(MediaTrackCueAlignment align) { m_alignment = align; }
	MediaTrackCueAlignment GetAlignment() const { return (MediaTrackCueAlignment)m_alignment; }

	// The following are methods used from DOM that will signal any
	// associated MediaElement about the change. They all correspond
	// to their non-DOM-prefixed counter-part.
	OP_STATUS DOMSetText(DOM_Environment* environment,
						 const StringWithLength& cue_text);
	void DOMSetStartTime(DOM_Environment* environment, double start_time);
	void DOMSetEndTime(DOM_Environment* environment, double end_time);
	void DOMSetDirection(DOM_Environment* environment, MediaTrackCueDirection dir);
	void DOMSetSnapToLines(DOM_Environment* environment, bool snap_to_lines);
	void DOMSetLinePosition(DOM_Environment* environment, int line_pos);
	void DOMSetTextPosition(DOM_Environment* environment, unsigned int text_pos);
	void DOMSetSize(DOM_Environment* environment, unsigned int size);
	void DOMSetAlignment(DOM_Environment* environment, MediaTrackCueAlignment align);

	OP_STATUS EnsureCueNodes();
	WVTT_Node* GetCueNodes() const { return m_cue_nodes; }

	bool StartsBefore(double current_time) const { return m_start_time <= current_time; }
	bool EndsAfter(double current_time) const { return m_end_time > current_time; }

	/** Will this cue be active at the given time?
	 *
	 * This only considers the start and end times of the cue - not
	 * its state.
	 */
	bool IsActiveAt(double current_time) const
	{
		return StartsBefore(current_time) && EndsAfter(current_time);
	}

	/** Is this cue currently active?
	 *
	 * Active cues will reside in their tracks 'active' list.
	 */
	bool IsActive() const;

	/** Add the cue to the active list of it's track. */
	void Activate();

	/** Remove the cue from the active list of it's track. */
	void Deactivate() { Out(); }

	/** Ordering comparator for cues.
	 *
	 * Orders cues based on their start and end times.
	 * http://www.whatwg.org/html#text-track-cue-order
	 *
	 * IntraTrackOrder assumes the owner track is the same for both
	 * cues - InterTrackOrder does not.
	 */
	static int InterTrackOrder(const MediaTrackCue* a, const MediaTrackCue* b);
	static int IntraTrackOrder(const MediaTrackCue* a, const MediaTrackCue* b);

	/** Order two timestamps.
	 *
	 * Factors in NaNs for lesser IEEEy platforms.
	 */
	static double MinTimestamp(double a, double b)
	{
		return op_isfinite(a) ? (op_isfinite(b) ? MIN(a, b) : a) : b;
	}

	/** Get the next <timestamp> node.
	 *
	 * Retrieve the (valid) timestamp following the start node in
	 * pre-order within the cue node fragment of this cue.
	 */
	WVTT_Node* GetNextTimestamp(WVTT_Node* start);

private:
	static OP_STATUS SetNonEmptyString(StringWithLength& dst, const StringWithLength& src);

	double m_start_time;
	double m_end_time;

	MediaTrack* m_track;
	StringWithLength m_identifier;
	StringWithLength m_text;
	WVTT_Node* m_cue_nodes;

	unsigned int m_order;

	signed int m_line_pos;
	unsigned int m_text_pos:7;
	unsigned int m_size:7;

	unsigned int m_snap_to_lines:1;
	unsigned int m_pause_on_exit:1;
	unsigned int m_line_pos_is_auto:1;

	unsigned int m_alignment:2;
	unsigned int m_direction:2;
};

/** States for :past and :future pseudo classes. */
enum MediaCueTimeState
{
	CUE_TIMESTATE_PAST,
	CUE_TIMESTATE_PRESENT,
	CUE_TIMESTATE_FUTURE
};

/** Display state for a cue.
 *
 * Contains state information for the display of cue. Is associated
 * with a generated HTML_Element (as a special attribute), but owned
 * by TrackDisplayState.
 */
class MediaCueDisplayState :
	public ComplexAttr,
	public ListElement<MediaCueDisplayState>
{
public:
	MediaCueDisplayState(MediaTrackCue* cue) :
		m_cue(cue),
		m_curr_timestamp(NULL),
		m_next_timestamp(NULL),
		m_direction(CSS_VALUE_ltr),
		m_computed_first_line_height(0),
		m_computed_fontsize(0) {}

	void CalculateDefaultPosition(int viewport_width, int viewport_height);
	void UpdatePosition(List<MediaCueDisplayState>& cue_list,
						const OpRect& video_area);

	OP_STATUS Attach(FramesDocument* frm_doc, HTML_Element* track_root);
	void Detach(FramesDocument* frm_doc);
	void EnsureAttachment(HTML_Element* track_root);
	void MarkDirty(FramesDocument* frm_doc);
	void MarkPropsDirty(FramesDocument* frm_doc);
	HTML_Element* GetTrackRoot() const;

	MediaTrackCue* GetCue() const { return m_cue; }
	CSSValue GetDirection() const { return m_direction; }
	CSSValue GetAlignment() const;
	WritingSystem::Script GetScript() const;
	float GetFontSize() const { return m_computed_fontsize; }

	/** Does this display state represent the controls?
	 *
	 * @return true if this is a dummy cue used to represent the
	 *         controls area to the cue layout algorithm.
	 */
	bool IsControlsArea() const { return m_cue == NULL; }

	bool IsRTL() const { return m_direction == CSS_VALUE_rtl; }

	void SetRect(const OpRect& pos_rect) { m_pos_rect = pos_rect; }
	const OpRect& GetRect() const { return m_pos_rect; }

	void SetHeight(int height) { m_pos_rect.height = height; }
	void SetFirstLineHeight(int first_line_height)
	{
		m_computed_first_line_height = first_line_height;
	}

	void ResetIntraCueState(double current_time, FramesDocument* doc);
	void AdvanceIntraCueState(double current_time, FramesDocument* doc);

	double NextEventTime() const;

	static MediaCueDisplayState* GetFromHtmlElement(HTML_Element* element);

	/** Get the current time state for the element.
	 *
	 * The element should be non-NULL and part of a cue.
	 */
	static MediaCueTimeState GetTimeState(HTML_Element* element);

private:
	bool IsEnclosed(const OpRect& area) const { return !!area.Contains(m_pos_rect); }
	bool IsOverlapping(List<MediaCueDisplayState>& cue_list) const;

	bool FindPosition(List<MediaCueDisplayState>& output, const OpRect& video_area,
					  OpRect& out_position) const;

	OP_STATUS CreateRenderingFragment(HLDocProfile* hld_profile, HTML_Element* track_root);

	void UpdateTimestamps(WVTT_Node* start, double current_time);
	HTML_Element* GetIntraBoundary();
	void ResetCueFragment(FramesDocument* doc);
	void UpdateCueFragment(FramesDocument* doc, WVTT_Node* prev_ts);

	OpRect m_pos_rect;
	AutoNullElementRef m_fragment;
	AutoNullElementRef m_current_ts_pred;
	MediaTrackCue* m_cue;
	WVTT_Node* m_curr_timestamp;
	WVTT_Node* m_next_timestamp;
	CSSValue m_direction;
	int m_computed_first_line_height;
	float m_computed_fontsize;
};

/** List for storage of cues (MediaTrackCue's).
 *
 * Sorted list of cues. For sorting criteria see the HTML5
 * specification (or MediaTrackCue::LessThan). Mirrors the DOM object
 * TextTrackCueList. Multiple implementations exist in order to
 * satisfy both 'cues' ("static") and 'activeCues' ("dynamic") type
 * lists.
 */
class MediaTrackCueList : public MediaDOMItem
{
public:
	virtual ~MediaTrackCueList() {}

	virtual MediaTrackCue* GetItem(unsigned idx) = 0;
	virtual unsigned GetLength() = 0;

	virtual MediaTrackCue* GetCueById(const StringWithLength& id_needle) = 0;

	virtual OP_STATUS Insert(MediaTrackCue* cue) { return OpStatus::ERR; }
	virtual OP_STATUS Update(MediaTrackCue* cue) { return OpStatus::ERR; }
	virtual void RemoveByItem(MediaTrackCue* cue) {}
};

/** TextTrack mode. */
enum MediaTrackMode
{
	TRACK_MODE_DISABLED = 0,
	TRACK_MODE_HIDDEN,
	TRACK_MODE_SHOWING,
	TRACK_MODE_SHOWING_BY_DEFAULT
};

class MediaTrackKind
{
public:
	enum State
	{
		SUBTITLES,
		CAPTIONS,
		DESCRIPTIONS,
		CHAPTERS,
		METADATA
	};

	explicit MediaTrackKind(const uni_char* kind);

	const uni_char* DOMValue() const;

	operator State() const { return state; }

private:
	State state;
};

class HTML_Element;
class TrackElement;
class TrackUpdateState;

/** Representation of a (text) track.
 *
 * The object backing a DOM TextTrack - and implicitly a track loaded
 * for a <track> element.
 */
class MediaTrack :
	public MediaDOMItem
{
public:
	virtual ~MediaTrack();

	static OP_STATUS Create(MediaTrack*& track, HTML_Element* track_element = NULL);

	/** Create MediaTrack for HTMLMediaElement.addTextTrack(). */
	static OP_STATUS DOMCreate(MediaTrack*& track, const uni_char* kind, const uni_char* label,
							   const uni_char* srclang);

	const uni_char* GetKindAsString() const;
	const uni_char* GetLabelAsString() const;
	const uni_char* GetLanguage() const;

	MediaTrackMode GetMode() const { return m_mode; }
	void SetMode(MediaTrackMode mode) { m_mode = mode; }

	bool IsShowing() const
	{
		return m_mode == TRACK_MODE_SHOWING ||
			m_mode == TRACK_MODE_SHOWING_BY_DEFAULT;
	}

	/** Is this a visual track?
	 *
	 * Is this track of a kind that should be overlaid on the video by
	 * us (the UA)?
	 */
	bool IsVisual() const
	{
		MediaTrackKind track_kind(GetKindAsString());
		return track_kind == MediaTrackKind::SUBTITLES ||
			track_kind == MediaTrackKind::CAPTIONS;
	}

	const uni_char* DOMGetMode() const;

	/** Set the track mode.
	 *
	 * Triggers loading if mode is any other than DISABLED.
	 */
	void DOMSetMode(DOM_Environment* environment, const uni_char* str, unsigned str_length);

	/** Add a cue to this track using TextTrack.addCue(). */
	OP_STATUS DOMAddCue(DOM_Environment* environment, MediaTrackCue* cue, DOM_Object* domcue);

	/** Remove a cue from this track using TextTrack.removeCue(). */
	void DOMRemoveCue(DOM_Environment* environment, MediaTrackCue* cue);

	WritingSystem::Script GetScript() const { return m_script; }
	void SetScript(WritingSystem::Script script) { m_script = script; }
	void SetScriptFromSrcLang();

	/** Deactivate this track. */
	void Deactivate();

	/** Remove/detach any cues from the cue list. */
	void Clear();

	MediaTrackCueList* GetCueList() const;
	MediaTrackCueList* GetActiveCueList() const;

	HTML_Element* GetHtmlElement() const { return m_element; }

	/** Add a cue originating from the WebVTT parser. */
	OP_STATUS AddParsedCue(MediaTrackCue* cue);

	/** Return the HTML_Element that this track is associated with, or NULL. */
	HTML_Element* GetAssociatedHtmlElement() const { return m_associated_element; }

	/** Associate this MediaTrack with a (media) HTML_Element. */
	void AssociateWith(HTML_Element* html_element);

	/** Break the association between this MediaTrack and its HTML_Element. */
	void ResetAssociation();

	/** Get the associated MediaElement, if any. */
	MediaElement* GetMediaElement() const;

	/** Get the <track> element this track stems from, if any. */
	TrackElement* GetTrackElement() const;

	/** Get the index of this track in the list it belongs to.
	 *
	 * @return List index or -1 if not in a list.
	 */
	int GetListIndex();

	/** Update the list of active cues for this track.
	 *
	 * @param tustate State object for the current track update.
	 * @param seeking true if the update is due to a seeking operation.
	 * @param current_time The current position on the timeline.
	 */
	OP_STATUS UpdateActive(TrackUpdateState& tustate,
						   bool seeking, double current_time);

	/** Indicate that a seek operation should be performed on the next track update. */
	void SetPendingSeek() { m_state.pending_seek = true; }

	/** Did the sweep operation generate any changes? */
	bool HasCueChanges() const { return m_state.has_cue_changes; }

	/** Are there any active cues on this track? */
	bool HasActiveCues() const { return !m_active_cues.Empty(); }

	/** Is the 'cue' in this track's active list? */
	bool HasActiveCue(const MediaTrackCue* cue) const { return !!m_active_cues.HasLink(cue); }

	/** Add a cue to this track's pending list. */
	void AddPendingCue(MediaTrackCue* cue) { cue->Into(&m_pending_cues); }

	/** Add cue to the active list.
	 *
	 * @param cue A cue belonging to this track.
	 */
	void ActivateCue(MediaTrackCue* cue);

	/** Get the first in the list of active cues. */
	MediaTrackCue* GetFirstActiveCue() const { return m_active_cues.First(); }

	/** Get the start time of the next cue based on current state. */
	double NextCueStartTime() const;

	/** Get the earliest time when a currently active cue will end. */
	double NextCueEndTime() const;

	/** Cache for computed line position. Managed via the associated MediaElement. */
	void SetCachedLinePosition(int cached_line) { m_cached_line = cached_line; }
	int GetCachedLinePosition() const { return m_cached_line; }

	/** The position of the track in the track list. -1 if not in a track list. */
	void SetCachedListPosition(int idx) { m_cached_index = idx; }
	int GetCachedListPosition() const { return m_cached_index; }

private:
	MediaTrack(HTML_Element* element) :
		m_element(element),
		m_associated_element(NULL),
		m_kind(NULL),
		m_label(NULL),
		m_srclang(NULL),
		m_cuelist(NULL),
		m_active_cuelist(NULL),
		m_current_cue_order_no(0),
		m_cached_index(-1),
		m_cached_line(0),
		m_script(WritingSystem::Unknown),
		m_mode(TRACK_MODE_DISABLED)
	{
		m_state.next_cue_index = 0;
		m_state.has_cue_changes = false;
		m_state.pending_seek = false;
	}

	void HandleModeTransition(MediaTrackMode prev_mode, ES_Thread* thread);

	/** Reset the sweep state based on the current time.
	 *
	 * Rewinds the "sweep" (sweep line/position) to the start of the
	 * timeline, and then advances it to the first cue in the cue list
	 * that does not start before the time passed in. During this
	 * operation state will be updated by moving cues to 'started',
	 * 'ended' and 'active' sets of cues as appropriate.
	 */
	OP_STATUS ResetSweep(TrackUpdateState& tustate, double current_time);

	/** Perform a sweep from the previous time to the current time.
	 *
	 * Starting from the current sweep position (as set by any
	 * previous sweep operation), advance to the first cue that does
	 * not start before the time passed in. During this operation
	 * state will be updated by moving cues to 'started', 'ended',
	 * 'missed' and 'active' sets of cues as appropriate.
	 */
	OP_STATUS PartitionSweep(TrackUpdateState& tustate, double current_time);

	/** Process the pending list of cues. */
	OP_STATUS ProcessPendingCues(TrackUpdateState& tustate, double current_time);

	List<MediaTrackCue> m_active_cues;
	/**< The list of currently active cues on this track. */

	List<MediaTrackCue> m_pending_cues;
	/**< The list of pending cues on this track. */

	// Sweep state.
	struct
	{
		unsigned next_cue_index;
		/**< Current position in the list of cues. Updated by
		 * Reset/PartitionSweep. Should point to the first cue that
		 * starts _after_ the last track update. */

		bool has_cue_changes;
		/**< Flag indicating if this track generated any events during
		 * the last track update. */

		bool pending_seek;
		/**< Flag indicating that a reset sweep should be run on this
		 * track even if a partition sweep is requested. */
	} m_state;

	HTML_Element* m_element;

	// HTML_Element this track is currently associated with.
	//
	// For a track stemming from a <track> element (m_element != NULL)
	// this will be set while the track is in the MediaElement's track
	// list - i.e, while it is a direct child of the MediaElement.
	//
	// For a track created via HTMLMediaElement.addTextTrack()
	// (m_element == NULL) this reference will be set and valid
	// througout the lifetime of the MediaElement. If the track
	// outlives the MediaElement then the reference can be NULL.
	HTML_Element* m_associated_element;

	uni_char* m_kind;
	uni_char* m_label;
	uni_char* m_srclang;

	class MediaTrackCueStorageList* m_cuelist;
	class MediaTrackCueActiveList* m_active_cuelist;

	unsigned int m_current_cue_order_no;

	int m_cached_index;
	int m_cached_line;
	WritingSystem::Script m_script;
	MediaTrackMode m_mode;
};

/** HTMLTrackElement ready state. */
enum MediaTrackReadyState
{
	TRACK_STATE_NONE = 0,
	TRACK_STATE_LOADING,
	TRACK_STATE_LOADED,
	TRACK_STATE_ERROR
};

/** Ownership container for a track from a <track> element. */
class TrackElement :
	public ComplexAttr,
	public ElementRef,
	private ES_ThreadListener,
	private TrackLoaderListener
{
public:
	TrackElement() :
		m_track(NULL),
		m_loader(NULL),
		m_ready_state(TRACK_STATE_NONE) {}
	virtual ~TrackElement();

	/** Ensure that a MediaTrack exists for this element. */
	OP_STATUS EnsureTrack(HTML_Element* element);

	// From ComplexAttr
	virtual OP_STATUS CreateCopy(ComplexAttr** copy_to);

	// From ElementRef
	virtual void OnDelete(FramesDocument* document);
	virtual void OnRemove(FramesDocument* document);

	/** Initiate the loading of the track data for this track. */
	OP_STATUS Load(FramesDocument* doc, ES_Thread* thread = NULL);

	/** Signal a change in an attribute value. */
	OP_STATUS HandleAttributeChange(FramesDocument* frm_doc, HTML_Element* element,
									int attr, ES_Thread* thread);

	/** Loading hooks called from the inline loading code. */
	void LoadingProgress(HEListElm* hle);
	void LoadingRedirected(HEListElm* helm);
	void LoadingStopped(HEListElm* hle);

	MediaTrack* GetTrack() const { return m_track; }

	MediaTrackReadyState GetReadyState() const { return m_ready_state; }

private:
	void SetReadyState(MediaTrackReadyState rstate) { m_ready_state = rstate; }

	// ES_ThreadListener
	virtual OP_STATUS Signal(ES_Thread* thread, ES_ThreadSignal signal);

	// TrackLoaderListener
	virtual void OnTrackLoaded(HEListElm* hle, MediaTrack* track);
	virtual void OnTrackLoadError(HEListElm* hle, MediaTrack* track);

	// This method is used to send 'error' events if the loading fails
	// before it has started.
	void SendErrorEvent(FramesDocument* doc);

	MediaTrack* m_track;
	TrackLoader* m_loader;
	MediaTrackReadyState m_ready_state;
};

/** Representation of a list of tracks.
 *
 * Keeps track of tracks of a media element - both the ones
 * originating from the logical tree, and those added via the DOM
 * addTrack() method. DOM-facing via TextTrackList, and thus closely
 * follows the semantics required in that context.
 */
class MediaTrackList : public MediaDOMItem
{
public:
	virtual ~MediaTrackList();

	MediaTrack* GetTrackAt(unsigned idx) const;
	MediaTrack* GetTrackByElement(HTML_Element* element) const;

	OP_STATUS AddTrack(MediaTrack* track);
	void RemoveTrack(MediaTrack* track);

	void ReleaseDOMTracks();

	unsigned GetLength() const { return m_tree_tracks.GetCount() + m_dom_tracks.GetCount(); }

private:
	OpVector<MediaTrack> m_tree_tracks;	///< Tracks originating from the document tree - <track>
	OpVector<MediaTrack> m_dom_tracks;	///< Tracks originating from DOM - addTextTrack()
};

#endif // MEDIA_HTML_SUPPORT

#endif // MEDIA_TRACK_H
