/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGTimedElementContext.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGTimeValue.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"

#include "modules/svg/src/animation/svgtimingarguments.h"

#ifdef SVG_SUPPORT_EXTERNAL_USE
# include "modules/dochand/fdelm.h"
#endif // SVG_SUPPORT_EXTERNAL_USE

#include "modules/layout/cascade.h"

SVGTimingInterface::SVGTimingInterface(HTML_Element *timed_element) :
	m_timed_element(timed_element),
	m_intrinsic_duration(SVGAnimationTime::Indefinite()),
	m_paused_at(SVGAnimationTime::Unresolved()),
	m_sync_offset(0)
{
}

SVGTimingInterface::~SVGTimingInterface()
{
#ifdef SVG_SUPPORT_MEDIA
	if (m_timed_element->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG) ||
		m_timed_element->IsMatchingType(Markup::SVGE_AUDIO, NS_SVG))
		if (g_svg_manager_impl)
			g_svg_manager_impl->GetMediaManager()->RemoveBinding(m_timed_element);
#endif // SVG_SUPPORT_MEDIA
}

/* If xlink:href isn't given then the target is the parent of this
 * element
 */
HTML_Element *
SVGTimingInterface::GetDefaultTargetElement(SVGDocumentContext* doc_ctx) const
{
	// Only supported for animation elements, other element has no default
	if (SVGUtils::IsAnimationElement(m_timed_element))
	{
		HTML_Element *target = NULL;
		if (AttrValueStore::HasObject(m_timed_element, Markup::XLINKA_HREF, NS_IDX_XLINK))
			target = SVGUtils::FindHrefReferredNode(NULL, doc_ctx, m_timed_element);
		else
			target = (HTML_Element*) m_timed_element->Parent();
		return target;
	}
	else
	{
		OP_ASSERT(SVGUtils::IsTimedElement(m_timed_element) ||
				  m_timed_element->Type() == Markup::SVGE_DISCARD);
		return m_timed_element;
	}
}

void
SVGTimingInterface::SetTimingArguments(SVGTimingArguments& timing_arguments)
{
	BOOL is_discard = m_timed_element->Type() == Markup::SVGE_DISCARD;

	SVGVector *begin_list;
	OP_STATUS status = AttrValueStore::GetVectorWithStatus(m_timed_element, Markup::SVGA_BEGIN, begin_list);
	if (OpStatus::IsSuccess(status))
	{
		if (!begin_list || begin_list->GetCount() == 0)
			timing_arguments.packed.begin_is_empty = 1;
	}
	else
	{
		begin_list = NULL;

		if(is_discard)
		{
			timing_arguments.packed.begin_is_empty = 1;
		}
	}

	timing_arguments.SetBeginList(begin_list);

	if (is_discard)
	{
		timing_arguments.SetEndList(NULL);

		timing_arguments.simple_duration = SVGAnimationTime::Indefinite();

		timing_arguments.repeat_duration = SVGAnimationTime::Unresolved();

		timing_arguments.active_duration_min = 0;
		timing_arguments.active_duration_max = SVGAnimationTime::Indefinite();

		timing_arguments.restart_type = SVGRESTART_NEVER;
	}
	else
	{
		SVGVector* end_list = NULL;
		AttrValueStore::GetVector(m_timed_element, Markup::SVGA_END, end_list);

		timing_arguments.SetEndList(end_list);

		SVG_ANIMATION_TIME simple_duration;
		OpStatus::Ignore(AttrValueStore::GetAnimationTime(m_timed_element, Markup::SVGA_DUR,
														  simple_duration,
														  m_intrinsic_duration));

		timing_arguments.simple_duration = simple_duration != 0 ? simple_duration :
			SVGAnimationTime::Indefinite(); // "Value must be greater than 0."

		SVG_ANIMATION_TIME repeat_duration;
		OpStatus::Ignore(AttrValueStore::GetAnimationTime(m_timed_element, Markup::SVGA_REPEATDUR,
														  repeat_duration,
														  SVGAnimationTime::Unresolved()));
		timing_arguments.repeat_duration = repeat_duration;

		AttrValueStore::GetAnimationTime(m_timed_element, Markup::SVGA_MIN,
										 timing_arguments.active_duration_min,
										 0);

		AttrValueStore::GetAnimationTime(m_timed_element, Markup::SVGA_MAX,
										 timing_arguments.active_duration_max,
										 SVGAnimationTime::Indefinite());

		if (timing_arguments.active_duration_min >
			timing_arguments.active_duration_max)
		{
			timing_arguments.active_duration_min = 0;
			timing_arguments.active_duration_max = SVGAnimationTime::Indefinite();
		}

		OpStatus::Ignore(AttrValueStore::GetRepeatCount(m_timed_element, timing_arguments.repeat_count));

 		timing_arguments.restart_type =
			(SVGRestartType)AttrValueStore::GetEnumValue(m_timed_element, Markup::SVGA_RESTART,
														 SVGENUM_RESTART, SVGRESTART_ALWAYS);
	}
}

