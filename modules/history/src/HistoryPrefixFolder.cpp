/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#include "core/pch.h"

#ifdef HISTORY_SUPPORT

#include "modules/history/OpHistoryPrefixFolder.h"

/*___________________________________________________________________________*/
/*                         CORE HISTORY MODEL PREFIX FOLDER                  */
/*___________________________________________________________________________*/


/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
HistoryPrefixFolder::HistoryPrefixFolder(INT32 index,
										 HistoryKey * key,
										 HistoryKey * prefix_key,
										 HistoryKey * suffix_key):
    HistoryFolder(key),
    m_index(index),    //index into prefix tables both trees and nodes
	m_contained(FALSE) //is a subfolder of another prefix folder - ie http://www is subfolder of http://
{
	// MatchNodes :
	m_prefix_node.Init(index, prefix_key);
	m_suffix_node.Init(index, suffix_key);
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
HistoryPrefixFolder* HistoryPrefixFolder::Create(const OpStringC& title, 
												 INT32 index,
												 HistoryKey * key,
												 HistoryKey * prefix_key,
												 HistoryKey * suffix_key)
{
	OP_ASSERT(key);
	OP_ASSERT(prefix_key);
	OP_ASSERT(suffix_key);

	OpAutoPtr<HistoryPrefixFolder> prefix_folder(OP_NEW(HistoryPrefixFolder, (index, 
																		 key, 
																		 prefix_key, 
																		 suffix_key)));

	if(!prefix_folder.get())
		return 0;

	RETURN_VALUE_IF_ERROR(prefix_folder->SetTitle(title), 0);

	return prefix_folder.release();
}


/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
const uni_char * HistoryPrefixFolder::GetPrefix()
{
	OP_ASSERT(GetPrefixNode().GetKey());

	if(GetPrefixNode().GetKey())
		return GetPrefixNode().GetKey()->GetKey();

	return 0;
}

#endif // HISTORY_SUPPORT
