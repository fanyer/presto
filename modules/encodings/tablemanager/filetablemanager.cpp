/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if (defined ENCODINGS_HAVE_TABLE_DRIVEN && !defined ENCODINGS_HAVE_ROM_TABLES) || defined DOXYGEN_DOCUMENTATION

#include "modules/encodings/tablemanager/filetablemanager.h"


#ifndef ENCODINGS_REGTEST
# include "modules/hardcore/mem/mem_man.h"
# include "modules/util/opfile/opfile.h"
# include "modules/pi/OpSystemInfo.h"
#endif

// ===== Module implementation

FileTableManager::FileTableManager()
: TableCacheManager(),
  m_table_file(NULL),
  m_no_file(TRUE)
{
}

FileTableManager::~FileTableManager()
{
	OP_DELETE(m_table_file);
}

void FileTableManager::ConstructL()
{
	m_table_file = OP_NEW_L(OpFile, ());
	OP_STATUS err = m_table_file->Construct(ENC_TABLE_FILE_NAME, OPFILE_RESOURCES_FOLDER);
	if (OpStatus::IsError(err))
	{
		OP_DELETE(m_table_file);
		LEAVE(err);
	}
}

OP_STATUS FileTableManager::Load()
{
#ifndef ENCODINGS_REGTEST
	if (m_table_file)
	{
		RETURN_IF_ERROR(m_table_file->Open(OPFILE_READ));
	}

	if (!m_table_file || !m_table_file->IsOpen())
	{
		// Couldn't load table, for whatever reason
		return OpStatus::ERR_FILE_NOT_FOUND;
	}
#endif // !ENCODINGS_REGTEST

	UINT16 magic;
	OpFileLength bytes_read;
	RETURN_IF_ERROR(m_table_file->Read(&magic, 2, &bytes_read)); //magic is 2 bytes
	if (bytes_read != 2)
		return OpStatus::ERR;
#ifdef ENCODINGS_OPPOSITE_ENDIAN
	magic = ENCDATA16(magic);
#endif

	if (magic == 0xFE01) //Magic number for what is considered version 1 of encoding.bin
	{
		UINT16 headers_size;
		RETURN_IF_ERROR(m_table_file->Read(&headers_size, 2, &bytes_read)); //headers_size is 2 bytes
		if (bytes_read != 2)
			return OpStatus::ERR;
#ifdef ENCODINGS_OPPOSITE_ENDIAN
		headers_size = ENCDATA16(headers_size);
#endif

		UINT8 *encoding_headers = OP_NEWA(UINT8, headers_size);
		if (!encoding_headers)
			return OpStatus::ERR_NO_MEMORY;

		//initialize the first 4 bytes, we have already read them from file
#ifdef ENCODINGS_OPPOSITE_ENDIAN
		*(reinterpret_cast<UINT16*>(encoding_headers)) = ENCDATA16(magic);
		*(reinterpret_cast<UINT16*>(encoding_headers+2)) = ENCDATA16(headers_size);
#else
		*(reinterpret_cast<UINT16*>(encoding_headers)) = magic;
		*(reinterpret_cast<UINT16*>(encoding_headers+2)) = headers_size;
#endif

		OP_STATUS s = m_table_file->Read(encoding_headers+4, headers_size-4, &bytes_read); // Read the remaining headers bytes
		if (OpStatus::IsSuccess(s) && bytes_read != static_cast<OpFileLength>(headers_size-4))
			s = OpStatus::ERR;
		if (OpStatus::IsError(s)) {
			OP_DELETEA(encoding_headers);
			return s;
		}

		RETURN_IF_ERROR(TableCacheManager::ParseRawHeaders(encoding_headers, headers_size, TRUE)); // Transfers ownership of encoding_headers
	}
	else
	{
		OP_ASSERT(!"Unknown encoding.bin file format");
	}

	m_table_file->Close();

	// The file is loaded
	m_no_file = FALSE;

	return OpStatus::OK;
}

/**
 * Load a data table.
 *
 * @param index The number of the table to load.
 * @return Status of the operation.
 */
OP_STATUS FileTableManager::LoadRawTable(int index)
{
	if (m_no_file)
	{
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

#ifndef ENCODINGS_REGTEST
	RETURN_IF_ERROR(m_table_file->Open(OPFILE_READ));

	if (!m_table_file->IsOpen())
		return OpStatus::ERR_FILE_NOT_FOUND;
#endif // !ENCODINGS_REGTEST

	TableDescriptor *table = GetTableDescriptor(index);
	if (!table)
		return OpStatus::ERR;

	m_table_file->SetFilePos(table->start_offset);

	int padding_bytes = (table->table_info&0x80 && table->byte_count>0) ? 1 : 0;
	long table_size = table->byte_count - padding_bytes;

	table->table = OP_NEWA(UINT8, table_size);
	table->table_was_allocated = table->table!=NULL;
	OP_STATUS rc = OpStatus::ERR_NO_MEMORY;
	if (table->table)
	{
		OpFileLength bytes_read;
		rc = m_table_file->Read(const_cast<UINT8*>(table->table), table_size, &bytes_read);
		if (OpStatus::IsSuccess(rc) && bytes_read != static_cast<OpFileLength>(table_size))
			rc = OpStatus::ERR;
	}

	if (OpStatus::IsError(rc))
	{
		OP_DELETEA(const_cast<UINT8*>(table->table));
		table->table = NULL;
	}

	m_table_file->Close();

	return rc;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN
