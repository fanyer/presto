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

#include "modules/history/OpHistoryFolder.h"
#include "modules/history/OpHistoryPrefixFolder.h"

#ifdef HISTORY_DEBUG
INT HistoryFolder::m_number_of_folders = 0;
#endif //HISTORY_DEBUG

/*___________________________________________________________________________*/
/*                         CORE HISTORY MODEL FOLDER                         */
/*___________________________________________________________________________*/

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
HistoryFolder::HistoryFolder(HistoryKey * key):
    
    HistoryItem(),
	m_key(0)
{
	OP_ASSERT(key);
	SetKey(key);

#ifdef HISTORY_DEBUG
	m_number_of_folders++;
#endif //HISTORY_DEBUG
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
HistoryFolder::HistoryFolder():
	m_key(0)
{
#ifdef HISTORY_DEBUG
	m_number_of_folders++;
#endif //HISTORY_DEBUG
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
OP_STATUS HistoryFolder::GetAddress(OpString & address) const
{
	const uni_char * stripped = GetKey()          ? GetKey()->GetKey()             : 0;
	const uni_char * prefix   = GetPrefixFolder() ? GetPrefixFolder()->GetPrefix() : 0;

	RETURN_IF_ERROR(address.Set(prefix));
	RETURN_IF_ERROR(address.Append(stripped));

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
HistoryFolder::~HistoryFolder()
{ 
	SetKey(0); 
#ifdef HISTORY_DEBUG
	m_number_of_folders--;
#endif //HISTORY_DEBUG
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
void HistoryFolder::SetKey(HistoryKey * key)
{
	if(key)
		key->Increment();

	if(m_key)
		m_key->Decrement();

	if(m_key && !m_key->InUse())
		OP_DELETE(m_key);

	m_key = key;
}

#endif // HISTORY_SUPPORT
