/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_ANIMATION_TARGET_H
#define SVG_ANIMATION_TARGET_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationvalue.h"

#include "modules/svg/SVGManager.h"
#include "modules/svg/src/SVGAttribute.h"
#include "modules/util/simset.h"

class SVGAnimationWorkplace;
class SVGAnimationCalculator;
class SVGAnimationArguments;
struct SVGTimingArguments;
class SVGTimeObject;

class SVGAnimationTarget
{
public:
    SVGAnimationTarget(HTML_Element *target_element);

    HTML_Element *TargetElement() { return animated_element; }

	OP_STATUS RegisterListener(SVGTimeObject* etv);
	/**< Register a time object for callback at this animation
	 * target */

	OP_STATUS UnregisterListener(SVGTimeObject* etv);
	/**< Un-register a time object for callback at this animation
	 * target */

    BOOL ListensToEvent(DOM_EventType type);
	/**< Checks if any of the listeners listens to this type of
	 * message */

	BOOL HandleEvent(HTML_Element* elm, const SVGManager::EventData& data);
	/**< Handle an event by alerting the matching listeners */

private:
	OpVector<SVGTimeObject> listeners;

    HTML_Element *animated_element;
};

#endif // SVG_SUPPORT
#endif // !SVG_ANIMATION_TARGET_H

