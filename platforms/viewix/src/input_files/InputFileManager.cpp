/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h" // -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-

#include "platforms/viewix/FileHandlerManager.h"
#include "platforms/viewix/src/input_files/InputFileManager.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"
#include "platforms/viewix/src/nodes/MimeTypeNode.h"
#include "platforms/viewix/src/nodes/ApplicationNode.h"
#include "platforms/viewix/src/input_files/AliasesFile.h"
#include "platforms/viewix/src/input_files/SubclassesFile.h"
#include "platforms/viewix/src/input_files/MailcapFile.h"
#include "platforms/viewix/src/input_files/DesktopFile.h"
#include "platforms/viewix/src/input_files/MimeXMLFile.h"
#include "platforms/viewix/src/input_files/IndexThemeFile.h"
#include "platforms/viewix/src/input_files/KDErcFile.h"

#include "modules/util/filefun.h"
#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 ** GetDataDirs
 **
 ** Fills the vector dirs with the directories in XDG_DATA_DIRS which by definition
 ** include the directories in XDG_DATA_HOME (if either is not set - it defaults
 ** according to the standard).
 **
 **	@param vector to be filled
 **
 ** @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
 **
 ** "$XDG_DATA_HOME defines the base directory relative to which user specific data
 **  files should be stored. If $XDG_DATA_HOME is either not set or empty, a default
 **  equal to $HOME/.local/share should be used.
 **
 **  $XDG_DATA_DIRS defines the preference-ordered set of base directories to search
 **  for data files in addition to the $XDG_DATA_HOME base directory.
 **
 **  The directories in $XDG_DATA_DIRS should be seperated with a colon ':'.
 **
 **  If $XDG_DATA_DIRS is either not set or empty, a value equal to
 **  /usr/local/share/:/usr/share/ should be used."
 **
 ** Ref: http://standards.freedesktop.org/basedir-spec/latest/ar01s03.html
 ***********************************************************************************/
OP_STATUS InputFileManager::InitDataDirs()
{
    OpString data_home;
    OpString data_dirs;
    OpString dir_string;

    // Get the XDG_DATA_HOME:
    // ----------------------

    data_home.Set(op_getenv("XDG_DATA_HOME"));

    if( data_home.IsEmpty() ) //Fall back to default value
    {
		OpString home;
		home.Set(op_getenv("HOME"));

		if(home.HasContent())
		{
			data_home.AppendFormat(UNI_L("%s/.local/share/"), home.CStr());
		}
    }

    // Get the XDG_DATA_DIRS:
    // ----------------------

    data_dirs.Set(op_getenv("XDG_DATA_DIRS"));

    if( data_dirs.IsEmpty() ) //Fall back to default value
    {
		data_dirs.Set("/usr/local/share/:/usr/share/");
    }

    // Make the path :
    // ---------------

    if(data_home.HasContent())
		dir_string.Append(data_home);

    if(data_home.HasContent() && data_dirs.HasContent())
		dir_string.Append(":");

    if(data_dirs.HasContent())
		dir_string.Append(data_dirs);

    // Place the dirs in the vector :
    // ------------------------------
    FileHandlerManagerUtilities::SplitString(m_data_dirs, dir_string, ':');

    return OpStatus::OK;
}

/***********************************************************************************
 ** GetIconSpecificDirs
 **
 ** "Icons and themes are looked for in a set of directories. By default, apps should
 **  look in $HOME/.icons (for backwards compatibility), in $XDG_DATA_DIRS/icons and
 **  in /usr/share/pixmaps (in that order). Applications may further add their own
 **  icon directories to this list, and users may extend or change the list (in
 **  application/desktop specific ways).In each of these directories themes are stored
 **  as subdirectories. A theme can be spread across several base directories by having
 **  subdirectories of the same name. This way users can extend and override system
 **  themes."
 **
 ** Ref: http://standards.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html
 ***********************************************************************************/
