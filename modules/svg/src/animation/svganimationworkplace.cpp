/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationlistitem.h"
#include "modules/svg/src/animation/svganimationsandwich.h"
#include "modules/svg/src/animation/svganimationarguments.h"
#include "modules/svg/src/animation/svgtimingarguments.h"
#include "modules/svg/src/animation/svganimationtarget.h"
#include "modules/svg/src/animation/svganimationworkplace.h"
#include "modules/svg/src/animation/svgtimeline.h"

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGAnimationContext.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGTimedElementContext.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGDependencyGraph.h"

#include "modules/logdoc/htm_elm.h"

#include "modules/dochand/fdelm.h"

SVGAnimationWorkplace::SVGAnimationWorkplace(SVGDocumentContext *doc_ctx) :
	document_time(0),
	forced_document_time(SVGAnimationTime::Unresolved()),
	start_time(0),
	document_real_time(0),
	document_next_real_time(0),
	time_state(TIME_BROKEN),
	time_multiplier(1.0),
	accumulated_interval_time(0),
	packed_init(0),
	activity_svganim(ACTIVITY_SVGANIM),
	doc_ctx(doc_ctx)
{
	packed.animation_at_prezero = 1;
}

SVGAnimationWorkplace::~SVGAnimationWorkplace()
{
	animations.Clear();
	timed_elements_in_error.Clear();
}

BOOL
SVGAnimationWorkplace::HasAnimations()
{
	return known_elements.GetCount() > 0;
}

void
SVGAnimationWorkplace::MarkPropsDirty(HTML_Element *element)
{
	HTML_Element* stop = element->NextSiblingActual();
	while (element != stop)
	{
		SVGAnimationListItem *item;
		if (OpStatus::IsSuccess(known_elements.GetData(element, &item)))
		{
			item->MarkPropsDirty();
			RequestUpdate();
		}

		element = element->Next();
	}
}

OP_STATUS
SVGAnimationWorkplace::UpdateAnimations()
{
	if (!packed.animation_pending || !IsAnimationsAllowed())
		return OpBoolean::IS_FALSE;

	if (SVGAnimationTime::IsUnresolved(document_time))
		return OpStatus::ERR;

	RETURN_IF_ERROR(UpdateAnimations(COMPUTE_VALUES, MOVE_NORMAL));

	if (packed.intervals_dirty)
		UpdateIntervals(MOVE_NORMAL, 0);

	SetAnimationPending(FALSE);

	if (time_state == TIME_TRACKING)
	{
		SVG_ANIMATION_TIME next_animation_time = NextAnimationTime(document_time);
		if (SVGAnimationTime::IsNumeric(next_animation_time))
		{
			SetAnimationPending(TRUE);
			OP_ASSERT(next_animation_time >= document_time);
			unsigned next_frame_time = (unsigned)((next_animation_time - document_time) / time_multiplier);
			doc_ctx->GetSVGImage()->ScheduleAnimationFrameAt(next_frame_time);
		}
	}

	return OpStatus::OK;
}

OP_STATUS
SVGAnimationWorkplace::UpdateAnimations(UpdatePolicy update_policy,
										MovePolicy move_policy)
{
	SVGWorkplaceImpl* workplace = doc_ctx->GetSVGImage()->GetSVGWorkplace();
	if (!workplace)
		return OpStatus::ERR;

	OP_STATUS status = OpStatus::OK;
	if (time_state == TIME_TRACKING)
	{
		OP_ASSERT(SVGAnimationTime::IsNumeric(document_time));

		// Sample the clock
		SetNextRealTime();

		if (workplace->GetLastLag() >= SVG_MAX_ANIMATION_LAG)
		{
			/* We were gone for a while, a while long enough to be
			   treated as a 'suspend'. During 'suspend' we pretend
			   that the animation was paused. */

			DisconnectFromRealTime();
			ConnectToRealTime();
			RequestUpdate();
		}
		else
		{
			RETURN_IF_ERROR(MoveAnimation(move_policy, RemainingDocumentTime()));
			OP_ASSERT(!packed.intervals_dirty);
		}
	}
	// else, TIME_BROKEN, no need to update intervals

	if (update_policy == COMPUTE_VALUES)
	{
		status = ApplyAnimations();
	}

	return status;
}

OP_STATUS
SVGAnimationWorkplace::SetDocumentTime(SVG_ANIMATION_TIME new_document_time)
{
	accumulated_interval_time = 0;

	if (!packed.timeline_has_started)
	{
		start_time = new_document_time;
		return OpStatus::OK;
	}

	if (new_document_time == document_time && !packed.animation_at_prezero)
		return OpStatus::OK;

	SetAnimationPending(TRUE);

	if (new_document_time == 0)
	{
		document_time = 0;
		packed.animation_at_prezero = 1;
		RETURN_IF_ERROR(ResetIntervals());
		UpdateIntervals(MOVE_NORMAL, 0);
		packed.animation_at_prezero = 0;
	}
	else
	{
		BOOL was_paused = (time_state == TIME_BROKEN);

		if (!was_paused)
			DisconnectFromRealTime();

		if (document_time < new_document_time)
		{
			RETURN_IF_ERROR(MoveAnimation(MOVE_NORMAL, new_document_time - document_time));
		}
		else
		{
			document_time = 0;
			packed.animation_at_prezero = 1;
			RETURN_IF_ERROR(ResetIntervals());
			UpdateIntervals(MOVE_FAST_FORWARD, 0);
			packed.animation_at_prezero = 0;

			SVG_ANIMATION_TIME fast_forward_time = new_document_time - 1;
			RETURN_IF_ERROR(MoveAnimation(MOVE_FAST_FORWARD, fast_forward_time));
			RETURN_IF_ERROR(MoveAnimation(MOVE_NORMAL, 1));
		}

		if (!was_paused)
			ConnectToRealTime();
	}

	return OpStatus::OK;
}

