/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

/** @file optablemanager.h
 *
 * This module provides an abstract interface for a system to provide
 * access to a set of named binary tables. At the moment this file is
 * used for character conversion and information tables, but it could
 * be used for any purpose.
 *
 * This interface provides only the ability to get a particular table,
 * the actual administration of the tables is hidden under the covers.
 */

#ifndef OPTABLEMANAGER_H
#define OPTABLEMANAGER_H

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN

/**
 * Abstract interface for access to named binary data tables.
 */
class OpTableManager
{
public:
	OpTableManager() {};
	virtual ~OpTableManager() {};

	/**
	 * Prepare structures and data.
	 *
	 * @return OpStatus::OK if success
	 */
	virtual void ConstructL() = 0;

	/**
	 * Load structures and data needed for providing the named binary data tables.
	 * A failure here is not serious, Opera will continue (with US-ASCII, Latin1 and UTF-8 support).
	 *
	 * @return OpStatus::OK if success
	 */
	virtual OP_STATUS Load() = 0;

	/**
	 * Retrieve a character data table from the data file. The pointer
	 * to the table is valid at least until you call Release or
	 * until OpTableManager is destructed, whichever is shorter.
	 *
	 * @param table_name Name of table to retrieve.
	 * @param byte_count Output: Length of retrieved table in bytes.
	 * @return Pointer to the table, or NULL if the table couldn't be
	 *         found or an error occured.  (This soft failure on OOM
	 *         is a feature.)
	 */
	virtual const void *Get(const char *table_name, long &byte_count) = 0;

	/**
	 * Release a table pointer. This tells OpTableManager that you are
	 * not using the table any longer. The table may or may not be
	 * deallocated, but you MUST not reference it after you have
	 * released it.
	 */
	virtual void Release(const void *table) = 0;

	/**
	 * Returns TRUE if the the tables are available, normally used to
	 * indicate that the table file has been installed. This function
	 * can be used to tell the user about a missing file during
	 * startup.
	 */
	virtual BOOL TablesAvailable() = 0;

	/**
	 * Check whether a named data table is available.
	 * @param table_name Name of table to check.
	 * @return 1 if table exists, 0 if table does not exist.
	 */
	virtual BOOL Has(const char *table_name) = 0;

#ifdef OUT_OF_MEMORY_POLLING
	/**
	 * Deallocate all unused tables.
	 */
	virtual void Flush() = 0;
#endif
};

/** The global singleton OpTableManager instance. */
#define g_table_manager (g_opera->encodings_module.GetOpTableManager())

// Internal macros to handle data in the wrong endian
#ifdef ENCODINGS_OPPOSITE_ENDIAN
# define ENCDATA16(x) ((((static_cast<UINT16>(x)) & 0xFF) << 8) | (static_cast<UINT16>(x)) >> 8)
# define ENCDATA32(x) ((((static_cast<UINT32>(x)) & 0xFF) << 24) | (((static_cast<UINT32>(x)) & 0xFF00) << 8) | (((static_cast<UINT32>(x)) & 0xFF0000) >> 8) | (static_cast<UINT32>(x)) >> 24)
#else
# define ENCDATA16(x) (x)
# define ENCDATA32(x) (x)
#endif

#endif // ENCODINGS_HAVE_TABLE_DRIVEN
#endif // OPTABLEMANAGER_H
