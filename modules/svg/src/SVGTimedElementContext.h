/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_TIMED_ELEMENT_CONTEXT_H
#define SVG_TIMED_ELEMENT_CONTEXT_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/animation/svganimationschedule.h"
#include "modules/svg/src/animation/svgtimingarguments.h"

struct SVGTimingArguments;

/**
 * A context for a timed element
 */
class SVGTimingInterface
{
public:
	SVGTimingInterface(HTML_Element *timed_element);
	virtual ~SVGTimingInterface();

	/**
	 * Copy timing parameters from element attributes to the
	 * SVGTimingArguments cache structure, for use by the animation
	 * engine.
	 */
	void SetTimingArguments(SVGTimingArguments& timing_arguments);

	HTML_Element* GetElement() const { return m_timed_element; }

	SVGAnimationSchedule &AnimationSchedule() { return m_schedule; }
	const SVGAnimationSchedule &AnimationSchedule() const { return m_schedule; }

	OP_STATUS OnPrepare();

	OP_STATUS OnIntervalBegin();
	OP_STATUS OnIntervalEnd();
	OP_STATUS OnIntervalRepeat();

	OP_STATUS OnTimelinePaused();
	OP_STATUS OnTimelineDelayed();
	OP_STATUS OnTimelineResumed();
	OP_STATUS OnTimelineRestart();

	BOOL IsPaused() { return SVGAnimationTime::IsNumeric(m_paused_at); }

	/**
	 * Get the element to animate
	 *
	 * @param doc_ctx The document where the animation is taking place.
	 *
	 * @return Returns the element the is to be animated,
	 * OpSVGStatus::INVALID_ANIMATION if no such element exists.
	 */
	HTML_Element *GetDefaultTargetElement(SVGDocumentContext* doc_ctx) const;

	SVGAnimationWorkplace *GetSubAnimationWorkplace();

#ifdef SVG_SUPPORT_MEDIA
	/**
	 * Get the calculated audio-level for this element
	 */
	OP_STATUS GetAudioLevel(float& audio_level);
#endif // SVG_SUPPORT_MEDIA

	void SetIntrinsicDuration(SVG_ANIMATION_TIME intrinsic_dur) { m_intrinsic_duration = intrinsic_dur; }
	SVG_ANIMATION_TIME GetIntrinsicDuration() { return m_intrinsic_duration; }

protected:
	/**
	 * Structure used to hold synchronization parameters for this
	 * element
	 */
	struct SyncParameters
	{
		SVG_ANIMATION_TIME tolerance;
		SVGSyncBehavior behavior;
		BOOL is_master;
	};

	void SetSyncParameters(SyncParameters& sync_params);

	SVGAnimationWorkplace* GetAnimationWorkplace();

	/* Hooks for element-specific pause/resume handling */
	OP_STATUS DoPause();
	OP_STATUS DoResume();

	/**
	 * This Element
	 */
	HTML_Element* m_timed_element;

	SVGDocumentContext *GetSubDocumentContext();

	/**
	 * Return the element that controls the time container this
	 * element is in, or NULL if this is the top-most document.
	 */
	HTML_Element* GetParentTimeContainer();

	/**
	 * Virtual casts
	 */
	virtual SVGTimingInterface* GetTimingInterface() { return this; }

private:
	SVGAnimationSchedule m_schedule;

	/**
	 * The "intrinsic duration" (as described in SMIL 2.1 section 7.3)
	 */
	SVG_ANIMATION_TIME m_intrinsic_duration;

	/**
	 * The point in (document) time when this element was paused
	 * ('unresolved' if never paused)
	 */
	SVG_ANIMATION_TIME m_paused_at;

	/**
	 * The 'accumulated synchronization offset'
	 */
	SVG_ANIMATION_TIME m_sync_offset;
};

class SVGInvisibleTimedElement : public SVGInvisibleElement, public SVGTimingInterface
{
public:
	SVGInvisibleTimedElement(HTML_Element* element) :
		SVGInvisibleElement(element), SVGTimingInterface(element) {}

	virtual class SVGTimingInterface* GetTimingInterface() { return static_cast<SVGTimingInterface*>(this); }
};

class SVGTimedExternalContentElement : public SVGExternalContentElement, public SVGTimingInterface
{
public:
	SVGTimedExternalContentElement(HTML_Element* element) :
		SVGExternalContentElement(element), SVGTimingInterface(element) {}

	virtual class SVGTimingInterface* GetTimingInterface() { return static_cast<SVGTimingInterface*>(this); }
};

#endif // SVG_SUPPORT
#endif // SVG_TIMED_ELEMENT_CONTEXT_H
