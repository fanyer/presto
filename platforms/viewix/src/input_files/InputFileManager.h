/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_INPUT_FILE_MANAGER_H__
#define __FILEHANDLER_INPUT_FILE_MANAGER_H__

#include "platforms/viewix/src/nodes/ThemeNode.h"

class InputFileManager
{
public:
    OP_STATUS InitDataDirs();

	OP_STATUS InitIconSpecificDirs();

    OP_STATUS InitMimetypeIconDesktopDirs();

    OP_STATUS GetFiles(OpVector<OpString>& dirs,
					   OpVector<OpString>& files,
					   const OpStringC & subdir,
					   const OpStringC & filename);

    OP_STATUS GetDirs(OpVector<OpString>& dirs,
					  OpVector<OpString>& subdirs,
					  const OpStringC & subdir);

    OP_STATUS GetFilesFromSubdirs(OpVector<OpString>& dirs,
								  OpVector<OpString>& files,
								  const OpStringC & subdir,
								  const OpStringC & filename);

    OP_STATUS GetSubfolders(const OpStringC & dir, OpVector<OpString>& dirs);

    OP_STATUS GetIndexThemeFiles(OpVector<OpString>& files);

	OP_STATUS GetGnomeVFSFiles(OpVector<OpString>& files);

    OP_STATUS GetDefaultsFiles(OpVector<OpString>& profilerc_files, OpVector<OpString>& files);

	OP_STATUS GetProfilercFiles(OpVector<OpString>& profilerc_files);

    OP_STATUS GetMimeInfoFiles(OpVector<OpString>& files);

    OP_STATUS GetSubclassesFiles(OpVector<OpString>& files);

    OP_STATUS GetAliasesFiles(OpVector<OpString>& files);

    OP_STATUS GetGlobsFiles(OpVector<OpString>& files);

    OP_STATUS UseAliasesAndSubclasses(OpVector<OpString>&aliases_files, OpVector<OpString>&subclasses_files);

    OP_STATUS ParseMailcapFiles();

    /**
       @param node
       @param icon_size        - preferred height/width in pixels
       @return
    */
    OP_STATUS MakeIconPath(class FileHandlerNode * node,
						   OpString & icon_path,
						   UINT32 icon_size  = 16);

    OP_STATUS MakeIconPathSimple(const OpStringC & icon,
								 const OpString& path,
								 OpString & icon_path,
								 UINT32 icon_size,
								 const OpStringC & subdir);

    const OpString & GetIconThemeFile() const { return m_theme_node.GetIndexThemeFile(); }

    const OpString & GetIconThemePath() const { return m_theme_node.GetThemePath(); }

    const OpString & GetIconThemeBackupPath() const { return m_backup_theme_node.GetThemePath(); }

    OpVector<OpString> & GetMimeTypeDesktopDirs() { return m_mimetype_desktop_dirs; }

    BOOL TryExec(const OpStringC & try_exec,
				 OpString & full_path);

	// Load

    void LoadNode(FileHandlerNode * node);

    OP_STATUS LoadIcon(FileHandlerNode* node,
					   UINT32 icon_size);

	void LoadMimeTypeNode(MimeTypeNode* node);

	void LoadApplicationNode(ApplicationNode* node);

	OP_STATUS InitTheme(OpVector<OpString>& theme_files);

	OP_STATUS FindDesktopFile(const OpStringC & filename,
							  OpVector<OpString>& files);

private:

	OP_STATUS SortThemes(OpVector<OpString>& theme_files);

	ThemeNode m_theme_node;
	ThemeNode m_backup_theme_node;

	OpString m_home_icons_dir;

	OpAutoVector<OpString> m_pixmaps_dirs;
	OpAutoVector<OpString> m_mimetype_desktop_dirs;
	OpAutoVector<OpString> m_data_dirs;
};

#endif //__FILEHANDLER_INPUT_FILE_MANAGER_H__
