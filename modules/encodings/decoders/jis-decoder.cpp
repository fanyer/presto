/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_JAPANESE

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/decoders/jis-decoder.h"

JIStoUTF16Converter::JIStoUTF16Converter()
	: m_jis_0208_table(NULL), m_jis0208_length(0),
	  m_prev_byte(0)
{
}

OP_STATUS JIStoUTF16Converter::Construct()
{
	if (OpStatus::IsError(this->InputConverter::Construct())) return OpStatus::ERR;

	m_jis_0208_table =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("jis-0208", m_jis0208_length));
	m_jis0208_length /= 2; // convert byte count to ushort count

	return m_jis_0208_table ? OpStatus::OK : OpStatus::ERR;
}

JIStoUTF16Converter::~JIStoUTF16Converter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_jis_0208_table);
	}
}

void JIStoUTF16Converter::Reset()
{
	this->InputConverter::Reset();
	m_prev_byte = 0;
}

JIS0212toUTF16Converter::JIS0212toUTF16Converter()
	: JIStoUTF16Converter(),
	  m_jis_0212_table(NULL), m_jis0212_length(0)
{
}

OP_STATUS JIS0212toUTF16Converter::Construct()
{
	if (OpStatus::IsError(this->JIStoUTF16Converter::Construct())) return OpStatus::ERR;

	m_jis_0212_table =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("jis-0212", m_jis0212_length));
	m_jis0212_length /= 2; // convert byte count to ushort count

	return m_jis_0212_table ? OpStatus::OK : OpStatus::ERR;
}

JIS0212toUTF16Converter::~JIS0212toUTF16Converter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_jis_0212_table);
	}
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_JAPANESE
