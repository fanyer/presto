/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ATTRIBUTE_PARSER_H
#define SVG_ATTRIBUTE_PARSER_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGTimeValue.h"
#include "modules/svg/src/SVGLength.h"
#include "modules/svg/src/OpBpath.h"

#include "modules/svg/src/animation/svganimationtime.h"
#include "modules/svg/src/animation/svganimationvalue.h"

class SVGPaintObject;
class SVGRepeatCountObject;
class SVGFontSizeObject;

#define SVG_CHECK_INPUT_STRING(X)				\
	if (!(X))									\
		return OpSVGStatus::BAD_PARAMETER;		\

// Make all functions static since the class carries no data
class SVGAttributeParser
{
private:
	/**
	 * Helper function for parsing keyspline lists
	 */
	static OP_STATUS ParseKeySplineList(const uni_char* input_string, unsigned strlen, SVGVector* result);

	/**
	 * Helper function for parsing lengthvector
	 */
	static OP_STATUS ParseLengthVector(const uni_char* input_string, unsigned strlen, BOOL is_dasharray, SVGVector* result);

public:
	/**
	 * Find the string in url(*) where * are the allowed characters in an url
	 * @param input_string
	 * @param strlen
	 * @param url_start
	 * @param url_len
	 * @return 
	 */
	static OP_STATUS ParseURL(const uni_char* input_string, unsigned strlen, const uni_char*& url_start, unsigned& url_len);

	/**
	 * Parse a attribute name as described in
	 * <URL:http://wwwi/~davve/specs/svg/REC-SVG11-20030114/animate.html#TargetAttributes>
	 *
	 * Result are saved in attribute_name and ns_idx if OpStatus::OK
	 * is returned.
	 *
	 * Returns OpSVGStatus::ATTRIBUTE_ERROR on malformed attribute
	 * name.
	 */
	static OP_STATUS ParseAttributeName(HTML_Element* elm,
										const uni_char* input_string,
										unsigned strlen,
										Markup::AttrType &attribute_name,
										short &ns_idx);

	/**
	* Parse a dasharray
	*/
	static OP_STATUS ParseDashArray(const uni_char* input_string, unsigned strlen, SVGVector* result) { 	return ParseLengthVector(input_string, strlen, TRUE, result); }

	/**
	 * Parse a vector of values
	 *
	 */
	static OP_STATUS ParseVector(const uni_char* input_string, unsigned strlen, SVGVector* result, SVGEnumType enum_type = SVGENUM_UNKNOWN);

	/**
	 * Number parser
	 */
	static OP_STATUS ParseNumber(const uni_char* input_string, unsigned strlen, BOOL normalize_percentage, SVGNumber& result);

	/**
	 * Clip shape parser (currently only rect)
	 */
	static OP_STATUS ParseClipShape(const uni_char* input_string, unsigned strlen, SVGRectObject** shape);

	/**
	 * Baseline shift parser
	 */
	static OP_STATUS ParseBaselineShift(const uni_char* input_string, unsigned strlen, SVGBaselineShiftObject** baseline_shift);

	/**
	 * Length parser
	 */
	static OP_STATUS ParseLength(const uni_char* input_string, unsigned strlen, SVGLengthObject **result);

	/**
	 * Parse a color
	 */
	static OP_STATUS ParseColorValue(const uni_char* input_string, unsigned strlen, SVGColorObject** color_val, BOOL is_viewportfill = FALSE);

	/**
	 * Parse a clock value
	 *
	 * (Or one of the following 'extra' values: 'default', 'inherit';
	 *  extra_values == 0 => accept 'indefinite'
	 *  extra_values == 1 => accept 'default'
	 *  extra_values == 2 => accept 'inherit')
	 */
	static OP_STATUS ParseAnimationTimeObject(const uni_char* input_string, unsigned strlen, SVGAnimationTimeObject*& animation_time_object, unsigned extra_values);

	/**
	 * Parse a from/to/by transformation value
	 */
	static OP_STATUS ParseTransformFromToByValue(const uni_char* input_string, unsigned strlen,
												 SVGTransformType type,
												 SVGTransform** outTransform);
	/**
	 * Parse a font size
	 */
	static OP_STATUS ParseFontSize(const uni_char *input_string, unsigned strlen, SVGFontSizeObject **font_size);

	/**
	 * Parse a rotation
	 */
	static OP_STATUS ParseRotate(const uni_char *input_string, unsigned strlen, SVGRotate*& type);
	static OP_STATUS ParseOrient(const uni_char *input_string, unsigned strlen, SVGOrient*& orient);

	/**
	 * Parse a paint
	 */
	static OP_STATUS ParsePaint(const uni_char* input_string, unsigned strlen, SVGPaintObject** paint);

	/**
	 * Parse a navigational reference
	 */
	static OP_STATUS ParseNavRef(const uni_char* input_string, unsigned strlen, SVGNavRef** navref);

	/**
	 * Parse a view box
	 */
	static OP_STATUS ParseViewBox(const uni_char *input_string, unsigned strlen, SVGRectObject** outviewbox);

	/**
	 * Parse a path
	 *
	 */
	static OP_STATUS ParseToNewPath(const uni_char* input_string, unsigned strlen, BOOL need_synchronized_seglist, OpBpath*& path);

	/**
	 * Parse a string to an existing path
	 *
	 */
	static OP_STATUS ParseToExistingPath(const uni_char* input_string, unsigned strlen, OpBpath* path);

	/**
	 * Parse a enumeration value
	 */
	static OP_STATUS ParseEnumValue(const uni_char* input_string, unsigned strlen, SVGEnumType enum_type, int& enum_value);

	/**
	 * Parse a aspect ratio
	 */
	static OP_STATUS ParsePreserveAspectRatio(const uni_char* input_string, unsigned strlen, SVGAspectRatio*& ratio);

#ifdef SVG_SUPPORT_FILTERS
	/**
	 * Parse an enable-background descriptor
	 */
	static OP_STATUS ParseEnableBackground(const uni_char* input_string, unsigned strlen, SVGEnableBackgroundObject** eb);
#endif // SVG_SUPPORT_FILTERS

	/**
	 * Parse a font family list
	 */
	static OP_STATUS ParseFontFamily(const uni_char *input_string, unsigned strlen, SVGVector* outFonts);

	/**
	 * Parse zoomAndPan
	 */
	static OP_STATUS ParseZoomAndPan(const uni_char *input_string, unsigned strlen, SVGZoomAndPan &result);

	/**
	 * Parse a repeat count attribute
	 */
	static OP_STATUS ParseRepeatCountObject(const uni_char *input_string, unsigned str_len, SVGRepeatCountObject *& repeat_count_object);
};

#endif // SVG_SUPPORT
#endif // !SVG_ATTRIBUTE_PARSER_H
