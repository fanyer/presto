/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN

#include "modules/encodings/tablemanager/tablemanager.h"

#include "modules/encodings/tablemanager/table_decompressor.h"

#ifdef TABLEMANAGER_DYNAMIC_REV_TABLES
#include "modules/encodings/tablemanager/reverse_table_builder.h"
#endif

#ifndef ENCODINGS_REGTEST
# include "modules/hardcore/mem/mem_man.h"
# include "modules/util/opfile/opfile.h"
# include "modules/pi/OpSystemInfo.h"
#endif

// Table bit flags
#define TABLE_PADDED		0x80	/**< Table length contains one padding byte. */
#define TABLE_COMPRESSED	0x01	/**< Table is compressed. */

#define TABLE_FORMAT		0x7F	/**< Bit mask for table storage format. */

// ===== Module implementation

TableCacheManager::TableCacheManager()
	: OpTableManager(),
	  m_encoding_headers(NULL),
	  m_encoding_headers_size(0),
	  m_delete_encoding_headers_on_exit(FALSE),
	  m_tables(NULL),
	  m_table_count(0)
{
#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
	for (int ix = 0; ix < ENCODINGS_LRU_SIZE; ix ++)
		m_lru[ix] = -1;
#endif
}

TableCacheManager::~TableCacheManager()
{
	int tbl_count = m_table_count;
	if (m_tables)
	{
		for (int ix = 0; ix < tbl_count; ix ++)
		{
#ifdef ENCODINGS_REGTEST
# if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
			assert(0 == m_tables[ix].ref_count);
# endif
#endif
			//Delete table_name if it is allocated by itself, and not as part of the m_encoding_headers
			if (m_tables[ix].table_name &&
			    (m_tables[ix].table_name<reinterpret_cast<const char*>(m_encoding_headers) || m_tables[ix].table_name>=reinterpret_cast<const char*>(m_encoding_headers+m_encoding_headers_size)) )
			{
				OP_DELETEA(const_cast<char*>(m_tables[ix].table_name));
			}
#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
			if (m_tables[ix].table_was_allocated)
				OP_DELETEA(const_cast<UINT8*>(m_tables[ix].table));
#endif
		}
		OP_DELETEA(m_tables);
	}

	if (m_delete_encoding_headers_on_exit)
		OP_DELETEA(const_cast<UINT8*>(m_encoding_headers));
}

const void *TableCacheManager::Get(const char *table_name, long &byte_count)
{
	byte_count = 0;
	int ix = GetIndex(table_name);
	TableDescriptor *table = (ix >= 0) ? GetTableDescriptor(ix) : NULL;
	if (-1 == ix || !table)
	{
		return NULL;
	}

	if (table->table == NULL) // Table not yet loaded
	{
#ifdef TABLEMANAGER_DYNAMIC_REV_TABLES
		if (-1 == table->start_offset)
		{
			// We know about the table, but haven't got it stored, so we need
			// to create it. We try two times.
			ReverseTableBuilder::BuildTable(this, table);

			if (table->table == NULL)
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return NULL;
			}
		}
		else 
#endif
		{
#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
			OP_ASSERT(0 == table->ref_count);
#endif
			OP_STATUS rc = LoadRawTable(ix);
			if (rc == OpStatus::ERR_NO_MEMORY)
			{
#ifndef ENCODINGS_REGTEST
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
#endif
				return NULL;
			}
			if (OpStatus::IsError(rc))
			{
				return NULL;
			}
#ifdef TABLEMANAGER_COMPRESSED_TABLES
			else
			{
				//Compressed?
				if (OpStatus::IsSuccess(rc) && table->table_info & TABLE_COMPRESSED)
				{
					TableDecompressor decompressor;
					rc = decompressor.Init();
					if (OpStatus::IsSuccess(rc))
					{
						const UINT8 *compressed_table = table->table;
						const UINT8 *decompress_ptr = compressed_table;
						BOOL compressed_table_was_allocated = table->table_was_allocated;

						table->table = OP_NEWA(UINT8, table->final_byte_count);
						table->table_was_allocated = table->table!=NULL;
						
						if (!table->table)
						{
							//Revert changes and return error
							table->table = compressed_table;
							table->table_was_allocated = compressed_table_was_allocated;
							rc = OpStatus::ERR_NO_MEMORY;
						}
						else
						{
							int src_len = table->byte_count;
							if (table->table_info & TABLE_PADDED && table->byte_count>0)
								src_len--;
							
							int read = 0;
							int written, total_written = 0;
							do	{
								written = decompressor.Decompress(decompress_ptr, src_len, const_cast<UINT8*>(table->table + total_written), table->final_byte_count - total_written, &read);
								if (written == -1)
								{
									OP_DELETEA(const_cast<UINT8*>(table->table));
									table->table = NULL;
									table->table_was_allocated = FALSE;

									rc = OpStatus::ERR;
									break;
								}
								total_written += written;
								decompress_ptr += read;
								src_len -= read;
							} while (src_len>0 && read!=0);
							OP_ASSERT(OpStatus::IsError(rc) || (src_len==0 && total_written==table->final_byte_count));
							if (compressed_table_was_allocated)
								OP_DELETEA(const_cast<UINT8*>(compressed_table));
						}
					}
				}
			}
#endif // TABLEMANAGER_COMPRESSED_TABLES
		}
	}

	if (!table->table)
		return NULL;

