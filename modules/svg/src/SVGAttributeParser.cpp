/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/display/vis_dev.h"
#include "modules/style/css_all_properties.h"
#include "modules/logdoc/htm_lex.h"

#include "modules/xmlutils/xmlnames.h"

#include "modules/svg/svg.h"
#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGAttributeParser.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGRepeatCountObject.h"
#include "modules/svg/src/SVGPaint.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGKeySpline.h"
#include "modules/svg/src/parser/svgtransformparser.h"
#include "modules/svg/src/parser/svgpathparser.h"
#include "modules/svg/src/parser/svgtimeparser.h"
#include "modules/svg/src/parser/svgtokenizer.h"
#include "modules/svg/src/parser/svgnumberparser.h"
#include "modules/svg/src/parser/svgstringparser.h"
#include "modules/svg/src/parser/svgpaintparser.h"
#include "modules/svg/src/parser/svglengthparser.h"
#include "modules/svg/src/parser/svgangleparser.h"

/* static */ OP_STATUS
SVGAttributeParser::ParseAttributeName(HTML_Element* elm,
									   const uni_char* input_string,
									   unsigned str_len,
									   Markup::AttrType &attribute_name,
									   short &ns_idx)
{
	XMLCompleteNameN complete_name(input_string, str_len);
	if (complete_name.GetPrefixLength())
	{
		if (!XMLNamespaceDeclaration::ResolveNameInScope(elm, complete_name))
			return OpSVGStatus::ATTRIBUTE_ERROR;
		ns_idx = complete_name.GetNsIndex();
	}
	else
	{
		ns_idx = NS_IDX_DEFAULT;
	}

	NS_Type attr_ns = g_ns_manager->GetNsTypeAt(elm->ResolveNsIdx(ns_idx));

	switch(attr_ns)
	{
		case NS_XLINK:
			attribute_name = XLink_Lex::GetAttrType(complete_name.GetLocalPart(), complete_name.GetLocalPartLength());
			break;
		case NS_SVG:
			attribute_name = SVG_Lex::GetAttrType(complete_name.GetLocalPart(), complete_name.GetLocalPartLength());
			if (attribute_name == Markup::HA_XML &&
				complete_name.GetLocalPartLength() == 6 &&
				uni_strncmp(complete_name.GetLocalPart(), "marker", 6) == 0)
				attribute_name = Markup::SVGA_ANIMATED_MARKER_PROP;
			break;
		default:
			return OpSVGStatus::ATTRIBUTE_ERROR; // We don't support other namespaces than xlink and svg.
	}

	return OpStatus::OK;
}

OP_STATUS
SVGAttributeParser::ParseClipShape(const uni_char* input_string, unsigned strlen, SVGRectObject** shape)
{
	OP_ASSERT(shape);
	SVGLengthParser length_parser;
	return length_parser.ParseClipShape(input_string, strlen, *shape);
}

OP_STATUS
SVGAttributeParser::ParseBaselineShift(const uni_char* input_string, unsigned strlen, SVGBaselineShiftObject** baseline_shift)
{
	OP_ASSERT(baseline_shift);
	SVGLengthParser length_parser;
	return length_parser.ParseBaselineShift(input_string, strlen, *baseline_shift);
}

