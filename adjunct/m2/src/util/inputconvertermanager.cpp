/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/util/inputconvertermanager.h"


/***********************************************************************************
 ** Convert (decode) a 8-bit string with the specified charset to utf-16
 **
 ** InputConverterManager::ConvertToChar16
 ***********************************************************************************/
OP_STATUS InputConverterManager::ConvertToChar16(const OpStringC8& charset,
												 const OpStringC8& source,
												 OpString16& dest,
												 BOOL accept_illegal_characters,
												 BOOL append,
												 int length)
{
	// Lockdown, getting and using the converters is not thread-safe
	DesktopMutexLock lock(m_mutex);

	// Get correct converter
	InputConverter* conv;
	RETURN_IF_ERROR(GetConverter(charset, conv));

	// Start converting
	int existing_length = append ? dest.Length() : 0;

	// Actual conversion
	OP_STATUS ret = ConvertToChar16(conv, source, dest, append, length);

	// Check if the conversion went ok, and if not, if we can try another one
	if ((OpStatus::IsError(ret) || (conv->GetNumberOfInvalid() > 0 && !accept_illegal_characters)) &&
		!strni_eq(conv->GetCharacterSet(), "ISO-8859-1", 10))
	{
		// Not satisfactory, try with a iso-8859-1 input converter instead
		// Remove any appended strings
		if (dest.HasContent())
			dest[existing_length] = 0;

		return ConvertToChar16("iso-8859-1", source, dest, TRUE, append, length);
	}

	return ret;
}


/***********************************************************************************
 ** Convert with known input converter
 **
 ** InputConverterManager::ConvertToChar16
 ***********************************************************************************/
OP_STATUS InputConverterManager::ConvertToChar16(InputConverter* converter, const OpStringC8& source, OpString16& dest, BOOL append, int length)
{
	if (!append)
		dest.Empty();

	int         src_len = length == KAll ? source.Length() : length;
	int         read = 0;
	int         total_written = UNICODE_SIZE(dest.Length());
	const char* src = source.CStr();
	uni_char*   dest_ptr;

	while (src_len > 0)
	{
		dest_ptr = dest.Reserve(UNICODE_DOWNSIZE(total_written) + src_len);
		if (!dest_ptr)
			return OpStatus::ERR_NO_MEMORY;

		total_written += converter->Convert(src, src_len, ((char*)dest_ptr) + total_written, UNICODE_SIZE(dest.Capacity()) - total_written, &read);

		if (!read) //We have data in the buffer that will not be converted. For now, we bail out with an OK
			break;

		src += read;
		src_len -= read;
		dest_ptr[UNICODE_DOWNSIZE(total_written)] = 0; // NULL-termination
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get a converter
 **
 ** InputConverterManager::GetConverter
 ** @param charset Charset to find
 ** @param input_converter Where to store the converter
 ***********************************************************************************/
OP_STATUS InputConverterManager::GetConverter(const OpStringC8& charset, InputConverter*& input_converter)
{
	// If we requested an empty charset, use iso-8859-1 instead
	if (charset.IsEmpty())
		return GetConverter("iso-8859-1", input_converter);

	CharsetMap* map;

	// Check if the converter is already there
	if (OpStatus::IsSuccess(m_charset_map.GetData(charset.CStr(), &map)))
	{
		input_converter = map->converter;
		input_converter->Reset();
		return OpStatus::OK;
	}

	// We don't have the converter, create a new converter
	map = OP_NEW(CharsetMap, ());
	OpAutoPtr<CharsetMap> new_map(map);
	if (!map)
		return OpStatus::ERR_NO_MEMORY;

	// If we can't find the converter, use iso-8859-1
#ifdef ENCODINGS_CAP_MUST_ALLOW_UTF7
	if (OpStatus::IsError(InputConverter::CreateCharConverter(charset.CStr(), &map->converter, TRUE)))
#else
	if (OpStatus::IsError(InputConverter::CreateCharConverter(charset.CStr(), &map->converter)))
#endif // ENCODINGS_CAP_MUST_ALLOW_UTF7
		return GetConverter("iso-8859-1", input_converter);

	RETURN_IF_ERROR(map->charset.Set(charset));

	// Insert charset-map into the map hashtable so we can find it later
	RETURN_IF_ERROR(m_charset_map.Add(map->charset.CStr(), map));
	new_map.release();

	// Return the input converter
	input_converter = map->converter;
	return OpStatus::OK;
}


#endif // M2_SUPPORT
