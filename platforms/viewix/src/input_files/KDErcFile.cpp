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
#include "platforms/viewix/src/input_files/KDErcFile.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 ** ParseDefaultFile
 **
 **
 **
 ** FOR KDE : /etc/kderc
 ***********************************************************************************/
OP_STATUS KDErcFile::ParseInternal(OpFile & file)
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
	
    OP_STATUS status = OpStatus::OK;
    OpString tag;
	
    for(UINT32 i = 0; i < list.GetCount(); i++)
    {
		OpString* line = list.Get(i);
		line->Strip();
		
		// Seperator line :
		if(line->IsEmpty())
		{
			continue;
		}
		
		if(ParsedTag(*line, tag, status))
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
			
			if(uni_strncmp(key->CStr(), UNI_L("prefixes"), 8) == 0)
			{
				m_dir.Set(value->CStr());
			}
		}
    }

    return status;
}

BOOL KDErcFile::ParsedTag(OpString & line,
						  OpString & tag,
						  OP_STATUS & status)
{
    status = OpStatus::OK;
	
    if (!tag.Reserve(256))
    {
		status = OpStatus::ERR_NO_MEMORY;
		return FALSE;
    }
	
    if (uni_sscanf(line.CStr(), UNI_L("[%256s]"), tag.CStr()) == 1)
    {
		return TRUE;
    }
	
    return FALSE;
}
