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
#include "platforms/viewix/src/nodes/ApplicationNode.h"
#include "platforms/viewix/src/input_files/DesktopFile.h"

#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 ** DesktopFile - constructor
 **
 **
 **
 **
 ***********************************************************************************/
DesktopFile::DesktopFile(FileHandlerNode * node)
    : m_node(node)
{}


/***********************************************************************************
 ** Parse
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DesktopFile::ParseInternal(OpFile & file)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //File pointer cannot be null
    OP_ASSERT(file.IsOpen());
    if(!file.IsOpen())
	return OpStatus::ERR;

    //Node pointer cannot be null
    OP_ASSERT(m_node);
    if(!m_node)
	return OpStatus::ERR;
    //-----------------------------------------------------

    //Split file into lines - only if smaller than 100k
    // - All desktop files should be less than 100k
    UINT32 max_size = 100000;
    OpAutoVector<OpString> list;
    FileHandlerManagerUtilities::FileToLineVector(&file, list, max_size);

    for(UINT32 i = 1; i < list.GetCount(); i++)
    {
		OpString* line = list.Get(i);
		OpAutoVector<OpString> items;
		FileHandlerManagerUtilities::SplitString(items, *line, '=');
		
		if(items.GetCount() == 2)
		{
			OpString * item_0 = items.Get(0);
			OpString * item_1 = items.Get(1);
			
			if(uni_strncmp(item_0->CStr(), UNI_L("TryExec"), 7) == 0)
			{
				if(m_node->GetType() == APPLICATION_NODE_TYPE)
				{
					ApplicationNode * app_node = (ApplicationNode *) m_node;
					app_node->SetTryExec(item_1->CStr());
				}
			}
			else if(uni_strncmp(item_0->CStr(), UNI_L("Exec"), 4) == 0)
			{
				if(m_node->GetType() == APPLICATION_NODE_TYPE)
				{
					ApplicationNode * app_node = (ApplicationNode *) m_node;
					app_node->SetCommand(item_1->CStr());
				}
			}
			else if(uni_strncmp(item_0->CStr(), UNI_L("Icon"), 4) == 0)
			{
				m_node->SetIcon(item_1->CStr());
			}
			else if(uni_strlen(item_0->CStr()) == 7 && uni_strncmp(item_0->CStr(), UNI_L("Comment"), 7) == 0)
			{
				m_node->SetComment(item_1->CStr());
			}
			else if(uni_strlen(item_0->CStr()) == 4 && uni_strncmp(item_0->CStr(), UNI_L("Name"), 4) == 0)
			{
				m_node->SetName(item_1->CStr());
			}
			else if(uni_strlen(item_0->CStr()) == 8 && uni_strncmp(item_0->CStr(), UNI_L("Terminal"), 8) == 0)
			{
				if(m_node->GetType() == APPLICATION_NODE_TYPE)
				{
					ApplicationNode * app_node = (ApplicationNode *) m_node;
					BOOL in_terminal =
						(uni_strncmp(item_1->CStr(), UNI_L("true"), 4) == 0) ||
						(uni_strncmp(item_1->CStr(), UNI_L("1"), 1) == 0);
					app_node->SetInTerminal(in_terminal);
				}
			}
			else if(uni_strlen(item_0->CStr()) == 17 && uni_strncmp(item_0->CStr(), UNI_L("InitialPreference"), 17) == 0)
			{
				//TODO KDE way to say the preference weight
			}
		}
    }
	
    return OpStatus::OK;
}
