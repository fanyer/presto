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

#include "modules/history/OpHistoryItem.h"
#include "modules/history/OpHistoryPage.h"
#include "modules/history/OpHistorySiteFolder.h"

/***********************************************************************************
 ** Creates a new HistoryItem with a uniqe id. The field m_title is
 ** set in the Construct method.
 **
 ** HistoryItem::HistoryItem
 ** @param acc
 ** @param average_interval
 ***********************************************************************************/
HistoryItem::HistoryItem() :
	m_siteFolder(0),
	m_prefixFolder(0),
	m_id(OpTypedObject::GetUniqueID())
{
}


/***********************************************************************************
 ** Deletes m_addr, m_title
 **
 ** HistoryItem::~HistoryItem
 ***********************************************************************************/
HistoryItem::~HistoryItem()
{
    if(m_siteFolder)
		m_siteFolder->RemoveChild();

	// Tell the listeners that you are being destroyed :
	BroadcastHistoryItemDestroying();
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
OP_STATUS HistoryItem::SetTitle(const OpStringC& title)
{
	RETURN_IF_ERROR(m_title.Set(title));

#ifdef HISTORY_DEBUG
	OnTitleSet();
#endif // HISTORY_DEBUG

	return OpStatus::OK;	
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
void HistoryItem::SetSiteFolder(HistorySiteFolder* folder) {

    if(m_siteFolder) //Should not really happen, should it?
		m_siteFolder->RemoveChild();

    m_siteFolder = folder;

    if(m_siteFolder)
	{
		m_siteFolder->AddChild();
		OnSiteFolderSet();
	}
}

#endif // HISTORY_SUPPORT
