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

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/parser/svglengthparser.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGFontSize.h"

OP_STATUS
SVGLengthParser::ParseLength(const uni_char *input, unsigned input_length,
							 SVGLengthObject *&length_object)
{
    status = OpStatus::OK;
    tokenizer.Reset(input, input_length);

    if (!ScanLength(length_object))
		status = OpStatus::ERR;

    return tokenizer.ReturnStatus(status);
}

OP_STATUS
SVGLengthParser::ParseLengthList(const uni_char *input, unsigned input_length,
								 SVGVector &vector)
{
	status = OpStatus::OK;
	tokenizer.Reset(input, input_length);

    SVGLengthObject* length_object;
    while(ScanLength(length_object) && OpStatus::IsSuccess(status))
    {
		status = vector.Append(length_object);
		if(OpStatus::IsError(status))
			OP_DELETE(length_object);

		tokenizer.EatWspSeparatorWsp(',');
    }

    return OpStatus::OK;
}

BOOL
SVGLengthParser::ScanLength(SVGLengthObject *&length_object)
{
    tokenizer.EatWsp();

    if (tokenizer.IsEnd())
		return FALSE;

    SVGLength length;

	if (ScanLength(length))
    {
		length_object = OP_NEW(SVGLengthObject, ());
		if (length_object)
		{
			length_object->GetLength() = length;
			return TRUE;
		}

		status = OpStatus::ERR_NO_MEMORY;
    }

    return FALSE;
}

BOOL
SVGLengthParser::ScanLength(SVGLength &length)
{
    double d;
    if (tokenizer.ScanNumber(d))
    {
		int unit = CSS_NUMBER;
		if (tokenizer.Scan("em"))
			unit = CSS_EM;
		else if (tokenizer.Scan("rem"))
			unit = CSS_REM;
		else if (tokenizer.Scan("ex"))
			unit = CSS_EX;
		else if(tokenizer.Scan('%'))
			unit = CSS_PERCENTAGE;
		else if(tokenizer.Scan("pt"))
			unit = CSS_PT;
		else if(tokenizer.Scan("px"))
			unit = CSS_PX;
		else if(tokenizer.Scan("pc"))
			unit = CSS_PC;
		else if(tokenizer.Scan("cm"))
			unit = CSS_CM;
		else if(tokenizer.Scan("mm"))
			unit = CSS_MM;
		else if(tokenizer.Scan("in"))
			unit = CSS_IN;

		length.SetUnit(unit);
		length.SetScalar(d);
		return TRUE;
    }
    else
    {
		return FALSE;
    }
}

OP_STATUS
SVGLengthParser::ParseBaselineShift(const uni_char *input, unsigned input_length, SVGBaselineShiftObject *&bls_object)
{
    status = OpStatus::OK;
    tokenizer.Reset(input, input_length);

    SVGLength length;
	SVGBaselineShift::BaselineShiftType bls_type = SVGBaselineShift::SVGBASELINESHIFT_BASELINE; // dummy value, warningfix
    if (tokenizer.Scan("baseline"))
    {
		bls_type = SVGBaselineShift::SVGBASELINESHIFT_BASELINE;
    }
    else if (tokenizer.Scan("sub"))
    {
		bls_type = SVGBaselineShift::SVGBASELINESHIFT_SUB;
    }
    else if (tokenizer.Scan("super"))
    {
		bls_type = SVGBaselineShift::SVGBASELINESHIFT_SUPER;
    }
    else if (ScanLength(length))
	{
		bls_type = SVGBaselineShift::SVGBASELINESHIFT_VALUE;
	}
	else
	{
		status = OpStatus::ERR;
	}

    if (OpStatus::IsSuccess(status))
    {
		bls_object = OP_NEW(SVGBaselineShiftObject, (bls_type));
		if (!bls_object)
			status = OpStatus::ERR_NO_MEMORY;
		else if (bls_type == SVGBaselineShift::SVGBASELINESHIFT_VALUE)
			bls_object->GetBaselineShift().GetValueRef() = length;
    }

    return tokenizer.ReturnStatus(status);
}

