/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef RESOURCE_FOLDERS_H
#define RESOURCE_FOLDERS_H

class OpDesktopResources;

namespace ResourceFolders
{
	/**
	 * Sets all of the folders inside the OperaInitInfo structure
	 * before they are passed to the opera->Init call
	 *
	 * @param pinfo  pointer to the opera init info
	 * @param desktop_resources pointer to the desktop resources; can be NULL
	 * @param profile_name  pointer to profile folder to use instead of the
	 *						standard "Opera" one
	 *
	 * @return OpStatus::OK if it deletes everything
	 */
    OP_STATUS SetFolders(OperaInitInfo *pinfo, OpDesktopResources* desktop_resources = NULL, const uni_char *profile_name = NULL);

	/**
	 * Set region-based folders inside the OperaInitInfo structure.
	 *
	 * @param pinfo pointer to the opera init info
	 * @param region name of the region
	 * @param language name of language
	 * @param default_language name of the default language for the region
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on error
	 */
	OP_STATUS SetRegionFolders(OperaInitInfo *pinfo, OpStringC region, OpStringC language, OpStringC default_language);

	/**
	 * Gets a folder from an opfilefolder making sure to take into account
	 * any parent relationships. This needs to be used before opera->Init call
	 * before there is a g_folder_manager
	 *
	 * @param opfolder  OpFolder to get
	 * @param pinfo		pointer to the opera init info
	 * @param folder	returns full path to the opfolder
	 *
	 * @return OpStatus::OK if it deletes everything
	 */
	OP_STATUS GetFolder(OpFileFolder opfilefolder, OperaInitInfo *pinfo, OpString &folder);

	/**
	 * Compares 2 paths, ignoring any trailing slashes.
	 *
	 * @param path1		Full path of the file/folder to be checked
	 * @param path2		Full path of the file/folder to be checked
	 * @param identical	On return, will be set to TRUE, if the paths point to the same file/folder. Undefined if the function returns an error.
	 * 
	 * @return OpStatus::OK if the comparison succeeded (independent of the result of that comparison).
	 */

	OP_STATUS ComparePaths(const OpStringC &path1, const OpStringC &path2, BOOL &identical);
};

#endif // RESOURCE_FOLDERS_H
