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
#include "platforms/viewix/src/input_files/AliasesFile.h"
#include "platforms/viewix/src/nodes/MimeTypeNode.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 ** AliasesFile - constructor
 **
 **
 **
 **
 ***********************************************************************************/
AliasesFile::AliasesFile()
{
    m_items_left = 0;
}

/***********************************************************************************
 ** Parse
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS AliasesFile::ParseInternal(OpFile & file)
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

    m_items_left = 0;

    for(UINT32 i = 0; i < list.GetCount(); i++)
    {
		OpString* line = list.Get(i);
		OpAutoVector<OpString> items;
		FileHandlerManagerUtilities::SplitString(items, *line, ' ');

		if(items.GetCount() == 2)
		{
			OpString * mime_type1_str    = items.Get(0);
			OpString * mime_type2_str    = items.Get(1);
			
			const uni_char * mime_type1  = mime_type1_str->CStr();
			const uni_char * mime_type2  = mime_type2_str->CStr();
			
			//Check to see if this mime-type is already registered
			MimeTypeNode* node1 = FileHandlerManager::GetManager()->GetMimeTypeNode(mime_type1);
			
			//Check to see if this mime-type is already registered
			MimeTypeNode* node2 = FileHandlerManager::GetManager()->GetMimeTypeNode(mime_type2);
			
			if(!node1 && node2)
			{
				FileHandlerManager::GetManager()->LinkMimeTypeNode(mime_type1, node2);
			}
			else if(node1 && !node2)
			{
				FileHandlerManager::GetManager()->LinkMimeTypeNode(mime_type2, node1);
			}
			else if(!node1 && !node2)
			{
				m_items_left++;
				
				MimeTypeNode * node = FileHandlerManager::GetManager()->MakeMimeTypeNode(mime_type1);
				FileHandlerManager::GetManager()->LinkMimeTypeNode(mime_type2, node);
			}
			else if(node1 != node2)
			{
				FileHandlerManager::GetManager()->MergeMimeNodes(node1, node2);
			}
		}
    }
	
    return OpStatus::OK;
}
