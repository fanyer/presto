/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/encodings/encodings_module.h"
#include "modules/encodings/tablemanager/filetablemanager.h"
#include "modules/encodings/tablemanager/romtablemanager.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/encodings/decoders/big5hkscs-decoder.h"

EncodingsModule::EncodingsModule()
	: m_charset_manager(NULL)
#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	, m_table_manager(NULL)
# ifdef ENC_BIG_HKSCS_TABLE
	, m_hkscs_table(NULL)
# endif
#endif
#ifdef SELFTEST
	, buffer(NULL), bufferback(NULL)
#endif
{
}

void EncodingsModule::InitL(const OperaInitInfo &)
{
	// Create the table manager, if we have one

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
# ifdef ENCODINGS_HAVE_ROM_TABLES
	// Use ROMable implementation
	m_table_manager = OP_NEW_L(RomTableManager, ());
# elif defined ENCODINGS_HAVE_TABLE_DRIVEN
	// Use standard file-based implementation
	m_table_manager = OP_NEW_L(FileTableManager, ());
# endif

	m_table_manager->ConstructL();

	// Load the file, but only fail on memory errors
	OP_STATUS rc = m_table_manager->Load();
	if (OpStatus::IsMemoryError(rc))
		LEAVE(rc);
	OP_ASSERT(OpStatus::IsSuccess(rc) || !"encoding.bin not found");
#endif // ENCODINGS_HAVE_TABLE_DRIVEN

	// The CharsetManager is always required
	m_charset_manager = OP_NEW_L(CharsetManager, ());
	m_charset_manager->ConstructL();
}

void EncodingsModule::Destroy()
{
	OP_DELETE(m_charset_manager);
	m_charset_manager = NULL;

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	OP_DELETE(m_table_manager);
	m_table_manager = NULL;
#endif

#ifdef ENC_BIG_HKSCS_TABLE
	OP_DELETEA(m_hkscs_table);
	m_hkscs_table = NULL;
#endif
}

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
BOOL EncodingsModule::FreeCachedData(BOOL)
{
	BOOL tried_free = FALSE;

#ifdef ENC_BIG_HKSCS_TABLE
	// Deallocate the generated Big5-HKSCS table. It will be regenerated the
	// next time we encounter a Big5-HKSCS page.
	if (m_hkscs_table)
	{
		OP_DELETEA(m_hkscs_table);
		m_hkscs_table = NULL;
		tried_free = TRUE;
	}
#endif

#ifdef OUT_OF_MEMORY_POLLING
	m_table_manager->Flush();
	tried_free = TRUE;
#endif

	return tried_free;
}
#endif
