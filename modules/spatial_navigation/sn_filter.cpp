/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
**
*/

#include "core/pch.h"
#include "modules/spatial_navigation/sn_filter.h"
#include "modules/spatial_navigation/sn_util.h"

#ifdef _SPAT_NAV_SUPPORT_

BOOL
SnDefaultNavFilter::AcceptElement(HTML_Element* element, FramesDocument* doc)
{
	if (ElementTypeCheck::IsClickable(element, doc))
		return TRUE;

#ifdef _PLUGIN_NAVIGATION_SUPPORT_
	if (ElementTypeCheck::IsNavigatePlugin(element))
		return TRUE;
#endif
	
	return FALSE;
}

BOOL
SnHeadingNavFilter::AcceptElement(HTML_Element* element, FramesDocument* doc)
{
	return ElementTypeCheck::IsHeading(element, doc);
}

BOOL
SnParagraphNavFilter::AcceptElement(HTML_Element* element, FramesDocument* doc)
{
	return ElementTypeCheck::IsParagraph(element, doc);
}

BOOL
SnFormNavFilter::AcceptElement(HTML_Element* element, FramesDocument* doc)
{
	return ElementTypeCheck::IsFormObject(element, doc);
}

BOOL
SnATagNavFilter::AcceptElement(HTML_Element* element, FramesDocument* doc)
{
	return ElementTypeCheck::IsLink(element, doc);
}

BOOL
SnImageNavFilter::AcceptElement(HTML_Element* element, FramesDocument* doc)
{
	return ElementTypeCheck::IsImage(element, doc);
}

BOOL
SnNonLinkNavFilter::AcceptElement(HTML_Element* element, FramesDocument* doc)
{
	return !ElementTypeCheck::IsLink(element, doc);
}

void SnConfigurableNavFilter::SetFilterArray(OpSnNavFilter* const * filter_array)
{
	accepts_plugins = FALSE;

	int i = 0;
	for (; *filter_array && i<MAX_NUMBERS_OF_FILTERS; filter_array++, i++)
	{
		filters[i] = *filter_array;
		if (filters[i]->AcceptsPlugins())
		{
			accepts_plugins = TRUE;
		}
	}
	OP_ASSERT(i<=MAX_NUMBERS_OF_FILTERS);
	filters[i] = NULL;
}

BOOL 
SnConfigurableNavFilter::AcceptElement(HTML_Element* element, FramesDocument* doc)
{
	OP_ASSERT(element);

	for (OpSnNavFilter** filter_array = filters; *filter_array; filter_array++)
	{
		if ((*filter_array)->AcceptElement(element, doc))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL
SnPickerModeFilter::AcceptElement(HTML_Element *element, FramesDocument* doc)
{
	OP_ASSERT(element);

	if (ElementTypeCheck::IsMailtoLink(element, doc) ||
		ElementTypeCheck::IsImage(element, doc) ||
		ElementTypeCheck::IsClickable(element, doc) ||
		ElementTypeCheck::IsTelLink(element, doc) ||
		ElementTypeCheck::IsWtaiLink(element, doc))
        return TRUE;

	return FALSE;
}

#endif // _SPAT_NAV_SUPPORT_
