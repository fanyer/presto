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
#include "platforms/viewix/src/input_files/DefaultListFile.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 ** ParseDefaultFile
 **
 **
 **
 ** FOR KDE : ${HOME}/.kde/share/config/profilerc
 ***********************************************************************************/
OP_STATUS DefaultListFile::ParseInternal(OpFile & file)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //File pointer cannot be null
    OP_ASSERT(file.IsOpen());
    if(!file.IsOpen())
		return OpStatus::ERR;
    //-----------------------------------------------------
	
    //Get path to this directory - desktop files will be located here
    OpString file_path;
    file_path.Set(file.GetFullPath());
    FileHandlerManagerUtilities::RemoveSuffix(file_path, UNI_L("defaults.list"));
	
    //Split file into lines - only if smaller than 10MB
    UINT32 max_size = 10000000;
    OpAutoVector<OpString> list;
    FileHandlerManagerUtilities::FileToLineVector(&file, list, max_size);
	
    for(UINT32 i = 1; i < list.GetCount(); i++)
    {
		OpString* line = list.Get(i);
		OpAutoVector<OpString> items;

		FileHandlerManagerUtilities::SplitString(items, *line, '=');
		
		if(items.GetCount() == 2)
		{
			OpString * mime_type_str = items.Get(0);
			OpString * desktop_list  = items.Get(1);
			uni_char * mime_type     = mime_type_str->CStr();
			
			//See if mime-type is already registered
            //TODO - According to the standard if a mime-type is mentioned further
            //       down in the same file the last entry should take precedence.
            //       This is not implemented.
			MimeTypeNode* node = FileHandlerManager::GetManager()->MakeMimeTypeNode(mime_type);
			
			//Split handler list - NOTE - this should (according to the standard) not be a list
			OpAutoVector<OpString> list_items;

			FileHandlerManagerUtilities::SplitString(list_items, *desktop_list, ';');
			
			for (UINT32 k = 0; k < list_items.GetCount(); k++)
			{
				//Insert handler in applications for this mime-type
				OpString * item = list_items.Get(k);
				FileHandlerManager::GetManager()->InsertIntoApplicationList(node,
																			item->CStr(),
																			file_path.CStr(),
																			0,
																			TRUE);
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
