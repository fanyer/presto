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

#include "modules/svg/src/animation/svganimationtarget.h"
#include "modules/svg/src/animation/svganimationcalculator.h"
#include "modules/svg/src/animation/svganimationworkplace.h"
#include "modules/svg/src/animation/svganimationschedule.h"

#include "modules/svg/src/SVGTimedElementContext.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGTimeValue.h"

#include "modules/layout/cascade.h"

SVGAnimationTarget::SVGAnimationTarget(HTML_Element *target_element) :
	animated_element(target_element)
{
}

OP_STATUS
SVGAnimationTarget::RegisterListener(SVGTimeObject* etv)
{
	OP_ASSERT(etv);
	if (listeners.Find(etv) != -1)
	{
		return OpStatus::OK;
	}

	return listeners.Add(etv);
}

OP_STATUS
SVGAnimationTarget::UnregisterListener(SVGTimeObject* etv)
{
	OP_ASSERT(listeners.Find(etv) != -1);
	OpStatus::Ignore(listeners.RemoveByItem(etv));
	return OpStatus::OK;
}

BOOL
SVGAnimationTarget::ListensToEvent(DOM_EventType type)
{
	for (UINT32 i=0; i<listeners.GetCount(); i++)
	{
		SVGTimeObject* etv = listeners.Get(i);
		if (etv->GetEventType() == type)
			return TRUE;
	}
    return FALSE;
}

BOOL
SVGAnimationTarget::HandleEvent(HTML_Element* elm, const SVGManager::EventData& data)
{
    OP_NEW_DBG("SVGElementStateContext::HandleEvent", "svg_animation");

    SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
    if (!doc_ctx)
        return FALSE;

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();
	if (!animation_workplace)
		return FALSE;

    BOOL handled = FALSE;
    for (UINT32 i=0; i<listeners.GetCount(); i++)
    {
        SVGTimeObject* etv = listeners.Get(i);
        if (etv->GetEventType() == data.type)
        {
			SVG_ANIMATION_TIME time_to_add = animation_workplace->VirtualDocumentTime() + etv->GetOffset();
			RETURN_VALUE_IF_ERROR(etv->AddInstanceTime(time_to_add), FALSE);

			if (SVGTimingInterface *notifier = etv->GetNotifier())
				animation_workplace->InvalidateTimingElement(notifier->GetElement());

			handled = TRUE;
		}
	}

    return handled;
}

#endif // SVG_SUPPORT