OP_STATUS InputFileManager::InitIconSpecificDirs()
{
    // Clean out the string :
    m_home_icons_dir.Empty();

    // Local copies :
    OpString home_icons_dir;
    OpString home;
    home.Set(op_getenv("HOME"));

    // Make $HOME/.icons
    if( home.HasContent() )
    {
		home_icons_dir.AppendFormat(UNI_L("%s/.icons"), home.CStr());
    }

    // Set the .icon directory :

    if(FileHandlerManagerUtilities::IsDirectory(home_icons_dir))
    {
		m_home_icons_dir.Set(home_icons_dir.CStr());
    }


    // Get the rest of the pixmap directories
    GetDirs(m_data_dirs, m_pixmaps_dirs, UNI_L("pixmaps"));

    return OpStatus::OK;
}

/***********************************************************************************
 ** GetSubclassesFiles
 **
 ** Extracts the full path filenames of all files in the directories listed in dirs
 ** that are in subdirectory "mime/" and are called "subclasses" and places them in the
 ** vector files.
 ***********************************************************************************/
OP_STATUS InputFileManager::GetSubclassesFiles(OpVector<OpString>& files)
{
    return GetFiles(m_data_dirs, files, UNI_L("mime/"), UNI_L("subclasses"));
}


/***********************************************************************************
 ** GetAliasesFiles
 **
 ** Extracts the full path filenames of all files in the directories listed in dirs
 ** that are in subdirectory "mime/" and are called "aliases" and places them in the
 ** vector files.
 ***********************************************************************************/
OP_STATUS InputFileManager::GetAliasesFiles(OpVector<OpString>& files)
{
    return GetFiles(m_data_dirs, files, UNI_L("mime/"), UNI_L("aliases"));
}


/***********************************************************************************
 ** GetGlobsFiles
 **
 ** Extracts the full path filenames of all files in the directories listed in dirs
 ** that are in subdirectory "mime/" and are called "globs" and places them in the
 ** vector files.
 ***********************************************************************************/
OP_STATUS InputFileManager::GetGlobsFiles(OpVector<OpString>& files)
{
    return GetFiles(m_data_dirs, files, UNI_L("mime/"), UNI_L("globs"));
}


/***********************************************************************************
 ** GetMimeInfoFiles
 **
 ** Extracts the full path filenames of all files in the directories listed in dirs
 ** that are in subdirectory "applications/" and are called "mimeinfo.cache" and
 ** places them in the vector files.
 ***********************************************************************************/
OP_STATUS InputFileManager::GetMimeInfoFiles(OpVector<OpString>& files)
{
    return GetFiles(m_data_dirs, files, UNI_L("applications/"), UNI_L("mimeinfo.cache"));
}


/***********************************************************************************
 ** GetGnomeVFSFiles
 **
 ** Extracts the full path filenames of all files in the directories listed in dirs
 ** that are in subdirectory "application-registry/" and are called
 ** "gnome-vfs.applications" and places them in the vector files.
 ***********************************************************************************/
OP_STATUS InputFileManager::GetGnomeVFSFiles(OpVector<OpString>& files)
{
    return GetFiles(m_data_dirs, files, UNI_L("application-registry/"), UNI_L("gnome-vfs.applications"));
}


