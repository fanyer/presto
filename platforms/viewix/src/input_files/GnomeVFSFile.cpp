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
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"
#include "platforms/viewix/src/input_files/GnomeVFSFile.h"
#include "platforms/viewix/src/nodes/ApplicationNode.h"
#include "platforms/viewix/src/nodes/MimeTypeNode.h"

#include "modules/util/opfile/opfile.h"


/***********************************************************************************
 ** ParseInternal
 **
 **
 **
 ** FOR GNOME : /usr/share/application-registry/gnome-vfs.applications
 ***********************************************************************************/
OP_STATUS GnomeVFSFile::ParseInternal(OpFile & file)
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
	
    OpString application;
	
    ApplicationNode * app_node = 0;
	
    for(UINT32 i = 0; i < list.GetCount(); i++)
    {
		OpString* line = list.Get(i);
		
		line->Strip();
		
		// Seperator line :
		if(line->IsEmpty())
		{
			application.Empty();
			app_node = 0;
			continue;
		}
		
		OpAutoVector<OpString> items;
		FileHandlerManagerUtilities::SplitString(items, *line, '=');
		
		if(items.GetCount() == 1)
		{
			items.Get(0)->Strip();
			application.Set(items.Get(0)->CStr());
		}
		else if(items.GetCount() == 2)
		{
			items.Get(0)->Strip();
			items.Get(1)->Strip();
			
			OpString * key   = items.Get(0);
			OpString * value = items.Get(1);

			if(uni_strncmp(key->CStr(), UNI_L("command"), 7) == 0)
			{
				if(value->HasContent())
					app_node = FileHandlerManager::GetManager()->MakeApplicationNode(0, 0, value->CStr(), 0);
			}
			else if(app_node && uni_strncmp(key->CStr(), UNI_L("name"), 4) == 0)
			{
				app_node->SetName(value->CStr());
			}
			else if(app_node && uni_strncmp(key->CStr(), UNI_L("requires_terminal"), 17) == 0)
			{
				if(uni_strncmp(value->CStr(), UNI_L("true"), 4) == 0)
				{
					app_node->SetInTerminal(TRUE);
				}
				else if(uni_strncmp(value->CStr(), UNI_L("false"), 5) == 0)
				{
					app_node->SetInTerminal(FALSE);
				}
			}
			else if(app_node && uni_strncmp(key->CStr(), UNI_L("mime_types"), 10) == 0)
			{
				OpAutoVector<OpString> mime_types;
				FileHandlerManagerUtilities::SplitString(mime_types, *value, ',');
				
				MimeTypeNode * node     = 0;
				
				for(UINT32 j = 0; j < mime_types.GetCount(); j++)
				{
					node = 0;
					
					mime_types.Get(j)->Strip();
					OpString * mime_type = mime_types.Get(j);
					
					if(!mime_type || mime_type->IsEmpty())
						continue;
					
					node = FileHandlerManager::GetManager()->MakeMimeTypeNode(mime_type->CStr());
					
					if(!node)
						continue;
					
					FileHandlerManager::GetManager()->InsertIntoApplicationList(node, app_node, FALSE);
				}
			}
		}
    }

    return OpStatus::OK;
}
