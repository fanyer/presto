/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef URL_UPLOAD_BASE64_SUPPORT

#include "modules/dom/src/js/dombase64.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/formats/base64_decode.h"
#include "modules/logdoc/htm_lex.h"

/* static */ int
DOM_Base64::atob(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OBJECT);
	DOM_CHECK_ARGUMENTS("z");

	const uni_char *arg_str = argv[0].value.string_with_length->string;
	unsigned arg_length = argv[0].value.string_with_length->length;

	unsigned char *value_buffer = OP_NEWA(unsigned char, arg_length + 4);
	if (!value_buffer)
		return ES_NO_MEMORY;

	ANCHOR_ARRAY(unsigned char, value_buffer);

	unsigned value_length = 0;
	const uni_char *input = arg_str;
	unsigned char *input8 = value_buffer;

	/* Copy down the relevant characters, raising an exception if a non-octet. */
	while (arg_length > 0)
	{
		if ((*input) < 0x80 && IsHTML5Space(*input))
			input++;
		else if ((*input & ~0xff) != 0)
			return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);
		else
		{
			value_length++;
			*input8++ = static_cast<unsigned char>(*input++);
		}
		arg_length--;
	}
	*input8 = 0;

	switch(value_length % 4)
	{
	case 0:
		if (value_length == 0)
		{
			DOM_Object::DOMSetString(return_value);
			return ES_VALUE;
		}
		/* Check if at most two trailing '=' characters. */
		else if (value_buffer[value_length - 1] == '=' && value_buffer[value_length - 2] == '=')
			if (value_buffer[value_length - 3] == '=')
				return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);
		break;

	case 1:
		return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);

	case 2:
	case 3:
		/* Illegal if a trailing '=', as it is not a (mod 4) padding character.
		   If not a '=', add as padding to prepare for decoding step. */
		if (value_buffer[value_length - 1] == '=')
			return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);
		else
			while (value_length % 4)
				value_buffer[value_length++] = '=';
		break;
	};
	OP_ASSERT(value_length % 4 == 0);
	value_buffer[value_length] = 0;

	TempBuffer *buf = DOM_Object::GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(buf->Expand((value_length / 4) * 3 + 4));
	uni_char *result_str = buf->GetStorage();

	unsigned long readpos;
	BOOL warning = FALSE;

	unsigned result_length = GeneralDecodeBase64(value_buffer, value_length, readpos, reinterpret_cast<unsigned char *>(result_str), warning, 0, NULL, NULL);
	if (warning)
		return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);

	make_doublebyte_in_place(result_str, result_length);
	DOM_Object::DOMSetStringWithLength(return_value, &(g_DOM_globalData->string_with_length_holder), result_str, result_length);
	return ES_VALUE;
}

/* static */ int
DOM_Base64::btoa(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OBJECT);
	DOM_CHECK_ARGUMENTS("z");

	unsigned arg_length = 0;
	const uni_char *arg_str = NULL;

	arg_length = argv[0].value.string_with_length->length;
	arg_str = argv[0].value.string_with_length->string;

	if (arg_length == 0)
	{
		DOM_Object::DOMSetString(return_value);
		return ES_VALUE;
	}

	/* Number of 6-bit digits in the output. */
	unsigned result_length = ((arg_length + 2) / 3) * 4;
	if (result_length < arg_length)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	TempBuffer *buf = DOM_Object::GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(buf->Expand(result_length + 1));
	uni_char *result_str = buf->GetStorage();

	char *arg8_str = OP_NEWA(char, arg_length + 1);
	if (!arg8_str)
		return ES_NO_MEMORY;

	ANCHOR_ARRAY(char, arg8_str);
	for (unsigned k = 0; k < arg_length; k++)
		if ((arg_str[k] & ~0xff) != 0)
			return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);
		else
			arg8_str[k] = static_cast<char>(arg_str[k]);

	arg8_str[arg_length] = 0;

	char *b64_str = NULL;
	int b64_length = 0;

	MIME_Encode_Error base64_encoding_status = MIME_Encode_SetStr(b64_str, b64_length, arg8_str, arg_length, NULL, GEN_BASE64_ONELINE);
	if (base64_encoding_status != MIME_NO_ERROR)
		return ES_NO_MEMORY;

	OP_ASSERT(b64_length == static_cast<int>(result_length));

	op_memcpy(result_str, b64_str, b64_length);
	OP_DELETEA(b64_str);

	make_doublebyte_in_place(result_str, result_length);
	result_str[result_length] = 0;

	DOM_Object::DOMSetString(return_value, buf);
	return ES_VALUE;
}

#endif // URL_UPLOAD_BASE64_SUPPORT