/***********************************************************************************
 ** GetDefaultsFiles
 **
 ** Extracts the full path filenames of all files in the directories listed in dirs
 ** that are in subdirectory "applications/" and are called "defaults.list" and
 ** places them in the vector files.
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::GetDefaultsFiles(OpVector<OpString>& profilerc_files,
											 OpVector<OpString>& files)
{
    RETURN_IF_ERROR(GetProfilercFiles(profilerc_files));
    return GetFiles(m_data_dirs, files, UNI_L("applications/"), UNI_L("defaults.list"));
}


/***********************************************************************************
 ** FindDesktopFile
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::FindDesktopFile(const OpStringC & input_filename,
											OpVector<OpString>& files)
{
    // Remove the full path if there :
    // ----------------------
    OpString filename;
    FileHandlerManagerUtilities::StripPath(filename, input_filename);

    // Look in the applications subdirectory :
    // ----------------------
    GetFiles(m_data_dirs, files, UNI_L("applications/"), filename.CStr());

    if(files.GetCount())
		return OpStatus::OK;

    // Look in the applications/kde subdirectory :
    // ----------------------
    GetFiles(m_data_dirs, files, UNI_L("applications/kde/"), filename.CStr());

    if(files.GetCount())
		return OpStatus::OK;

    // Look in the mimelnk/application subdirectory :
    // ----------------------
    OpString mimelnk_filename;
    mimelnk_filename.AppendFormat(UNI_L("x-%s"), filename.CStr());
    GetFiles(m_data_dirs, files, UNI_L("mimelnk/application/"), mimelnk_filename.CStr());

    if(files.GetCount())
		return OpStatus::OK;

    // Look in the applnk subdirectory :
    // ----------------------
    GetFiles(m_data_dirs, files, UNI_L("applnk/"), filename.CStr());

    if(files.GetCount())
		return OpStatus::OK;

    return OpStatus::OK;
}

/***********************************************************************************
 ** GetProfilercFiles
 **
 ** "Configuration files are located in a number of places, largely based on which
 ** distribution is being used. When an application attempts to find its configuration,
 ** it scans according to a predefined search order. The list of directories that are
 ** searched for config files is seen by using the command kde-config --path config.
 ** The directories shown actually are searched in the reverse order in which they are
 ** listed. This search order is put together by the following set of rules:
 **
 ** 1. /etc/kderc: a search path of directories can be specified within this file.
 **
 ** 2. KDEDIRS: a standard environment variable that is set to point KDE applications
 **    to the installation directories of KDE libraries and applications. It most
 **    likely already is set at login time. The installation directory of KDE
 **    automatically is appended to this list if it is not already present.
 **
 ** 3. KDEDIR: an older environment variable now considered deprecated in favor of
 **    KDEDIRS. If KDEDIRS is set, this variable is ignored for configuration.
 **
 ** 4. The directory of the executable file being run
 **
 ** 5. KDEHOME or KDEROOTHOME: usually set to ~/.kde. The former is for all users, and the latter is for root."
 **
 ** http://developer.kde.org/documentation/tutorials/kiosk/index.html
 ***********************************************************************************/
OP_STATUS InputFileManager::GetProfilercFiles(OpVector<OpString>& profilerc_files)
{
    OpString home;
    home.Set(op_getenv("HOME"));

    if( home.HasContent() )
    {
		OpString * user_profilerc = OP_NEW(OpString, ());

		if(!user_profilerc)
			return OpStatus::ERR_NO_MEMORY;

		user_profilerc->AppendFormat(UNI_L("%s/.kde/share/config/profilerc"), home.CStr());

		profilerc_files.Add(user_profilerc);
    }

    OpString kderc_filename;
    kderc_filename.Set("/etc/kderc");

    if(FileHandlerManagerUtilities::IsFile(kderc_filename))
    {
		KDErcFile kderc_file;
		kderc_file.Parse(kderc_filename);

		if(FileHandlerManagerUtilities::IsDirectory(kderc_file.GetDir()))
		{
			OpString * global_profilerc = OP_NEW(OpString, ());

			if(!global_profilerc)
				return OpStatus::ERR_NO_MEMORY;

			global_profilerc->AppendFormat(UNI_L("%s/share/config/profilerc"), kderc_file.GetDir().CStr());

			if(FileHandlerManagerUtilities::IsFile(*global_profilerc))
				profilerc_files.Add(global_profilerc);
			else
				OP_DELETE(global_profilerc);
		}
    }

    return OpStatus::OK;
}