OP_STATUS
SVGAnimationWorkplace::MoveAnimation(MovePolicy move_policy, SVG_ANIMATION_TIME animation_time)
{
	SVG_ANIMATION_TIME next_document_time = document_time + animation_time;

	if (next_document_time > SVGAnimationTime::Latest() || next_document_time < document_time)
	{
		/* Stepped too far into the future... */
		next_document_time = SVGAnimationTime::Latest();
	}

	if (packed.intervals_dirty)
		UpdateIntervals(move_policy, 0);

	while (next_document_time > document_time)
	{
		SVG_ANIMATION_TIME next_interval_time = NextIntervalTime();

		OP_ASSERT(next_interval_time > document_time);

		if (next_interval_time <= next_document_time)
		{
			SVG_ANIMATION_TIME delta_time = next_interval_time - document_time;

			MoveDocumentTime(delta_time);
			UpdateIntervals(move_policy, accumulated_interval_time + delta_time);
			accumulated_interval_time = 0;
		}
		else
		{
			SVG_ANIMATION_TIME delta_time = next_document_time - document_time;
			accumulated_interval_time += delta_time;
			MoveDocumentTime(delta_time);
		}

		if (packed.intervals_dirty)
			UpdateIntervals(move_policy, 0);
	}

	return OpStatus::OK;
}

SVG_ANIMATION_TIME
SVGAnimationWorkplace::NextIntervalTime() const
{
	SVG_ANIMATION_TIME next_interval_time = SVGAnimationTime::Indefinite();

	if (packed.timeline_has_started)
	{
		SVGAnimationListItem* item;
		for (item = animations.First();item; item = item->Suc())
			next_interval_time = MIN(item->NextIntervalTime(document_time, packed.animation_at_prezero), next_interval_time);
	}

	return next_interval_time;
}

void
SVGAnimationWorkplace::RequestUpdate()
{
	if (doc_ctx->IsExternalAnimation())
		return;

	SetAnimationPending(TRUE);
	doc_ctx->GetSVGImage()->ScheduleUpdate();
}

SVG_ANIMATION_TIME
SVGAnimationWorkplace::NextAnimationTime(SVG_ANIMATION_TIME document_time) const
{
	SVG_ANIMATION_TIME next_animation_time = SVGAnimationTime::Indefinite();

	if (packed.timeline_has_started)
	{
		SVGAnimationListItem* item;
		for (item = animations.First(); item && next_animation_time > document_time; item = item->Suc())
			next_animation_time = MIN(item->NextAnimationTime(document_time), next_animation_time);
	}

	return next_animation_time;
}

OP_STATUS
SVGAnimationWorkplace::HandleEvent(const SVGManager::EventData& data)
{
	if (!IsAnimationsAllowed())
		return OpStatus::OK;

	if (data.target != NULL)
	{
		SVGDocumentContext *doc_ctx = AttrValueStore::GetSVGDocumentContext(data.target);
		if (!doc_ctx)
			return OpStatus::ERR;

		// We ignore events sent to us when we have no document
		if (doc_ctx->GetDocument() == NULL)
			return OpStatus::OK;

		// We ignore events when the DOM environment handles them
		if (doc_ctx->GetDOMEnvironment())
		{
			return OpStatus::OK;
		}

		HTML_Element* element = data.target;
		while(element != NULL)
		{
			HTML_Element* layouted_elm = SVGUtils::GetElementToLayout(element);
			SVGElementContext* elemctx = AttrValueStore::GetSVGElementContext(layouted_elm);
			if (elemctx != NULL && elemctx->ListensToEvent(data.type))
			{
				elemctx->HandleEvent(layouted_elm, data);
			}
			element = element->Parent();
		}
	}
	return OpStatus::OK;
}

OP_STATUS SVGAnimationWorkplace::StartTimeline()
{
	OP_STATUS status = OpStatus::OK;
	if (packed.timeline_has_started == 0)
	{
		packed.timeline_has_started = 1;

		RETURN_IF_ERROR(RecoverTimedElementsInError());

		// External animations are controlled and started by a parent
		// document, not by the load event.
		if (!doc_ctx->IsExternalAnimation())
		{
#ifdef SVG_SUPPORT_ANIMATION_LISTENER
			g_svg_manager_impl->ConnectAnimationListeners(this);
#endif // SVG_SUPPORT_ANIMATION_LISTENER

			ConnectToRealTime();

			packed.timeline_has_started = 1;

			SetDocumentTime(start_time);
			start_time = 0;

			// We do the update through the SVG image to respect that an
			// asynchronous rendering may be in progress. If that is the case,
			// we are queued up to be updated as soon as the rendering is
			// complete.
			SetAnimationPending(TRUE);
			doc_ctx->GetSVGImage()->UpdateState(SVGImageImpl::UPDATE_ANIMATIONS);
		}
	}
	return status;
}

