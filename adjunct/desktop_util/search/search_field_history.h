/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Petter Nilsen
 */

#ifndef _SEARCH_FIELD_HISTORY_H_INCLUDED
#define _SEARCH_FIELD_HISTORY_H_INCLUDED

#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

#define g_search_field_history (&OpSearchFieldHistory::GetInstance())

/** 
** @brief Class to keep a record on disk with the frequent searches
*/

class OpSearchFieldHistory
{
public:
	OpSearchFieldHistory() : m_is_initialized(FALSE) {}
	~OpSearchFieldHistory();
	
	static OpSearchFieldHistory& GetInstance(); // Get rid of

	OP_STATUS		Init(const uni_char* override_filename = NULL);
	OP_STATUS		Write(const uni_char* override_filename = NULL);
	BOOL			IsInitialized() { return m_is_initialized; }

	class OpSearchFieldHistoryEntry
	{
	public:
		OpSearchFieldHistoryEntry(const uni_char* id, const uni_char *search_term)
		{
			m_id.Set(id);
			m_search_term.Set(search_term);
		}
		// the search term associated with this search engine
		const uni_char* GetSearchTerm() { return m_search_term.CStr(); }

		// the unique id of the search used for this term
		const uni_char*	GetID()			{ return m_id.CStr(); }
	
	private:
		OpString	m_search_term;	// the search term used
		OpString	m_id;			// id of the search engine used for this search entry
	};
	class Iterator
	{
		friend class OpSearchFieldHistory;

	public:
		OpSearchFieldHistoryEntry* GetNext() 
		{
			UINT32 max_count = m_entries->GetCount();

			while(max_count > m_offset)
			{
				OpSearchFieldHistoryEntry *entry = m_entries->Get(m_offset++);

				if(m_filter.HasContent())
				{
					const uni_char *term = entry->GetSearchTerm();

					if(uni_stristr(term, m_filter.CStr()))
					{
						return entry;
					}
				}
				else
				{
					return entry;
				}
			}
			return NULL;
		}
		OP_STATUS	SetFilter(OpString& filter) { return m_filter.Set(filter.CStr()); }

	protected:
		Iterator(OpVector<OpSearchFieldHistoryEntry>* entries) : m_offset(0), m_entries(entries)
		{

		}

	private:
		OpString								m_filter;
		UINT32									m_offset;
		OpVector<OpSearchFieldHistoryEntry>*	m_entries;
	};

	// add new entries to the top
	OP_STATUS		AddEntry(OpSearchFieldHistoryEntry *item);
	OP_STATUS		AddEntry(const uni_char* id, const uni_char *search_term) { return AddEntry(OP_NEW(OpSearchFieldHistoryEntry, (id, search_term))); }

	Iterator*		GetIterator() 
	{
		return OP_NEW(Iterator, (&m_entries));
	}

	// remove an entry matching the given information, return TRUE if successful. 
	BOOL			RemoveEntry(const uni_char* id, const uni_char *search_term);

	// remove all entries matching given search engine id, return TRUE if at least one entry was removed.
	BOOL			RemoveEntries(const uni_char* id);

	// remove all previous searches
	void			DeleteAllItems();

private:
	OP_STATUS		AddLast(OpSearchFieldHistoryEntry *item) { return m_entries.Add(item); }

private:
	OpVector<OpSearchFieldHistoryEntry>	m_entries;
	BOOL			m_is_initialized;
};

#endif // _SEARCH_FIELD_HISTORY_H_INCLUDED
