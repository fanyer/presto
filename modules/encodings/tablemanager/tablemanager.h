/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file tablemanager.h
 *
 * This module provides caching and management of a set of named
 * binary tables.
 */

#ifndef TABLEMANAGER_H
#define TABLEMANAGER_H

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN

# include "modules/encodings/tablemanager/optablemanager.h"


class TableCacheManager : 
	public OpTableManager
{
public:
	TableCacheManager();
	virtual ~TableCacheManager();

	// Inherited interfaces ---
	virtual void ConstructL() = 0;
	virtual OP_STATUS Load() = 0;
	virtual const void *Get(const char *table_name, long &byte_count);
	virtual void Release(const void *table);
	virtual BOOL TablesAvailable() = 0;
	virtual BOOL Has(const char *table_name);
#ifdef OUT_OF_MEMORY_POLLING
	virtual void Flush();
#endif

	/**
	 * Load whatever should be at the table offset. Do not worry about
	 * decompressing.
	 * @param index Index of table to load.
	 * @return Status of the operation.
	 */
	virtual OP_STATUS LoadRawTable(int index) = 0;

	/**
	 * Internal structured used to describe a table.
	 */
	struct TableDescriptor
	{
		const char *table_name;	    ///< Name of data table
		long start_offset;      ///< Starting offset of this table
		long byte_count;        ///< Length in bytes of this table
		long final_byte_count;  ///< Length in bytes of this table (after decompress)
		UINT8 table_info;       ///< bitmask. bit7=tablepadding (used when parsing), bit6..1=reserved, bit0=compressed
		const UINT8 *table;            ///< Pointer to table, NULL if not loaded
#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
		BOOL table_was_allocated;	///< TRUE if the char *table was allocated, FALSE if it points into ROM etc
		unsigned int ref_count; ///< Number of references held to this table
#endif
	};

#ifdef TABLEMANAGER_DYNAMIC_REV_TABLES
	OP_STATUS AddTable(const TableDescriptor& new_table);	///< This is safe to call even when m_tables contains tables
#endif

protected:
	OP_STATUS ParseRawHeaders(const UINT8 *encoding_headers, UINT16 headers_size, BOOL delete_on_exit);

	OP_STATUS AddTables(TableDescriptor* tables, UINT16 count);	///< Never call this when m_tables contains tables
#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
	void AddToLRU(UINT16 index);
	void RemoveFromLRU(UINT16 index);
#endif

	int GetIndex(const char *table_name);
	TableDescriptor *GetTableDescriptor(UINT16 index) const {return (index<m_table_count) ? &m_tables[index] : NULL;}

private:
	const UINT8      *m_encoding_headers;
	UINT16            m_encoding_headers_size;
	BOOL			  m_delete_encoding_headers_on_exit;

	TableDescriptor  *m_tables;			///< Data about available tables.
	UINT16            m_table_count;	///< Number of available tables.

#if !defined ENCODINGS_HAVE_ROM_TABLES || defined TABLEMANAGER_COMPRESSED_TABLES || defined TABLEMANAGER_DYNAMIC_REV_TABLES
	// Will break if we use more than 32767 tables :-)
	short             m_lru[ENCODINGS_LRU_SIZE];	///< LRU queue of unreferenced data tables.
#endif

	static int tablenamecmp(const void *p1, const void *p2);
};
#endif // ENCODINGS_HAVE_TABLE_DRIVEN

#endif // TABLEMANAGER_H