/***********************************************************************************
 ** GetIndexThemeFiles
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::GetIndexThemeFiles(OpVector<OpString>& files)
{
    return GetFilesFromSubdirs(m_data_dirs, files, UNI_L("icons/"), UNI_L("index.theme"));
}


/***********************************************************************************
 ** GetMimetypeIconDesktopDirs
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::InitMimetypeIconDesktopDirs()
{
    return GetDirs(m_data_dirs, m_mimetype_desktop_dirs, UNI_L("mimelnk"));
}


/***********************************************************************************
 ** UseAliasesAndSubclasses
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::UseAliasesAndSubclasses(OpVector<OpString>&aliases_files,
													OpVector<OpString>&subclasses_files)
{
    UINT32 i = 0;

    UINT32 aliases_left    = 0;
    UINT32 subclasses_left = 0;

    UINT32 last_alias_count    = 0;
    UINT32 last_subclass_count = 0;

    //TODO:

    //This should be rewritten - the unmatched mimetypes
    //should be put in a vector and reprossesed there
    //it is not necessary to reread the file.

    // TODO:  while(true) and break when the set of unmatched
    //        nodes did not change in an iteration

    for(INT32 c = 0; c < 2; c++)
    {
		last_alias_count    = aliases_left;
		last_subclass_count = subclasses_left;

		aliases_left    = 0;
		subclasses_left = 0;

		// Read aliases files:
		// -------------------

		for(i = 0; i < aliases_files.GetCount(); i++)
		{
			AliasesFile alias_file;
			alias_file.Parse(*aliases_files.Get(i));
			aliases_left = alias_file.GetItemsLeft();
		}

		// Read Subclasses files:
		// ----------------------

		for(i = 0; i < subclasses_files.GetCount(); i++)
		{
			SubclassesFile subclasses_file;
			subclasses_file.Parse(*subclasses_files.Get(i));
			subclasses_left = subclasses_file.GetItemsLeft();
		}

		if((aliases_left == 0 && subclasses_left == 0) ||
		   (aliases_left == last_alias_count && subclasses_left == last_subclass_count))
			break;
    }

    return OpStatus::OK;
}

/***********************************************************************************
 ** ParseMailcapFiles
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::ParseMailcapFiles()
{
    OpString etc_mailcap_string;
    OpString home_mailcap_string;

    // Get the global mailcap :
    // ------------------------

    etc_mailcap_string.Set("/etc/mailcap");

    // Get the user specific mailcap :
    // -------------------------------

    OpString home;
    home.Set(op_getenv("HOME"));

    if( home.HasContent() )
    {
		home_mailcap_string.AppendFormat(UNI_L("%s/.mailcap"), home.CStr());
    }

    // Parse the user mailcap:
    // -----------------------

    if(FileHandlerManagerUtilities::IsFile(home_mailcap_string))
    {
		MailcapFile mailcap_file;
		mailcap_file.Parse(home_mailcap_string);
    }

    // Parse the global mailcap:
    // -------------------------

    if(FileHandlerManagerUtilities::IsFile(etc_mailcap_string))
    {
		MailcapFile mailcap_file;
		mailcap_file.Parse(etc_mailcap_string);
    }

    return OpStatus::OK;
}


/***********************************************************************************
 ** GetFilesFromSubdirs
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::GetFilesFromSubdirs(OpVector<OpString>& dirs,
												OpVector<OpString>& files,
												const OpStringC & subdir,
												const OpStringC & filename)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //Subdirectory cannot be null
    OP_ASSERT(subdir.HasContent());
    if(subdir.IsEmpty())
		return OpStatus::ERR;

    //File name cannot be null
    OP_ASSERT(filename.HasContent());
    if(filename.IsEmpty())
		return OpStatus::ERR;
    //-----------------------------------------------------

    OpAutoVector<OpString> subdirs;

    UINT32 i = 0;

    // Get the subdirectories :
    // ----------------------

    for(i = 0; i < dirs.GetCount(); i++)
    {
		OpString * dir_str = dirs.Get(i);

		OpString dirName;
		dirName.AppendFormat(UNI_L("%s/%s"), dir_str->CStr(), subdir.CStr());

		if(FileHandlerManagerUtilities::IsDirectory(dirName))
		{
			GetSubfolders(dirName.CStr(), subdirs);
		}
    }

    // Get the files :
    // ----------------------
	GetFiles(subdirs, files, UNI_L(""), filename);

    return OpStatus::OK;
}

/***********************************************************************************
 ** GetSubfolders
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::GetSubfolders(const OpStringC & dir,
										  OpVector<OpString>& dirs)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    OP_ASSERT(dir.HasContent());
    if(dir.IsEmpty())
		return OpStatus::ERR;
    //-----------------------------------------------------

    OP_STATUS status = OpStatus::OK;

    OpFolderLister* folder_lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), dir.CStr());

    while( folder_lister ) // We need a loop and a NULL pointer test
    {
		if( folder_lister->IsFolder() )
		{
			OpString * full_path = OP_NEW(OpString, ());

			if(!full_path)
			{
				status = OpStatus::ERR_NO_MEMORY;
				break;
			}

			full_path->AppendFormat(UNI_L("%s%s/"), dir.CStr(), folder_lister->GetFileName());
			dirs.Add(full_path);
		}

		if(!folder_lister->Next())
			break;
    }

    OP_DELETE(folder_lister);

    return status;
}

/***********************************************************************************
 ** GetDirs
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::GetDirs(OpVector<OpString>& dirs,
									OpVector<OpString>& subdirs,
									const OpStringC & subdir)
{
    OP_STATUS status = OpStatus::OK;

    for(UINT32 i = 0; i < dirs.GetCount(); i++)
    {
		OpString * dir_str = dirs.Get(i);

		OpString dirName;
		dirName.Set(dir_str->CStr());

		if(subdir.HasContent())
		{
			FileHandlerManagerUtilities::RemoveSuffix(dirName, UNI_L("/")); //Just in case
			dirName.Append("/");
			dirName.Append(subdir);
		}

		if(FileHandlerManagerUtilities::IsDirectory(dirName))
		{
			OpString * dir = OP_NEW(OpString, ());

			if(!dir)
			{
				status = OpStatus::ERR_NO_MEMORY;
				break;
			}

			dir->AppendFormat(UNI_L("%s/"), dirName.CStr());
			subdirs.Add(dir);
		}
    }

    return status;
}

/***********************************************************************************
 ** GetFiles
 **
 ** Appends subdir and filename to all dirs in dirs and checks whether this is a
 ** regular file with stat. If so the filename is appended to files.
 **
 ** NOTE: The strings returned in files are the caller's resposibility to delete
 ***********************************************************************************/
