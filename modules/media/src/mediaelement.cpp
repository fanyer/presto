/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_HTML_SUPPORT

#include "modules/content_filter/content_filter.h"
#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/domutils.h"
#include "modules/layout/box/box.h"
#include "modules/layout/layout_workplace.h"
#include "modules/media/mediaelement.h"
#include "modules/media/mediastreamresource.h"
#include "modules/media/mediatrack.h"
#include "modules/media/src/trackdisplaystate.h"
#include "modules/media/src/controls/mediacontrols.h"
#include "modules/pi/OpTimeInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/style/css_media.h"

/** Set flag to value. If this causes predicate to change, call callback. */
#define UPDATE_STATE(flag, value, predicate, callback)	\
	do													\
	{													\
		BOOL initial = predicate();						\
		flag = value;									\
		if (predicate() != initial)						\
			callback();									\
	} while (FALSE)

#define UPDATE_POSTER_STATE(flag) UPDATE_STATE(flag, TRUE, DisplayPoster, MarkDirty)

#define MEDIA_PROGRESS_INTERVAL 350

/** State to manage progress event throttling.
 *
 * Actual progress is signalled via MediaPlayerListener::OnProgress,
 * but the progress events are throttled to be fired no more often
 * than MEDIA_PROGRESS_INTERVAL, as per below:
 *
 *                        +---------+
 *                        | WAITING |<-----------------------------+
 *                        +---------+                              |
 * if: OnProgress            |   ^                                 |
 * do: fire progress event,  |   |  if: progress timeout           |
 *     set progress timeout  v   |  do: no side effects            |
 *                        +---------+                              |
 *                        |  FIRED  |----------------------------->+
 *                        +---------+  if: networkState=IDLE       |
 *                           |   ^     do: clear progress timeout  |
 *                           |   |                                 |
 *      if: OnProgress       |   |  if: progress timeout           |
 *      do: no side effects  |   |  do: fire progress event,       |
 *                           v   |      set progress timeout       |
 *                        +---------+                              |
 *                        | DELAYED |------------------------------+
 *                        +---------+  if: networkState=IDLE
 *                                     do: fire progress event,
 *                                         clear progress timeout
 *
 * Additionally, all state is reset and the progress timeout is
 * cleared when the player is destroyed.
 */
enum MediaProgressState
{
	/** Idly waiting for actual progress. */
	MEDIA_PROGRESS_WAITING,
	/** A progress event was fired, further events will be delayed. */
	MEDIA_PROGRESS_FIRED,
	/** A progress event is delayed until the timeout. */
	MEDIA_PROGRESS_DELAYED
};

/** State for determining whether we have skipped a @c MEDIATIMEUPDATE event.
 *
 * We need to know whether we have skipped a "plain" @c MEDIATIMEUPDATE event
 * because such an event was already queued. If we have skipped an event, it
 * means that the previous event handler(s) used more time than
 * @c PrefsCollectionDoc::MediaPlaybackTimeUpdateInterval milliseconds, and we
 * need to queue the event as soon as the event handler threads complete.
 *
 * "Plain" @c MEDIATIMEUPDATE are those event which are fired because
 * @c PrefsCollectionDoc::MediaPlaybackTimeUpdateInterval milliseconds (or
 * more) has passed since last time it was fired.
 *
 * We transition to @c MEDIA_TIMEUPDATE_SKIPPED when we skip a
 * @c MEDIATIMEUPDATE event, and transition to @c MEDIA_TIMEUPDATE_NORMAL
 * when we (eventually) do fire the skipped @c MEDIATIMEUPDATE event.
 *
 * @c MEDIA_TIMEUPDATE_NORMAL is the initial state.
 *
 * @see MediaElement::m_timeupdate_state
 */
enum MediaTimeUpdateState
{
	/** Initial state. No @c MEDIATIMEUPDATE events skipped. */
	MEDIA_TIMEUPDATE_NORMAL,
	/** At least one @c MEDIATIMEUPDATE has been skipped, and an event should
	    be queued when the current @c MEDIATIMEUPDATE event threads complete. */
	MEDIA_TIMEUPDATE_SKIPPED
};

MediaElement::MediaElement(HTML_Element* elm)
	: Media(elm)
	, m_frm_doc(NULL)
	, m_player(NULL)
	, m_controls(NULL)
	, m_track_state(NULL)
	, m_video_width(0)
	, m_video_height(0)
	, m_content_width(0)
	, m_content_height(0)
	, m_error(MEDIA_ERR_NONE)
	, m_default_playback_rate(1.0)
	, m_playback_rate(1.0)
	, m_volume(1.0)
	, m_thread_queue(this)
	, m_select_state(SELECT_NULL)
	, m_select_pointer(this)
	, m_select_candidate(this)
#ifdef DOM_STREAM_API_SUPPORT
	, m_stream_resource(NULL)
#endif //DOM_STREAM_API_SUPPORT
	, m_track_list(NULL)
	, m_last_track_time(0.0)
	, m_last_track_realtime(0.0)
	, m_paused(TRUE)
	, m_playback_ended(FALSE)
	, m_muted(FALSE)
	, m_flag_autoplaying(TRUE)
	, m_flag_delaying_load(FALSE)
	, m_has_set_callbacks(FALSE)
	, m_fired_loadeddata(FALSE)
	, m_suspended(FALSE)
	, m_poster_failed(FALSE)
	, m_have_played(FALSE)
	, m_have_frame(FALSE)
	, m_invalidate_controls(TRUE)
	, m_select_resource_pending(FALSE)
	, m_select_tracks_pending(FALSE)
	, m_track_update_pending(FALSE)
	, m_track_sync_pending(FALSE)
	, m_track_sync_seeking(FALSE)
	, m_cached_linepositions_valid(FALSE)
	, m_cached_listpositions_valid(FALSE)
	, m_progress_state(MEDIA_PROGRESS_WAITING)
	, m_timeupdate_state(MEDIA_TIMEUPDATE_NORMAL)
#ifdef DOM_STREAM_API_SUPPORT
	, m_is_stream_resource(FALSE)
#endif // DOM_STREAM_API_SUPPORT
#ifdef DEBUG_ENABLE_OPASSERT
	, m_paused_for_buffering(FALSE)
	, m_in_state_transitions(FALSE)
#endif // DEBUG_ENABLE_OPASSERT

#ifdef PI_VIDEO_LAYER
	, m_use_video_layer(FALSE)
	, m_use_video_layer_opacity(FALSE)
#endif // PI_VIDEO_LAYER
{
	OP_ASSERT(elm->Type() == Markup::HTE_AUDIO || elm->Type() == Markup::HTE_VIDEO);
	if (elm->HasAttr(Markup::HA_MUTED))
		m_muted = TRUE;
}

MediaElement::~MediaElement()
{
	if (m_frm_doc)
		m_frm_doc->RemoveMedia(this);

#ifdef DOM_STREAM_API_SUPPORT
	ClearStreamResource(FALSE);
#endif // DOM_STREAM_API_SUPPORT

	UnsetCallbacks();

	ES_ThreadListener::Remove();

	// Unassociate with any DOM-created tracks.
	if (m_track_list)
		m_track_list->ReleaseDOMTracks();

	MediaDOMItem::DetachOrDestroy(m_track_list);

	OP_DELETE(m_player);
	OP_DELETE(m_controls);
	OP_DELETE(m_track_state);

#ifdef PI_VIDEO_LAYER
	CoreViewScrollListener::Out();
#endif // PI_VIDEO_LAYER
}

void
MediaElement::Suspend(BOOL removed)
{
	if (!m_suspended)
	{
		m_suspended = TRUE;
		if (m_player)
			OpStatus::Ignore(m_player->Suspend());
	}

	if (removed)
	{
		m_frm_doc = NULL;
	}

	OP_DELETE(m_controls);
	m_controls = NULL;

#ifdef PI_VIDEO_LAYER
	CoreViewScrollListener::Out();
#endif // PI_VIDEO_LAYER
}

OP_STATUS
MediaElement::Resume()
{
	if (m_suspended)
	{
		m_suspended = FALSE;
		if (m_player)
		{
			RETURN_IF_ERROR(m_player->Resume());
			if (IsPotentiallyPlaying())
			{
				HandleTimeUpdate();
				SynchronizeTracks();
			}
		}

		EnsureMediaControls();
	}
	return OpStatus::OK;
}

/* virtual */ void
MediaElement::OnProgress(MediaPlayer* player)
{
	OP_ASSERT(InFetchResource());

	SetNetworkState(MediaNetwork::LOADING);
	if (m_progress_state == MEDIA_PROGRESS_WAITING)
		HandleProgress();
	if (m_progress_state < MEDIA_PROGRESS_DELAYED)
		m_progress_state++;
}

/* virtual */ void
MediaElement::OnStalled(MediaPlayer* player)
{
	OP_ASSERT(InFetchResource());
	QueueDOMEvent(m_element, ONSTALLED);
}

/* virtual */ void
MediaElement::OnError(MediaPlayer* player)
{
	OP_ASSERT(InFetchResource());

	if (m_state.GetReady() == MediaReady::NOTHING)
	{
		// If the media data cannot be fetched at all, due to network
		// errors, causing the user agent to give up trying to fetch
		// the resource...

		// If the media resource is found to have Content-Type
		// metadata that, when parsed as a MIME type (including any
		// codecs described by the codec parameter), represents a type
		// that the user agent knows it cannot render (even if the
		// actual media data is in a supported format)

		SelectResourceReturn();
	}
	else
	{
		// If the connection is interrupted, causing the user agent to
		// give up trying to fetch the resource...

		// 1. The user agent should cancel the fetching process.

		// 2. Set the error attribute to a new MediaError object whose
		// code attribute is set to MEDIA_ERR_NETWORK.
		m_error = MEDIA_ERR_NETWORK;

		// 3. Queue a task to fire a simple event named error at
		// the media element.
		QueueDOMEvent(m_element, ONERROR);

		// 4. If the media element's readyState attribute has a value
		// equal to HAVE_NOTHING, set the element's networkState
		// attribute to the NETWORK_EMPTY value and queue a task to fire
		// a simple event named emptied at the element. Otherwise, set
		// the element's networkState attribute to the NETWORK_IDLE
		// value.
		// http://www.w3.org/Bugs/Public/show_bug.cgi?id=12598
		SetNetworkState(MediaNetwork::IDLE);

		// 5. Set the element's delaying-the-load-event flag to
		// false. This stops delaying the load event.
		SetDelayingLoad(FALSE);

		// 6. Abort the overall resource selection algorithm.
		m_select_state = SELECT_NULL;
	}
}

/* virtual */ void
MediaElement::OnIdle(MediaPlayer* player)
{
	OP_ASSERT(InFetchResource());

	// Once the entire media resource has been fetched (but potentially
	// before any of it has been decoded) queue a task to fire a simple
	// event named progress at the media element.
	if (m_state.GetNetwork() == MediaNetwork::LOADING)
		OnProgress(player);

	// When a media element's download has been suspended, the user
	// agent must set the networkState to NETWORK_IDLE and queue a
	// task to fire a simple event named suspend at the element.
	SetNetworkState(MediaNetwork::IDLE);
}

/* virtual */ void
MediaElement::OnForcePause(MediaPlayer* player)
{
	OpStatus::Ignore(Pause());
}

/* virtual */ void
MediaElement::OnPauseForBuffering(MediaPlayer* player)
{
	OP_ASSERT(m_state.GetReady() >= MediaReady::CURRENT_DATA);
#ifdef _DEBUG
	OP_ASSERT(!m_paused_for_buffering);
#endif // _DEBUG

	SetReadyState(MediaReady::CURRENT_DATA);
#ifdef _DEBUG
	m_paused_for_buffering = TRUE;
#endif // _DEBUG
}

/* virtual */ void
MediaElement::OnPlayAfterBuffering(MediaPlayer* player)
{
#ifdef _DEBUG
	OP_ASSERT(m_paused_for_buffering);
#endif // _DEBUG

	SetReadyState(MediaReady::ENOUGH_DATA);
#ifdef _DEBUG
	m_paused_for_buffering = FALSE;
#endif // _DEBUG
}

#if defined(PI_VIDEO_LAYER) && defined(CSS_TRANSFORMS)
static bool
HasPositiveScaleAndTranslationOnly(const AffinePos& doc_ctm)
{
	// If the document CTM is not a transform it's a simple
	// translation.
	if (!doc_ctm.IsTransform())
		return true;

	const AffineTransform& m = doc_ctm.GetTransform();

	// Is the document CTM (a transform) composed of scale of
	// translation only, and is the scale positive?
	return m.IsNonSkewing() && (m[0] > 0 && m[4] > 0);
}
#endif // PI_VIDEO_LAYER && CSS_TRANSFORMS