#ifdef SVG_SUPPORT_MEDIA
static OP_STATUS AssureBinding(HTML_Element* timed_element)
{
	SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();

	if (!mm->GetBinding(timed_element))
	{
		// Register url with stream/media manager
		SVGDocumentContext* doc_ctx =
			AttrValueStore::GetSVGDocumentContext(timed_element);

		if(!doc_ctx)
			return OpStatus::ERR;

		URL *imageURL = NULL;
		RETURN_IF_MEMORY_ERROR(AttrValueStore::GetXLinkHREF(doc_ctx->GetURL(),
															timed_element, imageURL));

		if (!imageURL || imageURL->IsEmpty())
		{
			return OpStatus::OK;
		}

		Window* window = NULL;
		if(FramesDocument* doc = doc_ctx->GetDocument())
			window = doc->GetWindow();

		return mm->AddBinding(timed_element, *imageURL, window);
	}
	return OpStatus::OK;
}

OP_STATUS SVGTimingInterface::GetAudioLevel(float& audio_level)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_timed_element);

	audio_level = 1;

	// still set the default audio-level
	if(!doc_ctx)
		return OpStatus::ERR;

	AutoDeleteHead prop_list;
	HLDocProfile* hld_profile = doc_ctx->GetHLDocProfile();
	if (!hld_profile)
	{
		OP_ASSERT(0);
		return OpStatus::ERR;
	}
	LayoutProperties* layprops =
		LayoutProperties::CreateCascade(m_timed_element, prop_list,
										LAYOUT_WORKPLACE(hld_profile));
	if (!layprops)
		return OpStatus::ERR_NO_MEMORY;

	const HTMLayoutProperties& props = *layprops->GetProps();
	const SvgProperties *svg_props = props.svg;
	audio_level = svg_props->computed_audiolevel.GetFloatValue();

	layprops = NULL;

	return OpStatus::OK;
}
#endif // SVG_SUPPORT_MEDIA

OP_STATUS
SVGTimingInterface::OnPrepare()
{
	if (m_timed_element->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG))
	{
		if (SVGDocumentContext* sub_doc_ctx = GetSubDocumentContext())
			SVGAnimationWorkplace::Prepare(sub_doc_ctx, sub_doc_ctx->GetSVGRoot());
	}

	// FIXME: Something similar for Markup::SVGE_ANIMATION
#ifdef SVG_SUPPORT_MEDIA
	if (m_timed_element->GetNsType() == NS_SVG &&
		(m_timed_element->Type() == Markup::SVGE_AUDIO ||
		 m_timed_element->Type() == Markup::SVGE_VIDEO))
	{
		RETURN_IF_ERROR(AssureBinding(m_timed_element));

		SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();
		SVGMediaBinding* mbind = mm->GetBinding(m_timed_element);
		if (mbind)
		{
			float audio_level;
			RETURN_IF_MEMORY_ERROR(GetAudioLevel(audio_level));
			mbind->GetPlayer()->SetVolume(audio_level);

			if (m_timed_element->Type() == Markup::SVGE_VIDEO)
			{
				SVGInitialVisibility init_visibility =
					(SVGInitialVisibility)AttrValueStore::GetEnumValue(m_timed_element,
																	   Markup::SVGA_INITIALVISIBILITY,
																	   SVGENUM_INITIALVISIBILITY,
																	   SVGINITIALVISIBILITY_WHENSTARTED);
				if (init_visibility == SVGINITIALVISIBILITY_ALWAYS)
				{
					return mbind->GetPlayer()->Pause();
				}
			}
		}
	}
#endif // SVG_SUPPORT_MEDIA
	return OpStatus::OK;
}

