/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DEVICEAPI_JIL_JILFOLDERLISTER_H
#define DEVICEAPI_JIL_JILFOLDERLISTER_H

#include "modules/util/simset.h"
#include "modules/pi/system/OpLowLevelFile.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/device_api/jil/JILFSMgr.h"

/**
 * folder lister class
 */
class JILFolderLister
{
private:
	BOOL m_is_real;
	OpFolderLister *m_real_lister;
	OpString m_pattern;
	OpString m_virt_path;
	OpString m_virt_name;
	OpString m_virt_dir_name;
	OpAutoVector<OpString> m_virt_nexts;
	OpString m_virt_start;
	UINT32 m_virt_next_idx;
	BOOL m_case_sensitive;

	OP_STATUS FillVirtNext(const OpString &path);
	void RemoveDuplicatesVirtNext();

	static int ComparePaths(const OpString **str1, const OpString **str2);

public:
	JILFolderLister()
	: m_is_real(TRUE)
	, m_real_lister(NULL)
	, m_virt_next_idx(0)
	, m_case_sensitive(FALSE)
	{}

	virtual ~JILFolderLister()
	{
		OP_DELETE(m_real_lister);
	}

	/**
	 * Contructs instance of JILFolderLister
	 *
	 * @param path<IN> - path of the folder to be listed
	 * @param pattern<IN> - file list pattern (e.g. *.bmp)
	 * @param case_sensitive<IN> - whether matching should be case sensitive or not
	 */
	virtual OP_STATUS Construct(const OpString &path, const OpString &pattern, BOOL case_sensitive = FALSE);

	/**
	 * Moves to the next file
	 */
	virtual BOOL Next();

	/**
	 * Checks whether current file matches specified criteria (path, pattern). It additionally possible to provide OpFileInfo to narrown down the criteria even more
	 */
	virtual BOOL Matches(OpFileInfo *file_info = NULL);

	/**
	 * Gets current entry's name
	 */
	virtual const uni_char *GetFileName();
	static OP_STATUS GetFileName(const OpString &path, OpString *filename);

	/**
	 * Gets current entry's directory
	 */
	virtual const uni_char *GetDirName();
	static OP_STATUS GetDirName(const OpString &path, OpString *dir, const OpString *filename = NULL);

	/**
	 * Gets current entry's full path
	 */
	virtual const uni_char *GetFullPath()
	{
		if (m_is_real)
		{
			OpString real_path;
			RETURN_VALUE_IF_ERROR(real_path.Set(m_real_lister->GetFullPath()), NULL);
			RETURN_VALUE_IF_ERROR(g_DAPI_jil_fs_mgr->SystemToJILPath(real_path, m_virt_path), NULL);
		}

		return m_virt_path.CStr();
	}

	/**
	 * Returns true if curent file is a folder
	 */
	virtual BOOL IsFolder() const
	{
		if (m_is_real)
			return m_real_lister->IsFolder();

		// "pure virtual" path are always folders
		return TRUE;
	}
};

#endif // DEVICEAPI_JIL_JILFOLDERLISTER_H
