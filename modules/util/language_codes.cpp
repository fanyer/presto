/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/util/language_codes.h"

#include "modules/util/language_code_conversions.inc"

OP_STATUS LanguageCodeUtils::ConvertTo3LetterCode(OpString8 * output, const char * input)
{
	int code_length = 0;
	int input_len = op_strlen(input);
	for (; code_length < input_len && op_isalpha(input[code_length]); ++code_length) {}
	OP_ASSERT(code_length == 2 || code_length == 3); // what language code is that?
	if (code_length == 3)
	{
		return output->Set(input, 3);
	}
	else if (code_length == 2)
	{
		char two_letter_code[3] = {0};
		op_memcpy(two_letter_code, input, 2* sizeof(two_letter_code[0]));
		return Convert2To3LetterCode(output, two_letter_code);
	}
	else
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}
}
/*static */
OP_STATUS LanguageCodeUtils::ConvertTo3LetterCode(OpString * output, const uni_char * input)
{
	OpString8 utf8_output, utf8_input;
	RETURN_IF_ERROR(utf8_input.SetUTF8FromUTF16(input));
	RETURN_IF_ERROR(ConvertTo3LetterCode(&utf8_output, utf8_input.CStr()));
	return output->SetFromUTF8(utf8_output.CStr());
}