OP_STATUS
MediaElement::Paint(VisualDevice* visdev, OpRect video, OpRect content)
{
	BOOL paint_frame = m_player && IsImageType() && !DisplayPoster();
#ifdef PI_VIDEO_LAYER
	AffinePos doc_ctm = visdev->GetCTM();
	m_use_video_layer = paint_frame && m_player->SupportsVideoLayer();
# ifdef CSS_TRANSFORMS
	m_use_video_layer &= HasPositiveScaleAndTranslationOnly(doc_ctm);
# endif // CSS_TRANSFORMS
	if (m_use_video_layer)
	{
		// Register scroll listener and remember document rect
		if (!static_cast<CoreViewScrollListener*>(this)->InList())
			visdev->GetView()->AddScrollListener(this);
		m_doc_rect = video;
		doc_ctm.Apply(m_doc_rect);
		UpdateVideoLayerRect(visdev);

		// Clear video area to chroma key color.
		UINT8 red, green, blue, alpha;
		m_player->GetVideoLayerColor(&red, &green, &blue, &alpha);
		visdev->SetColor(red, green, blue, alpha);
		visdev->ClearRect(video);

		// Disable opacity for a non-transparent chroma key.
		if (alpha == 0)
			m_use_video_layer_opacity = TRUE;
	}
	else
	{
		if (m_player)
		{
			// An empty rect tells the platform that there's nothing to paint.
			OpRect empty_rect;
			m_player->SetVideoLayerRect(empty_rect, empty_rect);
		}
	}

	if (!m_use_video_layer)
#endif // PI_VIDEO_LAYER
	if (paint_frame)
	{
		OpBitmap* bm = NULL;

		RETURN_IF_MEMORY_ERROR(m_player->GetFrame(bm));

		if (bm)
			visdev->BitmapOut(bm, OpRect(0, 0, bm->Width(), bm->Height()), video);
	}

	// <video> elements paint controls via the layout engine.
	if (m_element->Type() == Markup::HTE_AUDIO)
		PaintControls(visdev, content);

	return OpStatus::OK;
}

void
MediaElement::PaintControls(VisualDevice* visdev, const OpRect& content)
{
	if (m_controls)
		m_controls->Paint(visdev, content);
}

UINT32
MediaElement::GetIntrinsicWidth()
{
	switch(m_element->Type())
	{
	case Markup::HTE_VIDEO:
		return m_video_width;

	case Markup::HTE_AUDIO:
		return 300;
	}
	return 0;
}

UINT32
MediaElement::GetIntrinsicHeight()
{
	switch(m_element->Type())
	{
	case Markup::HTE_VIDEO:
		return m_video_height;

	case Markup::HTE_AUDIO:
		if (EnsureMediaControls())
		{
			INT32 width, height;
			m_controls->GetPreferedSize(&width, &height, 0, 0);
			return height;
		}
	}
	return 0;
}

void
MediaElement::Update()
{
	VisualDevice* visdev;
	if (m_element->GetLayoutBox() && EnsureFramesDocument() &&
		(visdev = m_frm_doc->GetVisualDevice()))
	{
		OpRect update_rect;

#ifdef PI_VIDEO_LAYER
		if (m_use_video_layer)
		{
			// Update only controls area.
			if (!m_controls)
				return; // do nothing
			RECT content_box;
			m_element->GetLayoutBox()->GetRect(m_frm_doc, CONTENT_BOX, content_box);
			const OpRect content = OpRect(&content_box);
			update_rect = m_controls->GetControlArea(content);
		}
		else
#endif // PI_VIDEO_LAYER
		{
			// Update entire video area.
			RECT bounding_box;
			m_element->GetLayoutBox()->GetRect(m_frm_doc, BOUNDING_BOX, bounding_box);
			update_rect = OpRect(&bounding_box);
		}

		visdev->Update(update_rect.x, update_rect.y, update_rect.width, update_rect.height);
	}

	// See HandleTimeUpdate()
	m_invalidate_controls = FALSE;
}

bool
MediaElement::EnsureFramesDocument()
{
	if (!m_frm_doc)
	{
		if (LogicalDocument* log_doc = m_element->GetLogicalDocument())
			m_frm_doc = log_doc->GetFramesDocument();
		else if (DOM_Environment* dom_env = DOM_Utils::GetDOM_Environment(m_element->GetESElement()))
			m_frm_doc = dom_env->GetFramesDocument();

		if (m_frm_doc)
		{
			// Don't acquire a reference to a document that is being deleted.
			if (m_frm_doc->IsBeingDeleted())
				m_frm_doc = NULL;
			else
			{
				OP_STATUS status = m_frm_doc->AddMedia(this);
				if (OpStatus::IsError(status))
				{
					RAISE_IF_MEMORY_ERROR(status);
					m_frm_doc = NULL;
				}
			}
		}
	}

	return m_frm_doc != NULL;
}

