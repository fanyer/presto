/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_ANIMATION_VALUE_CONTEXT_H
#define SVG_ANIMATION_VALUE_CONTEXT_H

#ifdef SVG_SUPPORT

class HTMLayoutProperties;

#include "modules/logdoc/htm_elm.h"
#include "modules/svg/src/animation/svganimationattributelocation.h"

class HTMLayoutProperties;

class SVGAnimationValueContext
{
public:
    SVGAnimationValueContext();
    ~SVGAnimationValueContext();

    void Bind(HTML_Element* animated_element) {	element = animated_element; }

    void UpdateToAttribute(const SVGAnimationAttributeLocation* location)
		{
			this->location = location;

			resolved_percentage = 0;
			resolved_paint_inheritance = 0;
		}

    float GetFontHeight() { AssertProps(); return font_height; }
    float GetExHeight() { AssertProps(); return ex_height; }
    float GetRootFontHeight() { AssertProps(); return root_font_height; }
    UINT32 GetCurrentColor() { AssertProps(); return current_color; }

    const HTMLayoutProperties* GetParentProps() { AssertProps(); return parent_props; }
    const HTMLayoutProperties* GetProps() { AssertProps(); return props; }

    float GetPercentageOf() { AssertPercentage(); return percentage_of; }
    UINT32 GetInheritRGBColor() { AssertPaintInheritance(); return inherit_rgb_color; }

private:
    void AssertProps()
	{
		if (!resolved_props)
			ResolveProps();
	}

    void ResolveProps();
    void ResolveViewport();

    void AssertPercentage()
	{
		if (!resolved_percentage)
			ResolvePercentageOf();
	}
    void AssertPaintInheritance()
	{
		if (!resolved_paint_inheritance)
			ResolvePaintInheritance();
	}

    void ResolvePercentageOf();
    void ResolvePaintInheritance();

    HTML_Element* element;
    const SVGAnimationAttributeLocation* location;

    // Element specific
    const HTMLayoutProperties *parent_props;
    const HTMLayoutProperties *props;

    Head prop_list;

    float font_height;
	/** For the referred font:
		The height of 'x' glyph part that is above the baseline. */
    float ex_height;
	/** The size of the font of the root element of the document in which
		the element, that is referred by this SVGAnimationValueContext,
		is located. */
    float root_font_height;
    UINT32 current_color;

    // This is 'derived' state, and is used by the percentage_of resolution
    float viewport_width;
    float viewport_height;
    long containing_block_width;
    long containing_block_height;

    // Attribute and type specific
    float percentage_of;
    UINT32 inherit_rgb_color;

    // State flags
    unsigned resolved_props : 1;
    unsigned resolved_viewport : 1;
    unsigned resolved_percentage : 1;
    unsigned resolved_paint_inheritance : 1;
};

#endif // SVG_SUPPORT
#endif // SVG_ANIMATION_VALUE_CONTEXT
