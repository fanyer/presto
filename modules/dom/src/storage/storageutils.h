/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef _MODULES_DOM_STORAGE_LOGERROR_H_
#define _MODULES_DOM_STORAGE_LOGERROR_H_

#if defined OPERA_CONSOLE && (defined DATABASE_STORAGE_SUPPORT || defined CLIENTSIDE_STORAGE_SUPPORT)

class DOM_Runtime;

# ifdef DATABASE_STORAGE_SUPPORT
class DOM_Database;
# endif // DATABASE_STORAGE_SUPPORT

# ifdef CLIENTSIDE_STORAGE_SUPPORT
class DOM_Storage;
# endif // CLIENTSIDE_STORAGE_SUPPORT

#endif // defined OPERA_CONSOLE && (defined DATABASE_STORAGE_SUPPORT || defined CLIENTSIDE_STORAGE_SUPPORT)

class DOM_PSUtils
{
public:
	struct PS_OriginInfo
	{
		const uni_char *m_origin;
		BOOL            m_is_persistent;
		URL_CONTEXT_ID  m_context_id;

		TempBuffer m_origin_buf;
	};

	/**
	 * Serializes and returns the origin information for the given runtime
	 * used by the WebStorage, WebSQLDatabase and IndexedDatabase APIs
	 * exported by the database module to access the storage areas.
	 *
	 * @param buf       Buffer where the origin will be stored.
	 * @param runtime   Runtime from which to fetch the origin info
	 * @param poi       Object which will store the origin info.
	 * @return ERR_NO_MEMORY is case of OOM, else OK
	 */
	static OP_STATUS GetPersistentStorageOriginInfo(DOM_Runtime *runtime, PS_OriginInfo &poi);

	/**
	 * Tells if the status is a file access error found in PS_Status.
	 */
	static BOOL IsStorageStatusFileAccessError(OP_STATUS status);

#ifdef OPERA_CONSOLE
# ifdef DATABASE_STORAGE_SUPPORT
	/**
	 * Posts an error message to the console about bad file access to web databases.
	 */
	static void PostWebDatabaseErrorToConsole(DOM_Runtime *runtime, const uni_char *es_thread_name, DOM_Database *dbo, OP_STATUS status);
	static void PostExceptionToConsole(DOM_Runtime *runtime, const uni_char *es_thread_name, const uni_char *message);
# endif //DATABASE_STORAGE_SUPPORT

# ifdef CLIENTSIDE_STORAGE_SUPPORT
	/**
	 * Posts an error message to the console about bad file access to web storage.
	 */
	static void PostWebStorageErrorToConsole(DOM_Runtime *runtime, const uni_char *es_thread_name, DOM_Storage *ops, OP_STATUS status);
# endif // CLIENTSIDE_STORAGE_SUPPORT
#endif //OPERA_CONSOLE
};

#endif //_MODULES_DOM_STORAGE_LOGERROR_H_