OP_STATUS
SVGTimingInterface::OnIntervalBegin()
{
	if (m_timed_element->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG))
	{
		SVGAnimationWorkplace *sub_animation_workplace = GetSubAnimationWorkplace();
		if (sub_animation_workplace)
		{
			sub_animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_STOP);
			sub_animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_UNPAUSE);
		}
	}
	else if (m_timed_element->IsMatchingType(Markup::SVGE_DISCARD, NS_SVG))
	{
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_timed_element);
		if(!doc_ctx)
			return OpStatus::ERR;

		HTML_Element *target = NULL;
		if (AttrValueStore::HasObject(m_timed_element, Markup::XLINKA_HREF, NS_IDX_XLINK))
		{
			// SVGT12: "If the target element is not part of the current SVG document fragment, then the discard element is ignored."
			target = SVGUtils::FindHrefReferredNode(NULL, doc_ctx, m_timed_element);

			if(!doc_ctx->GetSVGRoot()->IsAncestorOf(target))
			{
				target = NULL;
			}
		}
		else
			target = (HTML_Element*) m_timed_element->Parent();

		if (target)
			SVGDynamicChangeHandler::HandleElementDiscard(doc_ctx, target);

		if (!target || !target->IsAncestorOf(m_timed_element))
			SVGDynamicChangeHandler::HandleElementDiscard(doc_ctx, m_timed_element);

		if (!target)
			return OpSVGStatus::INVALID_ANIMATION;
	}
#ifdef SVG_SUPPORT_MEDIA
	else if (m_timed_element->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG) ||
			 m_timed_element->IsMatchingType(Markup::SVGE_AUDIO, NS_SVG))
	{
		RETURN_IF_ERROR(AssureBinding(m_timed_element));

		// FIXME: Potentially rewind/restart the stream
		SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();
		SVGMediaBinding* mbind = mm->GetBinding(m_timed_element);
		if (mbind)
		{
			return mbind->GetPlayer()->Play();
		}
	}
#endif // SVG_SUPPORT_MEDIA
	return OpStatus::OK;
}

OP_STATUS
SVGTimingInterface::OnIntervalEnd()
{
	if (m_timed_element->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG))
	{
		SVGAnimationWorkplace *sub_animation_workplace = GetSubAnimationWorkplace();
		if (sub_animation_workplace)
			sub_animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_PAUSE);
	}
#ifdef SVG_SUPPORT_MEDIA
	else if (m_timed_element->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG) ||
			 m_timed_element->IsMatchingType(Markup::SVGE_AUDIO, NS_SVG))
	{
		SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();

		// Stop playback
		SVGMediaBinding* mbind = mm->GetBinding(m_timed_element);
		if (mbind)
		{
			mbind->GetPlayer()->Pause();
			mbind->GetPlayer()->SetPosition(0);
		}
	}
#endif // SVG_SUPPORT_MEDIA
	return OpStatus::OK;
}

OP_STATUS
SVGTimingInterface::OnIntervalRepeat()
{
	if (m_timed_element->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG))
	{
		SVGAnimationWorkplace *sub_animation_workplace = GetSubAnimationWorkplace();
		if (sub_animation_workplace)
		{
			sub_animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_STOP);
			sub_animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_UNPAUSE);
		}
	}
#ifdef SVG_SUPPORT_MEDIA
	else if (m_timed_element->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG) ||
			 m_timed_element->IsMatchingType(Markup::SVGE_AUDIO, NS_SVG))
	{
		SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();

		// Stop playback
		SVGMediaBinding* mbind = mm->GetBinding(m_timed_element);
		if (mbind)
		{
			mbind->GetPlayer()->SetPosition(0);
		}
	}
#endif // SVG_SUPPORT_MEDIA
	return OpStatus::OK;
}

void SVGTimingInterface::SetSyncParameters(SyncParameters& sync_params)
{
	sync_params.is_master = FALSE; // FIXME: Need to determine this

	sync_params.behavior = (SVGSyncBehavior)AttrValueStore::GetEnumValue(m_timed_element,
																		 Markup::SVGA_SYNCBEHAVIOR,
																		 SVGENUM_SYNCBEHAVIOR,
																		 SVGSYNCBEHAVIOR_INHERIT);
	if (sync_params.behavior == SVGSYNCBEHAVIOR_INHERIT)
	{
		// Attempt to resolve from surrounding time-container
		if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_timed_element))
		{
			sync_params.behavior = doc_ctx->GetDefaultSyncBehavior();
		}
	}

	OpStatus::Ignore(AttrValueStore::GetAnimationTime(m_timed_element, Markup::SVGA_SYNCTOLERANCE,
													  sync_params.tolerance,
													  SVGAnimationTime::Indefinite()));
	if (sync_params.tolerance == SVGAnimationTime::Indefinite())
	{
		// Attempt to resolve from surrounding time-container
		if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_timed_element))
		{
			sync_params.tolerance = doc_ctx->GetDefaultSyncTolerance();
		}
	}
}

