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
#include "platforms/viewix/src/input_files/MailcapFile.h"
#include "platforms/viewix/src/nodes/ApplicationNode.h"
#include "platforms/viewix/src/nodes/MimeTypeNode.h"

#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 ** ParseMailcapFile
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS MailcapFile::ParseInternal(OpFile & file)
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

    for(UINT32 i = 0; i < list.GetCount(); i++)
    {
		OpString* line = list.Get(i);

		if(line->FindFirstOf('#') == 0)
			continue;

		OpAutoVector<OpString> items;
		FileHandlerManagerUtilities::SplitString(items, *line, ';');

		MimeTypeNode    * node     = 0;
		ApplicationNode * app_node = 0;

		if(items.GetCount() >= 2)
		{
			items.Get(0)->Strip();
			items.Get(1)->Strip();

			OpString * mime_type_str = items.Get(0);
			OpString * command_str   = items.Get(1);

			uni_char * mime_type = mime_type_str->CStr();

			node = FileHandlerManager::GetManager()->MakeMimeTypeNode(mime_type);

			if(command_str->HasContent())
			{
				uni_char * command = command_str->CStr();
				app_node = FileHandlerManager::GetManager()->InsertIntoApplicationList(node, 0, 0, command);
			}
		}
		else
		{
            //Error in reading line - should have at least two elements
			continue;
		}

		for(UINT32 i = 2; i < items.GetCount(); i++)
		{
			OpString* pair = items.Get(i);
			pair->Strip();

			OpAutoVector<OpString> pair_items;
			FileHandlerManagerUtilities::SplitString(pair_items, *pair, '=');

			if(pair_items.GetCount() >= 2)
			{
				pair_items.Get(0)->Strip();
				pair_items.Get(1)->Strip();

				OpString * key   = pair_items.Get(0);
				OpString * value = pair_items.Get(1);

				if(uni_strncmp(key->CStr(), UNI_L("description"), 11) == 0)
				{
					if(node)
						node->SetName(value->CStr());
				}
			}
			if(pair_items.GetCount() == 1)
			{
				pair_items.Get(0)->Strip();

				OpString * value = pair_items.Get(0);

				if(uni_strncmp(value->CStr(), UNI_L("needsterminal"), 13) == 0)
				{
					if(app_node)
						app_node->SetInTerminal(TRUE);
				}
			}
		}

    }

    return OpStatus::OK;
}