OP_STATUS InputFileManager::GetFiles(OpVector<OpString>& dirs,
									 OpVector<OpString>& files,
									 const OpStringC & subdir,
									 const OpStringC & filename)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //File name has to have content
    OP_ASSERT(filename.HasContent());
    if(filename.IsEmpty())
		return OpStatus::ERR;
    //-----------------------------------------------------

    OP_STATUS status = OpStatus::OK;

    for(UINT32 i = 0; i < dirs.GetCount(); i++)
    {
		OpString * dir_str = dirs.Get(i);

		OpString dirName;
		dirName.Set(dir_str->CStr());

		if(subdir.HasContent())
		{
			dirName.Append("/");
			dirName.Append(subdir);
		}

		if(FileHandlerManagerUtilities::IsDirectory(dirName))
		{
			OpString fileName;
			fileName.Set(dirName);
			fileName.Append(filename);

			if(FileHandlerManagerUtilities::IsFile(fileName))
			{
				OpString * file = OP_NEW(OpString, ());

				if(!file)
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}

				file->Set(fileName.CStr());
				files.Add(file);
			}
		}
    }

    return status;
}

/***********************************************************************************
 ** MakeIconPath
 **
 **
 **
 ** client code must delete the returned string
 ***********************************************************************************/
OP_STATUS InputFileManager::MakeIconPath(FileHandlerNode * node,
										 OpString & icon_path,
										 UINT32 icon_size)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //Application must be set
    OP_ASSERT(node);
    if(!node)
		return OpStatus::ERR;

	// If it has one, just return that one
	if(node->HasIcon(icon_size) == (BOOL3) TRUE)
		return icon_path.Set(node->GetIconPath(icon_size));

	// If it doesn't then don't try again
	if(node->HasIcon(icon_size) != MAYBE)
		return OpStatus::OK;

    const uni_char * subdir = 0;

    if(node->GetType() == APPLICATION_NODE_TYPE)
		subdir = UNI_L("/apps/");
    else if(node->GetType() == MIME_TYPE_NODE_TYPE)
		subdir = UNI_L("/mimetypes/");
    else
		return OpStatus::ERR;

    //-----------------------------------------------------

    const OpStringC icon = node->GetIcon();

    if(icon.IsEmpty())
    {
		return OpStatus::OK;
    }

    uni_char * iconfile = 0;
    OpString iconfile_path;

    if(!iconfile && GetIconThemePath().HasContent())
    {
		MakeIconPathSimple(icon, GetIconThemePath(), iconfile_path, icon_size, subdir);

		if(FileHandlerManagerUtilities::IsFile(iconfile_path))
		{
			iconfile = iconfile_path.CStr();
		}
    }

    if(!iconfile && GetIconThemeBackupPath().HasContent())
    {
		//TODO - search themes this theme inherits from
		MakeIconPathSimple(icon, GetIconThemeBackupPath(), iconfile_path, icon_size, subdir);

		if(FileHandlerManagerUtilities::IsFile(iconfile_path))
		{
			iconfile = iconfile_path.CStr();
		}
    }

    //Maybe its a full path
    if(!iconfile)
    {
		iconfile_path.Set(icon);

		if(FileHandlerManagerUtilities::IsFile(iconfile_path))
		{
			iconfile = iconfile_path.CStr();
		}
    }

    //Last ditch attempt - go look in pixmaps
    if(!iconfile)
    {
		// Make filename
		OpString iconfile_name;
		iconfile_name.Append(icon);
		FileHandlerManagerUtilities::RemoveSuffix(iconfile_name, UNI_L(".png")); //if it's already there take it away
		iconfile_name.Append(".png");

		// Go look for it in the pixmaps directories
		OpAutoVector<OpString> files;
		GetFiles(m_pixmaps_dirs, files, 0, iconfile_name.CStr());

		if(files.GetCount())
			return icon_path.Set(files.Get(0)->CStr());

		node->SetHasIcon((BOOL3) FALSE);
    }

    return icon_path.Set(iconfile);
}