SVGAnimationWorkplace* SVGTimingInterface::GetAnimationWorkplace()
{
	if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_timed_element))
		if (SVGAnimationWorkplace* anim_workplace = doc_ctx->GetAnimationWorkplace())
			return anim_workplace;

	return NULL;
}

OP_STATUS SVGTimingInterface::DoPause()
{
	if (m_timed_element->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG))
	{
		SVGAnimationWorkplace *sub_animation_workplace = GetSubAnimationWorkplace();
		if (sub_animation_workplace)
			sub_animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_PAUSE);
	}
#ifdef SVG_SUPPORT_MEDIA
	else if (m_timed_element->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG) ||
			 m_timed_element->IsMatchingType(Markup::SVGE_AUDIO, NS_SVG))
	{
		SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();

		// Pause playback
		SVGMediaBinding* mbind = mm->GetBinding(m_timed_element);
		if (mbind)
			mbind->GetPlayer()->Pause();
	}
#endif // SVG_SUPPORT_MEDIA
	return OpStatus::OK;
}

OP_STATUS SVGTimingInterface::DoResume()
{
	if (m_timed_element->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG))
	{
		SVGAnimationWorkplace *sub_animation_workplace = GetSubAnimationWorkplace();
		if (sub_animation_workplace)
			sub_animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_UNPAUSE);
	}
#ifdef SVG_SUPPORT_MEDIA
	else if (m_timed_element->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG) ||
			 m_timed_element->IsMatchingType(Markup::SVGE_AUDIO, NS_SVG))
	{
		SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();

		// Resume playback
		SVGMediaBinding* mbind = mm->GetBinding(m_timed_element);
		if (mbind)
			mbind->GetPlayer()->Play();
	}
#endif // SVG_SUPPORT_MEDIA
	return OpStatus::OK;
}

HTML_Element* SVGTimingInterface::GetParentTimeContainer()
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_timed_element);
	if (!doc_ctx)
		return NULL;

	return doc_ctx->GetReferencingElement();
}

OP_STATUS SVGTimingInterface::OnTimelinePaused()
{
	// FIXME: Fix 'pause' w/ semantics for non-media elements
	//  | Mark intervals dirty and signal that the element should not
	//  | have further values calculated (until resume happens) [flag
	//  | in schedule, elsewhere?].
	//
	// FIXME: Should this _also_ introduce a delay in the timeline (modulo synch. semantics)?
	//  | Synch. semantics should be considered for media-elements,
	//  | I.e. potentially pause time container.
	return DoPause();
}

OP_STATUS SVGTimingInterface::OnTimelineDelayed()
{
	if (!(m_timed_element->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG)
#ifdef SVG_SUPPORT_MEDIA
		  || m_timed_element->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG)
		  || m_timed_element->IsMatchingType(Markup::SVGE_AUDIO, NS_SVG)
#endif // SVG_SUPPORT_MEDIA
		  ))
		return OpStatus::OK;

	// Already paused (delayed)?
	if (SVGAnimationTime::IsNumeric(m_paused_at))
		return OpStatus::OK;

	SyncParameters params;
	SetSyncParameters(params);

	// Condensed version of SMIL 2.1, section 10.4.1, "The
	// syncBehavior, syncTolerance, and syncMaster attributes:
	// controlling runtime synchronization"
	//
	// The use of the term 'container' below roughly(?) translates to
	// 'containing svg fragment'.
	//
	// * syncTolerance is ignored unless behavior is 'locked'
	//
	// * 'locked' should keep in sync with the parent container
	//
	// * 'independent' is equivalent to syncMaster='true' and
	//   syncBehavior='canSlip' - i.e. independent of the parent
	//   container but determine time for the current container
	//

	SVGAnimationWorkplace* anim_workplace = GetAnimationWorkplace();
	if (!anim_workplace)
		return OpStatus::ERR;

	if (params.behavior == SVGSYNCBEHAVIOR_CANSLIP ||
		params.behavior == SVGSYNCBEHAVIOR_INDEPENDENT)
	{
		// NOTE: is_master should implicitly be set when behavior='independent'
		if (params.is_master)
		{
			// Pause the time container
			RETURN_IF_ERROR(anim_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_PAUSE));
		}
		else
		{
			// Let it slip
		}
	}
	else /* SVGSYNCBEHAVIOR_LOCKED */
	{
		if (params.is_master)
		{
			// I am the master of time! (At least within the ancestor
			// containers that can't slip)
			RETURN_IF_ERROR(anim_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_PAUSE));

			// Propagate the delaying
			if (HTML_Element* parent_time_container = GetParentTimeContainer())
			{
				SVGTimingInterface* parent_tec =
					AttrValueStore::GetSVGTimingInterface(parent_time_container);

				if (parent_tec /* && !parent_tec->CanSlip()*/)
					parent_tec->OnTimelineDelayed();
			}
		}
		else
		{
			// Pause
			m_paused_at = anim_workplace->DocumentTime();
		}
	}
	return OpStatus::OK;
}

