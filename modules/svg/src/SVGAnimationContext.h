/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_ANIMCONTEXT_H
#define SVG_ANIMCONTEXT_H

#ifdef SVG_SUPPORT

#include "modules/dom/domeventtypes.h"
#include "modules/logdoc/htm_elm.h"

#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGAttribute.h"
#include "modules/svg/src/SVGTimeValue.h"
#include "modules/svg/src/SVGTimedElementContext.h"
#include "modules/svg/src/SVGKeySpline.h"
#include "modules/svg/src/SVGUtils.h"

#include "modules/svg/src/animation/svganimationinterval.h"
#include "modules/svg/src/animation/svganimationschedule.h"
#include "modules/svg/src/animation/svganimationcalculator.h"


class LayoutProperties;
class SVGMotionPath;
class SVGDocumentContext;
class SVGAnimationWorkplace;
class SVGAnimationArguments;

/**
 * This class contains state information about one animation element.
 *
 * A pointer to an instance of this class is given to the HTML_Element
 * that is of type {Markup::SVGE_SET, Markup::SVGE_ANIMATE,
 * Markup::SVGE_ANIMATETRANSFORM, Markup::SVGE_ANIMATECOLOR,
 * Markup::SVGE_ANIMATEMOTION}.
 *
 */
class SVGAnimationInterface : public SVGTimingInterface
{
public:
	SVGAnimationInterface(HTML_Element *animation_element) : SVGTimingInterface(animation_element) {}
	virtual ~SVGAnimationInterface() {}

	OP_STATUS SetAnimationArguments(SVGDocumentContext *doc_ctx,
									SVGAnimationArguments& animation_arguments,
									SVGAnimationTarget *&animation_target);
	/**< Set animation arguments
	 *
	 * By returning some error other than OpStatus::ERR_NO_MEMORY, the
	 * animation of this element is disabled. The element will still
	 * act as a timed element, creating intervals and dispatching
	 * events. */

#ifdef SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN
	/**
	 * Undocumented. Move to SVGUtils?
	 */
	void MarkTransformAsNonAdditive(HTML_Element *animated_element);
#endif // SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN

	static BOOL IsFilteredByRequirements(HTML_Element* layouted_elm)
	{
		return !SVGUtils::ShouldProcessElement(layouted_elm);
	}

protected:
	static BOOL IsAnimatable(HTML_Element* target_element, Markup::AttrType attr_name, int ns_idx);
	virtual SVGAnimationInterface* GetAnimationInterface() { return this; }
};

class SVGAnimationElement : public SVGInvisibleElement, public SVGAnimationInterface
{
public:
	SVGAnimationElement(HTML_Element* element) :
		SVGInvisibleElement(element), SVGAnimationInterface(element) {}

	virtual class SVGTimingInterface* GetTimingInterface() { return static_cast<SVGTimingInterface*>(this); }
	virtual class SVGAnimationInterface* GetAnimationInterface() { return static_cast<SVGAnimationInterface*>(this); }
};

#endif // SVG_SUPPORT
#endif // SVG_ANIMCONTEXT_H
