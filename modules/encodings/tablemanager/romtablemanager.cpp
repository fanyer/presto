/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if (defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_ROM_TABLES) || defined DOXYGEN_DOCUMENTATION

#include "modules/encodings/tablemanager/romtablemanager.h"

RomTableManager::RomTableManager()
: TableCacheManager()
{
}

OP_STATUS RomTableManager::Load()
{
	const unsigned int * const encodingtables = g_encodingtables();

	UINT16 magic = *(reinterpret_cast<const UINT16 *>(encodingtables));
#ifdef ENCODINGS_OPPOSITE_ENDIAN
	magic = ENCDATA16(magic);
#endif

	if (magic == 0xFE01) //Magic number for what is considered version 1 of encoding.bin
	{
		UINT16 headers_size = *(reinterpret_cast<const UINT16 *>(encodingtables) + 1);
#ifdef ENCODINGS_OPPOSITE_ENDIAN
		headers_size = ENCDATA16(headers_size);
#endif

		RETURN_IF_ERROR(TableCacheManager::ParseRawHeaders(reinterpret_cast<const UINT8 *>(encodingtables), headers_size, FALSE));
	}
	else
	{
		OP_ASSERT(!"Unknown encoding.bin file format (wrong endian?)");
	}

	return OpStatus::OK;
}

/**
 * Load a data table.
 *
 * @param index The number of the table to load.
 * @return Status of the operation.
 */
OP_STATUS RomTableManager::LoadRawTable(int index)
{
	TableDescriptor *table = GetTableDescriptor(index);
	if (!table)
		return OpStatus::ERR;

#if defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
	OP_ASSERT(0 == table->ref_count);
	table->table_was_allocated = FALSE;
#endif
	table->table = reinterpret_cast<const UINT8 *>(g_encodingtables()) + table->start_offset;
	return OpStatus::OK;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_ROM_TABLES