OP_STATUS SVGTimingInterface::OnTimelineResumed()
{
	// NOTE: This assumes that m_paused_at is only set to a numeric
	// value if the timeline is delayed
	if (SVGAnimationTime::IsNumeric(m_paused_at))
	{
		SyncParameters params;
		SetSyncParameters(params);

		if (SVGAnimationTime::IsNumeric(params.tolerance))
		{
			SVGAnimationWorkplace* anim_workplace = GetAnimationWorkplace();
			if (!anim_workplace)
				return OpStatus::ERR;

			SVG_ANIMATION_TIME elapsed = anim_workplace->DocumentTime() - m_paused_at;

			if (params.behavior == SVGSYNCBEHAVIOR_LOCKED ||
				m_sync_offset + elapsed >= params.tolerance)
			{
				// Advance by elapsed
			}
			else
			{
				// Within tolerance, ignore
				m_sync_offset += elapsed;
			}

			m_paused_at = SVGAnimationTime::Unresolved();
		}
	}
	else
	{
		return DoResume();
	}
	return OpStatus::OK;
}

OP_STATUS SVGTimingInterface::OnTimelineRestart()
{
#ifdef SVG_TINY_12
	// Don't expect calls to this for elements of other types
	OP_ASSERT(m_timed_element->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG) ||
			  m_timed_element->IsMatchingType(Markup::SVGE_AUDIO, NS_SVG) ||
			  m_timed_element->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG));

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_timed_element);
	if (!doc_ctx)
		return OpStatus::ERR;

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();
	if (!animation_workplace)
		return OpStatus::ERR;

	if (AnimationSchedule().IsActive(animation_workplace->DocumentTime()))
	{
#ifdef SVG_SUPPORT_MEDIA
		if (m_timed_element->IsMatchingType(Markup::SVGE_VIDEO, NS_SVG) ||
			m_timed_element->IsMatchingType(Markup::SVGE_AUDIO, NS_SVG))
		{
			// Remove the current binding (if any)
			g_svg_manager_impl->GetMediaManager()->RemoveBinding(m_timed_element);
		}
#endif // SVG_SUPPORT_MEDIA

		// Active
		RETURN_IF_ERROR(OnIntervalEnd());
		RETURN_IF_ERROR(OnIntervalBegin());

		// Potentially adjust media based on interval timing (but that
		// is more about spec. interpretation than anything else)
	}
	else
	{
		// Inactive
		RETURN_IF_ERROR(OnPrepare());
	}
#endif // SVG_TINY_12
	return OpStatus::OK;
}

SVGDocumentContext *
SVGTimingInterface::GetSubDocumentContext()
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_timed_element);
	FramesDocElm* frame = NULL;

	if(doc_ctx)
	{
		SVG_DOCUMENT_CLASS* document = doc_ctx->GetDocument();
		if(document)
			frame = FramesDocElm::GetFrmDocElmByHTML(m_timed_element);
	}

	if(!frame || !frame->IsLoaded())
		return NULL;

	HTML_Element *sub_root = NULL;

	FramesDocument* frm_doc = frame->GetCurrentDoc();
	if(frm_doc && frm_doc->IsLoaded())
	{
		HTML_Element *target = frm_doc->GetDocRoot();
		if(target)
		{
			LogicalDocument* logdoc = frm_doc->GetLogicalDocument();
			if(logdoc)
			{
				target = logdoc->GetDocRoot();
				if(target && target->GetNsType() == NS_SVG)
					sub_root = target;
			}
		}
	}

	return AttrValueStore::GetSVGDocumentContext(sub_root);
}

SVGAnimationWorkplace *
SVGTimingInterface::GetSubAnimationWorkplace()
{
	SVGDocumentContext *sub_doc_ctx = GetSubDocumentContext();
	return (sub_doc_ctx) ? sub_doc_ctx->GetAnimationWorkplace() : NULL;
}

#endif // SVG_SUPPORT
