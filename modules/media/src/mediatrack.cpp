/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_HTML_SUPPORT

#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"

#include "modules/media/mediatrack.h"
#include "modules/media/mediaelement.h"
#include "modules/media/src/webvttparser.h"
#include "modules/media/src/trackdisplaystate.h"

#include "modules/unicode/unicode.h"
#include "modules/unicode/unicode_stringiterator.h"

#include "modules/util/OpRegion.h"

/** List of cues for "normal" storage. */
class MediaTrackCueStorageList : public MediaTrackCueList
{
public:
	virtual ~MediaTrackCueStorageList() { Reset(); }

	virtual MediaTrackCue* GetItem(unsigned idx) { return m_cues.Get(idx); }
	virtual unsigned GetLength() { return m_cues.GetCount(); }

	virtual MediaTrackCue* GetCueById(const StringWithLength& id_needle);

	virtual OP_STATUS Insert(MediaTrackCue* cue);
	virtual OP_STATUS Update(MediaTrackCue* cue);
	virtual void RemoveByItem(MediaTrackCue* cue) { m_cues.RemoveByItem(cue); }

	void Reset();

private:
	OpVector<MediaTrackCue> m_cues;
};

/** List of cues for keeping the active list.
 *
 * This is a 'derived' list, and does not own the cues it allows
 * access to. The actual list is owned by a MediaTrack object. If the
 * backing object is destroyed it should sever the link from this
 * object to itself.
 */
class MediaTrackCueActiveList : public MediaTrackCueList
{
public:
	MediaTrackCueActiveList(List<MediaTrackCue>* active_cues) : m_cues(active_cues) {}

	virtual MediaTrackCue* GetItem(unsigned idx);
	virtual unsigned GetLength() { return m_cues ? m_cues->Cardinal() : 0; }

	virtual MediaTrackCue* GetCueById(const StringWithLength& id_needle);

	void Reset() { m_cues = NULL; }

private:
	List<MediaTrackCue>* m_cues;
};

void
MediaDOMItem::DetachOrDestroy(MediaDOMItem* domitem)
{
	if (!domitem)
		return;

	if (domitem->m_dom_object)
		domitem->m_dom_object = NULL;
	else
		OP_DELETE(domitem);
}

MediaTrackKind::MediaTrackKind(const uni_char* kind)
{
	state = SUBTITLES;

	if (!kind)
		return;

	size_t kind_len = uni_strlen(kind);
	if (kind_len == 8)
	{
		if (uni_strni_eq_lower_ascii(kind, UNI_L("captions"), 8))
			state = CAPTIONS;
		else if (uni_strni_eq_lower_ascii(kind, UNI_L("chapters"), 8))
			state = CHAPTERS;
		else if (uni_strni_eq_lower_ascii(kind, UNI_L("metadata"), 8))
			state = METADATA;
	}
	else if (kind_len == 12 && uni_strni_eq_lower_ascii(kind, UNI_L("descriptions"), 12))
		state = DESCRIPTIONS;
}

const uni_char*
MediaTrackKind::DOMValue() const
{
	const uni_char* str;
	switch (state)
	{
	case CAPTIONS:
		str = UNI_L("captions");
		break;
	case DESCRIPTIONS:
		str = UNI_L("descriptions");
		break;
	case CHAPTERS:
		str = UNI_L("chapters");
		break;
	case METADATA:
		str = UNI_L("metadata");
		break;
	default:
		OP_ASSERT(!"Unknown state");
	case SUBTITLES:
		str = UNI_L("subtitles");
		break;
	}
	return str;
}

TrackElement::~TrackElement()
{
	OP_DELETE(m_loader);

	ES_ThreadListener::Remove();

	MediaDOMItem::DetachOrDestroy(m_track);
}

OP_STATUS
TrackElement::Load(FramesDocument* doc, ES_Thread* thread /* = NULL */)
{
	if (m_track->GetMode() == TRACK_MODE_DISABLED)
		return OpStatus::OK;

	HTML_Element* element = m_track->GetHtmlElement();
	HEListElm* hle = element->GetHEListElmForInline(TRACK_INLINE);
	if (hle)
		return OpStatus::OK;

	// On first load there won't be a previous URL to stop loading and
	// hence no TrackLoader.
	if (m_loader)
	{
		doc->StopLoadingInline(m_loader->GetURL(), element, TRACK_INLINE);

		OP_DELETE(m_loader);
		m_loader = NULL;
	}

	// If associated with a MediaElement, notify it about the change.
	if (MediaElement* media_element = m_track->GetMediaElement())
		media_element->HandleTrackChange(m_track, MediaElement::TRACK_CLEARED, thread);

	// Remove any old cues from the track.
	m_track->Clear();

	SetReadyState(TRACK_STATE_LOADING);

	// Run the asynchronous steps by finding the "most interrupted"
	// thread (if any) and continuing after it has finished.
	if (thread)
	{
		ES_ThreadListener::Remove();
		thread->GetRunningRootThread()->AddListener(this);
		return OpStatus::OK;
	}

	URL* url = element->GetUrlAttr(Markup::HA_SRC, NS_IDX_HTML, doc->GetLogicalDocument());
	if (!url || url->IsEmpty())
	{
		SendErrorEvent(doc);
		return OpStatus::OK;
	}

	m_track->SetScriptFromSrcLang();

	m_loader = OP_NEW(TrackLoader, (*url, this, m_track));
	if (!m_loader)
	{
		SendErrorEvent(doc);
		return OpStatus::ERR_NO_MEMORY;
	}

	element->RemoveSpecialAttribute(Markup::LOGA_INLINE_ONLOAD_SENT, SpecialNs::NS_LOGDOC);

	OP_LOAD_INLINE_STATUS status = doc->LoadInline(url, element, TRACK_INLINE);
	OP_ASSERT(status != LoadInlineStatus::USE_LOADED);

	if (OpStatus::IsError(status))
	{
		doc->StopLoadingInline(m_loader->GetURL(), element, TRACK_INLINE);

		OP_DELETE(m_loader);
		m_loader = NULL;

		SendErrorEvent(doc);
	}

	return status;
}

OP_STATUS
TrackElement::HandleAttributeChange(FramesDocument* frm_doc, HTML_Element* element,
									int attr, ES_Thread* thread)
{
	OP_ASSERT(element->Type() == Markup::HTE_TRACK);
	OP_ASSERT(!m_track || m_track->GetHtmlElement() == element);

	if (attr == Markup::HA_SRC)
	{
		// If 'src' changes but we don't actually have a track yet we
		// can ignore the change. The track will be created (and
		// loading started) either via a change in 'mode' or by adding
		// the <track> element to a media element.
		if (!m_track)
			return OpStatus::OK;

		return Load(frm_doc, thread);
	}
	return OpStatus::OK;
}

void
TrackElement::LoadingProgress(HEListElm* hle)
{
	OP_ASSERT(hle->HElm() == m_track->GetHtmlElement());

	m_loader->HandleData(hle);
}

void
TrackElement::LoadingRedirected(HEListElm* hle)
{
	OP_ASSERT(hle->HElm() == m_track->GetHtmlElement());

	m_loader->HandleRedirect(hle);
}

void
TrackElement::LoadingStopped(HEListElm* hle)
{
	OP_ASSERT(hle->HElm() == m_track->GetHtmlElement());

	m_loader->HandleData(hle);
}

/* virtual */ OP_STATUS
TrackElement::Signal(ES_Thread* thread, ES_ThreadSignal signal)
{
	ES_ThreadListener::Remove();

	if (signal == ES_SIGNAL_FINISHED || signal == ES_SIGNAL_FAILED)
		if (ES_ThreadScheduler* scheduler = thread->GetScheduler())
			if (FramesDocument* doc = scheduler->GetFramesDocument())
				return Load(doc);

	return OpStatus::OK;
}

/* virtual */ void
TrackElement::OnTrackLoaded(HEListElm* hle, MediaTrack* track)
{
	OP_ASSERT(hle);
	OP_ASSERT(m_track == track);

	hle->OnLoad();

	// FIXME: do the state transition in the same task that fires load
	SetReadyState(TRACK_STATE_LOADED);

	if (MediaElement* media_element = m_track->GetMediaElement())
		media_element->HandleTrackChange(m_track, MediaElement::TRACK_READY);
}

void
TrackElement::SendErrorEvent(FramesDocument* doc)
{
	m_track->GetHtmlElement()->SendEvent(ONERROR, doc);

	SetReadyState(TRACK_STATE_ERROR);
}

/* virtual */ void
TrackElement::OnTrackLoadError(HEListElm* hle, MediaTrack* track)
{
	OP_ASSERT(hle);
	OP_ASSERT(m_track == track);

	hle->SendOnError();

	// FIXME: do the state transition in the same task that fires error
	SetReadyState(TRACK_STATE_ERROR);
}

/* static */ OP_STATUS
TrackElement::EnsureTrack(HTML_Element* element)
{
	OP_ASSERT(element);
	OP_ASSERT(element->IsMatchingType(Markup::HTE_TRACK, NS_HTML));

	if (m_track)
	{
		OP_ASSERT(m_track->GetHtmlElement() == element);
		return OpStatus::OK;
	}

	return MediaTrack::Create(m_track, element);
}

/* virtual */ OP_STATUS
TrackElement::CreateCopy(ComplexAttr** copy_to)
{
	TrackElement* elm_copy = OP_NEW(TrackElement, ());
	if (!elm_copy)
		return OpStatus::ERR_NO_MEMORY;

	*copy_to = elm_copy;
	return OpStatus::OK;
}

/* virtual */ void
TrackElement::OnDelete(FramesDocument* document)
{
	if (MediaElement* media_element = m_track->GetMediaElement())
		media_element->NotifyTrackRemoved(GetElm());
}

/* virtual */ void
TrackElement::OnRemove(FramesDocument* document)
{
	// Only signal a removal if we have been detached from our parent
	// (which should be a <video>/<audio>).
	if (GetElm()->ParentActual() == NULL)
		OnDelete(document);
}

/* static */ OP_STATUS
MediaTrack::Create(MediaTrack*& track, HTML_Element* track_element /* = NULL */)
{
	track = OP_NEW(MediaTrack, (track_element));
	if (!track)
		return OpStatus::ERR_NO_MEMORY;

	track->m_cuelist = OP_NEW(MediaTrackCueStorageList, ());
	track->m_active_cuelist = OP_NEW(MediaTrackCueActiveList, (&track->m_active_cues));

	if (!track->m_cuelist || !track->m_active_cuelist)
	{
		OP_DELETE(track);
		track = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
MediaTrack::DOMCreate(MediaTrack*& track, const uni_char* kind, const uni_char* label,
					  const uni_char* srclang)
{
	RETURN_IF_ERROR(Create(track));

	// Expect these to never be NULL (empty string if not specified)
	OP_ASSERT(label && srclang);

	track->m_kind = UniSetNewStr(kind);
	track->m_label = UniSetNewStr(label);
	track->m_srclang = UniSetNewStr(srclang);

	// Since none of the input string ought to be NULL, this should
	// check for failed allocations
	if (!track->m_kind || !track->m_label || !track->m_srclang)
	{
		OP_DELETE(track);
		track = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	track->SetMode(TRACK_MODE_HIDDEN);
	return OpStatus::OK;
}

const uni_char*
MediaTrack::DOMGetMode() const
{
	switch (m_mode)
	{
	case TRACK_MODE_DISABLED:
		return UNI_L("disabled");

	case TRACK_MODE_HIDDEN:
		return UNI_L("hidden");

	case TRACK_MODE_SHOWING:
	case TRACK_MODE_SHOWING_BY_DEFAULT:
		return UNI_L("showing");
	}
	return NULL;
}

void
MediaTrack::DOMSetMode(DOM_Environment* environment,
					   const uni_char* str, unsigned str_length)
{
	MediaTrackMode mode = m_mode;
	if (str_length == 8 && uni_strncmp(str, "disabled", 8) == 0)
		mode = TRACK_MODE_DISABLED;
	else if (str_length == 6 && uni_strncmp(str, "hidden", 6) == 0)
		mode = TRACK_MODE_HIDDEN;
	else if (str_length == 7 && uni_strncmp(str, "showing", 7) == 0)
		mode = TRACK_MODE_SHOWING;

	if (mode == GetMode())
		return;

	MediaTrackMode prev_mode = m_mode;

	SetMode(mode);

	ES_Thread* current_thread = environment->GetCurrentScriptThread();

	if (TrackElement* track_element = GetTrackElement())
		RAISE_IF_MEMORY_ERROR(track_element->Load(environment->GetFramesDocument(),
												  current_thread));

	HandleModeTransition(prev_mode, current_thread);
}

void
MediaTrack::HandleModeTransition(MediaTrackMode prev_mode, ES_Thread* thread)
{
	// If the track is not associated with a MediaElement no action
	// needs to be taken.
	MediaElement* media_element = GetMediaElement();
	if (!media_element)
		return;

	// If the mode was set to 'showing' and the mode was previously
	// 'showing-by-default' then nothing needs to be done.
	if (prev_mode == TRACK_MODE_SHOWING_BY_DEFAULT && m_mode == TRACK_MODE_SHOWING)
		return;

	//
	// Modelling the mode-transitions as a FSM we get:
	//
	// +----------+    connect     +--------+   show   +----------+
	// |          |--------------->|        |--------->|          |
	// | DISABLED |                | HIDDEN |          | SHOWING* |
	// |          |<---------------|        |<---------|          |
	// +----------+   disconnect   +--------+   hide   +----------+
	//
	// (self-transitions excluded)
	//

	MediaElement::TrackChangeReason reason;

	if (prev_mode > m_mode)
	{
		if (m_mode == TRACK_MODE_DISABLED)
			// If the mode transitioned into DISABLED we will tear
			// down the track regardless of the previous mode.
			reason = MediaElement::TRACK_CLEARED;
		else
			// If the track was not disabled, hiding any active cues
			// is enough.
			reason = MediaElement::TRACK_VISIBILITY;
	}
	else
	{
		// Mode equalities have been filtered out.
		OP_ASSERT(prev_mode < m_mode);

		if (prev_mode == TRACK_MODE_DISABLED)
			// If the track transitioned out of DISABLED, we will
			// signal that it's ready, which will handle changes in
			// visibility too.
			reason = MediaElement::TRACK_READY;
		else
			// If the track was already enabled, showing any active
			// cues is enough.
			reason = MediaElement::TRACK_VISIBILITY;
	}

	media_element->HandleTrackChange(this, reason, thread);

	// If the track was disabled, remove any cues from the active list.
	if (m_mode == TRACK_MODE_DISABLED)
		Deactivate();
}

TrackElement*
MediaTrack::GetTrackElement() const
{
	return m_element ? m_element->GetTrackElement() : NULL;
}

MediaElement*
MediaTrack::GetMediaElement() const
{
	return m_associated_element ? m_associated_element->GetMediaElement() : NULL;
}

int
MediaTrack::GetListIndex()
{
	if (MediaElement* media_element = GetMediaElement())
		return media_element->GetTrackListPosition(this);

	return -1;
}

void
MediaTrack::AssociateWith(HTML_Element* html_element)
{
	OP_ASSERT(html_element);
	OP_ASSERT(html_element->GetMediaElement() != NULL);

	m_associated_element = html_element;

	if (TrackElement* track_element = GetTrackElement())
		track_element->SetElm(m_element);
}

void
MediaTrack::ResetAssociation()
{
	m_associated_element = NULL;

	if (TrackElement* track_element = GetTrackElement())
		track_element->Reset();
}

OP_STATUS
MediaTrack::AddParsedCue(MediaTrackCue* cue)
{
	RETURN_IF_ERROR(m_cuelist->Insert(cue));

	cue->SetOrder(m_current_cue_order_no++);
	cue->SetOwnerTrack(this);
	return OpStatus::OK;
}

OP_STATUS
MediaTrack::DOMAddCue(DOM_Environment* environment,
					  MediaTrackCue* cue, DOM_Object* domcue)
{
	OP_ASSERT(cue);
	OP_ASSERT(cue->GetOwnerTrack() == NULL);
	OP_ASSERT(cue->GetDOMObject() == NULL);

	RETURN_IF_ERROR(m_cuelist->Insert(cue));

	cue->SetOrder(m_current_cue_order_no++);
	cue->SetOwnerTrack(this);

	cue->SetDOMObject(domcue);

	if (MediaElement* media_element = GetMediaElement())
		media_element->HandleCueChange(cue, MediaElement::CUE_ADDED,
									   environment->GetCurrentScriptThread());

	return OpStatus::OK;
}

void
MediaTrack::DOMRemoveCue(DOM_Environment* environment, MediaTrackCue* cue)
{
	OP_ASSERT(cue);
	OP_ASSERT(cue->GetOwnerTrack() == this);

	if (MediaElement* media_element = GetMediaElement())
		media_element->HandleCueChange(cue, MediaElement::CUE_REMOVED,
									   environment->GetCurrentScriptThread());

	m_cuelist->RemoveByItem(cue);

	// Make sure the cue is no longer in the active list.
	cue->Deactivate();
	// Disassociate the cue with this track.
	cue->SetOwnerTrack(NULL);
	// Detach the DOM object (transfer ownership to the DOM object).
	cue->SetDOMObject(NULL);
}

void
MediaTrack::SetScriptFromSrcLang()
{
	const uni_char* srclang = GetLanguage();

	if (srclang && *srclang)
		m_script = WritingSystem::FromLanguageCode(srclang);
	else
		m_script = WritingSystem::Unknown;
}

void
MediaTrack::Deactivate()
{
	m_state.next_cue_index = 0;
	m_state.pending_seek = false;
	m_active_cues.RemoveAll();
	m_pending_cues.RemoveAll();
}

void
MediaTrack::Clear()
{
	Deactivate();

	if (m_cuelist)
		m_cuelist->Reset();
}

MediaTrack::~MediaTrack()
{
	m_active_cues.RemoveAll();
	m_pending_cues.RemoveAll();

	if (m_active_cuelist)
		m_active_cuelist->Reset();

	MediaDOMItem::DetachOrDestroy(m_cuelist);
	MediaDOMItem::DetachOrDestroy(m_active_cuelist);

	OP_DELETEA(m_kind);
	OP_DELETEA(m_label);
	OP_DELETEA(m_srclang);
}

const uni_char*
MediaTrack::GetKindAsString() const
{
	if (m_element)
		return MediaTrackKind(m_element->GetStringAttr(Markup::HA_KIND)).DOMValue();

	return m_kind;
}

const uni_char*
MediaTrack::GetLabelAsString() const
{
	if (m_element)
		return m_element->GetStringAttr(Markup::HA_LABEL);

	return m_label;
}

const uni_char*
MediaTrack::GetLanguage() const
{
	if (m_element)
		return m_element->GetStringAttr(Markup::HA_SRCLANG);

	return m_srclang;
}

MediaTrackCueList*
MediaTrack::GetCueList() const
{
	return m_cuelist;
}

MediaTrackCueList*
MediaTrack::GetActiveCueList() const
{
	return m_active_cuelist;
}

OP_STATUS
MediaTrack::UpdateActive(TrackUpdateState& tustate,
						 bool seeking, double current_time)
{
	if (seeking || m_state.pending_seek)
	{
		m_pending_cues.RemoveAll();

		return ResetSweep(tustate, current_time);
	}
	else
	{
		if (!m_pending_cues.Empty())
			ProcessPendingCues(tustate, current_time);

		return PartitionSweep(tustate, current_time);
	}
}

OP_STATUS
MediaTrack::ResetSweep(TrackUpdateState& tustate, double current_time)
{
	m_state.next_cue_index = 0;
	m_state.pending_seek = false;

	unsigned prev_event_count = tustate.EventCount();

	// The time window was reset and is thus empty. No cues should be
	// considered 'missed' in this case.

	// Go through the 'active' set and remove the cues that start
	// after the new position while keeping the cues that straddles
	// the current time.
	for (MediaTrackCue* cue = m_active_cues.First(); cue; cue = cue->Suc())
		// If an active cue no longer straddles the current time it has ended.
		if (!cue->IsActiveAt(current_time))
			RETURN_IF_ERROR(tustate.Ended(cue));

	// Collect all cues that have started, and avoid adding cues that
	// are in the current 'active' set.
	while (m_state.next_cue_index < m_cuelist->GetLength())
	{
		MediaTrackCue* cue = m_cuelist->GetItem(m_state.next_cue_index);
		if (!cue->StartsBefore(current_time))
			break;

		// Cues that were in the active set are processed above.
		if (!cue->IsActive() && cue->EndsAfter(current_time))
			// Started but not ended, and not in the 'active' set =>
			// put in started list.
			RETURN_IF_ERROR(tustate.Started(cue));

		m_state.next_cue_index++;
	}

	m_state.has_cue_changes = tustate.EventCount() != prev_event_count;

	return OpStatus::OK;
}

OP_STATUS
MediaTrack::PartitionSweep(TrackUpdateState& tustate, double current_time)
{
	unsigned prev_event_count = tustate.EventCount();

	// Go through cues starting from next_cue_index and add cues that
	// start within this sweep interval.
	while (m_state.next_cue_index < m_cuelist->GetLength())
	{
		MediaTrackCue* cue = m_cuelist->GetItem(m_state.next_cue_index);
		if (!cue->StartsBefore(current_time))
			break;

		// If the cue has already ended it is to be considered
		// 'missed'.
		if (!cue->EndsAfter(current_time))
			RETURN_IF_ERROR(tustate.Missed(cue));
		else
			RETURN_IF_ERROR(tustate.Started(cue));

		m_state.next_cue_index++;
	}

	// Go through the 'active' set and remove the cues that will end
	// within the current time window.
	for (MediaTrackCue* cue = m_active_cues.First(); cue; cue = cue->Suc())
		if (!cue->EndsAfter(current_time))
			RETURN_IF_ERROR(tustate.Ended(cue));

	m_state.has_cue_changes = tustate.EventCount() != prev_event_count;

	return OpStatus::OK;
}

OP_STATUS
MediaTrack::ProcessPendingCues(TrackUpdateState& tustate, double current_time)
{
	// If the sweep index is pointing at the first cue there is no
	// work to be done (no cue can have been inserted before the sweep
	// index).
	if (m_state.next_cue_index == 0)
	{
		m_pending_cues.RemoveAll();
		return OpStatus::OK;
	}

	unsigned prev_event_count = tustate.EventCount();

	List<MediaTrackCue> pending_activation;

	while (MediaTrackCue* cue = m_pending_cues.First())
	{
		cue->Out();

		if (!cue->IsActiveAt(current_time))
			continue;

		// Emit (enter) event.
		OP_STATUS status = tustate.Started(cue);
		if (OpStatus::IsError(status))
		{
			m_pending_cues.RemoveAll();
			pending_activation.RemoveAll();
			return status;
		}

		cue->Into(&pending_activation);
	}

	m_state.has_cue_changes = tustate.EventCount() != prev_event_count;

	// Move the sweep index past active, pending and ended cues.
	while (m_state.next_cue_index < m_cuelist->GetLength())
	{
		MediaTrackCue* cue = m_cuelist->GetItem(m_state.next_cue_index);
		if (!(cue->IsActive() || pending_activation.HasLink(cue) ||
			  !cue->EndsAfter(current_time)))
			break;

		m_state.next_cue_index++;
	}

	pending_activation.RemoveAll();

	return OpStatus::OK;
}

void
MediaTrack::ActivateCue(MediaTrackCue* cue)
{
	OP_ASSERT(cue->GetOwnerTrack() == this);

	// Insert the cue in the correct order in the list. In general
	// it's assumed that newly activated cues will go at the end of
	// the active list. During out-of-order activations - like cues
	// added via DOM when the timeline is running - could however
	// require sorting.
	for (MediaTrackCue* candidate = m_active_cues.Last();
		 candidate; candidate = candidate->Pred())
		if (MediaTrackCue::IntraTrackOrder(candidate, cue) < 0)
		{
			cue->Follow(candidate);
			return;
		}

	cue->IntoStart(&m_active_cues);
}

double
MediaTrack::NextCueStartTime() const
{
	if (m_state.next_cue_index < m_cuelist->GetLength())
		return m_cuelist->GetItem(m_state.next_cue_index)->GetStartTime();

	return op_nan(NULL);
}

double
MediaTrack::NextCueEndTime() const
{
	double next_end = op_nan(NULL);
	for (MediaTrackCue* cue = m_active_cues.First(); cue; cue = cue->Suc())
		next_end = MediaTrackCue::MinTimestamp(next_end, cue->GetEndTime());

	return next_end;
}

MediaTrackList::~MediaTrackList()
{
	for (unsigned i = 0; i < m_dom_tracks.GetCount(); i++)
		DetachOrDestroy(m_dom_tracks.Get(i));
}

MediaTrack*
MediaTrackList::GetTrackAt(unsigned idx) const
{
	unsigned tree_track_count = m_tree_tracks.GetCount();
	if (idx < tree_track_count)
		return m_tree_tracks.Get(idx);

	idx -= tree_track_count;

	if (idx < m_dom_tracks.GetCount())
		return m_dom_tracks.Get(idx);

	return NULL;
}

MediaTrack*
MediaTrackList::GetTrackByElement(HTML_Element* element) const
{
	for (unsigned idx = 0; idx < m_tree_tracks.GetCount(); idx++)
	{
		MediaTrack* track = m_tree_tracks.Get(idx);
		if (track->GetHtmlElement() == element)
			return track;
	}
	return NULL;
}

OP_STATUS
MediaTrackList::AddTrack(MediaTrack* track)
{
	if (HTML_Element* track_element = track->GetHtmlElement())
	{
		unsigned candidate_idx = m_tree_tracks.GetCount();
		while (candidate_idx)
		{
			MediaTrack* candidate_track = m_tree_tracks.Get(candidate_idx - 1);
			if (candidate_track->GetHtmlElement()->Precedes(track_element))
				break;

			candidate_idx--;
		}

		return m_tree_tracks.Insert(candidate_idx, track);
	}
	else
	{
		return m_dom_tracks.Add(track);
	}
}

void
MediaTrackList::RemoveTrack(MediaTrack* track)
{
	OpVector<MediaTrack>* sub_collection;

	if (track->GetHtmlElement())
		sub_collection = &m_tree_tracks;
	else
		sub_collection = &m_dom_tracks;

	OpStatus::Ignore(sub_collection->RemoveByItem(track));
}

void
MediaTrackList::ReleaseDOMTracks()
{
	for (unsigned i = 0; i < m_dom_tracks.GetCount(); i++)
		m_dom_tracks.Get(i)->ResetAssociation();
}

/* static */ OP_STATUS
MediaTrackCue::DOMCreate(MediaTrackCue*& cue,
						 double start_time, double end_time,
						 const StringWithLength& text)
{
	OpAutoPtr<MediaTrackCue> new_cue(OP_NEW(MediaTrackCue, ()));
	if (!new_cue.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_cue->SetText(text));

	cue = new_cue.release();

	cue->SetStartTime(start_time);
	cue->SetEndTime(end_time);

	return OpStatus::OK;
}

MediaElement*
MediaTrackCue::GetMediaElement() const
{
	return m_track ? m_track->GetMediaElement() : NULL;
}

OP_STATUS
MediaTrackCue::DOMSetText(DOM_Environment* environment,
						  const StringWithLength& cue_text)
{
	RETURN_IF_ERROR(SetText(cue_text));

	// If the cue text has not yet been parsed, then be lazy and do
	// nothing more.
	if (!m_cue_nodes)
		return OpStatus::OK;

	// Remove the cue nodes.
	OP_DELETE(m_cue_nodes);
	m_cue_nodes = NULL;

	// If this cue is not active, then we can wait with the recreation
	// of the nodes until we need them.
	if (!IsActive())
		return OpStatus::OK;

	// Reparse (treating OOM as hard error).
	RETURN_IF_ERROR(EnsureCueNodes());

	// Signal the MediaElement that the cue layout has top be redone
	// since the cue rendering fragment changed.
	if (MediaElement* media_element = GetMediaElement())
		media_element->HandleCueChange(this, MediaElement::CUE_CONTENT_CHANGED,
									   environment->GetCurrentScriptThread());

	return OpStatus::OK;
}

void
MediaTrackCue::DOMSetStartTime(DOM_Environment* environment, double start_time)
{
	if (m_start_time == start_time)
		return;

	SetStartTime(start_time);

	if (!m_track)
		return;

	OP_ASSERT(m_track->GetCueList());

	// Update the position in the hosting cue list.
	m_track->GetCueList()->Update(this);

	// Signal the MediaElement.
	if (MediaElement* media_element = GetMediaElement())
		media_element->HandleCueChange(this, MediaElement::CUE_TIME_CHANGED,
									   environment->GetCurrentScriptThread());
}

void
MediaTrackCue::DOMSetEndTime(DOM_Environment* environment, double end_time)
{
	if (m_end_time == end_time)
		return;

	SetEndTime(end_time);

	if (!m_track)
		return;

	OP_ASSERT(m_track->GetCueList());

	// Update the position in the hosting cue list.
	m_track->GetCueList()->Update(this);

	// Signal the MediaElement.
	if (MediaElement* media_element = GetMediaElement())
		media_element->HandleCueChange(this, MediaElement::CUE_TIME_CHANGED,
									   environment->GetCurrentScriptThread());
}

#define DEFINE_CUE_DOM_WRAPPER(METHOD, TYPE, ARG, MEMBER)				\
void MediaTrackCue::DOM##METHOD(DOM_Environment* environment, TYPE ARG)	\
{																		\
	if (MEMBER == ARG)													\
		return;															\
	METHOD(ARG);														\
	if (!m_track)														\
		return;															\
	if (MediaElement* media_element = GetMediaElement())				\
		media_element->HandleCueChange(this, MediaElement::CUE_LAYOUT_CHANGED, \
									   environment->GetCurrentScriptThread()); \
}

DEFINE_CUE_DOM_WRAPPER(SetDirection, MediaTrackCueDirection, dir, GetDirection())
DEFINE_CUE_DOM_WRAPPER(SetSnapToLines, bool, snap_to_lines, static_cast<bool>(m_snap_to_lines))
DEFINE_CUE_DOM_WRAPPER(SetTextPosition, unsigned int, text_pos, m_text_pos)
DEFINE_CUE_DOM_WRAPPER(SetSize, unsigned int, size, m_size)
DEFINE_CUE_DOM_WRAPPER(SetAlignment, MediaTrackCueAlignment, align, GetAlignment())

#undef DEFINE_CUE_DOM_WRAPPER

void
MediaTrackCue::DOMSetLinePosition(DOM_Environment* environment, int line_pos)
{
	if (m_line_pos == line_pos && !IsLinePositionAuto())
		return;

	SetLinePosition(line_pos);

	if (!m_track)
		return;

	if (MediaElement* media_element = GetMediaElement())
		media_element->HandleCueChange(this, MediaElement::CUE_LAYOUT_CHANGED,
									   environment->GetCurrentScriptThread());
}

OP_STATUS MediaTrackCue::DOMGetAsHTML(HLDocProfile* hld_profile, HTML_Element* root)
{
	RETURN_IF_ERROR(EnsureCueNodes());

	return m_cue_nodes->CloneSubtreeForDOM(hld_profile, root, NS_IDX_HTML);
}

int
MediaTrackCue::GetComputedLinePosition() const
{
	if (!IsLinePositionAuto())
		return GetLinePosition();

	if (!GetSnapToLines())
		return 100;

	if (MediaElement* media_element = GetMediaElement())
		return -(media_element->GetVisualTrackPosition(GetOwnerTrack()) + 1);

	return -1;
}

bool
MediaTrackCue::IsActive() const
{
	return m_track && m_track->HasActiveCue(this);
}

void
MediaTrackCue::Activate()
{
	OP_ASSERT(m_track);

	m_track->ActivateCue(this);
}

WVTT_Node*
MediaTrackCue::GetNextTimestamp(WVTT_Node* start)
{
	double previous_ts = m_start_time;

	if (start)
	{
		// If we start on a timestamp node it is assumed to be valid.
		if (start->Type() == WVTT_TIMESTAMP)
			previous_ts = start->GetTimestamp();

		start = start->Next();
	}

	WVTT_Node* iter = start;

	while (iter)
	{
		if (iter->Type() == WVTT_TIMESTAMP)
		{
			double ts = iter->GetTimestamp();

			// Is the timestamp valid?
			if (ts > previous_ts && ts < m_end_time)
				break;
		}

		iter = iter->Next();
	}
	return iter;
}

OP_STATUS MediaTrackCue::EnsureCueNodes()
{
	if (!m_cue_nodes)
	{
		WebVTT_Parser cue_parser;
		m_cue_nodes = cue_parser.ParseCueText(m_text.string, m_text.length);
		if (!m_cue_nodes)
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
MediaTrackCue::SetNonEmptyString(StringWithLength& dst, const StringWithLength& src)
{
	OP_DELETEA(dst.string);
	dst.string = UniSetNewStrN(src.length > 0 ? src.string : UNI_L(""), src.length);
	if (dst.string)
	{
		dst.length = src.length;
		return OpStatus::OK;
	}
	else
	{
		dst.length = 0;
		return OpStatus::ERR_NO_MEMORY;
	}
}

MediaTrackCue::~MediaTrackCue()
{
	OP_DELETE(m_cue_nodes);
	OP_DELETEA(const_cast<uni_char*>(m_identifier.string));
	OP_DELETEA(const_cast<uni_char*>(m_text.string));
}

/* static */ int
MediaTrackCue::InterTrackOrder(const MediaTrackCue* a, const MediaTrackCue* b)
{
	MediaTrack* a_track = a->GetOwnerTrack();
	MediaTrack* b_track = b->GetOwnerTrack();
	if (a_track != b_track)
	{
		OP_ASSERT(a_track && b_track);
		return a_track->GetListIndex() - b_track->GetListIndex();
	}

	return IntraTrackOrder(a, b);
}

/* static */ int
MediaTrackCue::IntraTrackOrder(const MediaTrackCue* a, const MediaTrackCue* b)
{
	// http://www.whatwg.org/html#text-track-cue-order
	//
	// "within each group [group == track] cues must be sorted by
	// their start time, earliest first; ..."
	if (a->m_start_time != b->m_start_time)
	{
		if (a->m_start_time > b->m_start_time)
			return 1;

		return -1;
	}

	// "then, any cues with the same start time must be sorted by
	// their end time, latest first; ..."
	if (a->m_end_time != b->m_end_time)
	{
		if (a->m_end_time > b->m_end_time)
			return -1;

		return 1;
	}

	// "and finally, any cues with identical end times must be sorted
	// in the order they were created ..."
	if (a->m_order < b->m_order)
		return -1;

	OP_ASSERT(a == b || a->m_order > b->m_order);
	return a == b ? 0 : 1;
}

static BidiCategory
ResolveBiDiParagraphLevel(WVTT_Node* node)
{
	// Walk the text of the cue, until we find the first non-weak
	// codepoint (excluding embeds or overrides). Expect this to
	// terminate fairly quickly in general. This will not correctly
	// handle surrogate pairs that straddle text nodes - although it
	// ought to not matter much in practice.
	while (node)
	{
		switch (node->Type())
		{
		case WVTT_RT:
			// Ruby text and descendants should not be considered.
			node = node->NextSibling();
			break;

		case WVTT_TEXT:
			{
				UnicodeStringIterator iter(node->GetText());

				while (!iter.IsAtEnd())
				{
					BidiCategory bidicat = Unicode::GetBidiCategory(iter.At());
					if (bidicat == BIDI_L || bidicat == BIDI_R || bidicat == BIDI_AL)
						return bidicat == BIDI_AL ? BIDI_R : bidicat;

					// If a paragraph separator (BiDi class B) is encountered
					// before the first strong character the resulting level is
					// 0 - i.e. even / left-to-right.
					if (bidicat == BIDI_B)
						return BIDI_L;

					iter.Next();
				}
			}
			// fall through

		default:
			node = node->Next();
		}
	}
	return BIDI_L;
}

WritingSystem::Script
MediaCueDisplayState::GetScript() const
{
	OP_ASSERT(m_cue);

	if (MediaTrack* track = m_cue->GetOwnerTrack())
		return track->GetScript();

	return WritingSystem::Unknown;
}

CSSValue
MediaCueDisplayState::GetAlignment() const
{
	CSSValue css_text_align;
	switch (m_cue->GetAlignment())
	{
	case CUE_ALIGNMENT_START:
		css_text_align = IsRTL() ? CSS_VALUE_right : CSS_VALUE_left;
		break;
	case CUE_ALIGNMENT_END:
		css_text_align = IsRTL() ? CSS_VALUE_left : CSS_VALUE_right;
		break;
	default:
		OP_ASSERT(!"Unexpected cue alignment value.");
	case CUE_ALIGNMENT_MIDDLE:
		css_text_align = CSS_VALUE_center;
		break;
	}
	return css_text_align;
}

// http://dev.w3.org/html5/webvtt/#webvtt-cue-text-rendering-rules
void
MediaCueDisplayState::CalculateDefaultPosition(int viewport_width, int viewport_height)
{
	// Step 10.2, 10.3
	BidiCategory bidi_category = ResolveBiDiParagraphLevel(m_cue->GetCueNodes());
	m_direction = bidi_category == BIDI_L ? CSS_VALUE_ltr : CSS_VALUE_rtl;

	// Step 10.4
	// Ignoring. Covers block-flow which is related to writing
	// direction/mode. We're only handling the horizontal cases.

	int text_pos = m_cue->GetTextPosition();
	int max_cue_size;

	// Step 10.5
	MediaTrackCueAlignment cue_align = m_cue->GetAlignment();
	switch (cue_align)
	{
	case CUE_ALIGNMENT_START:
		max_cue_size = 100 - text_pos;
		break;
	case CUE_ALIGNMENT_END:
		max_cue_size = text_pos;
		break;
	default:
		OP_ASSERT(!"Unexpected cue alignment value.");
	case CUE_ALIGNMENT_MIDDLE:
		int s = text_pos <= 50 ? text_pos : 100 - text_pos;
		max_cue_size = s * 2;
		break;
	}

	if (m_direction == CSS_VALUE_rtl &&
		(cue_align == CUE_ALIGNMENT_START || cue_align == CUE_ALIGNMENT_END))
	{
		// Cue is RTL, so swap start and end.
		max_cue_size = 100 - max_cue_size;
	}

	// Step 10.6
	int cue_size = MIN(static_cast<int>(m_cue->GetSize()), max_cue_size);

	// Step 10.7
	// "size vw"
	m_pos_rect.width = (cue_size * viewport_width + 50) / 100;
	m_pos_rect.height = 0;

	// Step 10.8
	if (m_direction == CSS_VALUE_rtl)
		// Cue is RTL, so reverse the position.
		text_pos = 100 - text_pos;

	int x_pos;
	switch (m_cue->GetAlignment())
	{
	case CUE_ALIGNMENT_START:
		x_pos = 2 * text_pos;
		break;
	case CUE_ALIGNMENT_END:
		x_pos = 2 * (text_pos - cue_size);
		break;
	default:
		OP_ASSERT(!"Unexpected cue alignment value.");
	case CUE_ALIGNMENT_MIDDLE:
		x_pos = 2 * text_pos - cue_size;
		break;
	}

	OP_ASSERT(x_pos >= 0 && x_pos <= 200);

	// Step 10.9
	int y_pos;
	if (m_cue->GetSnapToLines())
		y_pos = 0;
	else
		y_pos = m_cue->GetLinePosition();

	OP_ASSERT(y_pos >= 0 && y_pos <= 100);

	// Step 10.10
	// "x-position vw", "y-position vh"
	m_pos_rect.x = (x_pos * viewport_width + 100) / 200;
	m_pos_rect.y = (y_pos * viewport_height + 50) / 100;

	// Compute the default font-size - this should be 5vh, but the
	// lack of support for 'vh', and the lack of a proper(?)
	// containing-block...
	m_computed_fontsize = viewport_height * 0.05f;
}

bool
MediaCueDisplayState::IsOverlapping(List<MediaCueDisplayState>& output) const
{
	// This cue should not be in output right now.
	OP_ASSERT(!output.HasLink(const_cast<MediaCueDisplayState*>(this)));

	for (MediaCueDisplayState* cuestate = output.First();
		 cuestate; cuestate = cuestate->Suc())
	{
		if (cuestate->m_pos_rect.Intersecting(m_pos_rect))
			return true;
	}
	return false;
}

void
MediaCueDisplayState::UpdatePosition(List<MediaCueDisplayState>& output,
									 const OpRect& video_area)
{
	// Step 10.12
	// Skip the following if there are no "line boxes" for the cue.

	// Step 10.13
	if (m_cue->GetSnapToLines())
	{
		// Step 10.13.1
		int step = m_computed_first_line_height;
		// Step 10.13.2
		if (step == 0)
			return;

		// Step 10.13.3
		int line_pos = m_cue->GetComputedLinePosition();
		// Step 10.13.5
		int pos = step * line_pos;
		// Step 10.13.7
		if (line_pos < 0)
		{
			pos += video_area.height;
			step = -step;
		}
		// Step 10.13.8
		// NOTE: Currently assuming that "all boxes" will be
		// positioned relative to the cue root, and thus adjusting the
		// bounding box will be the same as adjusting "all boxes".
		m_pos_rect.y += pos;
		// Step 10.13.9
		OpRect default_pos = m_pos_rect;
		// Step 10.13.10
		bool switched = false;

		// Note: The fallback position concept has been suggested in
		// https://www.w3.org/Bugs/Public/show_bug.cgi?id=17483 but is
		// not in the spec. The added steps are labeled "fallback."
		OpRect fallback_pos;
		while (true)
		{
			// Step 10.13.11
			if (!IsOverlapping(output))
			{
				if (IsEnclosed(video_area))
					break;

				// fallback
				OpRect prev = fallback_pos;
				prev.IntersectWith(video_area);
				OpRect cand = m_pos_rect;
				cand.IntersectWith(video_area);
				if (cand.width * cand.height > prev.width * prev.height)
					fallback_pos = m_pos_rect;
			}

			// Step 10.13.12
			if (m_pos_rect.Intersecting(video_area))
			{
				// Step 10.13.13
				m_pos_rect.y += step;

				// Step 10.13.14
			}
			else
			{
				// Step 10.13.16
				if (switched)
				{
					// fallback
					if (!fallback_pos.IsEmpty())
						m_pos_rect = fallback_pos;
					break;
				}

				// Step 10.13.15
				m_pos_rect = default_pos;

				// Step 10.13.17
				step = -step;

				// Step 10.13.18
				switched = true;
			}
		}
	}
	else
	{
		// Note: "else branch" of 10.13, i.e. the numbering overlaps
		// with the above, but the steps are different.

		// Step 10.13.1
		int pos_x = m_cue->GetTextPosition();
		int pos_y = m_cue->GetLinePosition();

		if (m_direction == CSS_VALUE_rtl)
			pos_x = 100 - pos_x;

		// Step 10.13.2
		int video_anchor_x = pos_x * video_area.width / 100;
		int video_anchor_y = pos_y * video_area.height / 100;

		m_pos_rect.x = video_anchor_x - pos_x * m_pos_rect.width / 100;
		m_pos_rect.y = video_anchor_y - pos_y * m_pos_rect.height / 100;

		// Step 10.13.3
		if (IsOverlapping(output) || !IsEnclosed(video_area))
		{
			// Step 10.13.4
			OpRect new_position;
			if (FindPosition(output, video_area, new_position))
				m_pos_rect = new_position;
			// else: Step 10.13.5
		}
	}

	// Step 10.14 - done positioning
	// Remove "line boxes" that do not fit inside the video area.
}

static bool
PositionIsBetter(double dist, const OpPoint& pos,
				 double curr_dist, const OpPoint& curr_pos)
{
	if (dist < curr_dist)
		return true;

	// "If there are multiple such positions that are equidistant from
	// their current position, use the highest one amongst them; if
	// there are several at that height, then use the leftmost one
	// amongst them."
	if (dist == curr_dist)
	{
		if (pos.y < curr_pos.y)
			return true;

		if (pos.y == curr_pos.y && pos.x < curr_pos.x)
			return true;
	}
	return false;
}

bool
MediaCueDisplayState::FindPosition(List<MediaCueDisplayState>& output, const OpRect& video_area,
								   OpRect& out_position) const
{
	// "If there is a position to which the boxes in boxes can be moved
	// while maintaining the relative positions of the boxes in boxes
	// to each other such that none of the boxes in boxes would
	// overlap any of the boxes in output, and all the boxes in output
	// would be within the video's rendering area, then move the boxes
	// in boxes to the closest such position to their current
	// position, and then jump to the step labeled done positioning
	// below. If there are multiple such positions that are
	// equidistant from their current position, use the highest one
	// amongst them; if there are several at that height, then use the
	// leftmost one amongst them."

	// We try to implement the above in approx. the following way:
	//
	// Q = input cue area.
	//
	// 1) Construct a set of points containing all the valid positions
	//    for the top-left corner of Q.
	//    a) Add the video area to the set.
	//    b) For each cue C in output:
	//      I) Remove the area of C.
	//     II) Remove the area resulting from sweeping the left and
	//         top sides of C with Q.
	//
	// 2) If the set is empty, there is no position exist to which Q
	//    can be moved - terminate.
	//
	// 3) Find the closest point in the set to which Q can be moved.
	//

	// Inset the video area on the bottom and right edges.
	OpRect search_area = video_area;
	search_area.width -= m_pos_rect.width - 1;
	search_area.height -= m_pos_rect.height - 1;

	// Does not fit at all.
	if (search_area.IsEmpty())
		return false;

	// Start with the computed search area - this is the initial
	// point set.
	OpRegion allowed_points;
	if (!allowed_points.IncludeRect(search_area))
		return false;

	// For each cue in output, remove all points from the point set
	// where 'rect' cannot be positioned.
	for (MediaCueDisplayState* cuestate = output.First();
		 cuestate; cuestate = cuestate->Suc())
	{
		OpRect r = cuestate->GetRect();

		// Expand 'r' to the left and top to remove any positions at
		// which 'rect' would overlap.
		r.x -= m_pos_rect.width - 1;
		r.y -= m_pos_rect.height - 1;
		r.width += m_pos_rect.width - 1;
		r.height += m_pos_rect.height - 1;

		if (!allowed_points.RemoveRect(r))
			return false;
	}

	// The region now only contains points where rect could be
	// positioned without overlapping any of the cues currently in
	// output. To find the best solution iterate the region and
	// compute the shortest distance for each area/point set.

	// Try to reduce the number of sub-areas/point sets that we need
	// to consider.
	allowed_points.CoalesceRects();

	OpRegionIterator iter = allowed_points.GetIterator();
	if (!iter.First())
		// The set is empty.
		return false;

	// Track the currently shortest move/distance (Euclidean distance).
	double best_distance = search_area.width + search_area.height;

	out_position = m_pos_rect;

	do
	{
		const OpRect& set = iter.GetRect();

		// Compute the distance from the original position of
		// 'rect' and to the closest point in the current
		// (sub)set.
		OpPoint closest = m_pos_rect.TopLeft();
		if (closest.x >= set.Right())
			closest.x = set.Right() - 1;
		else if (closest.x < set.Left())
			closest.x = set.Left();

		if (closest.y >= set.Bottom())
			closest.y = set.Bottom() - 1;
		else if (closest.y < set.Top())
			closest.y = set.Top();

		OP_ASSERT(set.Contains(closest));

		double dx = closest.x - m_pos_rect.x;
		double dy = closest.y - m_pos_rect.y;
		double distance = op_sqrt(dx * dx + dy * dy);

		if (PositionIsBetter(distance, closest,
							 best_distance, out_position.TopLeft()))
		{
			best_distance = distance;

			out_position.x = closest.x;
			out_position.y = closest.y;
		}

	} while (iter.Next());

	return true;
}

OP_STATUS
MediaCueDisplayState::Attach(FramesDocument* frm_doc, HTML_Element* track_root)
{
	OP_ASSERT(m_fragment.GetElm() == NULL);

	HLDocProfile* hld_profile = frm_doc->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	RETURN_IF_ERROR(CreateRenderingFragment(hld_profile, track_root));

	m_current_ts_pred.Reset();

	return m_fragment->UnderSafe(frm_doc, track_root);
}

void
MediaCueDisplayState::Detach(FramesDocument* frm_doc)
{
	if (HTML_Element* cue_root = m_fragment.GetElm())
	{
		m_fragment.Reset();

		cue_root->Remove(frm_doc, TRUE);

#ifdef DEBUG_ENABLE_OPASSERT
		BOOL can_free =
#endif // DEBUG_ENABLE_OPASSERT
		cue_root->Clean(frm_doc);

		// There should never be any references to these fragments
		// from the scripting environment.
		OP_ASSERT(can_free);
		cue_root->Free(frm_doc);
	}
}

void
MediaCueDisplayState::EnsureAttachment(HTML_Element* track_root)
{
	OP_ASSERT(track_root && track_root->IsMatchingType(Markup::MEDE_VIDEO_TRACKS, NS_HTML));
	OP_ASSERT(m_fragment.GetElm() != NULL);

	if (m_fragment.GetElm()->Parent() != track_root)
	{
		// Reparent the fragment to the actual track root.
		m_fragment->Out();
		m_fragment->Under(track_root);
	}
}

void
MediaCueDisplayState::MarkDirty(FramesDocument* frm_doc)
{
	m_fragment->MarkDirty(frm_doc);
}

void
MediaCueDisplayState::MarkPropsDirty(FramesDocument* frm_doc)
{
	m_fragment->MarkPropsDirty(frm_doc);
}

HTML_Element*
MediaCueDisplayState::GetTrackRoot() const
{
	return m_fragment->Parent();
}

OP_STATUS
MediaCueDisplayState::CreateRenderingFragment(HLDocProfile* hld_profile, HTML_Element* track_root)
{
	HTML_Element* cue_root = NEW_HTML_Element();

	// 'id' + terminator
	HtmlAttrEntry attr_list[2]; // ARRAY OK 2011-12-20 fs
	HtmlAttrEntry* attrs = NULL;

	const StringWithLength& cue_identifier = m_cue->GetIdentifier();
	if (cue_identifier.length > 0)
	{
		attr_list[0].attr = Markup::HA_ID;
		attr_list[0].ns_idx = NS_IDX_HTML;
		attr_list[0].value = cue_identifier.string;
		attr_list[0].value_len = cue_identifier.length;

		attrs = attr_list;
	}

	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if (!cue_root ||
		OpStatus::IsError(status = cue_root->Construct(hld_profile, NS_IDX_CUE, Markup::CUEE_ROOT, attrs)))
	{
		DELETE_HTML_Element(cue_root);
		return status;
	}

	m_cue->GetCueNodes()->CloneSubtreeForDOM(hld_profile, cue_root, NS_IDX_CUE);

	// Mark the elements as HE_INSERTED_BY_TRACK (CloneSubtreeForDOM
	// does not for obvious reasons).
	for (HTML_Element* iter = cue_root; iter; iter = iter->Next())
		iter->SetInserted(HE_INSERTED_BY_TRACK);

	cue_root->SetSpecialAttr(Markup::MEDA_COMPLEX_CUE_REFERENCE, ITEM_TYPE_COMPLEX,
							 static_cast<void*>(this), FALSE,
							 SpecialNs::NS_MEDIA);

	m_fragment.SetElm(cue_root);
	return OpStatus::OK;
}

void
MediaCueDisplayState::UpdateTimestamps(WVTT_Node* start, double current_time)
{
	if (!start)
		start = m_cue->GetCueNodes();

	WVTT_Node* timestamp = m_cue->GetNextTimestamp(start);
	while (timestamp)
	{
		if (timestamp->GetTimestamp() > current_time)
			break;

		m_curr_timestamp = timestamp;

		timestamp = m_cue->GetNextTimestamp(timestamp);
	}

	m_next_timestamp = timestamp;
}

void
MediaCueDisplayState::ResetIntraCueState(double current_time, FramesDocument* doc)
{
	OP_ASSERT(m_cue);

	m_curr_timestamp = NULL;

	// Set the current timestamp to the first timestamp in the
	// fragment which has a time that is less than or equal to the
	// current time.
	UpdateTimestamps(m_curr_timestamp, current_time);

	ResetCueFragment(doc);

	// If no current or next timestamp was found, just leave the
	// fragment in the reset state ('present').
	if (m_curr_timestamp || m_next_timestamp)
		UpdateCueFragment(doc, NULL);
}

// Find the first element in pre-order that isn't in the past.
HTML_Element*
MediaCueDisplayState::GetIntraBoundary()
{
	HTML_Element* cue_root = m_fragment.GetElm();
	if (!cue_root)
		return NULL;

	OP_ASSERT(cue_root->IsMatchingType(Markup::CUEE_ROOT, NS_CUE));
	OP_ASSERT(cue_root->FirstChild());

	if (m_current_ts_pred.GetElm())
		return m_current_ts_pred.GetElm()->Next();

	// No reference to the last timestamp predecessor - find it.

	HTML_Element* iter = cue_root->FirstChild();
	HTML_Element* stop = cue_root->NextSibling();

	// No intra-boundary if the cue is empty.
	if (!iter)
		return NULL;

	// Skip the background box if it's there.
	if (iter->IsMatchingType(Markup::CUEE_BACKGROUND, NS_CUE))
		iter = iter->FirstChild();

	while (iter != stop)
	{
		if (!iter->IsText())
			if (GetTimeState(iter) > CUE_TIMESTATE_PAST)
				return iter;

		iter = iter->Next();
	}
	return NULL;
}

static inline void
SetTimeState(HTML_Element* element, MediaCueTimeState state)
{
	element->SetNumAttr(Markup::MEDA_CUE_TIMESTATE, state, NS_IDX_CUE);
}

static inline void
ResetTimeState(HTML_Element* element)
{
	element->RemoveAttribute(Markup::MEDA_CUE_TIMESTATE, NS_IDX_CUE);
}

// Clear the intra-state in the cue fragment.
void
MediaCueDisplayState::ResetCueFragment(FramesDocument* doc)
{
	HTML_Element* cue_root = m_fragment.GetElm();
	if (!cue_root)
	{
		OP_ASSERT(m_current_ts_pred.GetElm() == NULL);
		return;
	}

	// If there is a "future" cue, set timestate to 'future' -
	// otherwise just set it to 'present' (the default value).
	bool set_to_future = m_next_timestamp != NULL;

	HTML_Element* stop = cue_root->NextSibling();
	HTML_Element* iter = cue_root->FirstChild();

	// Do nothing if the cue is empty.
	if (!iter)
		return;

	// Skip the background box if it's there.
	if (iter->IsMatchingType(Markup::CUEE_BACKGROUND, NS_CUE))
		iter = iter->FirstChild();

	// Set or reset time-state on relevant elements.
	while (iter != stop)
	{
		if (!iter->IsText())
		{
			if (set_to_future)
				SetTimeState(iter, CUE_TIMESTATE_FUTURE);
			else
				ResetTimeState(iter);
		}

		iter = iter->Next();
	}

	m_fragment->MarkPropsDirty(doc, 0, TRUE);

	m_current_ts_pred.Reset();
}

/** Simple helper for synchronized walking of the different cue fragments. */
class TimeStateUpdater
{
public:
	TimeStateUpdater(HTML_Element* start, HTML_Element* stop,
					 WVTT_Node* node) :
		m_current_node(node),
		m_prev(start),
		m_current(start),
		m_stop(stop) {}

	void UpdateTo(FramesDocument* doc, WVTT_Node* target_timestamp,
				  MediaCueTimeState timestate);

	bool DidUpdate() const { return m_prev != m_current; }
	HTML_Element* GetLastVisited() const { return m_prev; }

#ifdef _DEBUG
	void VerifyTimeStateBefore(HTML_Element* cue_root);
	void VerifyTimeStateAfter(WVTT_Node* next_ts);
#endif // _DEBUG

private:
	WVTT_Node* m_current_node;
	HTML_Element* m_prev;
	HTML_Element* m_current;
	HTML_Element* m_stop;
};

void
TimeStateUpdater::UpdateTo(FramesDocument* doc,
						   WVTT_Node* target_timestamp,
						   MediaCueTimeState timestate)
{
	OP_ASSERT(!m_current_node && m_current == m_stop ||
			  m_current_node && m_current_node->Type() != WVTT_ROOT);

	while (m_current != m_stop)
	{
		OP_ASSERT(m_current_node);

		// Timestamps not included in the HTML representation.
		if (m_current_node->Type() == WVTT_TIMESTAMP)
		{
			// Have we reached the target timestamp?
			if (m_current_node == target_timestamp)
				break;
		}
		else
		{
			if (!m_current->IsText())
			{
				SetTimeState(m_current, timestate);

				m_current->MarkPropsDirty(doc);
			}

			m_prev = m_current;
			m_current = m_current->Next();
		}

		m_current_node = m_current_node->Next();
	}
}

#ifdef _DEBUG
void
TimeStateUpdater::VerifyTimeStateBefore(HTML_Element* cue_root)
{
	HTML_Element* current = m_current->Prev();

	while (current != cue_root)
	{
		// Filter out the 'background box' by only checking elements
		// inserted-by-track.
		if (!current->IsText() && current->GetInserted() == HE_INSERTED_BY_TRACK)
			OP_ASSERT(MediaCueDisplayState::GetTimeState(current) == CUE_TIMESTATE_PAST);

		current = current->Prev();
	}
}

void
TimeStateUpdater::VerifyTimeStateAfter(WVTT_Node* next_ts)
{
	// If there's no 'next timestamp' we expect 'present' - else 'future'.
	MediaCueTimeState expected =
		next_ts != NULL ? CUE_TIMESTATE_FUTURE : CUE_TIMESTATE_PRESENT;
	HTML_Element* current = m_current;

	while (current != m_stop)
	{
		if (!current->IsText())
			OP_ASSERT(MediaCueDisplayState::GetTimeState(current) == expected);

		current = current->Next();
	}
}
#endif // _DEBUG

// Update the cue fragment between the previous timestamp (prev_ts),
// the current timestamp (m_curr_timestamp) and the next timestamp
// (m_next_timestamp). The part of the fragment between prev_ts and
// m_curr_timestamp will be marked as 'past' (matched by :past) and
// the part between m_curr_timestamp and m_next_timestamp will be
// marked as 'present' (matched neither of :past or :future). The
// remaining elements are assumed to have been marked correctly by
// ResetCueFragment.
//
// The caller is expected to have updated the timestamps as necessary.
//
// Exploits the isomorphism between the WVTT_Node tree and the cue fragment.
void
MediaCueDisplayState::UpdateCueFragment(FramesDocument* doc, WVTT_Node* prev_ts)
{
	// Get the last marked element.
	HTML_Element* current = GetIntraBoundary();
	if (!current)
		return;

	if (!prev_ts)
		prev_ts = m_cue->GetCueNodes();

	TimeStateUpdater updater(current, m_fragment->NextSibling(), prev_ts->Next());

#ifdef _DEBUG
	// Verify that the "head" has the timestate we'd expect ('past').
	updater.VerifyTimeStateBefore(m_fragment.GetElm());
#endif // _DEBUG

	// Update range from previous to current timestamp to 'past'.
	if (m_curr_timestamp)
		updater.UpdateTo(doc, m_curr_timestamp, CUE_TIMESTATE_PAST);

	// Don't update the current timestamp element reference if we
	// didn't move in the rendering fragment. This happens for
	// instance when the first element in the cue fragment is a
	// timestamp node.
	if (updater.DidUpdate())
		m_current_ts_pred.SetElm(updater.GetLastVisited());

	// Update range from current to next timestamp to 'present'.
	// (m_next_timestamp is allowed to be NULL here - meaning that
	// the rest of the fragment will be updated.)
	updater.UpdateTo(doc, m_next_timestamp, CUE_TIMESTATE_PRESENT);

#ifdef _DEBUG
	// Verify that the "tail" has the timestate we'd expect ('present'
	// or 'future').
	updater.VerifyTimeStateAfter(m_next_timestamp);
#endif // _DEBUG
}

void
MediaCueDisplayState::AdvanceIntraCueState(double current_time, FramesDocument* doc)
{
	OP_ASSERT(m_cue);

	// If time was rewound - implying a seek - reset the intra-cue
	// state on the fragment before advancing.
	bool needs_reset = false;
	if (m_curr_timestamp && m_curr_timestamp->GetTimestamp() > current_time)
	{
		needs_reset = true;
		m_curr_timestamp = NULL;
	}

	WVTT_Node* prev_timestamp = m_curr_timestamp;

	// Advance the current timestamp.
	UpdateTimestamps(m_curr_timestamp, current_time);

	if (needs_reset)
		ResetCueFragment(doc);

	if (prev_timestamp != m_curr_timestamp)
		UpdateCueFragment(doc, prev_timestamp);
}

double
MediaCueDisplayState::NextEventTime() const
{
	if (m_next_timestamp)
		return m_next_timestamp->GetTimestamp();

	return m_cue->GetEndTime();
}

/* static */ MediaCueDisplayState*
MediaCueDisplayState::GetFromHtmlElement(HTML_Element* element)
{
	OP_ASSERT(element);
	OP_ASSERT(element->GetInserted() == HE_INSERTED_BY_TRACK);
	OP_ASSERT(element->IsMatchingType(Markup::CUEE_ROOT, NS_CUE));

	void* value = element->GetSpecialAttr(Markup::MEDA_COMPLEX_CUE_REFERENCE,
										  ITEM_TYPE_COMPLEX, NULL,
										  SpecialNs::NS_MEDIA);
	return static_cast<MediaCueDisplayState*>(value);
}

/* static */ MediaCueTimeState
MediaCueDisplayState::GetTimeState(HTML_Element* element)
{
	OP_ASSERT(element);
	OP_ASSERT(element->GetInserted() == HE_INSERTED_BY_TRACK);
	OP_ASSERT(element->GetNsType() == NS_CUE);

	// Defaulting to 'present' (neither :past nor :future apply) since
	// that is what should happen when the cue doesn't contain any
	// timestamp nodes.
	INTPTR num_value = element->GetNumAttr(Markup::MEDA_CUE_TIMESTATE, NS_IDX_CUE,
										   CUE_TIMESTATE_PRESENT);
	return static_cast<MediaCueTimeState>(num_value);
}

void
MediaTrackCueStorageList::Reset()
{
	for (unsigned i = 0; i < m_cues.GetCount(); i++)
		DetachOrDestroy(m_cues.Get(i));

	m_cues.Empty();
}

OP_STATUS
MediaTrackCueStorageList::Insert(MediaTrackCue* cue)
{
	unsigned start = 0;
	unsigned end = m_cues.GetCount();

	// Simple insertion at end of vector?
	if (end == 0 || MediaTrackCue::IntraTrackOrder(m_cues.Get(end - 1), cue) < 0)
	{
		start = end;
	}
	else
	{
		while (end > start)
		{
			unsigned cmp_idx = start + (end - start) / 2;
			if (MediaTrackCue::IntraTrackOrder(m_cues.Get(cmp_idx), cue) < 0)
				start = cmp_idx + 1;
			else
				end = cmp_idx;
		}
	}

	return m_cues.Insert(start, cue);
}

OP_STATUS
MediaTrackCueStorageList::Update(MediaTrackCue* cue)
{
	RemoveByItem(cue);
	return Insert(cue);
}

MediaTrackCue*
MediaTrackCueStorageList::GetCueById(const StringWithLength& id_needle)
{
	OP_ASSERT(id_needle.string);

	if (id_needle.length == 0)
		return NULL;

	for (unsigned i = 0; i < m_cues.GetCount(); i++)
	{
		MediaTrackCue* cue = m_cues.Get(i);
		if (cue->GetIdentifier() == id_needle)
			return cue;
	}
	return NULL;
}

MediaTrackCue*
MediaTrackCueActiveList::GetItem(unsigned idx)
{
	if (!m_cues)
		return NULL;

	for (MediaTrackCue* cue = m_cues->First(); cue; cue = cue->Suc(), --idx)
		if (idx == 0)
			return cue;

	return NULL;
}

MediaTrackCue*
MediaTrackCueActiveList::GetCueById(const StringWithLength& id_needle)
{
	OP_ASSERT(id_needle.string);

	if (!m_cues || id_needle.length == 0)
		return NULL;

	for (MediaTrackCue* cue = m_cues->First(); cue; cue = cue->Suc())
		if (cue->GetIdentifier() == id_needle)
			return cue;

	return NULL;
}

#endif //MEDIA_HTML_SUPPORT