OP_STATUS
SVGLengthParser::ParseFontsize(const uni_char *input, unsigned input_length,
							   SVGFontSizeObject *&fontsize_object)
{
    status = OpStatus::OK;
    tokenizer.Reset(input, input_length);

    SVGAbsoluteFontSize abs_fontsize = SVGABSOLUTEFONTSIZE_MEDIUM;
    SVGRelativeFontSize rel_fontsize = SVGRELATIVEFONTSIZE_SMALLER; 
    SVGLength length;
    SVGFontSize::FontSizeMode fontsize_mode = SVGFontSize::MODE_UNKNOWN;

    if (tokenizer.Scan("smaller"))
    {
		rel_fontsize = SVGRELATIVEFONTSIZE_SMALLER;
		fontsize_mode = SVGFontSize::MODE_RELATIVE;
    }
    else if (tokenizer.Scan("larger"))
    {
		rel_fontsize = SVGRELATIVEFONTSIZE_LARGER;
		fontsize_mode = SVGFontSize::MODE_RELATIVE;
    }
    else if (tokenizer.Scan("xx-small"))
    {
		abs_fontsize = SVGABSOLUTEFONTSIZE_XXSMALL;
		fontsize_mode = SVGFontSize::MODE_ABSOLUTE;
    }
    else if (tokenizer.Scan("x-small"))
    {
		abs_fontsize = SVGABSOLUTEFONTSIZE_XSMALL;
		fontsize_mode = SVGFontSize::MODE_ABSOLUTE;
    }
    else if (tokenizer.Scan("small"))
    {
		abs_fontsize = SVGABSOLUTEFONTSIZE_SMALL;
		fontsize_mode = SVGFontSize::MODE_ABSOLUTE;
    }
    else if (tokenizer.Scan("medium"))
    {
		abs_fontsize = SVGABSOLUTEFONTSIZE_MEDIUM;
		fontsize_mode = SVGFontSize::MODE_ABSOLUTE;
    }
    else if (tokenizer.Scan("large"))
    {
		abs_fontsize = SVGABSOLUTEFONTSIZE_LARGE;
		fontsize_mode = SVGFontSize::MODE_ABSOLUTE;
    }
    else if (tokenizer.Scan("x-large"))
    {
		abs_fontsize = SVGABSOLUTEFONTSIZE_XLARGE;
		fontsize_mode = SVGFontSize::MODE_ABSOLUTE;
    }
    else if (tokenizer.Scan("xx-large"))
    {
		abs_fontsize = SVGABSOLUTEFONTSIZE_XXLARGE;
		fontsize_mode = SVGFontSize::MODE_ABSOLUTE;
    }
    else
    {
		if (ScanLength(length))
		{
			fontsize_mode = SVGFontSize::MODE_LENGTH;
		}
		else
		{
			status = OpStatus::ERR;
		}
    }

    if (OpStatus::IsSuccess(status))
    {
		fontsize_object = OP_NEW(SVGFontSizeObject, ());
		if (!fontsize_object)
			status = OpStatus::ERR_NO_MEMORY;
		else if (fontsize_mode == SVGFontSize::MODE_LENGTH)
			status = fontsize_object->font_size.SetLength(length);
		else if (fontsize_mode == SVGFontSize::MODE_ABSOLUTE)
			fontsize_object->font_size.SetAbsoluteFontSize(abs_fontsize);
		else if (fontsize_mode == SVGFontSize::MODE_RELATIVE)
			fontsize_object->font_size.SetRelativeFontSize(rel_fontsize);
#ifdef _DEBUG
		else
			OP_ASSERT(!"Not reached");
#endif // _DEBUG
    }

    return tokenizer.ReturnStatus(status);
}

OP_STATUS
SVGLengthParser::ParseCoordinatePair(const uni_char *input, unsigned input_length,
									 SVGLength &coordinate_x, SVGLength &coordinate_y)
{
    status = OpStatus::OK;
    tokenizer.Reset(input, input_length);

    ScanCoordinatePair(coordinate_x, coordinate_y);

    return tokenizer.ReturnStatus(status);
}

void
SVGLengthParser::ScanCoordinatePair(SVGLength &coordinate_x, SVGLength &coordinate_y)
{
    tokenizer.EatWsp();

    if (tokenizer.IsEnd())
	{
		status = OpStatus::ERR;
		return;
	}

    BOOL l1b = ScanLength(coordinate_x);
    tokenizer.EatWspSeparatorWsp(',');
    BOOL l2b = ScanLength(coordinate_y);
    tokenizer.EatWsp();

    if (!l1b || !l2b)
    {
		status = OpStatus::ERR;
    }
}

OP_STATUS
SVGLengthParser::ParseClipShape(const uni_char *input, unsigned input_length,
								SVGRectObject*& shape)
{
	status = OpStatus::OK;
	tokenizer.Reset(input, input_length);

	if (tokenizer.Scan("auto"))
	{
		shape = OP_NEW(SVGRectObject, ());
		if (!shape)
			return OpStatus::ERR_NO_MEMORY;
	}
	else if (tokenizer.Scan("rect("))
	{
		int i = 0;
		SVGLength lengths[4]; // This is the order from CSS2.1: top, right, bottom, left
		for (;i<4;i++)
		{
			if (ScanLength(lengths[i]))
			{
				/* empty */
			}
			else if (tokenizer.Scan("auto"))
			{
				lengths[i] = SVGLength(); // Zero length
			}
			else
			{
				break;
			}

			tokenizer.EatWspSeparatorWsp(',');
		}

		if (i == 4)
		{
			// Ignores unit, we have no datatype for a rectangle of
			// lengths.

			shape = OP_NEW(SVGRectObject, (SVGRect(lengths[0].GetScalar(),
											  lengths[1].GetScalar(),
											  lengths[2].GetScalar(),
											  lengths[3].GetScalar())));
			if (!shape)
				return OpStatus::ERR_NO_MEMORY;

			if (!tokenizer.Scan(')'))
				status = OpSVGStatus::ATTRIBUTE_ERROR;
		}
		else
		{
			return OpSVGStatus::ATTRIBUTE_ERROR;
		}
	}
	else
	{
		return OpSVGStatus::ATTRIBUTE_ERROR;
	}

	return OpStatus::OK;
}

#endif // SVG_SUPPORT
