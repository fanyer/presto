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
#include "modules/svg/src/parser/svgnumberparser.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGPoint.h"

OP_STATUS
SVGNumberParser::ParseNumber(const uni_char *input, unsigned str_len, BOOL normalize_percentage, SVGNumber& num)
{
	tokenizer.Reset(input, str_len);
	tokenizer.EatWsp();
	double d;
	if (tokenizer.ScanNumber(d))
	{
		if (normalize_percentage && tokenizer.Scan('%'))
			d /= 100.0;

		num = d;
		return OpStatus::OK;
	}
	else
	{
		return OpSVGStatus::ATTRIBUTE_ERROR;
	}
}

OP_STATUS SVGNumberParser::ParseUnicodeRange(const uni_char* input, unsigned str_len,
											 unsigned& uc_start, unsigned& uc_end)
{
	tokenizer.Reset(input, str_len);
	tokenizer.EatWsp();

	if (ScanUnicodeRange(uc_start, uc_end))
		return tokenizer.ReturnStatus(OpStatus::OK);

	return OpSVGStatus::ATTRIBUTE_ERROR;
}

BOOL
SVGNumberParser::ScanUnicodeRange(unsigned& uc_start, unsigned& uc_end)
{
	if (!tokenizer.Scan("U+"))
		return FALSE;

	uc_start = 0;
	if (tokenizer.ScanHexDigits(uc_start) == 0)
		return FALSE;

	uc_end = uc_start;

	while (tokenizer.Scan('?'))
	{
		// Implicit range (assumed to be suffix, and only suffix - the
		// CSS spec. doesn't really mention anything about it)
		uc_start <<= 4;
		uc_end = (uc_end << 4) | 0xF;
	}

	if (uc_start == uc_end)
	{
		if (tokenizer.Scan('-'))
		{
			// Explicit range
			uc_end = 0;
			if (tokenizer.ScanHexDigits(uc_end) == 0)
				return FALSE;
		}
	}

	OP_ASSERT(uc_start <= uc_end);
	return TRUE;
}

/**
 * This number list parser does not have mandatory separators.
 */

OP_STATUS
SVGNumberParser::ParseNumberList(const uni_char *input, unsigned str_len, uni_char separator, SVGVector &vector)
{
	tokenizer.Reset(input, str_len);
	OP_STATUS status = OpStatus::OK;
	tokenizer.EatWsp();
	while(!tokenizer.IsEnd() && OpStatus::IsSuccess(status))
	{
		double d;
		if (!tokenizer.ScanNumber(d))
		{
			status = OpSVGStatus::ATTRIBUTE_ERROR;
		}
		else
		{
			SVGNumber num(d);

			SVGNumberObject *num_val = OP_NEW(SVGNumberObject, (num));
			if (!num_val)
			{
				status = OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				status = vector.Append(num_val);
				if(OpStatus::IsError(status))
				{
					OP_DELETE(num_val);
				}
			}

			// This allows a separator between elements and at the end
			tokenizer.EatWspSeparatorWsp((char)separator);
		}
	}

	return status;
}

OP_STATUS
SVGNumberParser::ParsePointList(const uni_char *input, unsigned str_len, SVGVector &vector)
{
	tokenizer.Reset(input, str_len);
	OP_STATUS status = OpStatus::OK;

	tokenizer.EatWsp();
	while(!tokenizer.IsEnd() && OpStatus::IsSuccess(status))
	{
		double n1, n2 = 0;
		BOOL nums = tokenizer.ScanNumber(n1);
		tokenizer.EatWspSeparatorWsp(',');
		if (nums)
			nums = tokenizer.ScanNumber(n2);
		tokenizer.EatWspSeparatorWsp(',');

		if (!nums)
		{
			status = OpSVGStatus::ATTRIBUTE_ERROR;
		}
		else
		{

			SVGPoint *point = OP_NEW(SVGPoint, (n1, n2));
			if (!point)
			{
				status = OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				status = vector.Append(point);
				if(OpStatus::IsError(status))
				{
					OP_DELETE(point);
				}
			}
		}
	}
	return status;
}

OP_STATUS
SVGNumberParser::ParseNumberCommaList(const uni_char *input, unsigned str_len, SVGVector &vector)
{
	return ParseNumberList(input, str_len, ',', vector);
}

OP_STATUS
SVGNumberParser::ParseNumberSemicolonList(const uni_char *input, unsigned str_len, SVGVector &vector)
{
	return ParseNumberList(input, str_len, ';', vector);
}

#endif // SVG_SUPPORT
