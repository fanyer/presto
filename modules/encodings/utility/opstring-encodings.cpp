/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/encodings/utility/opstring-encodings.h"
#include "modules/encodings/charconverter.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/encodings/tablemanager/optablemanager.h"

int SetToEncodingL(OpString8 *target, const char *aEncoding, const uni_char *aUTF16str, int aLength)
{
	if (NULL == aUTF16str || 0 == aLength)
	{
		target->Empty();
		return 0;
	}

	OutputConverter *out;
	LEAVE_IF_ERROR(OutputConverter::CreateCharConverter(aEncoding, &out));
	OpStackAutoPtr<OutputConverter> encoder(out);

	return SetToEncodingL(target, encoder.get(), aUTF16str, aLength);
}

int SetToEncodingL(OpString8 *target, OutputConverter *aEncoder, const uni_char *aUTF16str, int aLength)
{
	target->Empty();

	if (NULL == aUTF16str || 0 == aLength)
		return 0;

	// Convert the string in chunks, since we cannot determine the size of the
	// output string in advance

	char buffer[OPSTRING_ENCODE_WORKBUFFER_SIZE]; /* ARRAY OK 2011-08-22 peter */
	const char *input = reinterpret_cast<const char *>(aUTF16str);
	const size_t givenLength = uni_strlen(aUTF16str);
	int input_length = UNICODE_SIZE((aLength == KAll || aLength > static_cast<int>(givenLength)) ? static_cast<int>(givenLength) : aLength);
	int processed_bytes;
	int written;

	int current_pos = 0;
	double last_size_factor_used = 0;

	while (input_length > 0)
	{
		written = aEncoder->Convert(input, input_length,
		                            buffer, OPSTRING_ENCODE_WORKBUFFER_SIZE,
		                            &processed_bytes);
		if (written == -1)
			LEAVE(OpStatus::ERR_NO_MEMORY);

		// Stop converting if the last character in the input stream cannot be converted
		if (written == 0 && processed_bytes == input_length)
			break;

		double size_factor = static_cast<double>(written) / processed_bytes;
		if (size_factor > last_size_factor_used)
		{
			int new_length = current_pos + static_cast<int>(op_ceil(input_length * size_factor));

			LEAVE_IF_ERROR(target->Grow(new_length));

			last_size_factor_used = size_factor;
		}

		op_strncpy(target->iBuffer + current_pos, buffer, written);

		current_pos += written;

		input += processed_bytes;
		input_length -= processed_bytes;

		target->iBuffer[current_pos] = 0;
	}

	// Make string self-contained
	written = aEncoder->ReturnToInitialState(NULL);
	if (written > OPSTRING_ENCODE_WORKBUFFER_SIZE)
		return aEncoder->GetNumberOfInvalid();

	aEncoder->ReturnToInitialState(buffer);

	if (written)
		target->AppendL(buffer, written);

	return aEncoder->GetNumberOfInvalid();
}

OP_STATUS SetFromEncoding(OpString16 *target, const char *aEncoding, const void *aEncodedStr, int aByteLength, int* aInvalidInputs)
{
	if (NULL == aEncodedStr)
	{
		target->Empty();
		if (aInvalidInputs)
			*aInvalidInputs = 0;
		return OpStatus::OK;
	}

	InputConverter *in;
	RETURN_IF_ERROR(InputConverter::CreateCharConverter(aEncoding, &in));
	OpAutoPtr<InputConverter> decoder(in);

	return SetFromEncoding(target, decoder.get(), aEncodedStr, aByteLength, aInvalidInputs);
}

int SetFromEncodingL(OpString16 *target, const char *aEncoding, const void *aEncodedStr, int aByteLength)
{
	int invalid_inputs;
	LEAVE_IF_ERROR(SetFromEncoding(target, aEncoding, aEncodedStr, aByteLength, &invalid_inputs));
	return invalid_inputs;
}

OP_STATUS SetFromEncoding(OpString16 *target, InputConverter *aDecoder, const void *aEncodedStr, int aByteLength, int* aInvalidInputs)
{
	target->Empty();

	if (NULL == aEncodedStr)
	{
		if (aInvalidInputs)
			*aInvalidInputs = 0;
		return OpStatus::OK;
	}

	// Convert the string in chunks, since we cannot determine the size of the
	// output string in advance

	uni_char buffer[UNICODE_DOWNSIZE(OPSTRING_ENCODE_WORKBUFFER_SIZE)]; /* ARRAY OK 2011-08-22 peter */
	const char *input = reinterpret_cast<const char *>(aEncodedStr);
	int processed_bytes;
	int written;

	int current_length = target->Length();

	while (aByteLength > 0)
	{
		int new_length = current_length;
		written = aDecoder->Convert(input, aByteLength,
									buffer, OPSTRING_ENCODE_WORKBUFFER_SIZE,
		 &processed_bytes);
		if (written == -1)
			return OpStatus::ERR_NO_MEMORY;

		if (written == 0 && processed_bytes == 0)
			break;

		new_length += UNICODE_DOWNSIZE(written);
		if (new_length + 1 > target->Capacity())
		{
			RETURN_IF_ERROR(target->Grow(2*new_length));
		}

		uni_char* t = target->iBuffer + current_length;
		const uni_char* s = buffer;
		for (int i=0; i < UNICODE_DOWNSIZE(written) && *s; ++i)
		{
			*t++ = *s++;
		}
		*t = 0;

		current_length = new_length;
		input += processed_bytes;
		aByteLength -= processed_bytes;
	}

	if (aInvalidInputs)
		*aInvalidInputs = aDecoder->GetNumberOfInvalid();

	return OpStatus::OK;
}

int SetFromEncodingL(OpString16 *target, InputConverter *aDecoder, const void *aEncodedStr, int aByteLength)
{
	int invalid_inputs;
	LEAVE_IF_ERROR(SetFromEncoding(target, aDecoder, aEncodedStr, aByteLength, &invalid_inputs));
	return invalid_inputs;
}