#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
	if (1 == ++table->ref_count)
	{
		RemoveFromLRU(ix);
	}
#endif

	byte_count = table->final_byte_count;
	return table->table;
}

void TableCacheManager::Release(const void *table)
{
	if (!table) return;

#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
	int i;
	for (i=0; i < m_table_count; i++)
	{
		if (m_tables[i].table == table)
		{
			if (0 == --m_tables[i].ref_count)
			{
				// We can now safely discard this table.
				AddToLRU(i);
			}
			break;
		}
	}
#endif
}

BOOL TableCacheManager::Has(const char* table_name)
{
	if (GetIndex(table_name) != -1)
		return TRUE;

	return FALSE;
}

/**
 * Retrieve the number of a named table.
 *
 * @param table_name Name of the table to find in the index.
 * @return The index of the table, or -1 if not found.
 */
int TableCacheManager::GetIndex(const char *table_name)
{
	if (m_tables && m_table_count)
	{
		TableDescriptor search =
			{ table_name, 0, 0, 0, 0, NULL,
#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
			  FALSE, 0
#endif
			};

		TableDescriptor *found = reinterpret_cast<TableDescriptor *>
			(op_bsearch(&search, m_tables, m_table_count, sizeof(TableDescriptor),
			            tablenamecmp));

		if (found)
		{
			OP_ASSERT(op_stricmp(found->table_name, table_name) == 0);
			OP_ASSERT(op_stricmp(m_tables[found - m_tables].table_name, table_name) == 0);
			return found - m_tables;
		}

#ifdef TABLEMANAGER_DYNAMIC_REV_TABLES
		if (ReverseTableBuilder::HasReverse(this, table_name))
			return GetIndex(table_name);
#endif
	}

	// Table not found or no tables loaded
	return -1;
}

#ifdef OUT_OF_MEMORY_POLLING
void TableCacheManager::Flush()
{
# if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
	for (int ix = 0; ix < ENCODINGS_LRU_SIZE; ix ++)
	{
		if (m_lru[ix] != -1)
		{
			OP_ASSERT(0 == m_tables[m_lru[ix]].ref_count);
			if (m_tables[m_lru[ix]].table_was_allocated)
				OP_DELETEA(const_cast<UINT8 *>(m_tables[m_lru[ix]].table));

			m_tables[m_lru[ix]].table = NULL;
			m_tables[m_lru[ix]].table_was_allocated = FALSE;

			m_lru[ix] = -1;
		}
	}
# endif
}
#endif // OUT_OF_MEMORY_POLLING


