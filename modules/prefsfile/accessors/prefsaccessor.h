/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef PREFSACCESSOR_H
#define PREFSACCESSOR_H

class OpFileDescriptor;
class PrefsMap;

/**
 * An interface for accessing different types of files containing
 * preferences.
 */
class PrefsAccessor
{
public:
	/** Standard destructor. Needs to be virtual due to inheritance. */
	virtual ~PrefsAccessor() {}

	/**
	 * Load the contents in the file.
	 *
	 * @param file The file to load stuff from
	 * @param map The map to store stuff in
	 * @return Status of the operation
	 */
	virtual OP_STATUS LoadL(OpFileDescriptor *file, PrefsMap *map) = 0;

#ifdef PREFSFILE_WRITE
	/**
	 * Store the contents in the map to a file.
	 *
	 * @param file The file to store stuff in
	 * @param map The map to get stuff from
	 * @return Status of the operation
	 */
	virtual OP_STATUS StoreL(OpFileDescriptor *file, const PrefsMap *map) = 0;
#endif // PREFSFILE_WRITE
};

#endif // PREFSACCESSOR_H
