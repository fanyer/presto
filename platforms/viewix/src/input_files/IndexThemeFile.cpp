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
#include "platforms/viewix/src/nodes/ThemeNode.h"
#include "platforms/viewix/src/input_files/IndexThemeFile.h"

#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 ** ParseIndexThemeFile
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS IndexThemeFile::ParseInternal(OpFile & file)
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

    OpString theme_path;
    theme_path.Set(file.GetFullPath());
    FileHandlerManagerUtilities::RemoveSuffix(theme_path, UNI_L("index.theme"));

    m_node->SetThemePath(theme_path.CStr());

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
			
			if(uni_strncmp(item_0->CStr(), UNI_L("Inherits"), 7) == 0)
			{
				m_node->SetInherits(item_1->CStr());
			}
		}
    }

    return OpStatus::OK;
}