OP_STATUS TableCacheManager::ParseRawHeaders(const UINT8 *encoding_headers, UINT16 headers_size, BOOL delete_on_exit)
{
	OP_ASSERT(encoding_headers!=NULL && headers_size>=8); //Don't try to parse before raw headers are allocated and read (at least 8 bytes..)
	if (!encoding_headers || headers_size<8)
	{
		if (delete_on_exit)
			OP_DELETEA(const_cast<UINT8*>(encoding_headers));

		m_encoding_headers = NULL;
		m_encoding_headers_size = 0;
		m_delete_encoding_headers_on_exit = FALSE;
		return OpStatus::OK; //Nothing to do
	}

	OP_ASSERT(!m_tables && m_table_count==0);
	if (m_tables || m_table_count>0)
		return OpStatus::ERR;

	m_encoding_headers = encoding_headers;
	m_encoding_headers_size = headers_size;
	m_delete_encoding_headers_on_exit = delete_on_exit;

	UINT16 magic = *(reinterpret_cast<const UINT16*>(m_encoding_headers));
#ifdef ENCODINGS_OPPOSITE_ENDIAN
	magic = ENCDATA16(magic);
#endif
	
	if (magic == 0xFE01) //Magic number for what is considered version 1 of encoding.bin
	{
		//File format:
		//UINT16 magic 	(For version 1, this should be 0xFE01)
		//UINT16 headers_size	(size of all data before tables, including magic and headers_size, in bytes)
		//UINT16 number_of_tables
		//UINT16 reserved	(should be set to 0)
		//
		//UINT32 table_data[number_of_tables+1]	(offset from beginning of file to table data (last item is a placeholder, and should equal to filelength))
		//UINT32 final_size[number_of_tables]	(number of bytes for uncompressed table)
		//UINT16 table_name[number_of_tables]	(offset from beginning of file to table name (which is zero-terminated))
		//UINT8 table_info[number_of_tables]	(bitmask. bit7=table data includes one padding byte, bit6..1=reserved, bit0=table data is compressed)
		//char names[<just-enough-bytes, including zero-terminators>]	(All table names, with a zero-terminator after each name)
		//char padding[0..1]	(padding, if needed. Should be set to 0)
		//table data..

		m_table_count = *(reinterpret_cast<const UINT16*>(m_encoding_headers+4));
#ifdef _DEBUG
		UINT16 headers_size = *(reinterpret_cast<const UINT16*>(m_encoding_headers+2));
		UINT16 reserved = *(reinterpret_cast<const UINT16*>(m_encoding_headers+6));
		OP_ASSERT(reserved == 0);
#endif

#ifdef ENCODINGS_OPPOSITE_ENDIAN
		m_table_count = ENCDATA16(m_table_count);
#ifdef _DEBUG
		headers_size = ENCDATA16(headers_size);
		reserved = ENCDATA16(reserved);
#endif // _DEBUG
#endif // ENCODINGS_OPPOSITE_ENDIAN

		// Allocate
		m_tables = OP_NEWA(TableDescriptor, m_table_count);
		if (!m_tables)
		{
			m_table_count = 0;

			return OpStatus::ERR_NO_MEMORY;
		}

		const UINT8 *table_data_ptr = m_encoding_headers + 8/*headers*/;
		const UINT8 *final_size_ptr = table_data_ptr + 4*(m_table_count+1)/*table_data*/;
		const UINT8 *table_name_ptr = final_size_ptr + 4*m_table_count/*final_size*/;
		const UINT8 *table_info_ptr = table_name_ptr + 2*m_table_count/*table_name*/;

		if (table_info_ptr+m_table_count >= m_encoding_headers+m_encoding_headers_size) //Fail if last table_info entry is outside buffer
		{
			OP_DELETEA(m_tables);
			m_tables = NULL;
			m_table_count = 0;
			return OpStatus::ERR_PARSING_FAILED;
		}

		int i;
		for (i=0; i<m_table_count; i++)
		{
			m_tables[i].start_offset = *(reinterpret_cast<const UINT32*>(table_data_ptr + 4*i));
			UINT32 next_table_start_offset = *(reinterpret_cast<const UINT32*>(table_data_ptr + 4*(i+1)));
			m_tables[i].final_byte_count = *(reinterpret_cast<const UINT32*>(final_size_ptr + 4*i));
			m_tables[i].table_name = reinterpret_cast<const char*>(m_encoding_headers + *(reinterpret_cast<const UINT16*>(table_name_ptr + 2*i))); //This points directly into the headers
			m_tables[i].table_info = *(reinterpret_cast<const UINT8*>(table_info_ptr + i));
			m_tables[i].table = NULL;
#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
			m_tables[i].table_was_allocated = FALSE;
			m_tables[i].ref_count = 0;
#endif

#ifdef ENCODINGS_OPPOSITE_ENDIAN
			m_tables[i].start_offset = ENCDATA32(m_tables[i].start_offset);
			next_table_start_offset = ENCDATA32(next_table_start_offset);
			m_tables[i].final_byte_count = ENCDATA32(m_tables[i].final_byte_count);
			m_tables[i].table_name = reinterpret_cast<const char*>(m_encoding_headers + ENCDATA16(*(reinterpret_cast<const UINT16*>(table_name_ptr + 2*i))));
#endif

			m_tables[i].byte_count = next_table_start_offset - m_tables[i].start_offset;

			if (m_tables[i].table_info & TABLE_PADDED && m_tables[i].byte_count>0) //table_data includes one byte padding
			{
				m_tables[i].table_info &= TABLE_FORMAT;
				m_tables[i].byte_count--;
			}

#ifndef TABLEMANAGER_COMPRESSED_TABLES
			OP_ASSERT(m_tables[i].table_info == 0 || !"Compressed table support disabled in this build");
#endif
		}
		OP_ASSERT(m_table_count>0 && headers_size==m_tables[0].start_offset);
	}

	if (m_tables && m_table_count>1)
	{
		//Don't worry about LRU here, it should not have any entries yet
		op_qsort(m_tables, m_table_count, sizeof(TableDescriptor), tablenamecmp);
	}

	return OpStatus::OK;
}

