/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_EXTERNAL_TIMELINE_H
#define SVG_EXTERNAL_TIMELINE_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svgtimingarguments.h"
#include "modules/svg/src/animation/svganimationlistitem.h"

class SVGTimeline : public SVGAnimationListItem
{
public:
    SVGTimeline(SVGTimingInterface *timing_if) : timing_if(timing_if), has_dirty_props(TRUE) {}

    virtual BOOL MatchesTarget(SVGAnimationTarget *target, SVGAnimationAttributeLocation attribute_location) { return FALSE; }

    virtual void InsertSlice(SVGAnimationSlice* animation_slice,
							 SVG_ANIMATION_TIME document_time) {}

    virtual void Reset(SVG_ANIMATION_TIME document_time);

    virtual OP_STATUS UpdateIntervals(SVGAnimationWorkplace *animation_workplace);

    virtual OP_STATUS CommitIntervals(SVGAnimationWorkplace *animation_workplace,
									  BOOL send_events,
									  SVG_ANIMATION_TIME document_time,
									  SVG_ANIMATION_TIME elapsed_time,
									  BOOL at_prezero);

    virtual OP_STATUS UpdateValue(SVGAnimationWorkplace* animation_workplace, SVG_ANIMATION_TIME document_time);

    virtual SVG_ANIMATION_TIME NextAnimationTime(SVG_ANIMATION_TIME document_time) const;

    virtual SVG_ANIMATION_TIME NextIntervalTime(SVG_ANIMATION_TIME document_time, BOOL at_prezero) const;

	virtual OP_STATUS HandleAccessKey(uni_char access_key, SVG_ANIMATION_TIME document_time);

	virtual OP_STATUS Prepare(SVGAnimationWorkplace* animation_workplace);

	virtual void RemoveElement(SVGAnimationWorkplace* animation_workplace,
							   HTML_Element* element,
							   BOOL& remove_item);

	virtual void UpdateTimingParameters(HTML_Element *element);

	virtual void Deactivate() { /* Do nothing */ }

	virtual HTML_Element *TargetElement() const { return timing_if->GetElement(); }

	virtual OP_STATUS RecoverFromError(SVGAnimationWorkplace* animation_workplace, HTML_Element *element);

	virtual void MarkPropsDirty();

    SVGTimingArguments timing_arguments;
    SVGTimingInterface* timing_if;
	BOOL has_dirty_props;
};

#endif // SVG_SUPPORT
#endif // !SVG_EXTERNAL_TIMELINE_H
