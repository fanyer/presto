// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef INPUT_CONVERTER_MANAGER_H
#define INPUT_CONVERTER_MANAGER_H

#include "adjunct/desktop_util/thread/DesktopMutex.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/util/OpHashTable.h"

class InputConverterManager
{
public:
	/** Initialize this object. Run before using functions
	  */
	OP_STATUS Init() { return m_mutex.Init(); }

	/** Convert (decode) a 8-bit string with the specified charset to utf-16
	  *
	  * This API is broken in so many ways (see discussion in DSK-270822)
	  *
	  * @param charset Character set to decode
	  * @param source String to decode
	  * @param dest Where to put decoded string
	  * @param accept_illegal_characters Whether to accept characters that are illegal in this charset
	  * @param append Whether to append to dest (will overwrite if FALSE)
	  * @param length How much of source to convert, use KAll to convert the whole string
	  */
	OP_STATUS ConvertToChar16(const OpStringC8& charset,
							  const OpStringC8& source,
							  OpString16& dest,
							  BOOL accept_illegal_characters = FALSE,
							  BOOL append = FALSE,
							  int length = KAll);


private:
	struct CharsetMap
	{
		OpString8		charset;
		InputConverter* converter;

		CharsetMap() : converter(0) {}
		~CharsetMap() { OP_DELETE(converter); }
	};

	OP_STATUS ConvertToChar16(InputConverter* converter, const OpStringC8& source, OpString16& dest, BOOL append, int length);
	OP_STATUS GetConverter(const OpStringC8& charset, InputConverter*& converter);

	OpAutoString8HashTable<CharsetMap>	m_charset_map;
	DesktopMutex						m_mutex;
};

#endif // INPUT_CONVERTER_MANAGER_H