OP_STATUS
SVGAttributeParser::ParseKeySplineList(const uni_char* input_string, unsigned strlen, SVGVector* result)
{
	SVGNumber n[4];
	SVGTokenizer tokenizer;
	tokenizer.Reset(input_string, strlen);
	tokenizer.EatWsp();

	while(!tokenizer.IsEnd())
	{
		double d;
		int i=0;
		while (i<4 && tokenizer.ScanNumber(d))
		{
			n[i++] = d;
			tokenizer.EatWspSeparatorWsp(',');
		}

		if (i == 4)
		{
			SVGKeySpline *ks = OP_NEW(SVGKeySpline, (n[0], n[1], n[2], n[3]));
			if (!ks)
				return OpStatus::ERR_NO_MEMORY;

			if (OpStatus::IsMemoryError(result->Append(ks)))
			{
				OP_DELETE(ks);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		else
		{
			return OpSVGStatus::ATTRIBUTE_ERROR;
		}

		tokenizer.EatWspSeparatorWsp(';');
	}
	return OpStatus::OK;
}

OP_STATUS
SVGAttributeParser::ParseLengthVector(const uni_char* input_string, unsigned strlen, BOOL is_dasharray, SVGVector* result)
{
	OP_ASSERT(result != NULL);
	OP_ASSERT(result->Separator() == SVGVECTORSEPARATOR_COMMA_OR_WSP);
	if(is_dasharray)
	{
		if (strlen == 4 && uni_strncmp(input_string, "none", strlen) == 0)
		{
			result->SetIsNone(TRUE);
			return OpStatus::OK;
		}
	}

	SVGLengthParser length_parser;
	return length_parser.ParseLengthList(input_string, strlen, *result);
}

OP_STATUS
SVGAttributeParser::ParseVector(const uni_char* input_string, unsigned strlen, SVGVector* result, SVGEnumType enum_type)
{
	OP_NEW_DBG("SVGAttributeParser::ParseVector()", "svg_parser");

	if (!result || strlen == 0)
		return OpStatus::OK;

	switch (result->VectorType())
	{
		case SVGOBJECT_NUMBER:
		{
			SVGNumberParser number_parser;
			OP_STATUS status = OpStatus::OK;
			switch(result->Separator())
			{
				case SVGVECTORSEPARATOR_SEMICOLON:
					status = number_parser.ParseNumberSemicolonList(input_string, strlen, *result);
					break;
				default:
				case SVGVECTORSEPARATOR_COMMA_OR_WSP:
					status = number_parser.ParseNumberCommaList(input_string, strlen, *result);
					break;
			}
			RETURN_IF_ERROR(status);
		}
		break;
		case SVGOBJECT_TIME:
		{
			SVGTimeParser time_parser;
			RETURN_IF_ERROR(time_parser.ParseTimeList(input_string, strlen, result));
		}
		break;
		case SVGOBJECT_STRING:
		{
			OP_STATUS status = OpStatus::OK;
			SVGStringParser string_parser;
			switch(result->Separator())
			{
				case SVGVECTORSEPARATOR_SEMICOLON:
					status = string_parser.ParseStringSemicolonList(input_string, strlen, *result);
					break;
				default:
				case SVGVECTORSEPARATOR_COMMA_OR_WSP:
					status = string_parser.ParseStringCommaList(input_string, strlen, *result);
					break;
			}
			RETURN_IF_ERROR(status);
		}
		break;
		case SVGOBJECT_KEYSPLINE:
		{
			RETURN_IF_ERROR(ParseKeySplineList(input_string, strlen, result));
		}
		break;
		case SVGOBJECT_POINT:
		{
			SVGNumberParser number_parser;
			RETURN_IF_ERROR(number_parser.ParsePointList(input_string, strlen, *result));
		}
		break;
		case SVGOBJECT_LENGTH:
		{
			RETURN_IF_ERROR(ParseLengthVector(input_string, strlen, FALSE, result));
		}
		break;
		case SVGOBJECT_TRANSFORM:
		{
			SVGTransformParser transform_parser;
			RETURN_IF_ERROR(transform_parser.ParseTransformList(input_string, strlen, result));
		}
		break;
		case SVGOBJECT_ENUM:
		{
			SVGTokenizer tokenizer;
			tokenizer.Reset(input_string, strlen);
			tokenizer.EatWsp();
			SVGTokenizer::StringRules rules;
			rules.Init(/* allow_quoting = */ TRUE,
			   /* wsp_separator = */ TRUE,
			   /* comma_separator = */ TRUE,
			   /* semicolon_separator = */ FALSE,
			   /* end_condition = */ '\0');

			OP_STATUS status = OpStatus::OK;
			while(OpStatus::IsSuccess(status))
			{
				const uni_char *sub_string;
				unsigned sub_string_length;
				if (tokenizer.ScanString(rules, sub_string, sub_string_length))
				{
					int enum_val;
					SVGEnum* value = NULL;
					if(OpStatus::IsSuccess(ParseEnumValue(sub_string, sub_string_length, enum_type, enum_val)))
					{
						value = OP_NEW(SVGEnum, (enum_type, enum_val));
						if (!value)
							return OpStatus::ERR_NO_MEMORY;
					}
					
					if(value)
					{
						status = result->Append(value);
						if(OpStatus::IsError(status))
						{
							OP_DELETE(value);
							return status;
						}
					}
				}
				else
					break;
			}
		}
		break;
		default:
		{
			OP_DBG(("Don't know how to parse vector of type: %d\n", result->VectorType()));
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

OP_STATUS
SVGAttributeParser::ParseLength(const uni_char* input_string, unsigned str_len, SVGLengthObject** result)
{
	OP_ASSERT(result != NULL);
	SVGLengthParser length_parser;
	return length_parser.ParseLength(input_string, str_len, *result);
}

OP_STATUS
SVGAttributeParser::ParseColorValue(const uni_char* input_string, unsigned str_len, SVGColorObject** color_val, BOOL is_viewportfill)
{
	OP_STATUS status = OpStatus::OK;
	SVGColor color;

	if (str_len == 12 && uni_strncmp(input_string, "currentColor", 12) == 0)
	{
		color.SetColorType(SVGColor::SVGCOLOR_CURRENT_COLOR);
	}
	else if (is_viewportfill && str_len == 4 && uni_strncmp(input_string, "none", 4) == 0)
	{
		color.SetColorType(SVGColor::SVGCOLOR_NONE);
	}
	else
	{
		SVGPaintParser paint_parser;
		status = paint_parser.ParseColor(input_string, str_len, color);
	}

	if (OpStatus::IsSuccess(status))
	{
		*color_val = OP_NEW(SVGColorObject, ());
		if (!*color_val)
			return OpStatus::ERR_NO_MEMORY;

		(*color_val)->GetColor() = color;
		return OpStatus::OK;
	}
	else
	{
		return status;
	}
}

OP_STATUS
SVGAttributeParser::ParseAnimationTimeObject(const uni_char* input_string, unsigned str_len,
											 SVGAnimationTimeObject*& animation_time_object,
											 unsigned extra_values)
{
	SVG_ANIMATION_TIME animation_time;

	BOOL allow_indef = extra_values == 0; // Allow 'indefinite'
	BOOL allow_def = extra_values == 1; // Allow 'default'
	BOOL allow_inh = extra_values == 2; // Allow 'inherit'

	OP_ASSERT(allow_indef || (allow_def && !allow_inh) || (!allow_def && allow_inh));

	if ((allow_indef && (str_len == 10 && uni_strncmp(input_string, "indefinite", str_len) == 0)) ||
		(allow_def && (str_len == 7 && uni_strncmp(input_string, "default", str_len) == 0)) ||
		(allow_inh && (str_len == 7 && uni_strncmp(input_string, "inherit", str_len) == 0)))
		animation_time = SVGAnimationTime::Indefinite();
	else
	{
		SVGTimeParser time_parser;
		RETURN_IF_ERROR(time_parser.ParseAnimationTime(input_string, str_len, animation_time));
	}

	animation_time_object = OP_NEW(SVGAnimationTimeObject, (animation_time));
	return animation_time_object ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
SVGAttributeParser::ParseFontSize(const uni_char* input_string, unsigned strlen, SVGFontSizeObject **font_size)
{
	OP_ASSERT(font_size != NULL);
	SVGLengthParser length_parser;
	return length_parser.ParseFontsize(input_string, strlen, *font_size);
}

OP_STATUS
SVGAttributeParser::ParseRotate(const uni_char* input_string, unsigned strlen, SVGRotate*& type)
{
	SVGAngleParser angle_parser;
	return angle_parser.ParseRotate(input_string, strlen, type);
}

OP_STATUS
SVGAttributeParser::ParseOrient(const uni_char* input_string, unsigned strlen, SVGOrient*& orient)
{
	SVGAngleParser angle_parser;
	return angle_parser.ParseOrient(input_string, strlen, orient);
}

OP_STATUS
SVGAttributeParser::ParseToNewPath(const uni_char* input_string, unsigned str_len, BOOL need_synchronized_seglist, OpBpath*& path)
{
	RETURN_IF_ERROR(OpBpath::Make(path, need_synchronized_seglist));

	SVGPathParser path_parser;
	OP_STATUS status = path_parser.ParsePath(input_string, str_len, path);

	path->CompactPath(); // we might have wasted space to make
						 // additions quick. Now is the time to trim
						 // to an optimal size
	return status;
}

OP_STATUS
SVGAttributeParser::ParseToExistingPath(const uni_char* input_string, unsigned str_len, OpBpath* path)
{
	SVGPathParser path_parser;
	return path_parser.ParsePath(input_string, str_len, path);
}

OP_STATUS
SVGAttributeParser::ParsePaint(const uni_char* input_string, unsigned str_len, SVGPaintObject** paint_object)
{
	SVGPaint paint;
	SVGPaintParser paint_parser;

	OP_STATUS status = paint_parser.ParsePaint(input_string, str_len, paint);
	if (OpStatus::IsSuccess(status))
	{
		*paint_object = OP_NEW(SVGPaintObject, ());
		if (!*paint_object)
			return OpStatus::ERR_NO_MEMORY;

		status = (*paint_object)->GetPaint().Copy(paint);
	}
	return status;
}

OP_STATUS
SVGAttributeParser::ParseNavRef(const uni_char* input_string, unsigned strlen, SVGNavRef** navref)
{
	SVGNavRef::NavRefType nav_ref_type;

	const uni_char *uri_string = NULL;
	unsigned uri_string_length = 0;

	SVGTokenizer tokenizer;
	tokenizer.Reset(input_string, strlen);
	if (tokenizer.Scan("auto"))
		nav_ref_type = SVGNavRef::AUTO;
	else if (tokenizer.Scan("self"))
		nav_ref_type = SVGNavRef::SELF;
	else if (tokenizer.ScanURL(uri_string, uri_string_length))
		nav_ref_type = SVGNavRef::URI;
	else
		return OpSVGStatus::ATTRIBUTE_ERROR;

	OP_STATUS status = OpStatus::OK;
	*navref = OP_NEW(SVGNavRef, (nav_ref_type));
	if (!*navref)
	{
		status = OpStatus::ERR_NO_MEMORY;
	}
	else if (nav_ref_type == SVGNavRef::URI)
	{
		status = (*navref)->SetURI(uri_string, uri_string_length);
	}

	return status;
}

/* static */ OP_STATUS
SVGAttributeParser::ParseTransformFromToByValue(const uni_char* input_string, unsigned strlen,
												SVGTransformType type,
												SVGTransform** outTransform)
{
	SVGTransform *transform = OP_NEW(SVGTransform, ());
	if (!transform)
		return OpStatus::ERR_NO_MEMORY;

	SVGTransformParser transform_parser;
	OP_STATUS status = transform_parser.ParseTransformArgument(input_string, strlen, type, *transform);

	if (OpStatus::IsSuccess(status))
	{
		*outTransform = transform;
	}
	else
	{
		OP_DELETE(transform);
	}
	return status;
}

OP_STATUS
SVGAttributeParser::ParseViewBox(const uni_char* input_string, unsigned strlen, SVGRectObject** outviewbox)
{
	OP_ASSERT(outviewbox);
	OP_STATUS status = OpStatus::OK;
	SVGTokenizer tokenizer;
	tokenizer.Reset(input_string, strlen);

	int i = 0;
	double nums[4];
	for (;i<4;i++)
	{
		if (!tokenizer.ScanNumber(nums[i]))
			break;
		tokenizer.EatWspSeparatorWsp(',');
	}

	if (i == 4)
	{
		*outviewbox = OP_NEW(SVGRectObject, (SVGRect(nums[0], nums[1],
												nums[2], nums[3])));
		if (!*outviewbox)
			status = OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		status = OpStatus::ERR;
	}

	return tokenizer.ReturnStatus(status);
}

OP_STATUS
SVGAttributeParser::ParseEnumValue(const uni_char* input_string, unsigned strlen, SVGEnumType enum_type, int& enum_value)
{
	SVG_CHECK_INPUT_STRING(input_string);
	enum_value = SVGEnumUtils::GetEnumValue(enum_type, input_string, strlen);
	return (enum_value == -1) ? (OP_STATUS)OpSVGStatus::ATTRIBUTE_ERROR : OpStatus::OK;
}

OP_STATUS
SVGAttributeParser::ParseFontFamily(const uni_char* input_string, unsigned strlen, SVGVector* outFonts)
{
	SVGStringParser string_parser;
	return string_parser.ParseFontFamilyList(input_string, strlen, *outFonts);
}

OP_STATUS
SVGAttributeParser::ParseURL(const uni_char* input_string, unsigned strlen, const uni_char*& url_start, unsigned& url_len)
{
	SVG_CHECK_INPUT_STRING(input_string);

	SVGTokenizer tokenizer;
	tokenizer.Reset(input_string, strlen);

	if (tokenizer.ScanURL(url_start, url_len))
		return OpStatus::OK;
	else
		return OpSVGStatus::ATTRIBUTE_ERROR;
}

#ifdef SVG_SUPPORT_FILTERS
OP_STATUS
SVGAttributeParser::ParseEnableBackground(const uni_char* input_string, unsigned strlen, SVGEnableBackgroundObject** eb)
{
	SVGTokenizer tokenizer;
	tokenizer.Reset(input_string, strlen);
	if (tokenizer.Scan("new"))
	{
		int i = 0;
		double nums[4];
		for (;i<4;i++)
		{
			tokenizer.EatWsp();
			if (!tokenizer.ScanNumber(nums[i]))
				break;
		}

		if (i == 4)
			*eb = OP_NEW(SVGEnableBackgroundObject, (SVGEnableBackground::SVGENABLEBG_NEW,
												nums[0], nums[1], nums[2], nums[3]));
		else if (i == 0)
			*eb = OP_NEW(SVGEnableBackgroundObject, (SVGEnableBackground::SVGENABLEBG_NEW));
		else
			return OpSVGStatus::ATTRIBUTE_ERROR;
	}
	else if (tokenizer.Scan("accumulate"))
	{
		*eb = OP_NEW(SVGEnableBackgroundObject, (SVGEnableBackground::SVGENABLEBG_ACCUMULATE));
	}
	else
	{
		return OpSVGStatus::ATTRIBUTE_ERROR;
	}

	return (*eb) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}
#endif // SVG_SUPPORT_FILTERS

OP_STATUS
SVGAttributeParser::ParsePreserveAspectRatio(const uni_char* input_string, unsigned strlen, SVGAspectRatio*& ratio)
{
	SVGTokenizer tokenizer;
	tokenizer.Reset(input_string, strlen);

	SVGAlignType align = SVGALIGN_XMIDYMID;
	SVGMeetOrSliceType mos = SVGMOS_MEET;

	tokenizer.EatWsp();
	BOOL defer = tokenizer.Scan("defer");
	tokenizer.EatWsp();

	if (tokenizer.Scan("none"))
		align = SVGALIGN_NONE;
	else if (tokenizer.Scan("xMinYMin"))
		align = SVGALIGN_XMINYMIN;
	else if (tokenizer.Scan("xMidYMin"))
		align = SVGALIGN_XMIDYMIN;
	else if (tokenizer.Scan("xMaxYMin"))
		align = SVGALIGN_XMAXYMIN;
	else if (tokenizer.Scan("xMinYMid"))
		align = SVGALIGN_XMINYMID;
	else if (tokenizer.Scan("xMidYMid"))
		align = SVGALIGN_XMIDYMID;
	else if (tokenizer.Scan("xMaxYMid"))
		align = SVGALIGN_XMAXYMID;
	else if (tokenizer.Scan("xMinYMax"))
		align = SVGALIGN_XMINYMAX;
	else if (tokenizer.Scan("xMidYMax"))
		align = SVGALIGN_XMIDYMAX;
	else if (tokenizer.Scan("xMaxYMax"))
		align = SVGALIGN_XMAXYMAX;
	else
		align = SVGALIGN_UNKNOWN;

	tokenizer.EatWsp();

	if (tokenizer.Scan("meet"))
		mos = SVGMOS_MEET;
	else if (tokenizer.Scan("slice"))
		mos = SVGMOS_SLICE;

	if (align != SVGALIGN_UNKNOWN)
	{
		ratio = OP_NEW(SVGAspectRatio, (defer, align, mos));
		if(!ratio)
			return OpStatus::ERR_NO_MEMORY;
		return OpStatus::OK;
	}
	else
	{
		return OpSVGStatus::ATTRIBUTE_ERROR;
	}
}

OP_STATUS
SVGAttributeParser::ParseNumber(const uni_char *input, unsigned str_len, BOOL normalize_percentage, SVGNumber& num)
{
	SVGNumberParser number_parser;
	return number_parser.ParseNumber(input, str_len, normalize_percentage, num);
}

OP_STATUS
SVGAttributeParser::ParseRepeatCountObject(const uni_char *input_string,
										   unsigned str_len,
										   SVGRepeatCountObject *&repeat_count_object)
{
	SVGRepeatCount repeat_count;

	if (str_len == 10 && uni_strncmp(input_string, "indefinite", str_len) == 0)
		repeat_count.repeat_count_type = SVGRepeatCount::SVGREPEATCOUNTTYPE_INDEFINITE;
	else
	{
		SVGNumber repeat_count_number;
		SVGNumberParser number_parser;
		RETURN_IF_ERROR(number_parser.ParseNumber(input_string, str_len, repeat_count_number));

		repeat_count.repeat_count_type = SVGRepeatCount::SVGREPEATCOUNTTYPE_VALUE;
		repeat_count.value = repeat_count_number.GetFloatValue();
	}

	repeat_count_object = OP_NEW(SVGRepeatCountObject, (repeat_count));
	return repeat_count_object != NULL ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS SVGAttributeParser::ParseZoomAndPan(const uni_char* input_string, unsigned strlen, SVGZoomAndPan &result)
{
	if (strlen == 7)
	{
		if (uni_strncmp(input_string, "disable", 7) == 0)
		{
			result = SVGZOOMANDPAN_DISABLE;
			return OpStatus::OK;
		}
		else if (uni_strncmp(input_string, "magnify", 7) == 0)
		{
			result = SVGZOOMANDPAN_MAGNIFY;
			return OpStatus::OK;
		}
	}
	return OpSVGStatus::ATTRIBUTE_ERROR;
}
#endif // SVG_SUPPORT

