/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Adam Minchinton (adamm)
 */

#ifndef SERVER_WHITELIST_H
#define SERVER_WHITELIST_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

#include "modules/prefsfile/accessors/ini.h"
#include "modules/util/opfile/opfile.h"

class PrefsFile;

#define g_server_whitelist (&ServerWhiteList::GetInstance())

class ServerWhiteList
{
public:
	static ServerWhiteList &GetInstance();

	ServerWhiteList();
	~ServerWhiteList();

	/**
	 * Reads whitelist from disk.
	 *
	 * Undeletable addresses come from standard_trusted_repositories.ini;
	 * addresses owned by user come from trusted_repositories.ini
	 *
	 * ServerWhiteList will be initialized automatically of first
	 * access to  g_server_whitelist. 
	 */
	OP_STATUS Init();

	/**
	 * Insert server pointed to by given address.
	 *
	 * @param address
	 */
	BOOL AddServer(const OpStringC& address);

	/**
	 * Removes given address from whitelist
	 *
	 * @param server
	 *
	 * @returns TRUE if address was removed; FALSE otherwise
	 */
	BOOL RemoveServer(const OpStringC& address);
		
	/**
	 * Looks for address in whitelist
	 * 
	 * @param address
	 *                  
	 * @returns TRUE if address was whitelisted; FALSE otherwise
	 */
	BOOL FindMatch(const OpStringC& address);

	/**
	 *
	 * @param address
	 *
	 * @returns TRUE if address is a valid entry; FALSE otherwise
	 */
	BOOL IsValidEntry(const OpStringC& address);

	/**
	 * Builds the model on the fly
	 *
	 * Returned object belongs to caller; NULL is returned in OOM situation.
	 *
	 * @returns SimpleTreeModel representation of the whiteliest
	 */
	SimpleTreeModel* GetModel();

	OP_STATUS GetRepositoryFromAddress(OpString& server, const OpStringC& address);

	/**
	 * Check if whitelist item with given address can be edited
	 *
 	 * @param address
	 *
 	 * @returns TRUE if address can be edited or deleted, FALSE otherwise
	 */
	BOOL ElementIsEditable(const OpStringC& address);

	/**
	 * Dumps list of addresses to file
	 *
	 * Duplicated entries will be removed before writing. If resulting
	 * section in ini file will be empty, then user's preference file
	 * will be removed.
	 *
	 * @retval OpStatus::ERR when it was impossible to update file
	 * @retval OpStatus::OK otherwise
	 */
	OP_STATUS Write();

private:

	/**
	 * Appends given address to whitelist
	 * 
	 * @param address
	 *
	 * @retval OpStatus::ERR when address is invalid or duplicated
	 * @retval OpStatus::ERR_NO_MEMORY on OOM situation
	 * @retval OpStatus::OK otherwise
	 */
	OP_STATUS Insert(const OpStringC& address);

	/**
	 * Turns whitelist into SimpleTreeModel
	 *
	 * @param model Object to be filled with data
	 */
	void BuildModel(SimpleTreeModel* model);

	/**
	 * Updates whitelist based on ini files
	 *
	 * Reads entries from [whitelist] sections from appropriate files.
	 * In result:
	 * m_fixed_vector is filled with addresses from global file.
	 * m_vector contains entries from m_fixed_vector concatenated with those
	 * from local file.
	 *
	 * m_vector is guaranteed not to contain duplicates
	 *
	 * @retval OpStatus::ERR when it was impossible to fill vectors with data
	 * @retval OpStatus::ERR_NO_MEMORY on OOM situation
	 * @retval OpStatus::OK otherwise
	 */
	OP_STATUS Read();

	/**
	 * Find address in vector
	 *
	 * @returns TRUE if address is already in vector; FALSE otherwise
	 */
	BOOL Find(const OpStringC& repository, OpAutoVector<OpString> & vector);

	/**
	 * Append new data to vector
	 *
	 * Data is read from [whitelist] section of file (treated as ini file).
	 * If file doesn't exists, no data is appended and OK is returned.
	 *
	 * @param file
	 * @param vector
	 *
	 * @retval OpStatus::ERR if it was impossible to determine if file exists
	 * @retval OpStatus::ERR_NO_MEMORY on OOM situation
	 * @retval OpStatus::OK otherwise
	 */
	OP_STATUS FillVector(OpFile & file, OpAutoVector<OpString> & vector);


	BOOL m_initialized;

	PrefsFile *m_whitelistfile;

	OpAutoVector<OpString> m_vector;

	OpAutoVector<OpString> m_fixed_vector;
};

#endif // SERVER_WHITELIST_H