OP_STATUS
MediaElement::GetDOMEnvironment(DOM_Environment*& environment)
{
	if (EnsureFramesDocument())
	{
		RETURN_IF_ERROR(m_frm_doc->ConstructDOMEnvironment());
		environment = m_frm_doc->GetDOMEnvironment();
		OP_ASSERT(environment);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

void
MediaElement::QueueDOMEvent(HTML_Element* target, DOM_EventType type)
{
	DOM_Environment* environment;
	OP_STATUS status = GetDOMEnvironment(environment);

	if (OpStatus::IsSuccess(status) && environment->IsEnabled())
	{
		DOM_Environment::EventData data;
		data.type = type;
		data.target = target;

		status = m_thread_queue.QueueEvent(environment, data);
	}

	RAISE_IF_MEMORY_ERROR(status);
}

void
MediaElement::QueueProgressEvent(HTML_Element* target)
{
	if (m_controls)
		m_controls->OnBufferedRangesChange();

	QueueDOMEvent(target, ONPROGRESS);
}

OP_STATUS
MediaElement::HandleElementChange(HTML_Element* element, BOOL inserted, ES_Thread* thread)
{
	switch (element->Type())
	{
	case Markup::HTE_AUDIO:
	case Markup::HTE_VIDEO:
		OP_ASSERT(element == m_element);
		if (inserted)
		{
			// If a media element whose networkState has the value
			// NETWORK_EMPTY is inserted into a document, the user
			// agent must invoke the media element's resource
			// selection algorithm.
			if (m_state.GetNetwork() == MediaNetwork::EMPTY)
				SelectResourceInvoke(thread);

			// Enable controls if appropriate
			EnsureMediaControls();
		}
		else
		{
			// When a media element is removed from a Document, if the
			// media element's networkState attribute has a value
			// other than NETWORK_EMPTY then the user agent must act
			// as if the pause() method had been invoked.
			if (m_state.GetNetwork() != MediaNetwork::EMPTY)
				RETURN_IF_ERROR(Pause(thread));

			// Disable controls unconditionally
			OP_DELETE(m_controls);
			m_controls = NULL;
		}
		break;
	case Markup::HTE_SOURCE:
		if (inserted)
		{
			OP_ASSERT(element->ParentActual() == m_element);
			// If a source element is inserted as a child of a media
			// element that has no src attribute and whose
			// networkState has the value NETWORK_EMPTY, the user
			// agent must invoke the media element's resource
			// selection algorithm.
			if (m_state.GetNetwork() == MediaNetwork::EMPTY &&
				!m_element->HasAttr(Markup::HA_SRC))
			{
				SelectResourceInvoke(thread);
			}
			// Continue the resource selection algorithm if waiting for
			// source. Only elements inserted after pointer are relevant.
			else if (m_select_state == SELECT_WAIT_FOR_SOURCE &&
					 SucSourceElement(m_select_pointer.GetElm()) == element)
			{
				SelectResource(thread);
			}
		}
		// Relevant removals handled via m_select_pointer and
		// m_select_candidate (SourceElementRefs).
		break;
	case Markup::HTE_TRACK:
		if (inserted)
		{
			OP_ASSERT(element->ParentActual() == m_element);

			// Insert track into tracklist
			HandleTrackAdded(element, thread);
		}
		// Removals are signaled using an ElementRef interface on
		// TrackElement.

		// Run track selection after awaiting a stable state.
		if (thread)
		{
			ES_ThreadListener::Remove();
			thread->GetRunningRootThread()->AddListener(this);
			m_select_tracks_pending = TRUE;
		}

		break;
	default:
		OP_ASSERT(FALSE);
	}

	return OpStatus::OK;
}

OP_STATUS
MediaElement::HandleAttributeChange(HTML_Element* element, short attr, ES_Thread* thread)
{
	OP_ASSERT(element == m_element);
	OP_ASSERT(element->Type() == Markup::HTE_AUDIO || element->Type() == Markup::HTE_VIDEO);
	switch (attr)
	{
	case Markup::HA_SRC:
		// If a src attribute of a media element is set or changed,
		// the user agent must invoke the media element's media
		// element load algorithm. (Removing the src attribute does
		// not do this, even if there are source elements present.)

		// Implementation note: we reach this point also when the
		// Audio(src) constructor is called, but Load is equivalent
		// to SelectResourceInvoke in this state.
		if (element->HasAttr(Markup::HA_SRC))
			RETURN_IF_ERROR(Load(thread));
		break;

	case Markup::HA_CONTROLS:
		EnsureMediaControls();
		if (GetElementType() == Markup::HTE_AUDIO)
			MarkDirty();
		break;

	case Markup::HA_POSTER:
		m_poster_failed = FALSE;
		break;

	case Markup::HA_PRELOAD:
		SetPreloadState(GetPreloadAttr());
		break;

	case Markup::HA_AUTOPLAY:
		if (UseAutoplay())
			SetPreloadState(MediaPreload::AUTO);
		break;
	}

	return OpStatus::OK;
}

BOOL
MediaElement::GCIsPlaying() const
{
	// Media elements must not stop playing just because all
	// references to them have been removed; only once a media element
	// to which no references exist has reached a point where no
	// further audio remains to be played for that element
	// (e.g. because the element is paused, or because the end of the
	// clip has been reached, or because its playbackRate is 0.0) may
	// the element be garbage collected.
	return !m_paused && !m_playback_ended && m_playback_rate != 0.0;
}

const uni_char *
MediaElement::CanPlayType(const uni_char* type)
{
	// The canPlayType(type) method must return the empty string if
	// type is a type that the user agent knows it cannot render; it
	// must return "probably" if the user agent is confident that the
	// type represents a media resource that it can render if used in
	// with this audio or video element; and it must return "maybe"
	// otherwise. Implementors are encouraged to return "maybe" unless
	// the type can be confidently established as being supported or
	// not. Generally, a user agent should never return "probably" if
	// the type doesn't have a codecs parameter.

	BOOL3 can_play;
	OP_STATUS status = g_media_module.CanPlayType(&can_play, type);
	if (OpStatus::IsError(status))
	{
		RAISE_IF_MEMORY_ERROR(status);
		can_play = NO;
	}

	switch (can_play)
	{
	case YES:
		return UNI_L("probably");
	case MAYBE:
		return UNI_L("maybe");
	case NO:
	default:
		return UNI_L("");
	}
}

void
MediaElement::SelectResourceInvoke(ES_Thread* thread)
{
	OP_ASSERT(m_state.GetNetwork() == MediaNetwork::EMPTY);
	OP_ASSERT(m_state.GetReady() == MediaReady::NOTHING);
	OP_ASSERT(m_select_state == SELECT_NULL);
	OP_ASSERT(m_player == NULL);

	// 1. Set the networkState to NETWORK_NO_SOURCE.
	SetNetworkState(MediaNetwork::NO_SOURCE);

	m_select_state = SELECT_LOADSTART;
	SelectResource(thread);
}

void
MediaElement::SelectResource(ES_Thread* thread)
{
	if (!EnsureFramesDocument())
	{
		// We can't call SelectResourceReturn, that would just call
		// this function again, creating an infinite loop.
		// Fortunately, since we haven't (successfully) called
		// FramesDocument::AddMedia, GetDelayingLoad won't be called,
		// so this isn't a never-stop-loading scenario.
		return;
	}

	// This function is big and full of goto. However, the algorithm is speced
	// in such a way that this makes it rather easier to follow and verify than
	// if it were refactored into several function calling each other. Only the
	// invoke and return parts are factored out because it is called from so
	// many places.

	// 2/10/20. Asynchronously await a stable state, allowing the task that
	// invoked this algorithm to continue. The synchronous section consists of
	// all the remaining steps of this algorithm until the algorithm says the
	// synchronous section has ended. (Steps in synchronous sections are marked
	// with [sync].)
	if (thread)
	{
		ES_ThreadListener::Remove();
		// Find the "most interrupted" thread which isn't completed...
		// and continue after it has finished.
		thread->GetRunningRootThread()->AddListener(this);

		m_select_resource_pending = TRUE;
		return;
	}

	switch (m_select_state)
	{
	case SELECT_LOADSTART:

		// 3. [sync] If the media element has a src attribute, then let mode be
		// attribute.
		if (m_element->HasAttr(Markup::HA_SRC))
		{
		}
		// [sync] Otherwise, if the media element does not have a src attribute
		// but has a source element child, then let mode be children and let
		// candidate be the first such source element child in tree order.
		else if (HTML_Element* candidate = SucSourceElement(NULL))
		{
			m_select_candidate.SetElm(candidate);
		}
		// [sync] Otherwise the media element has neither a src attribute nor a
		// source element child: set the networkState to NETWORK_EMPTY, and
		// abort these steps; the synchronous section ends.
		else
		{
			SetNetworkState(MediaNetwork::EMPTY);
			m_select_state = SELECT_NULL;
			break;
		}

		// 4. [sync] Set the media element's delaying-the-load-event flag to
		// true (this delays the load event), and set its networkState to
		// NETWORK_LOADING.
		SetDelayingLoad(TRUE);
		SetNetworkState(MediaNetwork::LOADING);

		// Normally, internal preload state is only allowed to
		// increase, but here we reset it so that
		// 1. an invoked preload state from a previous resource does
		// not taint the preload state of this resource,
		// 2. a preload state set by scripts before the async part of
		// the resource selection algorithm is run is respected.
		m_state.ResetPreload();
		{
			MediaPreload state;
			if (UseAutoplay())
				state = MediaPreload::AUTO;
			else if (!m_paused)
				state = MediaPreload::INVOKED;
			else
				state = GetPreloadAttr();
			SetPreloadState(state);
		}

		// 5. [sync] Queue a task to fire a simple event named loadstart at
		// the media element.
		QueueDOMEvent(m_element, ONLOADSTART);

		if (!m_element->HasAttr(Markup::HA_SRC))
			goto source_children_mode;

		// 6. If mode is attribute, then run these substeps:
		{
			// 6.1 [sync] Let absolute URL be the absolute URL that would have
			// resulted from resolving the URL specified by the src attribute's
			// value relative to the media element when the src attribute was last
			// changed. (i.e. changing xml:base or <base> after src="" has no
			// effect)
			URL* url = m_element->GetUrlAttr(Markup::HA_SRC, NS_IDX_HTML, m_frm_doc->GetLogicalDocument());
			// FIXME: should resolve URL earlier to be spec compliant

			// 6.2 [sync] If absolute URL was obtained successfully, set the
			// currentSrc attribute to absolute URL.
			// Implementation note: m_url is set in FetchResource

			// 6.3 End the synchronous section, continuing the remaining steps
			// asynchronously.

			// 6.4 If absolute URL was obtained successfully, run the resource fetch
			// algorithm with absolute URL. If that algorithm returns without
			// aborting this one, then the load failed.
			if (url && !url->IsEmpty())
			{
				m_select_state = SELECT_FETCH_SRC;
				FetchResourceInvoke(*url);
				break;
			}
			// fall through
		}

	case SELECT_FETCH_SRC_FAIL:

		// 6.5 Reaching this step indicates that the media resource failed to
		// load or that the given URL could not be resolved. Set the error
		// attribute to a new MediaError object whose code attribute is set to
		// MEDIA_ERR_SRC_NOT_SUPPORTED.
		m_error = MEDIA_ERR_SRC_NOT_SUPPORTED;

		// 6.6 Set the element's networkState attribute to the NETWORK_NO_SOURCE
		// value.
		SetNetworkState(MediaNetwork::NO_SOURCE);

		// 6.7 Queue a task to fire a simple event named error at the media
		// element.
		QueueDOMEvent(m_element, ONERROR);

		// 6.8 Set the element's delaying-the-load-event flag to false. This
		// stops delaying the load event.
		SetDelayingLoad(FALSE);

		// 6.9 Abort these steps. Until the load() method is invoked, the
		// element won't attempt to load another resource.
		m_select_state = SELECT_NULL;
		break;

	source_children_mode:
		// Otherwise, the source elements will be used; run these substeps:

		// 1. [sync] Initially, let pointer be the position between the
		// candidate node and the next node, if there are any, or the end of the
		// list, if it is the last node.
		m_select_pointer.SetElm(m_select_candidate.GetElm());

		// Implementation note: pointer is updated in HandleElementChange

	process_candidate:
		{
			// 2. [sync] Process candidate: If candidate does not have a src
			// attribute, then end the synchronous section, and jump down to the
			// failed step below.

			// 3. [sync] Let absolute URL be the absolute URL that would have
			// resulted from resolving the URL specified by candidate's src
			// attribute's value relative to the candidate when the src
			// attribute was last changed. (i.e. changing xml:base or <base>
			// after src="" has no effect)
			URL* url = m_select_candidate->GetUrlAttr(Markup::HA_SRC, NS_IDX_HTML, m_frm_doc->GetLogicalDocument());
			// FIXME: should resolve URL earlier to be spec compliant

			// 4. [sync] If absolute URL was not obtained successfully, then end
			// the synchronous section, and jump down to the failed step below.
			if (!url || url->IsEmpty())
				goto CASE_SELECT_FETCH_SOURCE_FAIL;

			// 5. [sync] If candidate has a type attribute whose value, when
			// parsed as a MIME type (including any codecs described by the
			// codec parameter), represents a type that the user agent knows it
			// cannot render, then end the synchronous section, and jump down to
			// the failed step below.
			const uni_char* type = m_select_candidate->GetStringAttr(Markup::HA_TYPE);
			if (type)
			{
				BOOL3 can_play = NO;
				RAISE_IF_MEMORY_ERROR(g_media_module.CanPlayType(&can_play, type));
				if (can_play == NO)
					goto CASE_SELECT_FETCH_SOURCE_FAIL;
			}

			// 6. [sync] If candidate has a media attribute whose value, when
			// processed according to the rules for media queries, does not
			// match the current environment, then end the synchronous section,
			// and jump down to the failed step below.
			CSS_MediaObject* media_obj = m_select_candidate->GetLinkStyleMediaObject();
			if (media_obj)
			{
				if ((media_obj->EvaluateMediaTypes(m_frm_doc) & (m_frm_doc->GetMediaType()|CSS_MEDIA_TYPE_ALL)) == 0)
					goto CASE_SELECT_FETCH_SOURCE_FAIL;
			}

			// 7. [sync] Set the currentSrc attribute to absolute URL.
			// Implementation note: m_url is set in FetchResource

			// 8. End the synchronous section, continuing the remaining steps
			// asynchronously.

			// 9. Run the resource fetch algorithm with absolute URL. If that
			// algorithm returns without aborting this one, then the load
			// failed.
			m_select_state = SELECT_FETCH_SOURCE;
			FetchResourceInvoke(*url);
			break;
		}

	case SELECT_FETCH_SOURCE_FAIL:
	CASE_SELECT_FETCH_SOURCE_FAIL:
		// 10. Failed: Queue a task to fire a simple event named error at the
		// candidate element.
		// Implementation note: candidate element may already have been removed.
		if (m_select_candidate.GetElm())
			QueueDOMEvent(m_select_candidate.GetElm(), ONERROR);

		// 11. Asynchronously await a stable state. (see above)

	find_next_candidate:
		// 12. [sync] Find next candidate: Let candidate be null.

		// 13. [sync] Search loop: If the node after pointer is the end of the
		// list, then jump to the waiting step below.

		// 14. [sync] If the node after pointer is a source element, let
		// candidate be that element.

		// 15. [sync] Advance pointer so that the node before pointer is now the
		// node that was after pointer, and the node after pointer is the node
		// after the node that used to be after pointer, if any.

		// 16. [sync] If candidate is null, jump back to the search loop
		// step. Otherwise, jump back to the process candidate step.

		m_select_candidate.SetElm(SucSourceElement(m_select_pointer.GetElm()));
		if (m_select_candidate.GetElm())
		{
			m_select_pointer.SetElm(m_select_candidate.GetElm());
			goto process_candidate;
		}

		// 17. [sync] Waiting: Set the element's networkState attribute to the
		// NETWORK_NO_SOURCE value.
		SetNetworkState(MediaNetwork::NO_SOURCE);

		// 18. [sync] Set the element's delaying-the-load-event flag to
		// false. This stops delaying the load event.
		SetDelayingLoad(FALSE);

		// 19. End the synchronous section, continuing the remaining steps
		// asynchronously.

		// 20. Wait until the node after pointer is a node other than the end of
		// the list. (This step might wait forever.)
		m_select_state = SELECT_WAIT_FOR_SOURCE;
		break;

	case SELECT_WAIT_FOR_SOURCE:
		// 21. Asynchronously await a stable state. (see above)

		// 22. [sync] Set the element's delaying-the-load-event flag back to
		// true (this delays the load event again, in case it hasn't been fired
		// yet).
		SetDelayingLoad(TRUE);

		// 23. [sync] Set the networkState back to NETWORK_LOADING.
		SetNetworkState(MediaNetwork::LOADING);

		// 24. [sync] Jump back to the find next candidate step above.
		goto find_next_candidate;

	default:
		OP_ASSERT(FALSE);
	}
}

void
MediaElement::SelectResourceReturn()
{
	// 1. The user agent should cancel the fetching process.
	// 2. Abort this subalgorithm, returning to the resource selection
	// algorithm.

	// It's possible to get both a network error and a decode error
	// simultaneously such that two conditions for returning from
	// the resource selections algorithm become true. Handle this by
	// checking m_select_state here and in the message callback.
	if (m_select_state == SELECT_NULL)
		return;

	OP_ASSERT(InFetchResource());

	// PostMessage is necessary to avoid stack overflow (CORE-18455)
	RAISE_AND_RETURN_VOID_IF_ERROR(SetCallbacks());
	g_main_message_handler->PostMessage(MSG_MEDIA_RESOURCE_SELECTION_RETURN,
										reinterpret_cast<MH_PARAM_1>(this), 0);
}

void
MediaElement::FetchResourceInvoke(const URL& url)
{
	OP_ASSERT(InFetchResource());

	// RFA-1. Let the current media resource be the resource given by
	// the absolute URL passed to this algorithm. This is now the
	// element's media resource.
	m_url = url;

	// Check if URL is blocked.
	DOM_Environment* env = NULL;
	RAISE_IF_ERROR(GetDOMEnvironment(env));

	URLFilterDOMListenerOverride lsn_over(env, m_element);
	HTMLLoadContext ctx(RESOURCE_MEDIA, m_frm_doc->GetURL(), &lsn_over, m_frm_doc->GetWindow()->GetType() == WIN_TYPE_GADGET);
	BOOL allowed = FALSE;

	if (OpStatus::IsError(g_urlfilter->CheckURL(&m_url, allowed, NULL, &ctx)) || !allowed)
	{
		SelectResourceReturn();
		return;
	}

	// Do nothing for preload=none.
	if (m_state.GetPreload() == MediaPreload::NONE)
	{
		SetNetworkState(MediaNetwork::IDLE);
		SetDelayingLoad(FALSE);
		return;
	}

	FetchResource();
}

void
MediaElement::FetchResource()
{
	OP_ASSERT(InFetchResource());

	// RFA-2. Begin to fetch the current media resource.
	OP_STATUS status = CreatePlayer();
	if (OpStatus::IsError(status))
	{
		SelectResourceReturn();
		RAISE_IF_MEMORY_ERROR(status);
	}
}

OP_STATUS
MediaElement::CreatePlayer()
{
	OP_ASSERT(!m_player);
	if (!m_player)
	{
#ifdef DOM_STREAM_API_SUPPORT
		if (IsStreamResource())
		{
			// The default preload=metadata makes no sense for live
			// streams, make them always behave as preload=auto.
			SetPreloadState(MediaPreload::AUTO);
			// Only camera streams supported for now
			RETURN_IF_ERROR(MediaPlayer::CreateStreamPlayer(m_player, m_stream_resource->GetOriginURL(), m_stream_resource));
		}
		else
#endif // DOM_STREAM_API_SUPPORT
		{
			// Note: SelectResource called EnsureFramesDocument()
			OP_ASSERT(m_frm_doc);
			RETURN_IF_ERROR(MediaPlayer::Create(m_player, m_url, m_frm_doc->GetURL(), IsImageType(), m_frm_doc->GetWindow()));
		}
		m_player->SetListener(this);
	}

	// possibly set via DOM before the player was created
	OpStatus::Ignore(m_player->SetPlaybackRate(m_playback_rate));
	OpStatus::Ignore(m_player->SetVolume(m_muted ? 0 : m_volume));

	UpdatePlayerPreload();

	if (m_paused)
		return m_player->Pause();
	else
		return m_player->Play();
}

void
MediaElement::ClearTrackTimeline()
{
	EnsureFramesDocument();

	if (m_track_state)
		m_track_state->Clear(m_frm_doc);

	if (!m_track_list)
		return;

	unsigned track_count = m_track_list->GetLength();
	for (unsigned i = 0; i < track_count; i++)
		m_track_list->GetTrackAt(i)->Deactivate();
}

void
MediaElement::DestroyPlayer()
{
	ClearTrackTimeline();

	OP_DELETE(m_player);
	m_player = NULL;
	m_playback_ended = FALSE;
	m_have_played = FALSE;
	m_have_frame = FALSE;
#ifdef PI_VIDEO_LAYER
	m_use_video_layer = FALSE;
#endif // PI_VIDEO_LAYER
	g_main_message_handler->RemoveDelayedMessage(MSG_MEDIA_PROGRESS, reinterpret_cast<MH_PARAM_1>(this), 0);
	m_progress_state = MEDIA_PROGRESS_WAITING;
}

void
MediaElement::UpdatePlayerPreload()
{
	OP_ASSERT(m_state.GetPreload() != MediaPreload::NONE && m_player);
	BOOL preload = m_state.GetPreload() > MediaPreload::METADATA;
	m_player->SetPreload(preload);
}

void
MediaElement::SetPlaybackEnded(BOOL ended)
{
	m_playback_ended = ended;
	UpdatePendingReady();
	HandleStateTransitions();
}

void
MediaElement::UpdatePendingReady()
{
	MediaReady pending = MediaReady::ENOUGH_DATA;
	if (m_state.GetPreload() == MediaPreload::NONE)
		pending = MediaReady::NOTHING;
	else if (m_state.GetPreload() == MediaPreload::METADATA || m_playback_ended)
		pending = MediaReady::CURRENT_DATA;
	// else preload==invoked/auto => pending=ENOUGH_DATA
	m_state.SetPendingReady(pending);
}

void
MediaElement::HandleStateTransitions()
{
#ifdef DEBUG_ENABLE_OPASSERT
	// This method is not reentrant and must not be run recursively,
	// as the state seen by inner invocations may be inconsistent.
	OP_ASSERT(!m_in_state_transitions);
	m_in_state_transitions = TRUE;
	unsigned transitions = 0;
#endif // DEBUG_ENABLE_OPASSERT
	while (TRUE)
	{
		const MediaPreload old_preload = m_state.GetPreload();
		const MediaNetwork old_network = m_state.GetNetwork();
		const MediaReady old_ready = m_state.GetReady();
		if (m_state.Transition())
		{
			// the theoretical maximum number of transitions is 5
			// using a data: URL with preload=auto:
			// readyState transitions NOTHING -> METADATA ->
			// CURRENT_DATA -> FUTURE_DATA -> ENOUGH_DATA and
			// networkState transitions LOADING -> IDLE
			OP_ASSERT(++transitions <= 5);
			const MediaPreload new_preload = m_state.GetPreload();
			const MediaNetwork new_network = m_state.GetNetwork();
			const MediaReady new_ready = m_state.GetReady();

			// preload transitions
			if (old_preload != new_preload)
			{
				if (m_player)
				{
					UpdatePlayerPreload();
				}
				else if (InFetchResource() && old_preload == MediaPreload::NONE)
				{
					// suspended for preload=none, now resume:
					FetchResource();
				}
				UpdatePendingReady();
			}

			// network transitions
			if (old_network != new_network)
			{
				if (m_progress_state == MEDIA_PROGRESS_DELAYED)
				{
					QueueProgressEvent(m_element);
					g_main_message_handler->RemoveDelayedMessage(MSG_MEDIA_PROGRESS, reinterpret_cast<MH_PARAM_1>(this), 0);
					m_progress_state = MEDIA_PROGRESS_WAITING;
				}
				if (new_network == MediaNetwork::IDLE)
					QueueDOMEvent(m_element, ONSUSPEND);
			}

			// ready transitions
			if (old_ready != new_ready)
				HandleReadyStateTransition(old_ready, new_ready);
		}
		else
			break;
	}
#ifdef DEBUG_ENABLE_OPASSERT
	m_in_state_transitions = FALSE;
#endif // DEBUG_ENABLE_OPASSERT
}

void
MediaElement::HandleReadyStateTransition(MediaReady old_state, MediaReady new_state)
{
	// If the previous ready state was HAVE_NOTHING, and the new ready
	// state is HAVE_METADATA
	if (old_state == MediaReady::NOTHING && new_state == MediaReady::METADATA)
	{
		// <track> will block HAVE_METADATA, so when support for that
		// is added this will have to be moved elsewhere.
		if (m_player)
			UpdatePlayerPreload();
		// Note: A loadedmetadata DOM event will be fired as part of
		// the load() algorithm.
	}
	// If the previous ready state was HAVE_METADATA and the new ready
	// state is HAVE_CURRENT_DATA or greater
	else if (old_state == MediaReady::METADATA && new_state >= MediaReady::CURRENT_DATA)
	{
		// If this is the first time this occurs for this media
		// element since the load() algorithm was last invoked, the
		// user agent must queue a task to fire a simple event called
		// loadeddata at the element.
		if (!m_fired_loadeddata)
		{
			QueueDOMEvent(m_element, MEDIALOADEDDATA);
			m_fired_loadeddata = TRUE;
		}

		// If the new ready state is HAVE_FUTURE_DATA or
		// HAVE_ENOUGH_DATA, then the relevant steps below must then
		// be run also.
	}
	// If the previous ready state was HAVE_FUTURE_DATA or more, and
	// the new ready state is HAVE_CURRENT_DATA or less
	else if (old_state >= MediaReady::FUTURE_DATA && new_state <= MediaReady::CURRENT_DATA)
	{
		// Note: A waiting DOM event can be fired, depending on the
		// current state of playback.
		if (!m_playback_ended)
			QueueDOMEvent(m_element, MEDIAWAITING);
	}
	// If the previous ready state was HAVE_CURRENT_DATA or less, and
	// the new ready state is HAVE_FUTURE_DATA
	if (old_state <= MediaReady::CURRENT_DATA && new_state == MediaReady::FUTURE_DATA)
	{
		// The user agent must queue a task to fire a simple event
		// named canplay.
		QueueDOMEvent(m_element, MEDIACANPLAY);
		// If the element is potentially playing, the user agent must
		// queue a task to fire a simple event named playing.
		if (IsPotentiallyPlaying())
			QueueDOMEvent(m_element, MEDIAPLAYING);
	}
	// If the new ready state is HAVE_ENOUGH_DATA
	else if (new_state == MediaReady::ENOUGH_DATA)
	{
		BOOL play = FALSE;

		// If the previous ready state was HAVE_CURRENT_DATA or less,
		if (old_state <= MediaReady::CURRENT_DATA)
		{
			// the user agent must queue a task to fire a simple event
			// named canplay, and,
			QueueDOMEvent(m_element, MEDIACANPLAY);
			// if the element is also potentially playing, queue a
			// task to fire a simple event named playing.
			if (IsPotentiallyPlaying())
			{
				QueueDOMEvent(m_element, MEDIAPLAYING);
				play = TRUE;
			}
		}

		// If the autoplaying flag is true, and the paused attribute
		// is true, and the media element has an autoplay attribute
		// specified,
		if (UseAutoplay())
		{
			// then the user agent may also set the paused attribute
			// to false, queue a task to fire a simple event called
			// play, and queue a task to fire a simple event called
			// playing.
			m_paused = FALSE;
			QueueDOMEvent(m_element, MEDIAPLAY);
			QueueDOMEvent(m_element, MEDIAPLAYING);

			// Note: User agents are not required to autoplay
			play = TRUE;
		}

		// In any case, the user agent must finally queue a task to
		// fire a simple event named canplaythrough.
		QueueDOMEvent(m_element, MEDIACANPLAYTHROUGH);

		if (play)
		{
			OpStatus::Ignore(m_player->Play());
			if (m_controls)
				m_controls->OnPlayPause();
			UPDATE_POSTER_STATE(m_have_played);
			HandleTimeUpdate();
			SynchronizeTracks();
		}
	}

	EnsureMediaControls();
}

OP_STATUS
MediaElement::Load(ES_Thread* thread)
{
	BOOL need_timeupdate = GetPosition() > 0;

	// Cancel loading/playing resource.
	DestroyPlayer();
	m_fired_loadeddata = FALSE;

	// force size update for new video
	// Note: will cause video element to revert to 300x150
	m_video_width = m_video_height = 0;

	// 1. Abort any already-running instance of the resource
	// selection algorithm for this element.
	m_select_state = SELECT_NULL;

	// 2. If there are any tasks from the media element's media
	// element event task source in one of the task queues, then
	// remove those tasks.
	m_thread_queue.CancelAll();

	// 3. If the media element's networkState is set to
	// NETWORK_LOADING or NETWORK_IDLE, queue a task to fire a simple
	// event named abort at the media element.
	if (m_state.GetNetwork() == MediaNetwork::LOADING || m_state.GetNetwork() == MediaNetwork::IDLE)
		QueueDOMEvent(m_element, ONABORT);

	// 4. If the media element's networkState is not set to
	// NETWORK_EMPTY, then run these substeps:
	if (m_state.GetNetwork() != MediaNetwork::EMPTY)
	{
		// 1. If a fetching process is in progress for the media
		// element, the user agent should stop it.

		// 2. Set the networkState attribute to NETWORK_EMPTY.
		SetNetworkState(MediaNetwork::EMPTY);
		// 3. If readyState is not set to HAVE_NOTHING, then set
		// it to that state.
		SetReadyState(MediaReady::NOTHING);
		// 4. If the paused attribute is false, then set to true.
		m_paused = TRUE;
		// 5. If seeking is true, set it to false.
		// Implicitly set to false when MediaPlayer is destroyed.
		// 6. Set the current playback position to 0. If this changed
		// the current playback position, then queue a task to fire a
		// simple event named timeupdate at the media element.
		if (need_timeupdate)
			HandleTimeUpdate();
		// 7. Queue a task to fire a simple event named emptied at the
		// media element.
		QueueDOMEvent(m_element, MEDIAEMPTIED);
	}

	// 5. Set the playbackRate attribute to the value of the
	// defaultPlaybackRate attribute.
	m_playback_rate = m_default_playback_rate;

	// 6. Set the error attribute to null and the autoplaying flag to true.
	m_error = MEDIA_ERR_NONE;
	m_flag_autoplaying = TRUE;

	// 7. Invoke the media element's resource selection algorithm.
	SelectResourceInvoke(thread);

	// 8. Note: Playback of any previously playing media resource
	// for this element stops.

	return OpStatus::OK;
}

/* virtual */ void
SourceElementRef::OnRemove(FramesDocument* document)
{
	m_media_element->NotifySourceRemoved(GetElm());
}

void
MediaElement::NotifySourceRemoved(HTML_Element* source_element)
{
	// Was the <source> element actually detached from its parent?
	if (source_element->ParentActual() != NULL)
		return;

	if (m_select_pointer.GetElm() == source_element)
	{
		HTML_Element* select_pointer = source_element->PredActual();
		for (; select_pointer && !select_pointer->IsMatchingType(Markup::HTE_SOURCE, NS_HTML);
			 select_pointer = select_pointer->PredActual())
			;

		m_select_pointer.SetElm(select_pointer);
	}
	else if (m_select_candidate.GetElm() == source_element)
	{
		// Queue a task to fire a simple event named error at the
		// candidate element.
		QueueDOMEvent(m_select_candidate.GetElm(), ONERROR);

		m_select_candidate.Reset();
	}
}

HTML_Element*
MediaElement::SucSourceElement(HTML_Element* elem)
{
	// NULL is used to indicate position before the first child.
	do
	{
		if (elem == NULL)
			elem = m_element->FirstChildActual();
		else
			elem = elem->SucActual();
	}
	while (elem && !elem->IsMatchingType(Markup::HTE_SOURCE, NS_HTML));
	return elem;
}

void
MediaElement::AttachCues(HTML_Element* track_root)
{
	if (!m_track_state)
		return;

	// It's our track root.
	OP_ASSERT(m_element == track_root->ParentActual());

	m_track_state->AttachCues(track_root);
}

OpPoint
MediaElement::GetCuePosition(HTML_Element* cue_root)
{
	OP_ASSERT(m_track_state);

	return m_track_state->GetCuePosition(cue_root);
}

OpPoint
MediaElement::OnCueLayoutFinished(HTML_Element* cue_root, int height, int first_line_height)
{
	OP_ASSERT(m_track_state);

	return m_track_state->OnCueLayoutFinished(cue_root, height, first_line_height);
}

bool
MediaElement::OnContentSize(int content_width, int content_height)
{
	m_content_width = content_width;
	m_content_height = content_height;

	bool cues_affected = false;
	if (m_track_state)
	{
		bool content_size_changed = m_track_state->OnContentSize(content_width, content_height);
		bool controls_changed = m_track_state->OnControlsVisibility(GetControlsArea());

		if ((content_size_changed || controls_changed) && EnsureFramesDocument())
			// This is called from reflow, so don't mark cue fragments dirty.
			cues_affected = m_track_state->InvalidateCueLayout(m_frm_doc, false);
	}
	return cues_affected;
}

void
MediaElement::MarkCuesDirty()
{
	if (!m_track_state || !EnsureFramesDocument())
		return;

	m_track_state->MarkCuesDirty(m_frm_doc);
}

void
MediaElement::GetContentSize(int& content_width, int& content_height)
{
	content_width = m_content_width;
	content_height = m_content_height;
}

int
MediaElement::GetTrackListPosition(MediaTrack* in_track)
{
	// Should only be called for tracks that are associated with this
	// MediaElement - and if they are, then the track list should
	// exist already.
	OP_ASSERT(m_track_list);

	if (!m_cached_listpositions_valid)
	{
		int list_position = 0;
		unsigned track_count = m_track_list->GetLength();
		for (unsigned i = 0; i < track_count; i++)
		{
			MediaTrack* track = m_track_list->GetTrackAt(i);

			track->SetCachedListPosition(list_position);
		}

		m_cached_listpositions_valid = TRUE;
	}
	return in_track->GetCachedListPosition();
}

int
MediaElement::GetVisualTrackPosition(MediaTrack* in_track)
{
	// Should only be called for tracks that are associated with this
	// MediaElement - and if they are, then the track list should
	// exist already.
	OP_ASSERT(m_track_list);

	if (!m_cached_linepositions_valid)
	{
		int visual_track_position = 0;
		unsigned track_count = m_track_list->GetLength();
		for (unsigned i = 0; i < track_count; i++)
		{
			MediaTrack* track = m_track_list->GetTrackAt(i);

			track->SetCachedLinePosition(visual_track_position);

			if (track->IsShowing())
				visual_track_position++;
		}

		m_cached_linepositions_valid = TRUE;
	}
	return in_track->GetCachedLinePosition();
}

void
MediaElement::InvalidatePosition(MediaTrack* track,
								 bool invalidate_listpos /* = true */)
{
	// Invalidate the line positions cache.
	m_cached_linepositions_valid = FALSE;

	// Invalidate the list positions cache.
	if (invalidate_listpos)
		m_cached_listpositions_valid = FALSE;
}

MediaTrackList*
MediaElement::GetTrackList()
{
	OpStatus::Ignore(EnsureTrackList());
	return m_track_list;
}

OP_STATUS
MediaElement::EnsureTrackList()
{
	if (!m_track_list)
		m_track_list = OP_NEW(MediaTrackList, ());

	return m_track_list ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

void
MediaElement::EnsureTrackState()
{
	if (m_track_state || !IsImageType())
		return;

	m_track_state = OP_NEW(TrackDisplayState, (m_content_width, m_content_height));

	if (m_track_state)
	{
		m_track_state->OnControlsVisibility(GetControlsArea());

		if (EnsureFramesDocument())
			m_track_state->InvalidateCueLayout(m_frm_doc);
	}
	else
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

void
MediaElement::NotifyTrackAdded(MediaTrack* track)
{
	if (EnsureFramesDocument())
		if (DOM_Environment* environment = m_frm_doc->GetDOMEnvironment())
			if (DOM_Object* domtracklist = m_track_list->GetDOMObject())
				environment->HandleTextTrackEvent(domtracklist, MEDIAADDTRACK, track);

	// Setup association between track and media element.
	track->AssociateWith(m_element);

	InvalidatePosition(track);
}

void
MediaElement::SelectTracks()
{
	if (!m_track_list)
		return;

	// Enable the first default subtitles or captions track, if any.
	MediaTrack* default_track = NULL;
	unsigned track_count = m_track_list->GetLength();
	for (unsigned i = 0; i < track_count; i++)
	{
		MediaTrack* track = m_track_list->GetTrackAt(i);

		// Do nothing if there are already enabled tracks.
		if (track->GetMode() != TRACK_MODE_DISABLED)
			return;

		if (!default_track)
		{
			MediaTrackKind track_kind(track->GetKindAsString());
			if (track_kind == MediaTrackKind::SUBTITLES ||
				track_kind == MediaTrackKind::CAPTIONS)
			{
				HTML_Element* track_element = track->GetHtmlElement();
				if (track_element && track_element->HasAttr(Markup::HA_DEFAULT))
					default_track = track;
			}
		}
	}

	if (default_track)
	{
		default_track->SetMode(TRACK_MODE_SHOWING_BY_DEFAULT);
		if (m_frm_doc)
			RAISE_IF_MEMORY_ERROR(default_track->GetTrackElement()->Load(m_frm_doc));
	}
}

void
MediaElement::HandleTrackAdded(HTML_Element* track_element, ES_Thread* thread)
{
	RAISE_AND_RETURN_VOID_IF_ERROR(EnsureTrackList());

	TrackElement* track_context = track_element->GetTrackElement();
	if (!track_context)
		return;

	RAISE_AND_RETURN_VOID_IF_ERROR(track_context->EnsureTrack(track_element));

	MediaTrack* track = track_context->GetTrack();

	// Add to list of tracks
	OP_STATUS status = m_track_list->AddTrack(track);
	if (OpStatus::IsError(status))
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return;
	}

	NotifyTrackAdded(track);

	// A track that finished loading before being added must be
	// activated here, since there's nothing else to trigger it.
	if (track_context->GetReadyState() == TRACK_STATE_LOADED)
		HandleTrackChange(track, TRACK_READY, thread);

	// Ensure we have track state (if tracks might be displayed).
	EnsureTrackState();
}

OP_STATUS
MediaElement::DOMAddTextTrack(MediaTrack* track, DOM_Object* domtrack)
{
	RETURN_IF_ERROR(EnsureTrackList());

	RETURN_IF_ERROR(m_track_list->AddTrack(track));
	track->SetDOMObject(domtrack);

	NotifyTrackAdded(track);

	// The track just added has mode 'hidden' and no cues. Hence there
	// are no cues that we may need to consider, so there's no need to
	// update track state or the display at this point. We may however
	// need the display state soon - and we don't create it
	// (on-demand) elsewhere, so create it now if it will be needed
	// and does not exist.

	// Ensure we have track state (if tracks might be displayed).
	EnsureTrackState();

	return OpStatus::OK;
}

void
MediaElement::HandleTrackRemoved(HTML_Element* track_element)
{
	if (!m_track_list)
		return;

	if (MediaTrack* track = m_track_list->GetTrackByElement(track_element))
	{
		HandleTrackChange(track, TRACK_CLEARED);

		InvalidatePosition(track);

		m_track_list->RemoveTrack(track);

		if (EnsureFramesDocument())
			if (DOM_Environment* environment = m_frm_doc->GetDOMEnvironment())
				if (DOM_Object* domtracklist = m_track_list->GetDOMObject())
					environment->HandleTextTrackEvent(domtracklist, MEDIAREMOVETRACK, track);

		track->ResetAssociation();
	}
}

void
MediaElement::HandleTrackChange(MediaTrack* track, TrackChangeReason reason,
								ES_Thread* thread /* = NULL */)
{
	OP_ASSERT(track);
	OP_ASSERT(track->GetMediaElement() == this);

	switch (reason)
	{
	case TRACK_CLEARED:
		if (!track->HasActiveCues())
			break;

		if (m_track_state && EnsureFramesDocument())
			m_track_state->RemoveCuesForTrack(track, m_frm_doc);
		break;

	case TRACK_VISIBILITY:
		InvalidatePosition(track, false /* only invalidate line position */);

		if (!m_track_state || !EnsureFramesDocument())
			break;

		if (!track->IsVisual())
			break;

		if (track->IsShowing())
			RAISE_IF_MEMORY_ERROR(m_track_state->AddCuesForTrack(track, m_frm_doc,
																 m_element));
		else
			m_track_state->RemoveCuesForTrack(track, m_frm_doc);

		// Reschedule
		CancelTrackUpdate();
		AdvanceTracks(thread);
		break;

	case TRACK_READY:
		SynchronizeTrack(track, thread);
		break;
	}
}

void
MediaElement::HandleCueChange(MediaTrackCue* cue, CueChangeReason reason,
							  ES_Thread* thread)
{
	OP_ASSERT(cue);
	OP_ASSERT(cue->GetOwnerTrack() != NULL);
	OP_ASSERT(cue->GetMediaElement() == this);

	if (reason != CUE_ADDED && !cue->IsActive())
		return;

	if (!m_track_state)
		return;

	if ((reason == CUE_CONTENT_CHANGED || reason == CUE_LAYOUT_CHANGED ||
		 reason == CUE_REMOVED) && !EnsureFramesDocument())
		return;

	switch (reason)
	{
	case CUE_CONTENT_CHANGED:
		// Mark cue's display state as 'extra' dirty (if it is
		// active). This will cause the cue (and potentially other
		// cues in the active list) to be relayouted.
		m_track_state->MarkCueExtraDirty(m_frm_doc, cue);
		break;

	case CUE_TIME_CHANGED:
		// Mark the cue's timing information as having changed.
		SynchronizeTracks(thread);
		break;

	case CUE_LAYOUT_CHANGED:
		// Mark the cue's display state as dirty (if it is
		// active). This will cause the cue (and potentially other
		// cues in the active list) to be relayouted.
		m_track_state->MarkCueDirty(m_frm_doc, cue);
		break;

	case CUE_ADDED:
		// If the cue is in a track that's showing (and actually
		// showing - i.e. visual) then check if it currently active
		// and trigger changes.
		SynchronizeCue(cue, thread);
		break;

	case CUE_REMOVED:
		// Mark the cue's display state as dirty to trigger changes to
		// dependent cues, and then remove the cue from display state.
		m_track_state->RemoveCue(cue, m_frm_doc);
		break;
	}
}

void
MediaElement::SynchronizeTrackClock()
{
	// Resync real-time
	m_last_track_realtime = g_op_time_info->GetRuntimeMS() / 1000;
	m_last_track_time = GetPosition();
}

void
MediaElement::SynchronizeTracks(ES_Thread* thread /* = NULL */)
{
	CancelTrackUpdate();

	UpdateTracks(true, thread);
}

void
MediaElement::SynchronizeTrack(MediaTrack* track, ES_Thread* thread /* = NULL */)
{
	if (track->GetMode() == TRACK_MODE_DISABLED)
		return;

	CancelTrackUpdate();

	// We want to do treat the update of this track as a seeking operation.
	// This basically means that no cues will be considered as 'missed'.
	track->SetPendingSeek();

	UpdateTracks(false, thread);
}

void
MediaElement::SynchronizeCue(MediaTrackCue* cue, ES_Thread* thread /* = NULL */)
{
	OP_ASSERT(cue);
	OP_ASSERT(cue->GetOwnerTrack());
	OP_ASSERT(!cue->IsActive());

	MediaTrack* track = cue->GetOwnerTrack();
	if (track->GetMode() == TRACK_MODE_DISABLED || !m_have_played)
		return;

	CancelTrackUpdate();

	// Put the cue in the 'pending' list. The cue will then be made
	// active on the next track update.
	track->AddPendingCue(cue);

	UpdateTracks(false, thread);
}

void
MediaElement::AdvanceTracks(ES_Thread* thread /* = NULL */)
{
	if (m_paused)
		return;

	UpdateTracks(false, thread);
}

TrackUpdateState::Event*
TrackUpdateState::NewEvent()
{
	if (m_event_count == m_size)
	{
		unsigned new_size = m_size * 2;
		Event* new_events = OP_NEWA(Event, new_size);
		if (!new_events)
			return NULL;

		op_memcpy(new_events, m_events, sizeof(*m_events) * m_size);

		FreeEvents();

		m_events = new_events;
		m_size = new_size;
	}

	Event* ev = m_events + m_event_count;
	m_event_count++;

	return ev;
}

OP_STATUS
TrackUpdateState::EmitEvent(MediaTrackCue* cue, bool is_start, bool is_missed)
{
	Event* ev = NewEvent();
	if (!ev)
		return OpStatus::ERR_NO_MEMORY;

	// If this is an end event check if it has the pauseOnExit flag
	// set, and update local state appropriately.
	if (!is_start && !m_seen_pause_on_exit)
		m_seen_pause_on_exit = cue->GetPauseOnExit();

	*ev = Event(cue, is_start, is_missed);
	return OpStatus::OK;
}

/* static */ int
TrackUpdateState::OrderEvents(const void* a, const void* b)
{
	const Event* ae = static_cast<const Event*>(a);
	const Event* be = static_cast<const Event*>(b);
	double atime = ae->GetTime();
	double btime = be->GetTime();
	int gt = atime > btime;
	int lt = atime < btime;
	// -1 if (a.time < b.time); 1 if (a.time > b.time); otherwise 0
	int cmp = gt - lt;
	if (cmp)
		return cmp;

	MediaTrackCue* a_cue = ae->GetCue();
	MediaTrackCue* b_cue = be->GetCue();

	// The spec lists this as the last resort - but only the same cue
	// ought to be able to have the same track cue order, so check
	// this first.
	if (a_cue == b_cue)
	{
		// Order 'enter' events before 'exit' events.
		int a_exit = !ae->IsStart();
		int b_exit = !be->IsStart();
		return a_exit - b_exit;
	}

	return MediaTrackCue::InterTrackOrder(a_cue, b_cue);
}

void
TrackUpdateState::SortEvents()
{
	if (m_event_count > 1)
		op_qsort(m_events, m_event_count, sizeof(*m_events), OrderEvents);
}

void
TrackUpdateState::DispatchCueEvents(DOM_Environment* environment)
{
	if (!environment ||
		!(environment->HasEventHandlers(MEDIACUEENTER) ||
		  environment->HasEventHandlers(MEDIACUEEXIT)))
		// No one is listening.
		return;

	for (Event* ev = m_events; ev != m_events + m_event_count; ev++)
		if (DOM_Object* domcue = ev->GetCue()->GetDOMObject())
			environment->HandleTextTrackEvent(domcue, ev->GetEventType());
}

void
TrackUpdateState::UpdateActive(TrackDisplayState* track_state, FramesDocument* frm_doc,
							   HTML_Element* track_root)
{
	if (track_state)
		track_root = TrackDisplayState::GetActualTrackRoot(track_root);

	for (Event* ev = m_events; ev != m_events + m_event_count; ev++)
	{
		if (ev->IsMissed())
			continue;

		MediaTrackCue* cue = ev->GetCue();

		if (ev->IsStart())
		{
			// Newly started cues should not be active.
			OP_ASSERT(!cue->IsActive());

			if (track_state)
			{
				MediaTrack* track = cue->GetOwnerTrack();

				// Only cues from showing and visual tracks have display
				// state.
				if (track->IsShowing() && track->IsVisual())
					track_state->AddCue(cue, frm_doc, track_root);
			}

			// Mark as active.
			cue->Activate();
		}
		else
		{
			// If the cue wasn't active we can ignore it.
			if (!cue->IsActive())
				continue;

			if (track_state)
			{
				MediaTrack* track = cue->GetOwnerTrack();

				// Only cues from showing and visual tracks have display
				// state.
				if (track->IsShowing() && track->IsVisual())
					track_state->RemoveCue(cue, frm_doc);
			}

			// No longer active.
			cue->Deactivate();
		}
	}
}

void
MediaElement::UpdateTracks(bool seeking, ES_Thread* thread /* = NULL */)
{
	if (!m_track_list)
		return;

	if (!m_have_played)
		return;

	if (thread)
	{
		ES_ThreadListener::Remove();
		thread->GetRunningRootThread()->AddListener(this);

		m_track_sync_pending = TRUE;
		m_track_sync_seeking = m_track_sync_seeking || seeking;
		return;
	}

	OP_ASSERT(!m_track_sync_pending);

	SynchronizeTrackClock();

	TrackUpdateState tustate;

	unsigned track_count = m_track_list->GetLength();

	// Fetch cues that started within the current time window (other
	// -> current).
	//
	// If not seeking also add started cues that end within the
	// current time window to the ended list (other -> missing).
	//
	// Also retrieve active cues that have ended (current -> other).
	unsigned i;
	OP_STATUS status = OpStatus::OK;
	for (i = 0; i < track_count && OpStatus::IsSuccess(status); i++)
	{
		MediaTrack* track = m_track_list->GetTrackAt(i);

		if (track->GetMode() == TRACK_MODE_DISABLED)
			continue;

		status = track->UpdateActive(tustate, seeking, m_last_track_time);
	}

	if (OpStatus::IsMemoryError(status))
	{
		// An error occurred while generating the list of events. To
		// avoid stale state we wipe the track display state and mark
		// the element extra dirty.
		if (m_track_state && EnsureFramesDocument())
			for (i = 0; i < track_count; i++)
				m_track_state->RemoveCuesForTrack(m_track_list->GetTrackAt(i), m_frm_doc);

		g_memory_manager->RaiseCondition(status);
		return;
	}

	// Event list generation can only fail with OOM - verify that no
	// unexpected error occurred.
	OP_ASSERT(OpStatus::IsSuccess(status));

	// Sort the event list using time as the key - start time for
	// started and end time for ended.
	tustate.SortEvents();

	EnsureFramesDocument();

	if (tustate.HasCueEvents())
	{
		// Try to acquire a DOM environment in case we want to dispatch some events.
		DOM_Environment* dom_environment = m_frm_doc ? m_frm_doc->GetDOMEnvironment() : NULL;
		if (dom_environment && !dom_environment->IsEnabled())
			dom_environment = NULL;

		// Determine if the media should be paused.
		bool should_pause = tustate.HasPauseOnExit();

		// Pause if required.
		if (!seeking && should_pause)
			Pause();

		// Iterate the sorted list of events and dispatch 'enter' and
		// 'exit' events as appropriate.
		tustate.DispatchCueEvents(dom_environment);

		// Dispatch 'cuechange' events to affected tracks.
		if (dom_environment && dom_environment->HasEventHandlers(MEDIACUECHANGE))
		{
			for (i = 0; i < track_count; i++)
			{
				MediaTrack* track = m_track_list->GetTrackAt(i);

				if (track->GetMode() == TRACK_MODE_DISABLED)
					continue;

				if (!track->HasCueChanges())
					continue;

				if (DOM_Object* domtrack = track->GetDOMObject())
					if (dom_environment)
						dom_environment->HandleTextTrackEvent(domtrack, MEDIACUECHANGE);

				if (HTML_Element* track_element = track->GetHtmlElement())
					QueueDOMEvent(track_element, MEDIACUECHANGE);
			}
		}
	}

	// Update active set and track state.
	tustate.UpdateActive(m_track_state, m_frm_doc, m_element);

	if (m_track_state)
		// Update any intra-cue timing-state.
		m_track_state->UpdateIntraCueState(m_frm_doc, m_last_track_time);

	// Don't schedule an update if the media is paused or ended.
	if (m_paused || m_playback_ended)
		return;

	double next_event = op_nan(NULL);

	if (m_track_state)
		// Next change in visual appearance.
		next_event = m_track_state->NextEventTime();

	// Determine next (non-visual) update.
	for (i = 0; i < track_count; i++)
	{
		MediaTrack* track = m_track_list->GetTrackAt(i);

		if (track->GetMode() == TRACK_MODE_DISABLED)
			continue;

		next_event = MediaTrackCue::MinTimestamp(next_event, track->NextCueStartTime());

		// Skip visual tracks that are showing because those will have
		// been handled above.
		if (m_track_state && track->IsShowing() && track->IsVisual())
			continue;

		next_event = MediaTrackCue::MinTimestamp(next_event, track->NextCueEndTime());
	}

	ScheduleTrackUpdate(next_event);
}

void
MediaElement::ScheduleTrackUpdate(double next_update)
{
	if (!op_isfinite(next_update))
		return;

	OP_ASSERT(next_update > m_last_track_time);

	RAISE_AND_RETURN_VOID_IF_ERROR(SetCallbacks());

	double delta = next_update - m_last_track_time;
	unsigned long delta_ms = static_cast<unsigned long>(op_ceil(1000 * delta));

	if (g_main_message_handler->PostDelayedMessage(MSG_MEDIA_TRACKUPDATE,
												   reinterpret_cast<MH_PARAM_1>(this), 0,
												   delta_ms))
		m_track_update_pending = TRUE;
}

void
MediaElement::CancelTrackUpdate()
{
	if (!m_track_update_pending)
		return;

	if (m_has_set_callbacks)
		g_main_message_handler->RemoveDelayedMessage(MSG_MEDIA_TRACKUPDATE,
													 reinterpret_cast<MH_PARAM_1>(this), 0);

	m_track_update_pending = FALSE;
}

OP_STATUS
MediaElement::Play(ES_Thread* thread /* = TRUE */)
{
	// 1. If the media element's networkState attribute has the value
	// NETWORK_EMPTY, invoke the media element's resource selection
	// algorithm.
	if (m_state.GetNetwork() == MediaNetwork::EMPTY)
		SelectResourceInvoke(thread);

	// 2. If the playback has ended, seek to the earliest possible
	// position of the media resource. Note: This will cause the user
	// agent to queue a task to fire a simple event named timeupdate
	// at the media element.
	if (m_playback_ended)
		RETURN_IF_ERROR(SetPosition(0));

	// 3. If the media element's paused attribute is true, run the
	// following substeps:
	if (m_paused)
	{
		// 3.1 Change the value of paused to false.
		m_paused = FALSE;

		// 3.2 Queue a task to fire a simple event named play at the element.
		QueueDOMEvent(m_element, MEDIAPLAY);

		// 3.3 If the media element's readyState attribute has the
		// value HAVE_NOTHING, HAVE_METADATA, or HAVE_CURRENT_DATA,
		// queue a task to fire a simple event named waiting at the
		// element.
		if (m_state.GetReady() <= MediaReady::CURRENT_DATA)
		{
			QueueDOMEvent(m_element, MEDIAWAITING);
		}
		else
		{
			// 3.4 Otherwise, the media element's readyState attribute
			// has the value HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA;
			// queue a task to fire a simple event named playing at
			// the element.
			OP_ASSERT(m_state.GetReady() >= MediaReady::FUTURE_DATA);
			QueueDOMEvent(m_element, MEDIAPLAYING);
		}

		UPDATE_POSTER_STATE(m_have_played);
		SetPreloadState(MediaPreload::INVOKED);
		HandleTimeUpdate();

		if (m_player)
			m_player->Play();

		if (m_controls)
			m_controls->OnPlayPause();
	}

	if (!m_playback_ended)
		SynchronizeTracks(thread);

	// 4. Set the media element's autoplaying flag to false.
	m_flag_autoplaying = FALSE;

	return OpStatus::OK;
}

OP_STATUS
MediaElement::Pause(ES_Thread* thread /* = NULL */)
{
	// 1. If the media element's networkState attribute has the value
	// NETWORK_EMPTY, invoke the media element's resource selection
	// algorithm.
	if (m_state.GetNetwork() == MediaNetwork::EMPTY)
		SelectResourceInvoke(thread);

	// 2. Set the media element's autoplaying flag to false.
	m_flag_autoplaying = FALSE;

	// 3. If the media element's paused attribute is false, run the
	// following steps:
	if (!m_paused)
	{
		// 3.1 Change the value of paused to true.
		m_paused = TRUE;

		// 3.2 Queue a task to fire a simple event named timeupdate at
		// the element.
		EnsureMediaControls();
		HandleTimeUpdate();

		// 3.3 Queue a task to fire a simple event named pause at the
		// element.
		QueueDOMEvent(m_element, MEDIAPAUSE);

		if (m_player)
			m_player->Pause();

		if (m_controls)
			m_controls->OnPlayPause();
	}

	return OpStatus::OK;
}

void
MediaElement::HandleProgress()
{
	QueueProgressEvent(m_element);

	RAISE_AND_RETURN_VOID_IF_ERROR(SetCallbacks());
	g_main_message_handler->PostDelayedMessage(MSG_MEDIA_PROGRESS,
		reinterpret_cast<MH_PARAM_1>(this), 0, MEDIA_PROGRESS_INTERVAL);
}

void
MediaElement::HandleTimeUpdate(TimeUpdateType type)
{
	// See MediaTimeUpdateState.
	switch (type)
	{
	case TIMEUPDATE_DELAYED:
		if (m_timeupdate_state != MEDIA_TIMEUPDATE_SKIPPED)
			return;
		m_timeupdate_state = MEDIA_TIMEUPDATE_NORMAL;
	case TIMEUPDATE_SKIPPABLE:
		if (m_thread_queue.IsTimeUpdateQueued())
		{
			m_timeupdate_state = MEDIA_TIMEUPDATE_SKIPPED;
			break;
		}
	case TIMEUPDATE_UNCONDITIONAL:
		QueueDOMEvent(m_element, MEDIATIMEUPDATE);
		break;
	}

	// Invalidate controls if we haven't since the last time,
	// or if we're not playing.
	if (m_controls)
		m_controls->OnTimeUpdate(m_invalidate_controls || !IsPotentiallyPlaying());
	m_invalidate_controls = TRUE;

	MessageHandler* mh = g_main_message_handler;
	MH_PARAM_1 param1 = reinterpret_cast<MH_PARAM_1>(this);

	// Always clear pending events, otherwise we might end up with several
	// event handler "loops" in parallel.
	mh->RemoveDelayedMessage(MSG_MEDIA_TIMEUPDATE, param1, 0);

	if (IsPotentiallyPlaying())
	{
		// Send the next timeupdate event in PrefsCollectionDoc::MediaPlaybackTimeUpdateInterval ms.
		RAISE_AND_RETURN_VOID_IF_ERROR(SetCallbacks());
		int timeupdate_interval = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::MediaPlaybackTimeUpdateInterval);
		mh->PostDelayedMessage(MSG_MEDIA_TIMEUPDATE, param1, 0, timeupdate_interval);
	}
}

/* virtual */ void
MediaElement::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (m_suspended)
	{
		// Messages can currently not be handled when suspended. A mechanism
		// for handling this is needed proper suspend/resume (CORE-27035).
		return;
	}

	switch (msg)
	{
	case MSG_MEDIA_PROGRESS:
		OP_ASSERT(InFetchResource());
		if (m_progress_state == MEDIA_PROGRESS_DELAYED)
			HandleProgress();
		if (m_progress_state > MEDIA_PROGRESS_WAITING)
			m_progress_state--;
		break;
	case MSG_MEDIA_RESOURCE_SELECTION_RETURN:
		// destroy the player to stop the download
		DestroyPlayer();
		switch (m_select_state)
		{
		case SELECT_FETCH_SRC:
			m_select_state = SELECT_FETCH_SRC_FAIL;
			break;
		case SELECT_FETCH_SOURCE:
			m_select_state = SELECT_FETCH_SOURCE_FAIL;
			break;
		case SELECT_NULL:
			// See comment in SelectResourceReturn()
			return;
		default:
			OP_ASSERT(!"Resource selection algorithm returning from wrong state");
			return;
		}
		SelectResource(NULL);
		break;
	case MSG_MEDIA_TIMEUPDATE:
		HandleTimeUpdate(TIMEUPDATE_SKIPPABLE);
		break;
	case MSG_MEDIA_TRACKUPDATE:
		m_track_update_pending = FALSE;
		AdvanceTracks();
		break;
	default:
		OP_ASSERT(!"Unknown message");
	}
}

/* virtual */ void
MediaElement::OnTimeUpdateThreadsFinished()
{
	HandleTimeUpdate(TIMEUPDATE_DELAYED);
}

/* virtual */ const OpMediaTimeRanges*
MediaElement::GetMediaBufferedTimeRanges()
{
	const OpMediaTimeRanges* ranges = NULL;
	RAISE_IF_MEMORY_ERROR(GetBufferedTimeRanges(ranges));
	return ranges;
}

static const OpMessage media_msgs[] = {
	MSG_MEDIA_PROGRESS,
	MSG_MEDIA_RESOURCE_SELECTION_RETURN,
	MSG_MEDIA_TIMEUPDATE,
	MSG_MEDIA_TRACKUPDATE
};

OP_STATUS
MediaElement::SetCallbacks()
{
	if (!m_has_set_callbacks)
	{
		RETURN_IF_ERROR(g_main_message_handler->SetCallBackList(this, reinterpret_cast<MH_PARAM_1>(this), media_msgs, ARRAY_SIZE(media_msgs)));
		m_has_set_callbacks = TRUE;
	}
	return OpStatus::OK;
}

void
MediaElement::UnsetCallbacks()
{
	if (m_has_set_callbacks)
	{
		g_main_message_handler->UnsetCallBacks(this);
		for (size_t i = 0; i < ARRAY_SIZE(media_msgs); i++)
			g_main_message_handler->RemoveDelayedMessage(media_msgs[i], reinterpret_cast<MH_PARAM_1>(this), 0);
		m_has_set_callbacks = FALSE;
	}
}

/* virtual */ OP_STATUS
MediaElement::Signal(ES_Thread* thread, ES_ThreadSignal signal)
{
	// FIXME: if element is suspended, wait until it is resumed
	ES_ThreadListener::Remove();
	if (m_select_resource_pending)
	{
		if (signal == ES_SIGNAL_FINISHED || signal == ES_SIGNAL_FAILED)
			SelectResource(NULL);

		m_select_resource_pending = FALSE;
	}
	if (m_select_tracks_pending)
	{
		SelectTracks();

		m_select_tracks_pending = FALSE;
	}
	if (m_track_sync_pending)
	{
		bool seeking = m_track_sync_seeking;
		m_track_sync_pending = FALSE;
		m_track_sync_seeking = FALSE;

		UpdateTracks(seeking);
	}
	return OpStatus::OK;
}

/* virtual */ void
MediaElement::OnDurationChange(MediaPlayer* player)
{
	OP_ASSERT(player == m_player);

	QueueDOMEvent(m_element, MEDIADURATIONCHANGE);

	// Once enough of the media data has been fetched to determine the
	// duration of the media resource, its dimensions, and other
	// metadata
	if (m_state.GetReady() == MediaReady::NOTHING)
	{
		// 1. Set the current playback position to the earliest possible position.

		// 2. Set the readyState attribute to HAVE_METADATA.
		SetReadyState(MediaReady::METADATA);

		// 3. For video elements, set the videoWidth and videoHeight attributes.

		// 4. Set the duration attribute to the duration of the resource.

		// 5. Queue a task to fire a simple event named loadedmetadata at the element.
		QueueDOMEvent(m_element, MEDIALOADEDMETADATA);

		// 6. If either the media resource or the address of the
		// current media resource indicate a particular start time,
		// then seek to that time. Ignore any resulting exceptions (if
		// the position is out of range, it is effectively ignored).
		// FIXME: support media fragment URIs

		// 7. Once the readyState attribute reaches HAVE_CURRENT_DATA,
		// set the element's delaying-the-load-event flag to
		// false. This stops delaying the load event.
		SetDelayingLoad(FALSE);

		// Faking it until CORE-27944
		SetReadyState(MediaReady::ENOUGH_DATA);
	}

	// Invalidate controls and repaint
	if (m_controls)
		m_controls->OnDurationChange();
}

/* virtual */ void
MediaElement::OnVideoResize(MediaPlayer* player)
{
	OP_ASSERT(player == m_player);

	UINT32 video_width, video_height;
	m_player->GetVideoSize(video_width, video_height);
	if (m_video_width != video_width || m_video_height != video_height)
	{
		m_video_width = video_width;
		m_video_height = video_height;
		MarkDirty();
	}

#ifdef PI_VIDEO_LAYER
	// When using PI_VIDEO_LAYER we have no way of knowing when the video
	// is being rendered, this is an approximation.
	UPDATE_POSTER_STATE(m_have_frame);
#endif // PI_VIDEO_LAYER
}

/* virtual */ void
MediaElement::OnFrameUpdate(MediaPlayer* player)
{
	OP_ASSERT(player == m_player);

	// Do nothing for <audio>
	if (!IsImageType())
		return;

	UPDATE_POSTER_STATE(m_have_frame);

	Update();
}

/* virtual */ void
MediaElement::OnDecodeError(MediaPlayer* player)
{
	OP_ASSERT(player == m_player);
	OP_ASSERT(m_select_state == SELECT_NULL || InFetchResource());

	if (m_state.GetReady() == MediaReady::NOTHING)
	{
		// If the media data can be fetched but is in an unsupported
		// format, or can otherwise not be rendered at all...
		SelectResourceReturn();
	}
	else
	{
		// If the media data is corrupted...

		// 1. The user agent should cancel the fetching process.
		// FIXME

		// 2. Set the error attribute to a new MediaError object whose
		// code attribute is set to MEDIA_ERR_ABORT.
		m_error = MEDIA_ERR_DECODE;
		// 3. Queue a task to fire a simple event named error at
		// the media element.
		QueueDOMEvent(m_element, ONERROR);
		// 4. Set the element's networkState attribute to the
		// NETWORK_EMPTY value and queue a task to fire a simple event
		// named emptied at the element.
		SetNetworkState(MediaNetwork::EMPTY);
		QueueDOMEvent(m_element, MEDIAEMPTIED);
		// 5. Set the element's delaying-the-load-event flag to
		// false. This stops delaying the load event.
		SetDelayingLoad(FALSE);
		// 6. Abort the overall resource selection algorithm.
		m_select_state = SELECT_NULL;
	}
}

/* virtual */ void
MediaElement::OnPlaybackEnd(MediaPlayer* player)
{
	OP_ASSERT(player == m_player);
	if (m_element->HasAttr(Markup::HA_LOOP))
	{
		RAISE_IF_MEMORY_ERROR(SetPosition(0));
		HandleTimeUpdate();
	}
	else
	{
		SetPlaybackEnded(TRUE);
		CancelTrackUpdate();
		HandleTimeUpdate();
		AdvanceTracks();
		QueueDOMEvent(m_element, MEDIAENDED);

		// by definition cannot have future data here
		SetReadyState(MediaReady::CURRENT_DATA);

		if (m_controls)
			m_controls->OnPlayPause();
	}
}

BOOL
MediaElement::IsPotentiallyPlaying() const
{
	// A media element is said to be potentially playing when its
	// paused attribute is false, the readyState attribute is either
	// HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA, the element has not ended
	// playback, playback has not stopped due to errors, and the
	// element has not paused for user interaction.
	return !m_paused && m_state.GetReady() >= MediaReady::FUTURE_DATA && !m_playback_ended;
}

BOOL
MediaElement::GetDelayingLoad() const
{
	return m_flag_delaying_load;
}

void
MediaElement::SetDelayingLoad(BOOL delaying_load)
{
	if ((BOOL)m_flag_delaying_load != delaying_load)
	{
		m_flag_delaying_load = delaying_load;
		if (m_element->GetEndTagFound() && !delaying_load)
		{
			if (EnsureFramesDocument())
				RAISE_IF_MEMORY_ERROR(m_frm_doc->CheckFinishDocument());
		}
	}
}

MediaPreload
MediaElement::GetPreloadAttr() const
{
	return MediaPreload(m_element->GetStringAttr(Markup::HA_PRELOAD));
}

OP_STATUS
MediaElement::GetBufferedTimeRanges(const OpMediaTimeRanges*& ranges)
{
	if (!m_player)
		return OpStatus::ERR;
	return m_player->GetBufferedTimeRanges(ranges);
}

BOOL MediaElement::GetSeeking() const
{
	if (m_player)
		return m_player->GetSeeking();
	return FALSE;
}

double
MediaElement::GetPosition()
{
	double position = 0.0;
	if (m_player)
		OpStatus::Ignore(m_player->GetPosition(position));
	return position;
}

OP_STATUS
MediaElement::SetPosition(const double position)
{
	double new_position = position;

	// 1. If the media element's readyState is HAVE_NOTHING, then the user agent
	//    must raise an INVALID_STATE_ERR exception (if the seek was in response
	//    to a DOM method call or setting of an IDL attribute), and abort these
	//    steps.
	// Handled in DOM_HTMLMediaElement::PutName()

	// 2. If the new playback position is later than the end of the media
	//    resource, then let it be the end of the media resource instead.
	if (new_position > GetDuration())
		new_position = GetDuration();

	// 3. If the new playback position is less than the earliest possible
	//    position, let it be that position instead.
	if (new_position < 0)
		new_position = 0;

	// 4. If the (possibly now changed) new playback position is not in one of
	//    the ranges given in the seekable attribute, then the user agent must
	//    raise an INDEX_SIZE_ERR exception (if the seek was in response to a
	//    DOM method call or setting of an IDL attribute), and abort these steps.
	// Should not implement, see CORE-31695

	// 5. The current playback position must be set to the given new playback
	//    position.
	if (m_player)
	{
		OP_STATUS status = m_player->SetPosition(new_position);
		if (status == OpStatus::ERR_NOT_SUPPORTED || status == OpStatus::ERR_OUT_OF_RANGE)
			return OpStatus::ERR_OUT_OF_RANGE;
		// Other errors are ignored
	}

	// 6. The seeking IDL attribute must be set to true.
	// The seeking attribute is handled by the MediaPlayer.

	// Reset playback ended state; needs to be done before timeupdate.
	if (m_playback_ended)
	{
		// Faking it until CORE-27944
		SetReadyState(MediaReady::ENOUGH_DATA);
		SetPlaybackEnded(FALSE);

		if (m_controls)
			m_controls->OnPlayPause();
	}

	// Cancel any pending track updates here since we will do a full
	// resync when the seek completes.
	CancelTrackUpdate();

	// 7. The user agent must queue a task to fire a simple event named
	//    timeupdate at the element.
	HandleTimeUpdate();

	// 8. If the media element was potentially playing immediately before it
	//    started seeking, but seeking caused its readyState attribute to change
	//    to a value lower than HAVE_FUTURE_DATA, the user agent must queue a task
	//    to fire a simple event named waiting at the element.
	// Remove from spec?

	// 9. If, when it reaches this step, the user agent has still not established
	//    whether or not the media data for the new playback position is available,
	//    and, if it is, decoded enough data to play back that position, the user
	//    agent must queue a task to fire a simple event named seeking at the element.
	QueueDOMEvent(m_element, MEDIASEEKING);

	// 10. If the seek was in response to a DOM method call or setting of an IDL
	//     attribute, then continue the script. The remainder of these steps must
	//     be run asynchronously.

	SetPreloadState(MediaPreload::INVOKED);

	return OpStatus::OK;
}

/* virtual */ void
MediaElement::OnSeekComplete(MediaPlayer* player)
{
	// 11. The user agent must wait until it has established whether or not the
	//     media data for the new playback position is available, and, if it is,
	//     until it has decoded enough data to play back that position.

	// 12. The seeking IDL attribute must be set to false.
	// The seeking attribute is handled by the MediaPlayer.

	// 13. The user agent must queue a task to fire a simple event named seeked
	//     at the element.
	QueueDOMEvent(m_element, MEDIASEEKED);

	// Remove poster after seeking in a paused video.
	UPDATE_POSTER_STATE(m_have_played);

	// Reset the track state to new position.
	SynchronizeTracks();
}

double
MediaElement::GetDuration() const
{
	double duration = op_nan(NULL);
	if (m_player)
		OpStatus::Ignore(m_player->GetDuration(duration));
	return duration;
}

OP_STATUS
MediaElement::SetDefaultPlaybackRate(const double rate)
{
	if (m_default_playback_rate != rate)
	{
		m_default_playback_rate = rate;
		QueueDOMEvent(m_element, MEDIARATECHANGE);
	}
	return OpStatus::OK;
}

OP_STATUS
MediaElement::SetPlaybackRate(const double rate)
{
	if (m_playback_rate != rate)
	{
		if (m_player)
			RETURN_IF_ERROR(m_player->SetPlaybackRate(rate));
		m_playback_rate = rate;
		QueueDOMEvent(m_element, MEDIARATECHANGE);
	}
	return OpStatus::OK;
}

OP_STATUS
MediaElement::GetPlayedTimeRanges(const OpMediaTimeRanges*& ranges)
{
	if (!m_player)
		return OpStatus::ERR;
	return m_player->GetPlayedTimeRanges(ranges);
}

OP_STATUS
MediaElement::GetSeekableTimeRanges(const OpMediaTimeRanges*& ranges)
{
	if (!m_player)
		return OpStatus::ERR;
	return m_player->GetSeekableTimeRanges(ranges);
}

OP_STATUS
MediaElement::SetVolume(const double volume)
{
	if (m_volume != volume)
	{
		m_volume = volume;
		if (m_player && !m_muted)
			m_player->SetVolume(volume);
		if (m_controls)
			m_controls->OnVolumeChange();
		QueueDOMEvent(m_element, MEDIAVOLUMECHANGE);
	}
	return OpStatus::OK;
}

OP_STATUS
MediaElement::SetMuted(const BOOL muted)
{
	if ((BOOL)m_muted != muted)
	{
		m_muted = muted;
		if (m_player)
			m_player->SetVolume(m_muted ? 0 : m_volume);
		if (m_controls)
			m_controls->OnVolumeChange();
		QueueDOMEvent(m_element, MEDIAVOLUMECHANGE);
	}
	return OpStatus::OK;
}

void
MediaElement::OnMouseEvent(DOM_EventType event_type, MouseButton button, int x, int y)
{
	if (m_controls)
	{
		// Element wide events
		switch(event_type)
		{
		case ONMOUSEOVER:
			m_controls->OnElementMouseOver();
			break;
		case ONMOUSEOUT:
			m_controls->OnElementMouseOut();
			break;
		default:
			break;
		}

		// Controls events
		Box* layout_box = m_element->GetLayoutBox();
		if (!layout_box)
			return;

		// Translate mouse position to controls coordinate space.
		RECT rect;
		AffinePos rect_pos;
		if (!layout_box->GetRect(m_frm_doc, CONTENT_BOX, rect_pos, rect))
			return;
		OpRect content = OpRect(0, 0, rect.right - rect.left, rect.bottom - rect.top);
		OpRect controls = m_controls->GetControlArea(content);
		OpPoint wpoint(x - controls.x, y - controls.y);

		if (m_controls->GetBounds().Contains(wpoint))
		{
			switch (event_type)
			{
			case ONMOUSEDOWN:
				m_controls->GenerateOnMouseDown(wpoint, button, 1);
				break;

			//case ONMOUSEUP:
				//Not needed. Hooked widgets gets mouseup in HTML_Document::MouseAction.

			case ONMOUSEMOVE:
				// Hooked mousemove events is given to the widget from HTML_Document::MouseAction
				if (!OpWidget::hooked_widget)
					m_controls->GenerateOnMouseMove(wpoint);
				break;

			default:
				break;
			}
		}
	}
}

const URL*
MediaElement::GetCurrentSrc() const
{
	return &m_url;
}

MediaPlayer*
MediaElement::GetPlayer() const
{
	return m_player;
}

HTML_ElementType
MediaElement::GetElementType() const
{
	return m_element->Type();
}

BOOL
MediaElement::IsFocusable() const
{
	return m_controls != NULL;
}

BOOL
MediaElement::FocusNextInternalTabStop(BOOL forward)
{
	if (m_controls)
	{
		OpWidget* next = m_controls->GetNextInternalTabStop(forward);
		if (next)
		{
			next->SetFocus(FOCUS_REASON_KEYBOARD);
			return TRUE;
		}
	}

	return FALSE;
}

void
MediaElement::FocusInternalEdge(BOOL forward)
{
	if (m_controls)
	{
		OpWidget* widget = m_controls->GetFocusableInternalEdge(forward);
		if (widget)
			widget->SetFocus(FOCUS_REASON_KEYBOARD);
	}
}

void
MediaElement::HandleFocusGained()
{
	if (m_frm_doc && m_frm_doc->GetHtmlDocument())
		m_frm_doc->GetHtmlDocument()->SetFocusedElement(m_element);
}

OpRect
MediaElement::GetControlsArea()
{
	if (m_controls && m_controls->IsVisible())
		return m_controls->GetControlArea(OpRect(0, 0, m_content_width, m_content_height));

	return OpRect();
}

void
MediaElement::MarkDirty()
{
	if (EnsureFramesDocument())
	{
		// Look for the video display element and mark it dirty if we
		// find it. Otherwise just mark m_element.
		if (IsImageType())
			for (HTML_Element* child = m_element->FirstChild();
				 child; child = child->Suc())
				if (child->GetInserted() == HE_INSERTED_BY_LAYOUT &&
					child->GetNsType() == NS_HTML)
					switch (child->Type())
					{
					case Markup::MEDE_VIDEO_DISPLAY:
					case Markup::MEDE_VIDEO_CONTROLS:
						child->MarkDirty(m_frm_doc);

						if (child->Type() == Markup::MEDE_VIDEO_CONTROLS)
							return;
					}

		m_element->MarkDirty(m_frm_doc, TRUE, TRUE);
	}
}

void
MediaElement::SetControlsVisible(bool visible)
{
	if (!m_track_state)
		return;

	if (m_track_state->OnControlsVisibility(GetControlsArea()) &&
		EnsureFramesDocument())
		// The area covered by the controls was changed, so the cue
		// layout needs to be updated.
		m_track_state->InvalidateCueLayout(m_frm_doc);
}

BOOL
MediaElement::ControlsNeeded() const
{
#ifdef MEDIA_FORCE_VIDEO_CONTROLS
	if (IsImageType())
		return TRUE;
#endif // MEDIA_FORCE_VIDEO_CONTROLS
	return m_element->HasAttr(Markup::HA_CONTROLS) ||
		!(m_frm_doc && DOM_Environment::IsEnabled(m_frm_doc));
}

bool
MediaElement::EnsureMediaControls()
{
	if (ControlsNeeded())
	{
		if (!m_controls)
		{
			RAISE_IF_MEMORY_ERROR(MediaControls::Create(m_controls, this));
			if (m_controls && EnsureFramesDocument())
			{
				VisualDevice* visdev = m_frm_doc->GetVisualDevice();
				m_controls->SetVisualDevice(visdev);
				m_controls->SetParentInputContext(visdev);

				// Don't fade in the controls when just created.
				m_controls->ForceVisibility();
			}
		}
		else
			m_controls->EnsureVisibility();
	}
	else
	{
		OP_DELETE(m_controls);
		m_controls = NULL;

		SetControlsVisible(false);
	}

	return m_controls != NULL;
}

void
MediaElement::PosterFailed()
{
	UPDATE_POSTER_STATE(m_poster_failed);
}

BOOL
MediaElement::UseAutoplay() const
{
	return m_flag_autoplaying && m_paused &&
		m_element->HasAttr(Markup::HA_AUTOPLAY) &&
		g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AllowAutoplay);
}

