/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef XRDATABASE_H
#define XRDATABASE_H

typedef struct _XrmHashBucketRec *XrmDatabase;

class XRDatabase
{
public:
	XRDatabase();
	~XRDatabase();

	/** Initialize the database
	  * @param databasestring X resource string, eg. as obtained by using
	  * XResourceManagerString(g_x11->GetDisplay())
	  */
	OP_STATUS Init(const char* databasestring);

	/** Get an integer value from the database
	  * @param key Which key to get value for
	  * @param default_value Value to return if key not found
	  * @return Value associated with key, or default_value if not found
	  */
	int GetIntegerValue(const char* key, int default_value);

	/** Get a boolean value from the database
	 * @param key Which key to get value for
	 * @param default_value Value to return if key not found
	 * @return Value associated with key, or default_value if not found
	 */
	bool GetBooleanValue(const char* key, bool default_value);

	/** Get a string value from the database
	  * @param key Which key to get value for
	  * @param default_value Value to return if key not found
	  * @return Value associated with key, or default_value if not found
	  */
	const char* GetStringValue(const char* key, const char* default_value);

private:
	const char* GetValue(const char* key);

	XrmDatabase m_database;
};

#endif // XRDATABASE_H
