/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/cache/cache_common.h"
#ifdef SEARCH_ENGINE_CACHE
#include "modules/url/url_stor.h"
#endif

// *** Changes to this function need to be ported to URL_Store::AppendFileName()
void IncFileString(uni_char* file_str, int max, int min, int i)
{
	while(i <= max && i >= min)
	{
		uni_char val = file_str[i];
		if (val == '-' || val == '_') // GI: need this in ComposeInlineFilename() function
		{
			val = '0';
		}
		else if (val == '9')
		{
			val = 'A';
		}
		else if (val == 'Z' || val == 'z')
		{
			file_str[i] = '0';
			i--;
			continue;
		}
		else
			 val ++;
		file_str[i] = val;
		break;
	}
}



#ifndef SEARCH_ENGINE_CACHE
void Context_Manager::IncFileStr()
{
	IncFileString(file_str, 7, 3, 7);
}
#endif

#ifdef CACHE_MULTIPLE_FOLDERS
OP_STATUS Context_Manager::GetNameWithFolders(OpString &full_name, BOOL session_only, const uni_char *file_name)
{
	OP_ASSERT(file_name);
	
	if(!file_name)
		return OpStatus::ERR_NULL_POINTER;
		
	if (session_only)
		return full_name.AppendFormat(UNI_L("%s%c%s"), CACHE_SESSION_FOLDER, PATHSEPCHAR, file_name);
		
	INT32 n=0;

	// Get "n" from the cache file name
	int cur=7;
	UINT32 mul=1;
	
	while(cur>=3)
	{
		uni_char c=file_name[cur];
		
		cur--;
		if(c>='0' && c<='9')
		{
			n+=(c-'0')*mul;
			mul*=36;
		}
		else if(c>='A' && c<='Z')
		{
			n+=(10+c-'A')*mul;
			mul*=36;
		}
		else
		{
			OP_ASSERT(FALSE);
		}
	}

	/*
		The directory number is in range 0 .. 799, for a cache with a single level of directories, with no more that 128 files.
	 */
	int num_dir=(n/GEN_CACHE_FILES_PER_GENERATION)%GEN_CACHE_GENERATIONS;
	
	return full_name.AppendFormat(UNI_L("g_%.04X%c%s"), num_dir, PATHSEPCHAR, file_name);
}
#endif

OP_STATUS Context_Manager::GetNewFileNameCopy(OpStringS &name, const uni_char* ext, OpFileFolder &folder, BOOL session_only
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
					,BOOL useOperatorCache
#endif
)
{
#ifdef _DEBUG_URLMAN_CHECK
	Check();
#endif

	if(context_id != 0)
	{
#ifdef DISK_CACHE_SUPPORT
		folder = (cache_loc != OPFILE_ABSOLUTE_FOLDER ? cache_loc : OPFILE_CACHE_FOLDER);
#else
		folder = OPFILE_ABSOLUTE_FOLDER;
#endif
	}
	else
	{
		folder = (
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
			useOperatorCache ? OPFILE_OCACHE_FOLDER :
#endif
			OPFILE_CACHE_FOLDER);
	}

	name.Empty();
	
#ifdef SEARCH_ENGINE_CACHE
	return url_store->AppendFileName(&name, GetDCacheSize(), session_only
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		, useOperatorCache
#endif
		);
#else

	IncFileStr();
	
#ifdef CACHE_MULTIPLE_FOLDERS
	if(enable_directories)
		RETURN_IF_ERROR(GetNameWithFolders(name, session_only, file_str));
	else
#endif // CACHE_MULTIPLE_FOLDERS
		RETURN_IF_ERROR(name.Set(file_str));
	
	while (true)
	{
		OpFile file;
		OP_STATUS op_err = file.Construct(name.CStr(), folder);
		
		if (OpStatus::IsSuccess(op_err))
		{
#ifdef URL_CHECK_CACHE_FILES
			BOOL exists = FALSE;
			RETURN_IF_ERROR(file.Exists(exists));

			if (exists)
			{
				if(name.CStr())
					name[0]=0; // Faster than Empty()

				IncFileStr();
			#ifdef CACHE_MULTIPLE_FOLDERS
				if(enable_directories)
					RETURN_IF_ERROR(GetNameWithFolders(name, session_only, file_str));
				else
			#endif // CACHE_MULTIPLE_FOLDERS
					RETURN_IF_ERROR(name.Set(file_str));
			}
			else
			{
				RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
				file.Close();
				break;
			}
#else //URL_CHECK_CACHE_FILES
			break;
#endif //URL_CHECK_CACHE_FILES
		}
		else if(OpStatus::IsMemoryError(op_err))
			return op_err;
	}

	return OpStatus::OK;
#endif  // SEARCH_ENGINE_CACHE
}