/***********************************************************************************
 ** MakeIconPathSimple
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::MakeIconPathSimple(const OpStringC & icon,
											   const OpString& path,
											   OpString & icon_path,
											   UINT32 icon_size,
											   const OpStringC & subdir)
{
    //BUILD PATH TO ICON:

    // 0. Get pixel string:
    //    -> Must have pixel string to obtain directory
    //-----------------------------------------------------
    OpString pixel_str;
    RETURN_IF_ERROR(FileHandlerManagerUtilities::MakePixelString(icon_size, pixel_str));
    //-----------------------------------------------------

    // 5. Assemble the sting:
    //-----------------------------------------------------
    OpString iconfile_path;
    iconfile_path.Set(path.CStr());
    iconfile_path.Append(pixel_str.CStr());
    iconfile_path.Append(subdir);
    iconfile_path.Append(icon);
    FileHandlerManagerUtilities::RemoveSuffix(iconfile_path, UNI_L(".png")); //if it's already there take it away
    iconfile_path.Append(".png");
    //-----------------------------------------------------

    return icon_path.Set(iconfile_path.CStr());
}

/***********************************************************************************
 ** TryExec
 **
 **
 **
 **
 ***********************************************************************************/
BOOL InputFileManager::TryExec(const OpStringC & try_exec,
							   OpString & full_path)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //try_exec cannot be null
    OP_ASSERT(try_exec.HasContent());
    if(try_exec.IsEmpty())
		return FALSE;
    //-----------------------------------------------------

    return FileHandlerManagerUtilities::ExpandPath( try_exec.CStr(), X_OK, full_path );
}

/***********************************************************************************
 ** LoadNode
 **
 **
 **
 **
 ***********************************************************************************/
void InputFileManager::LoadNode(FileHandlerNode* node)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //node pointer cannot be null
    OP_ASSERT(node);
    if(!node)
		return;
    //-----------------------------------------------------

    if(node->GetType() == APPLICATION_NODE_TYPE)
		LoadApplicationNode((ApplicationNode*) node);
    else if(node->GetType() == MIME_TYPE_NODE_TYPE)
		LoadMimeTypeNode((MimeTypeNode*) node);
}

/***********************************************************************************
 ** LoadMimeTypeNode
 **
 **
 **
 **
 ***********************************************************************************/
void InputFileManager::LoadMimeTypeNode(MimeTypeNode* node)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //node pointer cannot be null
    OP_ASSERT(node);
    if(!node)
		return;
    //-----------------------------------------------------

    UINT32 i = 0;

    if(node->HasDesktopFile() == MAYBE)
    {
		for(i = 0; i < m_mimetype_desktop_dirs.GetCount(); i++)
		{
			OpString * subdir = m_mimetype_desktop_dirs.Get(i);

			// Try to find an icon for one of the mimetype names
			for(unsigned int j = 0; j < node->GetMimeTypes().GetCount(); j++)
			{
				// Get the mimetype :
				OpString * mime_type = node->GetMimeTypes().Get(j);

				OpString path;
				path.AppendFormat(UNI_L("%s%s.desktop"), subdir->CStr(), mime_type->CStr());

				if(FileHandlerManagerUtilities::IsFile(path))
				{
					DesktopFile desktop_file = DesktopFile(node);
					desktop_file.Parse(path);

					node->SetHasDesktopFile((BOOL3) TRUE);
					node->SetDesktopFileName(path);

					break;
				}
			}
		}

		if(node->HasDesktopFile() == MAYBE)
			node->SetHasDesktopFile((BOOL3) FALSE);

		OpAutoVector<OpString> files;

		// Try to find an icon for one of the mimetype names
		for(i = 0; i < node->GetMimeTypes().GetCount(); i++)
		{
			// Get the mimetype :
			OpString * mime_type = node->GetMimeTypes().Get(i);

			OpString filename;
			filename.AppendFormat(UNI_L("%s.xml"), mime_type->CStr());

			GetFiles(m_data_dirs, files, UNI_L("mime/"), filename.CStr());
		}

		for(i = 0; i < files.GetCount(); i++)
		{
			MimeXMLFile mime_xml_file = MimeXMLFile(node);
			mime_xml_file.Parse(*files.Get(i));
		}
    }
}

/***********************************************************************************
 ** LoadMimeTypeNodeIcon
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::LoadIcon(FileHandlerNode* node,
									 UINT32 icon_size)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //node pointer cannot be null
    OP_ASSERT(node);
    if(!node)
		return OpStatus::ERR;
    //-----------------------------------------------------

    //The path to be returned :
    OpString iconfile;

    if(node && !node->GetIconPath(icon_size))
    {
		//Find the desktopfile and parse it if it exists:
		if(node->HasDesktopFile() == MAYBE)
		{
			LoadNode(node);
		}

		OpString icon;

		if(node->GetIcon().IsEmpty())
		{
			if(node->GetType() == APPLICATION_NODE_TYPE)
			{
				ApplicationNode * app_node = (ApplicationNode *) node;
				app_node->GuessIconName(icon);

				node->SetIcon(icon.CStr());

				MakeIconPath(node, iconfile, icon_size);
			}
			else if(node->GetType() == MIME_TYPE_NODE_TYPE)
			{
				MimeTypeNode* mime_node = (MimeTypeNode*) node;

				// Try to find an icon for one of the mimetype names
				for(unsigned int i = 0; i < mime_node->GetMimeTypes().GetCount() && iconfile.IsEmpty(); i++)
				{
					// Get the mimetype :
					OpString * mime_type = mime_node->GetMimeTypes().Get(i);

					FileHandlerManagerUtilities::StripPath(icon, *mime_type);

					node->SetIcon(icon.CStr());

					MakeIconPath(node, iconfile, icon_size);
				}
			}
		}
		else
		{
			MakeIconPath(node, iconfile, icon_size);
		}

		if(iconfile.IsEmpty())
		{
			if(node->GetType() == MIME_TYPE_NODE_TYPE)
			{
				MimeTypeNode* mime_node = (MimeTypeNode*) node;
				mime_node->GetDefaultIconName(icon);
				node->SetIcon(icon.CStr());
				MakeIconPath(node, iconfile, icon_size);
			}
		}

		if(iconfile.HasContent())
		{
			node->SetIconPath(iconfile.CStr(), icon_size);
		}
    }

    return OpStatus::OK;
}


/***********************************************************************************
 ** LoadApplicationNode
 **
 **
 **
 **
 ***********************************************************************************/
