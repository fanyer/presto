/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/opdata/OpDataUTF8.h"
#include "modules/opdata/OpDataFragmentIterator.h"
#include "modules/opdata/UniStringFragmentIterator.h"

/* static */
OP_STATUS UTF8OpData::ToUniString(const OpData& source, UniString& dest)
{
	UTF8Decoder decoder;
	OpDataFragmentIterator itr(source);
	size_t input_length = itr ? itr.GetLength() : 0;
	const char* data = itr.GetData();
	const size_t temp_len = 128;
	uni_char temp_buf[temp_len];	// ARRAY OK 2011-12-15 markuso
	size_t temp_written = 0;
	while (input_length)
	{
		int read = 0;
		int written = decoder.Convert(data, input_length,
									  temp_buf + temp_written,
									  (temp_len - temp_written) * sizeof(uni_char),
									  &read);
		if (written < 0)
			return OpStatus::ERR_NO_MEMORY;

		OP_ASSERT(written % sizeof(uni_char) == 0);
		size_t written_uni = static_cast<size_t>(written) / sizeof(uni_char);
		OP_ASSERT(written_uni <= (temp_len - temp_written));
		temp_written += written_uni;

		OP_ASSERT(0 <= read && static_cast<size_t>(read) <= input_length);
		input_length -= read;
		if (input_length)
			data += read;
		else if (++itr)
		{
			input_length = itr.GetLength();
			data = itr.GetData();
		}

		if (!input_length || //< all input data was handled
			// there was not enough space to write the next output character:
			(!read && !written) ||
			(temp_written == temp_len)) //< temp_buf is full
		{
			OP_ASSERT(temp_written > 0);
			RETURN_IF_ERROR(dest.AppendCopyData(temp_buf, temp_written));
			temp_written = 0;
		}
	}

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	if (!decoder.IsValidEndState())
		return OpStatus::ERR_OUT_OF_RANGE;
#endif // ENCODINGS_HAVE_CHECK_ENDSTATE
	return OpStatus::OK;
}


/* static */
OP_STATUS UTF8OpData::FromUniString(const UniString& source, OpData& dest)
{
	UTF8Encoder encoder;
	UniStringFragmentIterator itr(source);
	size_t input_length = itr ? itr.GetLength() : 0;
	const uni_char* data = itr.GetData();
	const size_t temp_len = 128;
	char temp_buf[temp_len];	// ARRAY OK 2011-12-15 markuso
	size_t temp_written = 0;
	while (input_length)
	{
		int read = 0;
		int written = encoder.Convert(data, input_length * sizeof(uni_char),
									  temp_buf + temp_written,
									  temp_len - temp_written, &read);
		if (written < 0)
			return OpStatus::ERR_NO_MEMORY;

		OP_ASSERT(static_cast<size_t>(written) <= (temp_len - temp_written));
		temp_written += written;

		OP_ASSERT(0 <= read &&
				  static_cast<size_t>(read) <= input_length * sizeof(uni_char));
		OP_ASSERT(read % sizeof(uni_char) == 0);
		size_t read_uni_char = read / sizeof(uni_char);
		input_length -= read_uni_char;
		if (input_length)
			data += read_uni_char;
		else if (++itr)
		{
			input_length = itr.GetLength();
			data = itr.GetData();
		}

		if (!input_length || //< all input data was handled
			// there was not enough space to write the next output character:
			(!read && !written) ||
			(temp_written == temp_len)) //< temp_buf is full
		{
			OP_ASSERT(temp_written > 0);
			RETURN_IF_ERROR(dest.AppendCopyData(temp_buf, temp_written));
			temp_written = 0;
		}
	}
	return OpStatus::OK;
}

/* static */
OP_BOOLEAN UTF8OpData::CompareWith(OpData utf8, UniString other)
{
	UTF8Decoder decoder;

	/* Convert some bit of the current OpData fragment to some UniString (in
	 * embedded mode, so we don't need to allocate memory for the fragments and
	 * chunks) and compare that to the start of other. If they are equal we
	 * continue, if they are different we can return without converting the
	 * rest. */
	UniString current;
	size_t append_length = OpData::EmbeddedDataLimit / sizeof(uni_char);
	uni_char* append = current.GetAppendPtr(append_length);
	RETURN_OOM_IF_NULL(append);

	OpDataFragmentIterator itr(utf8);
	size_t input_length = itr ? itr.GetLength() : 0;
	const char* data = itr.GetData();
	while (input_length)
	{
		int read = 0;
		int written = decoder.Convert(data, input_length,
									  static_cast<void*>(append),
									  append_length * sizeof(uni_char),
									  &read);
		if (written < 0)
			return OpStatus::ERR_NO_MEMORY;

		OP_ASSERT(read > 0 || written > 0);
		OP_ASSERT(0 <= read && static_cast<size_t>(read) <= input_length);
		input_length -= read;
		if (input_length)
			data += read;
		else if (++itr)
		{
			input_length = itr.GetLength();
			data = itr.GetData();
		}

		OP_ASSERT(written % sizeof(uni_char) == 0);
		size_t written_uni = static_cast<size_t>(written) / sizeof(uni_char);
		OP_ASSERT(written_uni <= append_length);
		if (written > 0)
		{
			if (other.StartsWith(current, static_cast<size_t>(written_uni)))
				other.Consume(static_cast<size_t>(written_uni));
			else
				return OpBoolean::IS_FALSE;
		}
	}

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	if (!decoder.IsValidEndState())	// could not convert full string
		return other.Equals(UNI_L("\xFFFD")) // NOT_A_CHARACTER?
			? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
#endif // ENCODINGS_HAVE_CHECK_ENDSTATE
	return other.IsEmpty() ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}
