/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef ENCODINGS_HAVE_TABLE_DRIVEN

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/encoders/encoder-utility.h"
#include "modules/encodings/encoders/sbcs-encoder.h"
#ifndef ENCODINGS_REGTEST
# include "modules/util/handy.h" // ARRAY_SIZE
#endif

/**
 * Perform binary lookup in a compressed Unicode-to-SBCS table
 * @param character  Unicode character to look up
 * @return SBCS character (zero if none found)
 */
char UTF16toSBCSConverter::lookup(uni_char character)
{
	// Look up
	const unsigned char *found =
		reinterpret_cast<const unsigned char *>(op_bsearch(&character, m_maptable, m_tablelen / 3, 3, unichar_compare));

	// Return found value, or zero if none was found
	return found ? found[2] : 0;
}

UTF16toSBCSConverter::UTF16toSBCSConverter(const char *charset, const char *tablename)
	: UTF16toISOLatin1Converter(TRUE),
	  m_maptable(NULL),
	  m_tablelen(0)
{
	op_strlcpy(m_charset, charset, ARRAY_SIZE(m_charset));
	op_strlcpy(m_tablename, tablename, ARRAY_SIZE(m_tablename));
}

OP_STATUS UTF16toSBCSConverter::Construct()
{
	if (OpStatus::IsError(this->UTF16toISOLatin1Converter::Construct())) return OpStatus::ERR;

	m_maptable =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get(m_tablename, m_tablelen));

	return m_maptable ? OpStatus::OK : OpStatus::ERR;
}

UTF16toSBCSConverter::~UTF16toSBCSConverter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_maptable);
	}
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN
