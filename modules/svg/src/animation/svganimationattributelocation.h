/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_ATTRIBUTE_LOCATION_H
#define SVG_ANIMATION_ATTRIBUTE_LOCATION_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGAttribute.h"

class SVGAnimationAttributeLocation
{
public:
    Markup::AttrType animated_name;
    Markup::AttrType base_name;
    short ns_idx;
    BOOL is_special;
    SVGAttributeType type;

    BOOL operator==(const SVGAnimationAttributeLocation &other)	{
		return animated_name == other.animated_name &&
			base_name == other.base_name &&
			ns_idx == other.ns_idx &&
			type == other.type;
	}

    int GetNsIdxOrSpecialNs() const	{
		return is_special ? (int)SpecialNs::NS_SVG : ns_idx;
	}
};

#endif // SVG_SUPPORT
#endif // SVG_ANIMATION_ATTRIBUTE_LOCATION_H