void InputFileManager::LoadApplicationNode(ApplicationNode* node)
{

    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //Application node pointer cannot be null
    OP_ASSERT(node);
    if(!node)
		return;
    //-----------------------------------------------------

    if(node->HasDesktopFile() == FALSE || node->HasDesktopFile() == TRUE)
    {
		return; // The file (if it exists) has already been parsed
    }
    else if(node->HasDesktopFile() == MAYBE)
    {
		const OpStringC path     = node->GetPath();
		const OpStringC filename = node->GetDesktopFileName();

		//Path and filename must be set - but if this is a mimecap node - there is no desktop file
		if(path.IsEmpty() || filename.IsEmpty())
		{
			node->SetHasDesktopFile((BOOL3) FALSE);
			return;
		}

		OpString full_name;
		full_name.Set(path);

		OpString file_name;
		file_name.Set(filename);

		//Note : This is a KDE specific hack
		FileHandlerManagerUtilities::ReplacePrefix(file_name, UNI_L("kde-"), UNI_L("kde/"));

		full_name.Append(file_name);

		DesktopFile desktop_file = DesktopFile(node);
		desktop_file.Parse(full_name);
		node->SetHasDesktopFile((BOOL3) TRUE);

		if(node->HasDesktopFile() == MAYBE)
			node->SetHasDesktopFile((BOOL3) FALSE);
    }
}

/***********************************************************************************
 ** SortThemes
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::SortThemes(OpVector<OpString>& theme_files)
{
	// Try to guess good themes :

	OpString fav_theme;
	OpString back_theme;

    if (FileHandlerManagerUtilities::isKDERunning())
		fav_theme.Set(UNI_L("crystalsvg"));
    else if (FileHandlerManagerUtilities::isGnomeRunning())
		fav_theme.Set(UNI_L("gnome"));

	back_theme.Set(UNI_L("hicolor"));

	// See if those are among the ones you have found :

	OpString* main_theme   = 0;
	OpString* backup_theme = 0;

	for(unsigned int i = 0; i < theme_files.GetCount(); i++)
	{
		// If we don't have one already, check if this is our main choice
		if(!main_theme && fav_theme.HasContent() && theme_files.Get(i)->Find(fav_theme.CStr()) != KNotFound)
			main_theme = theme_files.Get(i);

		// If we don't have one already, check if this is our backup choice
		if(!backup_theme && back_theme.HasContent() && theme_files.Get(i)->Find(back_theme.CStr()) != KNotFound)
			backup_theme = theme_files.Get(i);
	}

	// If you didn't find your main one, try the backup
	if(!main_theme)
	{
		main_theme   = backup_theme;
		backup_theme = 0;
	}

	// If you found a backup insert it first
	if(backup_theme)
	{
		theme_files.RemoveByItem(backup_theme);
		theme_files.Insert(0, backup_theme);
	}

	// If you found the main one insert it first
	if(main_theme)
	{
		theme_files.RemoveByItem(main_theme);
		theme_files.Insert(0, main_theme);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 ** ParseIndexThemeFile
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS InputFileManager::InitTheme(OpVector<OpString>& theme_files)
{
	if(theme_files.GetCount() == 0)
		return OpStatus::OK;

	RETURN_IF_ERROR(SortThemes(theme_files));

	// Process the main theme file
	// ----------------------
	OpString * themefile = theme_files.Get(0);

	IndexThemeFile index_theme_file(&m_theme_node);
	RETURN_IF_ERROR(index_theme_file.Parse(*themefile));

	// Set the path to the index.theme file
	RETURN_IF_ERROR(m_theme_node.SetIndexThemeFile(themefile->CStr()));

	if(theme_files.GetCount() == 1)
		return OpStatus::OK;

	// Process the backup theme file
	// ----------------------

	OpString * backup_themefile = theme_files.Get(1);

	IndexThemeFile index_theme_backup(&m_backup_theme_node);
	RETURN_IF_ERROR(index_theme_backup.Parse(*backup_themefile));

	// Set the path to the index.theme file
	RETURN_IF_ERROR(m_backup_theme_node.SetIndexThemeFile(backup_themefile->CStr()));

    return OpStatus::OK;
}