#ifdef DOM_STREAM_API_SUPPORT
void
MediaElement::SetStreamResource(MediaStreamResource* stream_resource)
{
	if (IsStreamResource())
		ClearStreamResource(FALSE);

	// NB: right now the stream resource represents only the default
	// camera. This will be extended in followup tasks to give the user
	// a choice of various devices and consequently this function will
	// deal with more details about the input feed.
	m_stream_resource = stream_resource;
	m_stream_resource->IncReferenceCount();
}

void
MediaElement::ClearStreamResource(BOOL repaint /* = TRUE */)
{
	if (IsStreamResource())
	{
		m_stream_resource->DecReferenceCount();
		m_stream_resource = NULL;

		if (repaint)
			Update();
	}
}
#endif // DOM_STREAM_API_SUPPORT

#ifdef PI_VIDEO_LAYER
// Find the element's closest CoreView
static CoreView*
GetView(HTML_Element* elm, VisualDevice* visdev)
{
	if (CoreView* core_view = LayoutWorkplace::FindParentCoreView(elm))
		return core_view;

	return visdev->GetView();
}

void
MediaElement::UpdateVideoLayerRect(VisualDevice* visdev /* = NULL */)
{
	// Get VisualDevice from FramesDocument if necessary
	if (!visdev && !(EnsureFramesDocument() &&
					 (visdev = m_frm_doc->GetVisualDevice())))
		return;

	// Offset is not updated on the VisualDevices of child frames when
	// scrolling, so force the update here.
	visdev->UpdateOffset();

	// Convert rect from document to screen coordinates
	OpRect screen_rect = visdev->ScaleToScreen(m_doc_rect);
	screen_rect = visdev->OffsetToContainerAndScroll(screen_rect);
	OpPoint screen_origo(0, 0);
	screen_origo = visdev->GetOpView()->ConvertToScreen(screen_origo);
	screen_rect.OffsetBy(screen_origo);

	// Calculate clip rect
	OpRect clip_rect = GetView(m_element, visdev)->GetVisibleRect();
	clip_rect.OffsetBy(screen_origo);
	clip_rect.IntersectWith(screen_rect);

	// Notify player on the change in video window
	m_player->SetVideoLayerRect(screen_rect, clip_rect);
}

/* virtual */ void
MediaElement::OnScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason)
{
	if (m_use_video_layer)
		UpdateVideoLayerRect();
}

/* virtual */ void
MediaElement::OnParentScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason)
{
	if (m_use_video_layer)
		UpdateVideoLayerRect();
}
#endif // PI_VIDEO_LAYER

#endif //MEDIA_HTML_SUPPORT
