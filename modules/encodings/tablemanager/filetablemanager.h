/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file filetablemanager.h
 *
 * This module provides access to a file that contains a set of named
 * binary tables.  At the moment this file is used for character
 * conversion and information tables, but it could be used for any
 * purpose.
 *
 * The module only provides the ability to get a particular table, the
 * actual administration of the tables is hidden under the covers.
 */

#ifndef FILETABLEMANAGER_H
#define FILETABLEMANAGER_H

#if (defined ENCODINGS_HAVE_TABLE_DRIVEN && !defined ENCODINGS_HAVE_ROM_TABLES) || defined DOXYGEN_DOCUMENTATION
# include "modules/encodings/tablemanager/tablemanager.h"

class OpFile;

/**
 * File based interface for access to named binary data tables.
 */
class FileTableManager : public TableCacheManager
{
public:
	FileTableManager();
	virtual ~FileTableManager();

	// Inherited interfaces ---
	virtual void ConstructL();
	virtual OP_STATUS Load();
	virtual OP_STATUS LoadRawTable(int index);
	virtual BOOL TablesAvailable() { return !m_no_file; };

protected:
	OpFile           *m_table_file;		///< File object holding the data tables.
	BOOL              m_no_file;		///< TRUE if the table file could not be loaded.

	int GetIndex(const char *table_name);
};
#endif // (ENCODINGS_HAVE_TABLE_DRIVEN && !ENCODINGS_HAVE_ROM_TABLES) || defined DOXYGEN_DOCUMENTATION

#endif // FILETABLEMANAGER_H
