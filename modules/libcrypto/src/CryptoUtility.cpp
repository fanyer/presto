/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/libcrypto/include/CryptoUtility.h"
#include "modules/formats/encoder.h"
#include "modules/formats/base64_decode.h"

OP_STATUS CryptoUtility::ConvertToBase64(const UINT8 *source, int source_length, OpString8 &base_64)
{
	if (!source)
		return OpStatus::ERR;

	char *blob_base64 = NULL;
	int blob_base64_length;
	MIME_Encode_Error base64_encoding_status =
		MIME_Encode_SetStr(blob_base64, blob_base64_length, reinterpret_cast<const char*>(source),
		                   source_length, NULL, GEN_BASE64_ONELINE);

	RETURN_OOM_IF_NULL(blob_base64);

	if (base64_encoding_status != MIME_NO_ERROR)
		return OpStatus::ERR;

	base_64.TakeOver(blob_base64);
	return OpStatus::OK;
}

OP_STATUS CryptoUtility::ConvertFromBase64(const char *source, UINT8 *&binary, int& length)
{
	if (!source)
		return OpStatus::ERR;

	int source_length = op_strlen(source);
	binary = OP_NEWA(unsigned char, ((source_length+3)/4)*3);
	if (!binary)
		return OpStatus::ERR_NO_MEMORY;

	unsigned long read_pos;
	BOOL warning;
	length = GeneralDecodeBase64(reinterpret_cast<const unsigned char*>(source), source_length, read_pos,
		                    binary, warning);
	if (warning)
	{
		OP_DELETEA(binary);
		binary = NULL;
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}