#ifdef TABLEMANAGER_DYNAMIC_REV_TABLES
/**
 * Add information about a table to TableCacheManager's list of known tables.
 * This is used by the reverse table builder to insert tables at run-time.
 * Only the fields table_name, byte_count and table are used from the data
 * supplied. The inherting class must implement the BuildTable() class for
 * any tables it inserts here, so that they can be rebuilt after having been
 * flushed.
 *
 * @param new_table Meta data for the new table
 * @return Status of the operation
 */
OP_STATUS TableCacheManager::AddTable(const TableDescriptor& new_table)
{
	// Reallocate
	TableDescriptor *new_table_list = OP_NEWA(TableDescriptor, m_table_count + 1);
	if (!new_table_list)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// Shuffle old stuff into the new list
	op_memcpy(new_table_list, m_tables, sizeof (TableDescriptor) * m_table_count);
	new_table_list[m_table_count] = new_table;

	// Initialize the new entry
	new_table_list[m_table_count].ref_count = 0;
	new_table_list[m_table_count].start_offset = -1;

	op_qsort(new_table_list, ++ m_table_count, sizeof(TableDescriptor), tablenamecmp);

	// Update LRU queue
	for (int ix = 0; ix < ENCODINGS_LRU_SIZE; ix ++)
	{
		if (m_lru[ix] != -1)
		{
			// We've inserted exactly one table, so unless the sorting
			// is unstable, it should not have moved away more than one
			// step.
			const UINT8 *old_table = m_tables[m_lru[ix]].table;
			for (int iy = m_lru[ix]; iy < m_table_count; iy ++)
			{
				if (new_table_list[iy].table == old_table)
				{
					m_lru[ix] = iy;
					break;
				}
			}
		}
	}

	OP_DELETEA(m_tables);
	m_tables = new_table_list;
	return OpStatus::OK;
}
#endif // TABLEMANAGER_DYNAMIC_REV_TABLES

#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
void TableCacheManager::AddToLRU(UINT16 index)
{
	// If there is an entry pending deletion, kill it.
	if (m_lru[0] != -1)
	{
		OP_ASSERT(0 == m_tables[m_lru[0]].ref_count);
		if (m_tables[m_lru[0]].table_was_allocated)
			OP_DELETEA(const_cast<UINT8*>(m_tables[m_lru[0]].table));

		m_tables[m_lru[0]].table = NULL;
		m_tables[m_lru[0]].table_was_allocated = FALSE;
	}

	// Put it in the LRU queue.
	int i;
	for (i=0; i < ENCODINGS_LRU_SIZE - 1; i++)
	{
		m_lru[i] = m_lru[i+1];
	}

	m_lru[ENCODINGS_LRU_SIZE-1] = index;
}
#endif // !ENCODINGS_HAVE_ROM_TABLES || TABLEMANAGER_COMPRESSED_TABLES || TABLEMANAGER_DYNAMIC_REV_TABLES

#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
void TableCacheManager::RemoveFromLRU(UINT16 index)
{
	// Remove from LRU buffer if it is listed there
	int i;
	for (i=0; i < ENCODINGS_LRU_SIZE; i++)
	{
		if (index == m_lru[i])
		{
			// Push any previous entries forward
			int j;
			for (j=i; j > 0; j--)
			{
				m_lru[j] = m_lru[j-1];
			}
			m_lru[0] = -1;
			break;
		}
	}
}
#endif // !ENCODINGS_HAVE_ROM_TABLES || TABLEMANAGER_COMPRESSED_TABLES || TABLEMANAGER_DYNAMIC_REV_TABLES

/** Comparison function for qsort and bsearch. */
int TableCacheManager::tablenamecmp(const void *p1, const void *p2)
{
	const TableDescriptor *d1 = reinterpret_cast<const TableDescriptor *>(p1);
	const TableDescriptor *d2 = reinterpret_cast<const TableDescriptor *>(p2);
	return op_stricmp(d1->table_name, d2->table_name);
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN
