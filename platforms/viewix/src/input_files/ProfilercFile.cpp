/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "platforms/viewix/FileHandlerManager.h"
#include "platforms/viewix/src/input_files/ProfilercFile.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 ** ParseDefaultFile
 **
 **
 **
 ** FOR KDE : ${HOME}/.kde/share/config/profilerc
 ***********************************************************************************/
OP_STATUS ProfilercFile::ParseInternal(OpFile & file)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //File pointer cannot be null
    OP_ASSERT(file.IsOpen());
    if(!file.IsOpen())
		return OpStatus::ERR;
    //-----------------------------------------------------
	
    //Split file into lines - only if smaller than 10MB
    UINT32 max_size = 10000000;
    OpAutoVector<OpString> list;
    FileHandlerManagerUtilities::FileToLineVector(&file, list, max_size);
	
    OpString mime_type;
    OpString desktop_file;
	
    unsigned int priority = 0;
    OP_STATUS status = OpStatus::OK;
	
    for(UINT32 i = 0; i < list.GetCount(); i++)
    {
		OpString* line = list.Get(i);
		line->Strip();
		
		// Seperator line :
		if(line->IsEmpty())
		{
			continue;
		}
		
		if(ParsedMime(*line, mime_type, priority, status))
			continue;
		
		RETURN_IF_ERROR(status);
		
		OpAutoVector<OpString> items;
		FileHandlerManagerUtilities::SplitString(items, *line, '=');
		
		if(items.GetCount() == 2)
		{
			items.Get(0)->Strip();
			items.Get(1)->Strip();
			
			OpString * key   = items.Get(0);
			OpString * value = items.Get(1);
			
			MimeTypeNode* node = FileHandlerManager::GetManager()->MakeMimeTypeNode(mime_type.CStr());
			
			if(uni_strncmp(key->CStr(), UNI_L("Application"), 11) == 0)
			{
				desktop_file.Set(value->CStr());
			}
			else if(uni_strncmp(key->CStr(), UNI_L("GenericServiceType"), 18) == 0)
			{
				if(uni_strncmp(value->CStr(), UNI_L("Application"), 11) == 0)
				{
					FileHandlerManager::GetManager()->InsertIntoApplicationList(node,
																				desktop_file.CStr(),
																				0,
																				0,
																				(priority == 1));
				}
				else
				{
					desktop_file.Empty();
				}
			}
		}
		else
		{
			//Error in reading line - should have two elements
			OP_ASSERT(FALSE);
		}
    }
	
    return OpStatus::OK;
}

BOOL ProfilercFile::ParsedMime(OpString & line,
							   OpString & mime_type,
							   unsigned int & priority,
							   OP_STATUS & status)
{
    status = OpStatus::OK;
	
    if (!mime_type.Reserve(256))
    {
		status = OpStatus::ERR_NO_MEMORY;
		return FALSE;
    }
	
    if (uni_sscanf(line.CStr(), UNI_L("[%256s - %d]"), mime_type.CStr(), &priority) == 2)
    {
		return TRUE;
    }
	
    return FALSE;
}