OP_STATUS
SVGAnimationWorkplace::ProcessAnimationCommand(AnimationCommand command)
{
	switch (command)
	{
	case ANIMATION_SUSPEND:
	case ANIMATION_PAUSE:
		packed.timeline_is_paused = 1;
		DisconnectFromRealTime();
		break;

	case ANIMATION_UNPAUSE:
		packed.timeline_is_paused = 0;
		ConnectToRealTime();
		RequestUpdate();
		break;

	case ANIMATION_STOP:
	{
		packed.timeline_is_paused = 1;
		DisconnectFromRealTime();
		RETURN_IF_ERROR(SetDocumentTime(0));
		RequestUpdate();
	}
	break;

	default:
		break;
	}

    return OpStatus::OK;
}

OP_STATUS
SVGAnimationWorkplace::HandleAccessKey(uni_char access_key)
{
	// Note: This makes a linear search over the timed elements in the document.
	// Possible to optimize either by keeping the list of accesskeys in svg, or by fixing the logdoc/doc side of
	// it to support multiple elements per accesskey.

	for (SVGAnimationListItem *item = animations.First(); item; item = item->Suc())
		item->HandleAccessKey(access_key, VirtualDocumentTime());

	MarkIntervalsDirty();
	RequestUpdate();

	return OpStatus::OK;
}

OP_STATUS
SVGAnimationWorkplace::HandleAccessKey(uni_char access_key, HTML_Element *focused_element)
{
	SVGTimingInterface* timed_elements_ctx = AttrValueStore::GetSVGTimingInterface(focused_element);
	if (timed_elements_ctx != NULL)
	{
		SVGAnimationSchedule &animation_schedule = timed_elements_ctx->AnimationSchedule();

		for (int i = 0; i < 2; i++)
		{
			Markup::AttrType attr = i ? Markup::SVGA_END : Markup::SVGA_BEGIN;
			SVGVector* list;
			AttrValueStore::GetVector(focused_element, attr, list);
			if (list)
			{
				UINT32 count = list->GetCount();
				for (UINT32 i=0;i<count;i++)
				{
					SVGTimeObject* time_val = static_cast<SVGTimeObject*>(list->Get(i));
					if (time_val->TimeType() == SVGTIME_ACCESSKEY)
					{
						if (access_key == uni_toupper(time_val->GetAccessKey()))
						{
							// Process it
							SVG_ANIMATION_TIME clk = VirtualDocumentTime() + time_val->GetOffset();
							RETURN_IF_ERROR(time_val->AddInstanceTime(clk));
						}
					}
				}
			}
		}

		animation_schedule.MarkInstanceListsAsDirty();
	}

	return OpStatus::OK;
}

BOOL
SVGAnimationWorkplace::IsValidCommand(AnimationCommand command)
{
	if (!IsAnimationsAllowed())
		return FALSE;

	switch(command)
	{
	case ANIMATION_UNPAUSE:
		return time_state == TIME_BROKEN;

	case ANIMATION_STOP:
	case ANIMATION_PAUSE:
		return time_state == TIME_TRACKING;
	}

	OP_ASSERT(!"Not reached");
	return FALSE;
}

void
SVGAnimationWorkplace::HandleRemovedSubtree(HTML_Element *subroot)
{
	HTML_Element* stop_it = subroot->NextSiblingActual();
	HTML_Element* it = subroot;
	while (it != stop_it)
	{
		SVGAnimationListItem *item;
		if (OpStatus::IsSuccess(known_elements.GetData(it, &item)))
		{
			known_elements.Remove(it, &item);

			BOOL remove_item = FALSE;
			item->RemoveElement(this, it, remove_item);

			if (remove_item)
			{
				item->Deactivate();
				item->Out();
				OP_DELETE(item);
			}

			TimedElementLink *iter = timed_elements_in_error.First();
			while (iter)
			{
				TimedElementLink *tmp = iter;
				iter = iter->Suc();

				if (tmp->GetElement() == it)
				{
					tmp->Out();
					OP_DELETE(tmp);
				}
			}
		}

		it = it->NextActual();
	}
}

void
SVGAnimationWorkplace::UpdateIntervals(MovePolicy move_policy, SVG_ANIMATION_TIME elapsed_time)
{
	packed.is_updating_intervals = 1;
	do
	{
		packed.intervals_dirty = 0;

		SVGAnimationListItem* item;
		for (item = animations.First(); item; item = static_cast<SVGAnimationListItem *>(item->Suc()))
			item->UpdateIntervals(this);

		for (item = animations.First(); item; item = static_cast<SVGAnimationListItem *>(item->Suc()))
			item->CommitIntervals(this, move_policy == MOVE_NORMAL, document_time, elapsed_time, packed.animation_at_prezero);

		elapsed_time = 0;
		packed.animation_at_prezero = 0;
	} while (packed.intervals_dirty);

	packed.is_updating_intervals = 0;
}

OP_STATUS
SVGAnimationWorkplace::ResetIntervals()
{
	SVGAnimationListItem* item;
	for (item = animations.First(); item; item = item->Suc())
		item->Reset(document_time);

	return OpStatus::OK;
}

