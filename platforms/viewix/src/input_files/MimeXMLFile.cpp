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
#include "platforms/viewix/src/input_files/MimeXMLFile.h"
#include "platforms/viewix/src/nodes/MimeTypeNode.h"

#include "modules/util/opfile/opfile.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS MimeXMLFile::ParseInternal(OpFile & file)
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

    //Split file into lines - only if smaller than 10MB
    UINT32 max_size = 10000000;
    OpAutoVector<OpString> list;
    FileHandlerManagerUtilities::FileToLineVector(&file, list, max_size);

    for(UINT32 i = 1; i < list.GetCount(); i++)
    {
	OpString* line = list.Get(i);
	OpString  line_stripped = line->Strip();
	INT32 index = line_stripped.Find("<comment>");

	if(index >= 0)
	{
	    FileHandlerManagerUtilities::RemovePrefix(line_stripped, UNI_L("<comment>"));
	    FileHandlerManagerUtilities::RemoveSuffix(line_stripped, UNI_L("</comment>"));
	    m_node->SetComment(line_stripped.CStr());
	    break;
	}
    }

    return OpStatus::OK;
}
