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
#include "platforms/viewix/src/input_files/MimeInfoCacheFile.h"

#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 ** ParseMimeFile
 **
 ** "Caching MIME Types
 **
 **  To make parsing of all the desktop files less costly, a update-desktop-database
 **  program is provided that will generate a cache file. The concept is identical
 **  to that of the 'update-mime-database' program in that it lets applications avoid
 **  reading in (potentially) hundreds of files. It will need to be run after every
 **  desktop file is installed. One cache file is created for every directory in
 **  $XDG_DATA_DIRS/applications/, and will create a file called
 **  $XDG_DATA_DIRS/applications/mimeinfo.cache.
 **
 **  The format of the cache is similar to that of the desktop file, and is just a
 **  list mapping mime-types to desktop files. Here's a quick example of what it
 **  would look like:
 **
 **  application/x-foo=foo.desktop;bar.desktop;
 **  application/x-bar=bar.desktop;
 **
 **  Each MIME Type is listed only once per cache file, and the desktop-id is
 **  expected to exist in that particular directory. That is to say, if the cache
 **  file is located at /usr/share/applications/mimeinfo.cache, bar.desktop refers
 **  to the file /usr/share/applications/bar.desktop."
 **
 **  Ref: http://standards.freedesktop.org/desktop-entry-spec/latest/ar01s07.html
 ***********************************************************************************/
OP_STATUS MimeInfoCacheFile::ParseInternal(OpFile & file)
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
    FileHandlerManagerUtilities::RemoveSuffix(file_path, UNI_L("mimeinfo.cache"));
	
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
			OpString * mime_type_str    = items.Get(0);
			OpString * desktop_list     = items.Get(1);
			uni_char * mime_type        = mime_type_str->CStr();
			
			//Check to see if this mime-type is already registered
			MimeTypeNode* node = FileHandlerManager::GetManager()->GetMimeTypeNode(mime_type);
			
			if(!node) //If not found this is a new mime-type
			{
				node = FileHandlerManager::GetManager()->MakeMimeTypeNode(mime_type);
			}
			
			//Split list of handlers:
			OpAutoVector<OpString> list_items;
			FileHandlerManagerUtilities::SplitString(list_items, *desktop_list, ';');
			
			for (UINT32 k = 0; k < list_items.GetCount(); k++)
			{
				//Insert handler into applications list for this mime-type
				OpString * item = list_items.Get(k);
				FileHandlerManager::GetManager()->InsertIntoApplicationList(node, item->CStr(), file_path.CStr(), 0);
			}
		}
		else
		{
			// Error in reading line - should have two elements
			OP_ASSERT(!"Malformed line in MIME file: should be name=value pair.");
		}
    }

    return OpStatus::OK;
}