OP_STATUS
SVGAnimationWorkplace::ApplyAnimations()
{
	OP_ASSERT(!packed.is_applying_anim);
	OP_ASSERT(!doc_ctx->GetRenderingState() || !doc_ctx->GetRenderingState()->IsActive());

	BOOL oom = FALSE;

	SVGAnimationListItem* item;
	for (item = animations.First(); item; item = item->Suc())
		if (OpStatus::IsMemoryError(item->UpdateValue(this, document_time)))
			oom = TRUE;

	return oom ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
}

#ifdef SVG_DOM
OP_STATUS
SVGAnimationWorkplace::BeginElement(HTML_Element *element, SVG_ANIMATION_TIME offset)
{
	SVGTimingInterface* timed_element_ctx = AttrValueStore::GetSVGTimingInterface(element);
	if (timed_element_ctx != NULL)
	{
		RETURN_IF_ERROR(timed_element_ctx->AnimationSchedule().BeginElement(this, offset));

		MarkIntervalsDirty();
		RequestUpdate();
	}

	return OpStatus::OK;
}

OP_STATUS
SVGAnimationWorkplace::EndElement(HTML_Element *element, SVG_ANIMATION_TIME offset)
{
	SVGTimingInterface* timed_element_ctx = AttrValueStore::GetSVGTimingInterface(element);
	if (timed_element_ctx != NULL)
	{
		RETURN_IF_ERROR(timed_element_ctx->AnimationSchedule().EndElement(this, offset));

		MarkIntervalsDirty();
		RequestUpdate();
	}

	return OpStatus::OK;
}
#endif

/* static */ OP_STATUS
SVGAnimationWorkplace::Prepare(SVGDocumentContext *doc_ctx, HTML_Element *root_element)
{
	if (doc_ctx->IsExternalResource())
		return OpStatus::OK;

	SVGWorkplaceImpl* workplace = doc_ctx->GetSVGImage()->GetSVGWorkplace();
	if (!workplace)
		return OpStatus::ERR;

	OP_BOOLEAN preparation_status = PrepareTimedElementsInSubtree(doc_ctx, root_element);
	RETURN_IF_ERROR(preparation_status);

	if (preparation_status == OpBoolean::IS_TRUE)
	{
		SVGAnimationWorkplace *animation_workplace = doc_ctx->AssertAnimationWorkplace();
		if (!animation_workplace)
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(animation_workplace->PrepareAnimationInSubtree(root_element));

		// StartTimeline calls SetDocumentTime(0) which triggers an UpdateIntervals call
		if (!animation_workplace->HasTimelineStarted() &&
			((doc_ctx->GetDocument()->IsLoaded() && workplace->CheckSVGLoadCounter() == 0) ||
			 AttrValueStore::GetEnumValue(doc_ctx->GetSVGRoot(), Markup::SVGA_TIMELINEBEGIN, SVGENUM_TIMELINEBEGIN, SVGTIMELINEBEGIN_ONLOAD) == SVGTIMELINEBEGIN_ONSTART))
		{
			RETURN_IF_ERROR(animation_workplace->StartTimeline());
		}

		if (animation_workplace->GetAnimationStatus() == SVGAnimationWorkplace::STATUS_RUNNING)
			animation_workplace->RequestUpdate();
	}

	if (SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace())
		if (animation_workplace->GetAnimationStatus() == SVGAnimationWorkplace::STATUS_RUNNING)
			return animation_workplace->RecoverTimedElementsInError();

	return OpStatus::OK;
}

/* static */ OP_STATUS
SVGAnimationWorkplace::PrepareTimedElement(HTML_Element* timed_element)
{
	OP_ASSERT(timed_element != NULL);
	OP_ASSERT(timed_element->GetNsType() == NS_SVG);
	OP_ASSERT(SVGUtils::IsTimedElement(timed_element) || timed_element->IsMatchingType(Markup::SVGE_DISCARD, NS_SVG));

	if (timed_element->GetSVGContext() != NULL)
		return OpStatus::OK;

	return SVGElementContext::Create(timed_element) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

static SVG_DOCUMENT_CLASS*
GetExternalDocument(SVG_DOCUMENT_CLASS* doc, HTML_Element* element)
{
	if (FramesDocElm *fdelm = FramesDocElm::GetFrmDocElmByHTML(element))
		if (FramesDocument *frm_doc = fdelm->GetCurrentDoc())
			if (frm_doc->IsLoaded())
				return frm_doc;

	return NULL;
}

/* static */ OP_BOOLEAN
SVGAnimationWorkplace::PrepareTimedElementsInSubtree(SVGDocumentContext *doc_ctx, HTML_Element *subroot)
{
	if (!subroot)
		return OpBoolean::IS_FALSE;

	SVG_DOCUMENT_CLASS *doc = doc_ctx->GetDocument();
	HTML_Element* stop = subroot->NextSiblingActual();
	HTML_Element* element = subroot;
	BOOL timed_element_found = FALSE;

	while (element != stop)
	{
		if ((SVGUtils::IsTimedElement(element) || element->IsMatchingType(Markup::SVGE_DISCARD, NS_SVG)) &&
			SVGUtils::ShouldProcessElement(element))
		{
			RETURN_IF_ERROR(PrepareTimedElement(element));
			timed_element_found = TRUE;
		}

		// We search for animation element in external trees also.
		if (SVGUtils::IsExternalProxyElement(element))
		{
			if (SVG_DOCUMENT_CLASS* ext_doc = GetExternalDocument(doc, element))
			{
				OP_BOOLEAN sub_status = PrepareTimedElementsInSubtree(doc_ctx, ext_doc->GetDocRoot());
				RETURN_IF_ERROR(sub_status);

				if (sub_status == OpBoolean::IS_TRUE)
					timed_element_found = TRUE;
			}
		}

		element = (HTML_Element*) element->Next();
	}

	return (timed_element_found) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_STATUS
SVGAnimationWorkplace::PrepareAnimationInSubtree(HTML_Element *subroot)
{
	if (!subroot)
		return OpStatus::OK;

	HTML_Element* stop = subroot->NextSiblingActual();
	HTML_Element* element = subroot;
	SVG_DOCUMENT_CLASS *doc = doc_ctx->GetDocument();

	OP_STATUS status = OpStatus::OK;
	while (element != stop && !OpStatus::IsMemoryError(status))
	{
		if (SVGUtils::ShouldProcessElement(element))
		{
			if (SVGUtils::IsTimedElement(element) || element->IsMatchingType(Markup::SVGE_DISCARD, NS_SVG))
				status = RegisterElement(element);
			else if (SVGUtils::IsExternalProxyElement(element))
			{
				// Search for animation elements in external trees.

				if (SVG_DOCUMENT_CLASS* ext_doc = GetExternalDocument(doc, element))
					status = PrepareAnimationInSubtree(ext_doc->GetDocRoot());
			}
		}

		element = element->Next();
	}
	return status;
}

OP_STATUS
SVGAnimationWorkplace::RegisterElement(HTML_Element *element)
{
	if (known_elements.Contains(element))
		return OpStatus::OK;

	if (SVGUtils::IsAnimationElement(element))
		return RegisterAnimation(element);
	else
		return RegisterTimeline(element);
}

#ifdef SVG_SUPPORT_MEDIA
void SVGAnimationWorkplace::SuspendResumeMedia(BOOL suspend)
{
	if (known_elements.GetCount() == 0)
		return;

	OpHashIterator* iter = known_elements.GetIterator();
	if (!iter)
		return;

	for (OP_STATUS status = iter->First();
		 OpStatus::IsSuccess(status); status = iter->Next())
	{
		HTML_Element* timed_elm =
			static_cast<HTML_Element*>(const_cast<void*>(iter->GetKey()));

		if (!timed_elm->IsMatchingType(Markup::SVGE_AUDIO, NS_SVG) &&
			!timed_elm->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG))
			continue;

		SVGTimingInterface* timed_elm_ctx = AttrValueStore::GetSVGTimingInterface(timed_elm);
		if (!timed_elm_ctx)
			continue;

		// Reusing the general pause/resume hooks, unsure if we need
		// special hooks for this case.
		if (suspend)
			timed_elm_ctx->OnTimelinePaused();
		else
			timed_elm_ctx->OnTimelineResumed();

#ifdef PI_VIDEO_LAYER
		if (suspend)
		{
			SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();
			SVGMediaBinding* mbind = mm->GetBinding(timed_elm);
			if (mbind)
				mbind->DisableOverlay();
		}
#endif // PI_VIDEO_LAYER
	}

	OP_DELETE(iter);
}
#endif // SVG_SUPPORT_MEDIA

void
SVGAnimationWorkplace::ConnectToRealTime()
{
	SVGWorkplaceImpl* workplace = doc_ctx->GetSVGImage()->GetSVGWorkplace();
	if (!workplace)
		return;

	if (!IsAnimationsAllowed())
		return;

	if (time_state == TIME_BROKEN && packed.timeline_is_paused == 0)
	{
		double onload_begin = -1;

		if (!packed.timeline_has_started && !doc_ctx->IsExternalAnimation())
			onload_begin = workplace->GetDocumentBegin();

		if (onload_begin >= 0)
			document_real_time = onload_begin;
		else
			document_real_time = g_op_time_info->GetRuntimeMS();

		document_next_real_time = document_real_time;
		time_state = TIME_TRACKING;

#ifdef SVG_SUPPORT_MEDIA
		// Resume media elements that were playing
		SuspendResumeMedia(FALSE /* resume */);
#endif // SVG_SUPPORT_MEDIA
	}
}

void
SVGAnimationWorkplace::DisconnectFromRealTime()
{
#ifdef SVG_SUPPORT_MEDIA
	// Suspend playing media elements
	SuspendResumeMedia(TRUE /* suspend */);
#endif // SVG_SUPPORT_MEDIA

	document_real_time = document_next_real_time = 0;
	time_state = TIME_BROKEN;
}

void
SVGAnimationWorkplace::SetNextRealTime()
{
	OP_ASSERT(time_state == TIME_TRACKING);
	document_next_real_time = g_op_time_info->GetRuntimeMS();

	OP_ASSERT(document_next_real_time >= document_real_time);
}

SVG_ANIMATION_TIME
SVGAnimationWorkplace::RemainingDocumentTime()
{
	OP_ASSERT(time_state == TIME_TRACKING);
	return (SVG_ANIMATION_TIME)((document_next_real_time - document_real_time) * time_multiplier);
}

void
SVGAnimationWorkplace::MoveDocumentTime(SVG_ANIMATION_TIME animation_time)
{
	OP_ASSERT(SVGAnimationTime::IsNumeric(document_time));

	document_time += animation_time;
	if (time_state == TIME_TRACKING)
	{
		document_real_time += (animation_time / time_multiplier);
		OP_ASSERT(document_real_time <= document_next_real_time);
	}
}

SVG_ANIMATION_TIME
SVGAnimationWorkplace::VirtualDocumentTime()
{
	if (!SVGAnimationTime::IsUnresolved(forced_document_time))
	{
		return forced_document_time;
	}

	if (time_state == TIME_TRACKING)
	{
		double current_real_time = g_op_time_info->GetRuntimeMS();
		SVG_ANIMATION_TIME delta_document_time = (SVG_ANIMATION_TIME)((current_real_time - document_real_time) * time_multiplier);

		delta_document_time = MAX(delta_document_time, 1);
		return DocumentTime() + delta_document_time;
	}
	else
	{
		return DocumentTime();
	}
}

static OP_STATUS
EnsureInstance(SVGDocumentContext *doc_ctx, HTML_Element *use_elm, HTML_Element **shadow_root)
{
	if (!SVGUtils::HasBuiltNormalShadowTree(use_elm))
		RETURN_IF_ERROR(SVGUtils::BuildShadowTreeForUseTag(NULL, use_elm, SVGUtils::GetElementToLayout(use_elm), doc_ctx));
	HTML_Element *shadow = use_elm->FirstChild();
	while (shadow && !shadow->IsMatchingType(Markup::SVGE_ANIMATED_SHADOWROOT, NS_SVG))
		shadow = shadow->Suc();
	*shadow_root = shadow;
	return OpStatus::OK;
}

static OP_STATUS
SearchInstance(SVGDocumentContext *doc_ctx, HTML_Element *shadow_root, HTML_Element *element, FramesDocument *doc, DOM_EventType event_type, unsigned detail)
{
	if (!shadow_root)
		return OpStatus::OK;

	OP_ASSERT(shadow_root->IsMatchingType(Markup::SVGE_ANIMATED_SHADOWROOT, NS_SVG));

	HTML_Element *stop = shadow_root->Suc();
	HTML_Element *iter = shadow_root;
	for (; iter != stop && SVGUtils::IsShadowNode(iter); iter = iter->Next())
	{
		HTML_Element *real_elm = SVGUtils::GetElementToLayout(iter);
		if (real_elm->IsMatchingType(Markup::SVGE_USE, NS_SVG))
		{
			HTML_Element *shadow;
			RETURN_IF_ERROR(EnsureInstance(doc_ctx, iter, &shadow));
			RETURN_IF_ERROR(SearchInstance(doc_ctx, shadow, element, doc, event_type, detail));
		}
		else
			if (real_elm == element)
				RETURN_IF_ERROR(doc->HandleEvent(event_type, NULL, iter, SHIFTKEY_NONE, detail));
	}

	return OpStatus::OK;
}

OP_STATUS
SVGAnimationWorkplace::SendEventToShadow(DOM_EventType event_type, unsigned detail, HTML_Element *element, SVG_DOCUMENT_CLASS *doc, SVG_DOCUMENT_CLASS *elm_doc)
{
	FramesDocElm* fdelm = elm_doc->GetDocManager()->GetFrame();
	HTML_Element* frame_elm = fdelm->GetHtmlElement();

	SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph();
	if (!dgraph)
		return OpStatus::ERR_NO_MEMORY;
	NodeSet* edgeset = NULL;
	OpStatus::Ignore(dgraph->GetDependencies(frame_elm, &edgeset));
	if (edgeset && !edgeset->IsEmpty())
	{
		NodeSetIterator ei = edgeset->GetIterator();

		for (OP_STATUS status = ei.First();
			 OpStatus::IsSuccess(status);
			 status = ei.Next())
		{
			HTML_Element* depelm = ei.GetEdge();
			if (!depelm || !depelm->IsMatchingType(Markup::SVGE_USE, NS_SVG))
				continue;

			HTML_Element *shadow = NULL;
			RETURN_IF_ERROR(EnsureInstance(doc_ctx, depelm, &shadow));
			RETURN_IF_ERROR(SearchInstance(doc_ctx, shadow, element, doc, event_type, detail));
		}
	}
	return OpStatus::OK;
}

OP_STATUS
SVGAnimationWorkplace::SendEvent(DOM_EventType event_type, unsigned detail, HTML_Element *element)
{
	SVG_DOCUMENT_CLASS *doc = doc_ctx->GetDocument();
	SVGDocumentContext *doc_ctx_for_elm = AttrValueStore::GetSVGDocumentContext(element);
	SVG_DOCUMENT_CLASS *elm_doc = doc_ctx_for_elm->GetDocument();
	if (doc != elm_doc)
		return SendEventToShadow(event_type, detail, element, doc, elm_doc);
	return doc->HandleEvent(event_type, NULL, element, SHIFTKEY_NONE, detail);
}

OP_STATUS
SVGAnimationWorkplace::SendEvent(DOM_EventType event_type, HTML_Element *element)
{
	return SendEvent(event_type, 0, element);
}

SVGAnimationWorkplace::AnimationStatus
SVGAnimationWorkplace::GetAnimationStatus()
{
	if (time_state == TIME_TRACKING)
	{
		OP_ASSERT(packed.timeline_is_paused == 0);
		return STATUS_RUNNING;
	}
	else /* if (time_state == TIME_BROKEN) */
	{
		OP_ASSERT(time_state == TIME_BROKEN);
		return STATUS_PAUSED;
	}
}

OP_STATUS
SVGAnimationWorkplace::Navigate(HTML_Element *animation_element)
{
	if (!IsAnimationsAllowed())
		return OpStatus::OK;

	SVGAnimationInterface* anim_ctx = AttrValueStore::GetSVGAnimationInterface(animation_element);
	if (anim_ctx == NULL)
	{
		// Ignore navigations to non-animation elements
		return OpStatus::OK;
	}

	SVGAnimationSchedule &animation_schedule = anim_ctx->AnimationSchedule();
	SVG_ANIMATION_TIME activation_time = animation_schedule.GetActivationTime();

	if (SVGAnimationTime::IsNumeric(activation_time))
	{
		RETURN_IF_ERROR(SetDocumentTime(activation_time));
	}
	else
	{
		activation_time = VirtualDocumentTime();
		animation_schedule.SetActivationTime(activation_time);
		animation_schedule.MarkInstanceListsAsDirty();

		MarkIntervalsDirty();
	}

	RequestUpdate();

	return OpStatus::OK;
}

/* static */ BOOL
SVGAnimationWorkplace::IsAnimationsAllowed()
{
	return doc_ctx->GetSVGImage()->IsAnimationAllowed();
}

void
SVGAnimationWorkplace::ForceTime(SVG_ANIMATION_TIME animation_time)
{
	forced_document_time = animation_time;
}

void
SVGAnimationWorkplace::UnforceTime()
{
	forced_document_time = SVGAnimationTime::Unresolved();
}

void
SVGAnimationWorkplace::AddElementInTimeValueError(HTML_Element *timed_element)
{
	AddElementInError(timed_element, TimedElementLink::UNRESOLVED_TIMEVALUE);
}

void
SVGAnimationWorkplace::AddElementInError(HTML_Element *timed_element, TimedElementLink::ErrorCode code)
{
	TimedElementLink *iter = timed_elements_in_error.First();

	while (iter)
	{
		if (timed_element == iter->GetElement())
		{
			if (iter->GetCode() < code)
				iter->SetCode(code);

			return;
		}

		iter = iter->Suc();
	}

	TimedElementLink *timed_element_in_error = OP_NEW(TimedElementLink, (code, timed_element));

	if (!timed_element_in_error)
		return;

	timed_element_in_error->IntoStart(&timed_elements_in_error);
}

OP_STATUS
SVGAnimationWorkplace::RecoverTimedElementsInError()
{
	TimedElementLink *iter = timed_elements_in_error.First();

	while (iter)
	{
		TimedElementLink *tmp = iter;
		iter = iter->Suc();

		tmp->Out();
		TimedElementLink::ErrorCode code = tmp->GetCode();
		HTML_Element *timed_element = tmp->GetElement();
		OP_DELETE(tmp);

		SVGAnimationListItem *item;

		/* Yes, there is potential quadratic behavior here.  The code
		   have been optimized for simplicity instead of speed.  The
		   assumption is that the number of elements in error will be
		   relatively low on a real page. */
		if (code == TimedElementLink::UNRESOLVED_HREF)
		{
			OP_ASSERT(SVGUtils::IsAnimationElement(timed_element));
			RETURN_IF_ERROR(InvalidateTimingElement(timed_element));
		}
		else if (OpStatus::IsSuccess(known_elements.GetData(timed_element, &item)))
		{
			OP_ASSERT(code == TimedElementLink::UNRESOLVED_TIMEVALUE);
			item->RecoverFromError(this, timed_element);
		}
	}

	return OpStatus::OK;
}

void
SVGAnimationWorkplace::InsertAnimationItem(SVGAnimationListItem *item)
{
	HTML_Element *a = item->TargetElement();
	SVGAnimationListItem *iter = animations.Last();

	while (iter)
	{
		HTML_Element *b = iter->TargetElement();
		if (b->Precedes(a))
			break;
		iter = iter->Pred();
	}

	if (iter)
		item->Follow(iter);
	else
		item->IntoStart(&animations);
}

OP_STATUS
SVGAnimationWorkplace::RegisterAnimation(HTML_Element* element)
{
	OP_STATUS status = OpStatus::OK;
	SVGAnimationTarget* animation_target;
	SVGAnimationSlice* slice = OP_NEW(SVGAnimationSlice, ());
	if (!slice)
		return OpStatus::ERR_NO_MEMORY;

	slice->animation = AttrValueStore::GetSVGAnimationInterface(element);
	if (!slice->animation)
	{
		/* Animation context not create for some unknown reason.
		 * Ignore this animation element. */

		OP_DELETE(slice);
		return OpStatus::OK;
	}

	slice->animation->SetTimingArguments(slice->timing_arguments);

	if (OpStatus::IsSuccess(status = slice->animation->SetAnimationArguments(doc_ctx,
																			 slice->animation_arguments,
																			 animation_target)))
	{
		SVGAnimationAttributeLocation attribute_location = slice->animation_arguments.attribute;

		SVGAnimationSandwich* new_sandwich = NULL;
		SVGAnimationListItem* item;
		for (item = animations.First();
			 item && !item->MatchesTarget(animation_target, attribute_location);
			 item = item->Suc())
		{
			/* Do nothing */
		}

		if (!item)
		{
			item = new_sandwich = OP_NEW(SVGAnimationSandwich, (animation_target, attribute_location));
			if (!new_sandwich)
			{
				OP_DELETE(slice);
				return OpStatus::ERR_NO_MEMORY;
			}
			InsertAnimationItem(item);
		}

		if (OpStatus::IsSuccess(status = known_elements.Add(element, item)))
		{
			item->InsertSlice(slice, document_time);
			item->Prepare(this);
			MarkIntervalsDirty();
		}
		else
		{
			OP_DELETE(slice);

			if (new_sandwich)
				new_sandwich->Out();
			OP_DELETE(new_sandwich);

			return status;
		}

		return OpStatus::OK;
	}
	else
	{
		/* Fallback to a simple timeline without any actual animation */
		OP_DELETE(slice);
		AddElementInError(element, TimedElementLink::UNRESOLVED_HREF);
		return RegisterTimeline(element);
	}
}

OP_STATUS
SVGAnimationWorkplace::RegisterTimeline(HTML_Element* element)
{
	SVGTimingInterface *timing_if = AttrValueStore::GetSVGTimingInterface(element);
	OP_ASSERT(timing_if);

	OP_STATUS status;
	SVGTimeline* timeline = OP_NEW(SVGTimeline, (timing_if));
	if (!timeline)
		return OpStatus::ERR_NO_MEMORY;

	timing_if->SetTimingArguments(timeline->timing_arguments);

	if (OpStatus::IsSuccess(status = known_elements.Add(element, timeline)))
	{
		timeline->Into(&animations);
		MarkIntervalsDirty();

		if (OpStatus::IsSuccess(status = timeline->Prepare(this)))
			return OpStatus::OK;
		else
		{
			SVGAnimationListItem *dummy;
			known_elements.Remove(element, &dummy);

			timeline->Out();
			OP_DELETE(timeline);
			return status;
		}
	}
	else
	{
		OP_DELETE(timeline);
		return status;
	}
}

OP_STATUS
SVGAnimationWorkplace::InvalidateTimingElement(HTML_Element* element)
{
	OP_ASSERT(!packed.is_updating_intervals);

	SVGAnimationListItem *item;

	if (OpStatus::IsSuccess(known_elements.GetData(element, &item)))
	{
		BOOL remove_item = FALSE;
		known_elements.Remove(element, &item);
		item->RemoveElement(this, element, remove_item);

		if (remove_item)
		{
			item->Deactivate();
			item->Out();
			OP_DELETE(item);
		}
	}

	RETURN_IF_ERROR(RegisterElement(element));

	RequestUpdate();

	return OpStatus::OK;
}

void
SVGAnimationWorkplace::UpdateTimingParameters(HTML_Element* element)
{
	SVGAnimationListItem *item;
	if (OpStatus::IsSuccess(known_elements.GetData(element, &item)))
		item->UpdateTimingParameters(element);

	MarkIntervalsDirty();

	if (!packed.is_updating_intervals)
		RequestUpdate();
}

SVGAnimationListItem*
SVGAnimationWorkplace::LookupItem(HTML_Element* element) const
{
	SVGAnimationListItem *item;
	if (OpStatus::IsSuccess(known_elements.GetData(element, &item)))
		return item;
	return NULL;
}

#ifdef SVG_SUPPORT_ANIMATION_LISTENER
void
SVGAnimationWorkplace::RegisterAnimationListener(AnimationListener *listener)
{
	listener->Into(&animation_listeners);
}

void
SVGAnimationWorkplace::NotifyIntervalCreated(const SVGTimingInterface *timing_interface,
											 const SVGTimingArguments *timing_arguments,
											 const SVGAnimationInterval *animation_interval)
{
	AnimationListener::NotificationData data;
	data.notification_type = AnimationListener::NotificationData::INTERVAL_CREATED;
	data.timing_arguments = timing_arguments;
	data.animation_arguments = NULL;
	data.timing_if = timing_interface;
	data.extra.interval_created.animation_interval = animation_interval;
	data.extra.interval_created.animation_schedule = &timing_interface->AnimationSchedule();
	Notify(data);
}

void
SVGAnimationWorkplace::NotifyAnimationValue(const SVGTimingInterface *timing_interface,
											const SVGAnimationArguments &animation_arguments,
											const SVGTimingArguments &timing_arguments,
											SVG_ANIMATION_INTERVAL_POSITION interval_position,
											const SVGAnimationValue &animation_value)
{
	AnimationListener::NotificationData data;
	data.notification_type = AnimationListener::NotificationData::ANIMATION_VALUE;
	data.timing_if = timing_interface;
	data.timing_arguments = &timing_arguments;
	data.animation_arguments = &animation_arguments;
	data.extra.animation_value.interval_position = interval_position;
	data.extra.animation_value.animation_value = &animation_value;
	Notify(data);
}

void
SVGAnimationWorkplace::Notify(const AnimationListener::NotificationData &data)
{
	AnimationListener *listener = static_cast<AnimationListener *>(animation_listeners.First());
	while (listener != NULL)
	{
		listener->Notification(data);
		listener = static_cast<AnimationListener *>(listener->Suc());
	}
}
#endif // SVG_SUPPORT_ANIMATION_LISTENER

#endif // SVG_SUPPORT
