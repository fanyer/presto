/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/cache/cache_int.h"
#include "modules/cache/cache_common.h"

void us_ascii_str_lower(char *str)
{
	if(str == NULL)
		return;

	char tmp;
	while((tmp = *str) != '\0')
	{
		if(tmp >= 'A' && tmp <= 'Z')
			*str = tmp | 0x20; // Lowercasing the character
		str++;
	}
}


#if defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)
FileNameElement::FileNameElement()
{
	file_size = 0;
	folder=OPFILE_CACHE_FOLDER;
}


OP_STATUS FileNameElement::Init(const OpStringC &name, const OpStringC &pname, OpFileFolder file_folder)
{
	OP_STATUS op_err = filename.Set(name);//FIXME:OOM
	if(OpStatus::IsSuccess(op_err))
		op_err = pathfilename.Set(pname);//FIXME:OOM
	if(OpStatus::IsSuccess(op_err))
	{
		op_err = linkid.SetUTF8FromUTF16(filename.CStr());
		linkid.MakeUpper();
	}
	
	folder=file_folder;

	return op_err;
}

#if !defined(SEARCH_ENGINE_CACHE)
#ifdef UNUSED_CODE
OP_STATUS FileName_Store::Create(FileName_Store **filename_store,
								 unsigned int size)
{
	*filename_store = OP_NEW(FileName_Store, (size));
	if (*filename_store == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS op_err = (*filename_store)->Construct();

	if (OpStatus::IsError(op_err))
	{
		OP_DELETE(*filename_store);
		*filename_store = NULL;
	}
	return op_err;
}
#endif // UNUSED_CODE

FileNameElement* FileName_Store::RetrieveFileName(const OpStringC &name, const OpStringC &pname/*, BOOL create*/)
{
	OP_ASSERT(is_initialized);

	OpString8 temp_name;

	temp_name.SetUTF8FromUTF16(name);
	temp_name.MakeUpper();

	FileNameElement* filename = (FileNameElement*) GetLinkObject(temp_name.CStr());
	/*if (!filename && create)
	{
		filename = OP_NEW(FileNameElement, ());

		if(filename)
		{
			OP_STATUS err = filename->InitL(name, pname);
			if(OpStatus::IsError(err))
			{
				OP_DELETE(filename);
				filename = NULL;
 				if (OpStatus::IsMemoryError(err))
					g_memory_manager->RaiseCondition(err);
 				return NULL;
			}

#if 0
			OP_ASSERT(GetLinkObject(filename->LinkId()) == NULL);
			OpString8 tempnam;
			tempnam.SetUTF8FromUTF16(name.CStr());
			PrintfTofile("filelist.txt", "Created File item %S (%s)\n\n", filename->LinkId(), tempnam.CStr());
#endif

			AddLinkObject(filename);
		}
		else
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return NULL;
		}
	}*/
	return filename;
}

void Context_Manager::ReadDCacheDir(FileName_Store &filenames, FileName_Store &associated, OpFileFolder specialFolder, BOOL check_generations, BOOL check_associated_files, const uni_char *sub_folder, int max_files, BOOL skip_directories, BOOL check_sessions)
{
	OP_NEW_DBG("Context_Manager::ReadDCacheDir", "cache.name");
	OP_DBG((UNI_L("*** Look for files in folder %d - %s - gen: %d - assoc: %d"), specialFolder, sub_folder, check_generations, check_associated_files));

	// Generations are checked calling ReadDCacheDir for each directory, disabling generations checking
	#if defined(CACHE_MULTIPLE_FOLDERS)
		if(check_generations)
		{
			OpFolderLister *dirfiles;	
			OpString pattern;
			uni_char *full_path=NULL;
			OpString full_path_str;
			OpFileFolder folder=specialFolder;

			OP_ASSERT(skip_directories); // generations are not intended to be checked also for directories, even if it should not be a problem

			// All the directories of the generational cache
			RETURN_VOID_IF_ERROR(pattern.SetConcat(UNI_L("g_"), UNI_L(SYS_CAP_WILDCARD_SEARCH)));

			// Switch to absolute path, as Linux seems to behave differently that Windows, not accepting wilcards with paths...
			if(sub_folder)
			{
				OpString tmp_storage;
				full_path_str.AppendFormat(UNI_L("%s%s"), g_folder_manager->GetFolderPathIgnoreErrors(specialFolder, tmp_storage), sub_folder);
				full_path=full_path_str.CStr();
				folder=OPFILE_ABSOLUTE_FOLDER;
				OP_DBG((UNI_L("*** Full path: %s"), full_path));
			}

			dirfiles = OpFile::GetFolderLister(folder, pattern.CStr(), full_path);

			while(dirfiles && dirfiles->Next())
			{
				if (!dirfiles->IsFolder())
					continue;
				
				if(sub_folder)
				{
					OpString folder_path;

					folder_path.AppendFormat(UNI_L("%s%s%s"), sub_folder, UNI_L(PATHSEP), dirfiles->GetFileName());
					ReadDCacheDir(filenames, associated, specialFolder, FALSE, FALSE, folder_path.CStr(), max_files);
				}
				else
					ReadDCacheDir(filenames, associated, specialFolder, FALSE, FALSE, dirfiles->GetFileName(), max_files);
			}

			// Required to count files associated to temporary URLs, for HTTPS testcases
			if(check_sessions && !check_associated_files && sub_folder)
			{
				OpString folder_path;

				folder_path.AppendFormat(UNI_L("%s%s%s"), sub_folder, UNI_L(PATHSEP), CACHE_SESSION_FOLDER);
				ReadDCacheDir(filenames, associated, specialFolder, FALSE, FALSE, folder_path.CStr(), max_files, skip_directories, FALSE);
			}
			
			OP_DELETE(dirfiles);
		}
	#endif
			
	// Count the files in each directory
	OpFolderLister *dirfiles;	
	OpString pattern;
	OpString name_with_folder;

	RETURN_VOID_IF_ERROR(pattern.SetConcat(UNI_L("opr"), UNI_L(SYS_CAP_WILDCARD_SEARCH)));

	dirfiles = OpFile::GetFolderLister(specialFolder, pattern.CStr(), sub_folder);

	if(!dirfiles)
		return;

	while(dirfiles->Next() && (max_files<0 || (int)filenames.LinkObjectCount()<max_files))
	{
		if (skip_directories && dirfiles->IsFolder())
			continue;

        FileNameElement *elm = OP_NEW(FileNameElement, ());
        const uni_char *file_name;

		if(sub_folder)
		{
			name_with_folder.Set(sub_folder);
			name_with_folder.AppendFormat(UNI_L("%c%s"), PATHSEPCHAR, dirfiles->GetFileName());
			file_name=name_with_folder.CStr();
		}
		else
			file_name=dirfiles->GetFileName();
		// Assumption: no two files will have the same name, even during multiple reads.
		if(elm == NULL || OpStatus::IsError(elm->Init(file_name, dirfiles->GetFullPath(), specialFolder)) )
		{
			OP_DELETE(elm);
			OP_DELETE(dirfiles);
			return;
		}
		
		elm->SetFileSize(dirfiles->GetFileLength());
		filenames.AddFileName(elm);
	}

	OP_DELETE(dirfiles);

#ifdef URL_ENABLE_ASSOCIATED_FILES
	// Associated files are checked calling ReadDCacheDir for each directory, enabling generations checking (which will call another level of ReadDCacheDir)
	if(check_associated_files)
	{
		OpFolderLister *dirfiles;	
		OpString pattern;

		OP_ASSERT(skip_directories); // generations are not intended to be checked also for directories, even if it should not be a problem

		// All the directories of the generational cache
		RETURN_VOID_IF_ERROR(pattern.SetConcat(UNI_L("asso"), UNI_L(SYS_CAP_WILDCARD_SEARCH)));

		dirfiles = OpFile::GetFolderLister(specialFolder, pattern.CStr(), NULL);

		while(dirfiles && dirfiles->Next())
		{
			if (!dirfiles->IsFolder())
				continue;
				
			// It's important that the first parameter is associated, as this is the array that we want to really fill
			ReadDCacheDir(associated, associated, specialFolder, TRUE, FALSE, dirfiles->GetFileName(), max_files, skip_directories, check_sessions);
		}
		
		OP_DELETE(dirfiles);
	}
#endif

#if 0
	{
		PrintfTofile("filelist.txt", "Starting Cache File list\n");
		FileNameElement *elm = filenames.GetFirstFileName();
		while(elm)
		{
			PrintfTofile("filelist.txt", "Cache File %s\n", elm->LinkId());
			elm = filenames.GetNextFileName();
		}
		PrintfTofile("filelist.txt", "end of Cache File list\n\n");
	}
#endif
}

#endif // !defined(SEARCH_ENGINE_CACHE)
#endif // defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)

