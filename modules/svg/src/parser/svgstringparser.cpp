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
#include "modules/svg/src/parser/svgstringparser.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/xmlutils/xmlutils.h"

OP_STATUS
SVGStringParser::ParseStringList(const uni_char *input, unsigned str_len, SVGTokenizer::StringRules& rules, SVGVector &vector)
{
	status = OpStatus::OK;
	tokenizer.Reset(input, str_len);
	OP_STATUS status = OpStatus::OK;
	tokenizer.EatWsp();

	while(OpStatus::IsSuccess(status))
	{
		const uni_char *sub_string;
		unsigned sub_string_length;
		if (tokenizer.ScanString(rules, sub_string, sub_string_length))
		{
			SVGString* string_object = OP_NEW(SVGString, ());
			if (!string_object)
			{
				status = OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				status = string_object->SetString(sub_string, sub_string_length);
				if (OpStatus::IsSuccess(status))
				{
					status = vector.Append(string_object);
				}
				
				if(OpStatus::IsError(status))
				{
					OP_DELETE(string_object);
				}
			}
		}
		else
			break;
	}

	return tokenizer.ReturnStatus(status);
}

OP_STATUS
SVGStringParser::ParseStringCommaList(const uni_char *input, unsigned str_len, SVGVector &vector)
{
	SVGTokenizer::StringRules rules;
	rules.Init(/* allow_quoting = */ TRUE,
			   /* wsp_separator = */ TRUE,
			   /* comma_separator = */ TRUE,
			   /* semicolon_separator = */ FALSE,
			   /* end_condition = */ '\0');

	return ParseStringList(input, str_len, rules, vector);
}

OP_STATUS
SVGStringParser::ParseStringSemicolonList(const uni_char *input, unsigned str_len, SVGVector &vector)
{
	SVGTokenizer::StringRules rules;
	rules.Init(/* allow_quoting = */ TRUE,
			   /* wsp_separator = */ FALSE,
			   /* comma_separator = */ FALSE,
			   /* semicolon_separator = */ TRUE,
			   /* end_condition = */ '\0');

	return ParseStringList(input, str_len, rules, vector);
}

OP_STATUS
SVGStringParser::ParseFontFamilyList(const uni_char *input, unsigned str_len, SVGVector &vector)
{
	SVGTokenizer::StringRules rules;
	rules.Init(/* allow_quoting = */ TRUE,
			   /* wsp_separator = */ FALSE,
			   /* comma_separator = */ TRUE,
			   /* semicolon_separator = */ FALSE,
			   /* end_condition = */ '\0');

	return ParseStringList(input, str_len, rules, vector);
}
#if 0
OP_STATUS
SVGStringParser::ParseQuotedString(const uni_char *input, unsigned input_length,
								   const uni_char *&string, unsigned& string_length)
{
	SVGTokenizer::StringRules rules;
	rules.Init(/* allow_quoting = */ TRUE,
			   /* wsp_separator = */ FALSE,
			   /* comma_separator = */ FALSE,
			   /* semicolon_separator = */ FALSE,
			   /* end_condition = */ '\0');

	status = OpStatus::OK;
	tokenizer.Reset(input, input_length);
	if (tokenizer.ScanString(rules, string, string_length))
		return OpStatus::OK;
	else
		return OpSVGStatus::ATTRIBUTE_ERROR;
}
#endif // 0
#endif // SVG_SUPPORT
