/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Per Hedbor
** Peter Karlsson
*/

#if !defined REVERSE_TABLE_BUILDER_H && defined TABLEMANAGER_DYNAMIC_REV_TABLES
#define REVERSE_TABLE_BUILDER_H
# include "modules/encodings/tablemanager/tablemanager.h"

/** 
 * Build reverse-lookup tables dynamically from the normal lookup tables.
 * Only used from the internal TableManager functions, never used
 * directly from outside this module.
 */
class ReverseTableBuilder
{
public:
	ReverseTableBuilder();

	/**
     * Generate the specified reverse table. Only used from inside the
     * TableManager.
     *
	 * @param table_manager The Table Manager to retrieve data from.
	 * @param table Meta data about table to generate.
     */
	static void BuildTable(OpTableManager *table_manager, TableCacheManager::TableDescriptor *table);

	/**
	 * Check whether a named reverse data table is available.
	 *
	 * @param table_manager The Table Manager to retrieve data from.
	 * @param table_name Name of table to check.
	 * @return 1 if table exists, 0 if table does not exist.
	 */
	static BOOL HasReverse(TableCacheManager *table_manager, const char *table_name);

private:
	/**
	 * The format of the forward table we are building the rev table from. 
	 * Detected using the _name_ of the forward table currently. Not all
	 * formats below are actually used, but these are all current possible
	 * formats.
	 */
	enum TableFormat
	{
		UNKNOWN,
		USHORT,
#ifdef ENCODINGS_HAVE_CHINESE
		USHORT_ROWCELL_BIG5,
		USHORT_ROWCELL_CNS11643,
		USHORT_ROWCELL_GBK,
#endif
#ifdef ENCODINGS_HAVE_KOREAN
		USHORT_ROWCELL_KSC5601,
#endif
#ifdef ENCODINGS_HAVE_JAPANESE
		USHORT_ROWCELL_JIS0208,
#endif
		USHORT_USHORT,
		BYTE_USHORT
	};

	static TableFormat GetTableFormat(OpTableManager *table_manager, const char *, const char *);

	// Various helpers
	static uni_char CodeForInd(TableFormat, uni_char, int);
	static char *BuildRev(const char *, const void *, long,
		const void *, TableFormat, int, int, long &);
	static const void *ExceptionTable(const char *, const unsigned char *,
		unsigned long);
	static BOOL Exclude(uni_char, uni_char, const void *, int &);
	static int AddExtras(const void *, uni_char *);
};

#endif
