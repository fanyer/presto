/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_HTML_SUPPORT

#include "modules/media/src/trackdisplaystate.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc.h"

TrackDisplayState::TrackDisplayState(int viewport_width, int viewport_height) :
	m_controls(NULL),
	m_viewport_width(viewport_width),
	m_viewport_height(viewport_height)
{
}

TrackDisplayState::~TrackDisplayState()
{
	m_controls.Out();
	m_pending_cues.Clear();
	m_active_cues.Clear();
}

OP_STATUS
TrackDisplayState::AddCue(MediaTrackCue* cue, FramesDocument* frm_doc, HTML_Element* track_root)
{
	OpAutoPtr<MediaCueDisplayState> cuestate(OP_NEW(MediaCueDisplayState, (cue)));
	if (!cuestate.get())
		return OpStatus::ERR_NO_MEMORY;

	// Verify that we have a rendering representation of the cue - if
	// not create one.
	RETURN_IF_ERROR(cue->EnsureCueNodes());

	// Determine default position and size for the cue.
	// [section 13.4.2.1, ~steps 10.2 - 10.10]
	cuestate->CalculateDefaultPosition(m_viewport_width, m_viewport_height);

	// Create and insert a new cue fragment.
	RETURN_IF_ERROR(cuestate->Attach(frm_doc, track_root));

	cuestate->Into(&m_pending_cues);
	cuestate.release();
	return OpStatus::OK;
}

double
TrackDisplayState::NextEventTime()
{
	double event_time = op_nan(NULL);

	for (MediaCueDisplayState* cuestate = m_pending_cues.First();
		 cuestate; cuestate = cuestate->Suc())
	{
		OP_ASSERT(!cuestate->IsControlsArea());

		event_time = MediaTrackCue::MinTimestamp(event_time, cuestate->NextEventTime());
	}

	for (MediaCueDisplayState* cuestate = m_active_cues.First();
		 cuestate; cuestate = cuestate->Suc())
	{
		if (cuestate->IsControlsArea())
			continue;

		event_time = MediaTrackCue::MinTimestamp(event_time, cuestate->NextEventTime());
	}

	return event_time;
}

MediaCueDisplayState*
TrackDisplayState::GetDisplayState(MediaTrackCue* cue)
{
	// Find the cue block - look in the active list first, and then in
	// the pending list.

	for (MediaCueDisplayState* cuestate = m_active_cues.First();
		 cuestate; cuestate = cuestate->Suc())
		if (cuestate->GetCue() == cue)
			return cuestate;

	for (MediaCueDisplayState* cuestate = m_pending_cues.First();
		 cuestate; cuestate = cuestate->Suc())
		if (cuestate->GetCue() == cue)
			return cuestate;

	return NULL;
}

void
TrackDisplayState::RemoveDisplayState(MediaCueDisplayState* cuestate,
									  FramesDocument* frm_doc)
{
	OP_ASSERT(cuestate);

	cuestate->Detach(frm_doc);
	cuestate->Out();
	OP_DELETE(cuestate);
}

void
TrackDisplayState::RemoveCue(MediaTrackCue* cue, FramesDocument* frm_doc)
{
	if (MediaCueDisplayState* cuestate = GetDisplayState(cue))
		RemoveDisplayState(cuestate, frm_doc);
}

OP_STATUS
TrackDisplayState::AddCuesForTrack(MediaTrack* track, FramesDocument* frm_doc,
								   HTML_Element* track_root)
{
	track_root = GetActualTrackRoot(track_root);

	// Iterate the active list of the track and add the cues in it.
	for (MediaTrackCue* cue = track->GetFirstActiveCue(); cue; cue = cue->Suc())
		RETURN_IF_ERROR(AddCue(cue, frm_doc, track_root));

	return OpStatus::OK;
}

void
TrackDisplayState::RemoveCuesForTrack(MediaTrack* track, FramesDocument* frm_doc)
{
	MediaCueDisplayState* cuestate = m_active_cues.First();

	while (cuestate)
	{
		MediaCueDisplayState* next = cuestate->Suc();

		if (!cuestate->IsControlsArea())
		{
			// If no track is associated with the cue, it shouldn't be active.
			OP_ASSERT(cuestate->GetCue()->GetOwnerTrack());

			if (cuestate->GetCue()->GetOwnerTrack() == track)
				RemoveDisplayState(cuestate, frm_doc);
		}

		cuestate = next;
	}

	cuestate = m_pending_cues.First();

	while (cuestate)
	{
		MediaCueDisplayState* next = cuestate->Suc();

		OP_ASSERT(!cuestate->IsControlsArea());

		// If no track is associated with the cue, it shouldn't be active.
		OP_ASSERT(cuestate->GetCue()->GetOwnerTrack());

		if (cuestate->GetCue()->GetOwnerTrack() == track)
			RemoveDisplayState(cuestate, frm_doc);

		cuestate = next;
	}
}

void
TrackDisplayState::Clear(FramesDocument* frm_doc)
{
	for (MediaCueDisplayState* cuestate = m_pending_cues.First();
		 cuestate; cuestate = cuestate->Suc())
		cuestate->Detach(frm_doc);

	for (MediaCueDisplayState* cuestate = m_active_cues.First();
		 cuestate; cuestate = cuestate->Suc())
		if (!cuestate->IsControlsArea())
			cuestate->Detach(frm_doc);

	bool controls_active = !!m_controls.InList();
	m_controls.Out();

	m_pending_cues.Clear();
	m_active_cues.Clear();

	if (controls_active)
		m_controls.IntoStart(&m_active_cues);
}

void
TrackDisplayState::UpdateIntraCueState(FramesDocument* doc, double current_time)
{
	// Initialize intra-cue time-state for pending cues.
	for (MediaCueDisplayState* cuestate = m_pending_cues.First();
		 cuestate; cuestate = cuestate->Suc())
	{
		OP_ASSERT(!cuestate->IsControlsArea());

		cuestate->ResetIntraCueState(current_time, doc);
	}

	// Update intra-cue time-state for active cues.
	for (MediaCueDisplayState* cuestate = m_active_cues.First();
		 cuestate; cuestate = cuestate->Suc())
	{
		if (cuestate->IsControlsArea())
			continue;

		cuestate->AdvanceIntraCueState(current_time, doc);
	}
}

void
TrackDisplayState::AttachCues(HTML_Element* track_root)
{
	// Attach cues from the active list.
	for (MediaCueDisplayState* cuestate = m_active_cues.First();
		 cuestate; cuestate = cuestate->Suc())
	{
		if (cuestate->IsControlsArea())
			continue; // Skip the controls area.

		cuestate->EnsureAttachment(track_root);
	}

	// Attach cues from the pending list.
	for (MediaCueDisplayState* cuestate = m_pending_cues.First();
		 cuestate; cuestate = cuestate->Suc())
	{
		// Controls area should never be pending.
		OP_ASSERT(!cuestate->IsControlsArea());

		cuestate->EnsureAttachment(track_root);
	}
}

OpPoint
TrackDisplayState::GetCuePosition(HTML_Element* cue_root)
{
	MediaCueDisplayState* cuestate = MediaCueDisplayState::GetFromHtmlElement(cue_root);

	// HTML_Elements passed here should always have an associated
	// state object.
	OP_ASSERT(cuestate);

	return cuestate->GetRect().TopLeft();
}

OpPoint
TrackDisplayState::OnCueLayoutFinished(HTML_Element* cue_root, int height, int first_line_height)
{
	MediaCueDisplayState* cuestate = MediaCueDisplayState::GetFromHtmlElement(cue_root);

	// HTML_Elements passed here should always have an associated
	// state object.
	OP_ASSERT(cuestate);

	// If active already, then ignore.
	if (m_active_cues.HasLink(cuestate))
		return OpPoint(0, 0);

	// This cue should be in the pending list.
	OP_ASSERT(m_pending_cues.HasLink(cuestate));

	cuestate->Out();

	cuestate->SetHeight(height);
	cuestate->SetFirstLineHeight(first_line_height);

	OpPoint old_position = cuestate->GetRect().TopLeft();
	OpRect rendering_area(0, 0, m_viewport_width, m_viewport_height);
	OP_ASSERT(!rendering_area.IsEmpty());

	// Update the position of the cue based on layout data
	// [section 13.4.2.1, ~step 10.13].
	cuestate->UpdatePosition(m_active_cues, rendering_area);

	cuestate->Into(&m_active_cues);

	// If the cue moved, we need to apply the new set of (left, top)
	// properties and reflow again (sigh).
	return cuestate->GetRect().TopLeft() - old_position;
}

/* static */ HTML_Element*
TrackDisplayState::GetTrackRoot(HTML_Element* video_element)
{
	OP_ASSERT(video_element->IsMatchingType(Markup::HTE_VIDEO, NS_HTML));

	for (HTML_Element* track_root = video_element->FirstChild();
		 track_root; track_root = track_root->Suc())
		if (track_root->GetInserted() == HE_INSERTED_BY_LAYOUT &&
			track_root->IsMatchingType(Markup::MEDE_VIDEO_TRACKS, NS_HTML))
			return track_root;

	return NULL;
}

/* static */ HTML_Element*
TrackDisplayState::GetActualTrackRoot(HTML_Element* video_element)
{
	if (HTML_Element* track_root = GetTrackRoot(video_element))
		return track_root;

	return video_element;
}

void
TrackDisplayState::MarkActiveSuccessorsDirty(FramesDocument* frm_doc,
											 MediaCueDisplayState* stop)
{
	// Only call this for cues that are in the active list.
	OP_ASSERT(m_active_cues.HasLink(stop));

	// Ensure that controls are not in the active list.
	bool controls_active = !!m_controls.InList();
	m_controls.Out();

	// Walk the current cue and all the successors in the display
	// state list, reset them, and move (prepend) them to the
	// pending list.
	while (MediaCueDisplayState* cuedisp = m_active_cues.Last())
	{
		cuedisp->Out();
		cuedisp->IntoStart(&m_pending_cues);

		if (cuedisp == stop)
			break;

		cuedisp->MarkDirty(frm_doc);
		cuedisp->CalculateDefaultPosition(m_viewport_width, m_viewport_height);
	}

	// Re-add the controls dummy to the active list if they are
	// active.
	if (controls_active)
		m_controls.IntoStart(&m_active_cues);
}

void
TrackDisplayState::MarkCueDirty(FramesDocument* frm_doc, MediaTrackCue* cue)
{
	MediaCueDisplayState* cuestate = GetDisplayState(cue);
	if (!cuestate)
		return;

	// Update the state of this element.
	cuestate->CalculateDefaultPosition(m_viewport_width, m_viewport_height);

	// If it is active, mark the cue fragment dirty and move the cue
	// and all its successors to the pending list to resolve any
	// overlap that could arise from the change.
	if (!m_active_cues.HasLink(cuestate))
		return;

	// Mark layout dirty.
	cuestate->MarkDirty(frm_doc);

	// Mark any successors in the active list as dirty (moving them to
	// the pending list).
	MarkActiveSuccessorsDirty(frm_doc, cuestate);
}

void
TrackDisplayState::MarkCueExtraDirty(FramesDocument* frm_doc, MediaTrackCue* cue)
{
	MediaCueDisplayState* cuestate = GetDisplayState(cue);
	if (!cuestate)
		return;

	// Save the current parent of the cue fragment.
	HTML_Element* track_root = cuestate->GetTrackRoot();

	// Detach the current cue fragment.
	cuestate->Detach(frm_doc);

	// Build and attach a new cue fragment.
	cuestate->Attach(frm_doc, track_root);

	// If the cue is pending there's no more work to be done.
	if (!m_active_cues.HasLink(cuestate))
		return;

	// Mark any successors in the active list as dirty (moving them to
	// the pending list).
	MarkActiveSuccessorsDirty(frm_doc, cuestate);
}

void
TrackDisplayState::MarkCuesDirty(FramesDocument* frm_doc)
{
	// Remove the controls dummy cue from the active list so that it
	// doesn't disturb the following condition.
	bool controls_active = !!m_controls.InList();
	m_controls.Out();

	// Move the active cues to the front of the pending list and clear
	// the active list.
	m_active_cues.Append(&m_pending_cues);
	m_pending_cues.Append(&m_active_cues);

	// Re-add the controls dummy to the active list if they are
	// active.
	if (controls_active)
		m_controls.IntoStart(&m_active_cues);

	for (MediaCueDisplayState* cuestate = m_pending_cues.First(); cuestate;
		 cuestate = cuestate->Suc())
	{
		cuestate->CalculateDefaultPosition(m_viewport_width, m_viewport_height);
		cuestate->MarkDirty(frm_doc);
		cuestate->MarkPropsDirty(frm_doc);
	}
}

bool
TrackDisplayState::OnControlsVisibility(const OpRect& controls_area)
{
	if (controls_area.Equals(m_controls.GetRect()))
		return false;

	m_controls.SetRect(controls_area);

	// Let the visibility of the controls be reflected by the dummy
	// cue's presence in the active list.
	if (controls_area.IsEmpty())
		m_controls.Out();
	else if (!m_controls.InList())
		m_controls.IntoStart(&m_active_cues);

	return true;
}

bool
TrackDisplayState::OnContentSize(int viewport_width, int viewport_height)
{
	if (m_viewport_width == viewport_width && m_viewport_height == viewport_height)
		return false;

	m_viewport_width = viewport_width;
	m_viewport_height = viewport_height;
	return true;
}

bool
TrackDisplayState::InvalidateCueLayout(FramesDocument* doc, bool mark_dirty)
{
	// Mark cue layout as dirty - move all active cues to the pending
	// list (while preserving the order).

	// Remove the controls dummy cue from the active list so that it
	// doesn't disturb the following condition.
	bool controls_active = !!m_controls.InList();
	m_controls.Out();

	// If there are active cues that need reprocessing, then layout
	// needs to be notified. If there are only pending cues, then
	// layout should've been notified already.
	bool had_active = !m_active_cues.Empty();

	if (mark_dirty)
		// Mark active cues as dirty.
		for (MediaCueDisplayState* cuestate = m_active_cues.First();
			 cuestate; cuestate = cuestate->Suc())
			cuestate->MarkDirty(doc);

	// Move the active cues to the front of the pending list and clear
	// the active list.
	m_active_cues.Append(&m_pending_cues);
	m_pending_cues.Append(&m_active_cues);

	// Re-add the controls dummy to the active list if they are
	// active.
	if (controls_active)
		m_controls.IntoStart(&m_active_cues);

	// Reset all previously active cues to their default position.
	for (MediaCueDisplayState* cuestate = m_pending_cues.First();
		 cuestate; cuestate = cuestate->Suc())
		cuestate->CalculateDefaultPosition(m_viewport_width, m_viewport_height);

	// In the sense mark_dirty != doc->IsReflowing(), we also need to
	// reload props on any pending cues since they've been added to
	// the tree already and hence have had their properties reloaded
	// already.
	return had_active || !(mark_dirty || m_pending_cues.Empty());
}

#endif // MEDIA_HTML_SUPPORT
