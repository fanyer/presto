/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file romtablemanager.h
 *
 * This module provides access to a file that contains a set of named
 * binary tables, which are stored in the program binary. This is
 * normally used for devices where Opera is stored in ROM, since using
 * files there can be cumbersome.
 *
 * The module only provides the ability to get a particular table, the
 * actual administration of the tables is hidden under the covers.
 */

#ifndef ROMTABLEMANAGER_H
#define ROMTABLEMANAGER_H

#if (defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_ROM_TABLES) || defined DOXYGEN_DOCUMENTATION
# include "modules/encodings/tablemanager/tablemanager.h"

/**
 * ROM based interface for access to named binary data tables.
 */
class RomTableManager : public TableCacheManager
{
public:
	RomTableManager();
	virtual ~RomTableManager() {}

	// Inherited interfaces ---
	virtual void ConstructL() {}
	virtual OP_STATUS Load();
	virtual OP_STATUS LoadRawTable(int index);
	virtual BOOL TablesAvailable() { return TRUE; }

#ifdef OUT_OF_MEMORY_POLLING
	virtual void Flush() {}
#endif
};

/**
 * Return pointer to the binary image of an encoding.bin file. Must be defined
 * by platform code. See the documentation in the i18ndata module for
 * ways to produce one.
 */
const unsigned int *g_encodingtables();

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_ROM_TABLES
#endif // ROMTABLEMANAGER_H
