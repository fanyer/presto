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

#include "modules/history/OpHistoryPage.h"
#include "modules/history/OpHistoryFolder.h"
#include "modules/history/OpHistoryPrefixFolder.h"
#include "modules/history/OpHistorySiteFolder.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#ifdef HISTORY_DEBUG
INT HistoryPage::m_number_of_pages      = 0;
INT HistoryPage::s_number_of_addresses  = 0;
INT HistoryPage::s_total_address_length = 0;
INT HistoryPage::s_number_of_titles     = 0;
INT HistoryPage::s_total_title_length   = 0;
#endif //HISTORY_DEBUG

/***********************************************************************************
 ** Creates a new HistoryPage with a uniqe id. The field m_title is
 ** set in the Construct method.
 **
 ** HistoryPage::HistoryPage
 ** @param acc
 ** @param average_interval
 ***********************************************************************************/
HistoryPage::HistoryPage(time_t acc, 
						 time_t average_interval, 
						 HistoryKey * key):
	m_accessed(acc),
	m_average_interval(average_interval),
	m_flags(0),
	m_key(0),
	m_associated_item(0)
{
	SetKey(key);

#ifdef HISTORY_DEBUG
	m_number_of_pages++;
#endif //HISTORY_DEBUG
}

/***********************************************************************************
 ** Attempts to allocate for the fields. Will return 0 if
 ** not successful. If successful will return pointer to a HistoryPage.
 **
 ** HistoryPage::Create
 ** @param acc
 ** @param average_interval
 ** @param title
 ** @param key
 **
 ** @return HistoryPage* (or 0 if out of memory)
 ***********************************************************************************/
HistoryPage* HistoryPage::Create(time_t acc, 
								 time_t average_interval, 
								 const OpStringC& title,
								 HistoryKey * key)
{
	OP_ASSERT(key);

	OpAutoPtr<HistoryPage> history_page(OP_NEW(HistoryPage, (acc, average_interval, key)));

	if(!history_page.get())
		return 0;

	RETURN_VALUE_IF_ERROR(history_page->SetTitle(title), 0);

	return history_page.release();
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
const uni_char * HistoryPage::GetStrippedAddress() const
{
	const LexSplayKey* key = GetKey();
	return key ? key->GetKey() : 0;
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
const uni_char * HistoryPage::GetPrefix() const
{
	HistoryPrefixFolder* folder = GetPrefixFolder();
	return folder ? folder->GetPrefix() : 0;
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
INT32 HistoryPage::GetPrefixNum() const
{
	HistoryPrefixFolder* folder = GetPrefixFolder();
	return folder ? folder->GetIndex() : 0;
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
OP_STATUS HistoryPage::GetAddress(OpString & address) const
{
	RETURN_IF_ERROR(address.Set(GetPrefix()));
	RETURN_IF_ERROR(address.Append(GetStrippedAddress()));

	return OpStatus::OK;
}

/***********************************************************************************
 ** Delete
 **
 ** HistoryPage::~HistoryPage
 ***********************************************************************************/
HistoryPage::~HistoryPage()
{
#ifdef HISTORY_DEBUG
	m_number_of_pages--;
#endif //HISTORY_DEBUG
	SetKey(0);
}

/***********************************************************************************
 ** Calculates an items average popularity in seconds. If item has no average 
 ** interval set the method will return with a monthly visit (2419200 = 28*24*60*60),
 ** otherwise if the average interval was greater than the new interval the new 
 ** interval is weighted at 20%, if the average interval was less than the new
 ** interval the new interval is weighted at 10%. Meaning an item will increase in 
 ** popularity faster than it will lose it.
 **
 ** HistoryPage::GetPopularity
 **	
 ** @return average interval for this item in seconds	
 ***********************************************************************************/
time_t HistoryPage::GetPopularity()
{
    time_t now = g_timecache->CurrentTime();
	
    if (m_average_interval == -1) 
    {
		return op_abs(long(now - m_accessed + 2419200)); // initialize with monthly visit
    } 
    else 
    {
		if (m_average_interval > (now - m_accessed)) 
		{
			// increase popularity faster
			return op_abs(long((m_average_interval * 4 + (now - m_accessed)) / 5));
		} 
		else 
		{
			// decrease popularity slower
			return op_abs(long((m_average_interval * 9 + (now - m_accessed)) / 10));
		}
    }
}


/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
void HistoryPage::UpdateAccessed(time_t acc)
{
	if(acc > m_accessed)
	{
		time_t now = g_timecache->CurrentTime();
		time_t last = m_accessed;
	
		if (now - last > 60) // update popularity max every min
			m_average_interval = GetPopularity();

		m_accessed = acc;

		BroadcastHistoryItemAccessed(acc);
	}
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
void HistoryPage::SetKey(HistoryKey * key)
{
	if(key)
		key->Increment();

	if(m_key)
		m_key->Decrement();

#ifdef HISTORY_DEBUG
	if(m_key)
	{
		s_number_of_addresses--;
		s_total_address_length -= (uni_strlen(m_key->GetKey()) * sizeof(uni_char));
	}

	if(key)
	{
		s_number_of_addresses++;
		s_total_address_length += (uni_strlen(key->GetKey()) * sizeof(uni_char));
	}
#endif //HISTORY_DEBUG

	if(m_key && !m_key->InUse())
		OP_DELETE(m_key);

	m_key = key;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryPage::SetAssociatedItem(HistoryPage * associated_item)
{
	if(m_associated_item)
		return OpStatus::ERR;

	m_associated_item = associated_item;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryPage::ClearAssociatedItem()
{
	HistoryPage * associated_item  = m_associated_item;

	m_associated_item = 0;

	if(associated_item)
		associated_item->ClearAssociatedItem();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
#ifdef HISTORY_DEBUG
void HistoryPage::OnTitleSet()
{
	OpString title;
	GetTitle(title);

	if(title.HasContent())
	{
		s_number_of_titles++;
		s_total_title_length += (title.Length() * sizeof(uni_char));
	}
}
#endif // HISTORY_DEBUG

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryPage::OnSiteFolderSet()
{
	if(GetSiteFolder())
	{
		if(GetSiteFolder()->GetKey() == GetKey())
			SetIsDomain();
		else
			ClearIsDomain();
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryPage::IsVisited() const
{
	const time_t now      = g_timecache->CurrentTime();
	const time_t accessed = GetAccessed();
	
	time_t elapsed = now - accessed;
	
	return elapsed <= g_pcnet->GetFollowedLinksExpireTime();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
const char* HistoryPage::GetImage() const
{
	return IsBookmarked() ?
		(IsVisited() ? "Bookmark Visited" : "Bookmark Unvisited")
		: "Window Document Icon";
}

#endif // HISTORY_SUPPORT
