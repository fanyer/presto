/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Petter Nilsen
 */

#include "core/pch.h"

#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opsafefile.h"

#include "adjunct/desktop_util/search/search_field_history.h"

#define SEARCH_FIELD_HISTORY_MAX_ENTRIES	100			// max entries to save to disk

/******************************************************************
*
* GetInstance
* 
* TODO: Remove
******************************************************************/
OpSearchFieldHistory& OpSearchFieldHistory::GetInstance()
{
	static OpSearchFieldHistory s_search_field_history;

	if(!s_search_field_history.IsInitialized())
	{
		s_search_field_history.Init();
	}
	return s_search_field_history;
}

OpSearchFieldHistory::~OpSearchFieldHistory()
{
	m_entries.DeleteAll();
}

OP_STATUS OpSearchFieldHistory::Init(const uni_char* override_filename)
{
	OpString filename;
	OpFile file;
	OpString tmp;
	XMLFragment root;
	BOOL exists;

	m_is_initialized = TRUE;

	if(override_filename)
	{
		RETURN_IF_ERROR(filename.Set(override_filename));
	}
	else
	{
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_USERPREFS_FOLDER, filename));
		RETURN_IF_ERROR(filename.Append(UNI_L("search_field_history.dat")));
	}
	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Exists(exists));

	if(!exists)
	{
		return OpStatus::OK;
	}
	RETURN_IF_ERROR(file.Open(OPFILE_READ | OPFILE_TEXT));

	root.SetDefaultWhitespaceHandling(XMLWHITESPACEHANDLING_PRESERVE);

	RETURN_IF_ERROR(root.Parse(static_cast<OpFileDescriptor*>(&file)));

	/*
	<search_field_history>
		<search_entry search_id="some_unique_id" term="search term" />
	</search_field_history>
	*/

	if (root.EnterElement(UNI_L("search_field_history")))
	{
		while(root.EnterElement(UNI_L("search_entry")))
		{
			OpString search_id, term;
			const uni_char *value_char;
			XMLCompleteName name;

			// lets get the attributes for this element
			while (root.GetNextAttribute(name, value_char))
			{
				if(!uni_strcmp(name.GetLocalPart(), UNI_L("search_id")))
				{
					RETURN_IF_ERROR(search_id.Set(value_char));
				}
				else if(!uni_strcmp(name.GetLocalPart(), UNI_L("term")))
				{
					RETURN_IF_ERROR(term.Set(value_char));
				}
			}
			if(search_id.HasContent() && term.HasContent())
			{
				OpAutoPtr<OpSearchFieldHistory::OpSearchFieldHistoryEntry> current_item(OP_NEW(OpSearchFieldHistory::OpSearchFieldHistoryEntry, (search_id.CStr(), term.CStr())));
				if(!current_item.get())
				{
					return OpStatus::ERR_NO_MEMORY;
				}
				RETURN_IF_ERROR(AddLast(current_item.release()));
			}
			else
			{
				OP_ASSERT(!"Missing attribute!");
			}
			root.LeaveElement();
		}
		root.LeaveElement();
	}
	return OpStatus::OK;
}

OP_STATUS OpSearchFieldHistory::Write(const uni_char* override_filename)
{
	UINT32 entries = 0;
	TempBuffer xml;
	XMLFragment xmlfragment;
	OpString filename;

	if(override_filename)
	{
		RETURN_IF_ERROR(filename.Set(override_filename));
	}
	else
	{
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_USERPREFS_FOLDER, filename));
		RETURN_IF_ERROR(filename.Append(UNI_L("search_field_history.dat")));
	}

	xml.SetExpansionPolicy(TempBuffer::AGGRESSIVE);
	xml.SetCachedLengthPolicy(TempBuffer::TRUSTED);

	OpAutoPtr<Iterator> iterator(GetIterator());

	/*
	<search_field_history>
		<search_entry search_id="some_unique_id" term="search term" />
	</search_field_history>
	*/
	RETURN_IF_ERROR(xmlfragment.OpenElement(UNI_L("search_field_history")));

	OpSearchFieldHistory::OpSearchFieldHistoryEntry *item = iterator->GetNext();
	while(item && entries++ < SEARCH_FIELD_HISTORY_MAX_ENTRIES)
	{
		RETURN_IF_ERROR(xmlfragment.OpenElement(UNI_L("search_entry")));

		RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("search_id"), item->GetID()));
		RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("term"), item->GetSearchTerm()));

		xmlfragment.CloseElement();	// </search_entry>

		item = iterator->GetNext();
	}
	xmlfragment.CloseElement();	// </search_field_history>

	RETURN_IF_ERROR(xmlfragment.GetXML(xml, TRUE, "utf-8", FALSE));

	OpSafeFile file;

	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE | OPFILE_TEXT));

	OpString8 tmp;

	RETURN_IF_ERROR(tmp.SetUTF8FromUTF16(xml.GetStorage(), xml.Length()));
	RETURN_IF_ERROR(file.Write(tmp.CStr(), tmp.Length()));
	RETURN_IF_ERROR(file.SafeClose());

	return OpStatus::OK;
}

OP_STATUS OpSearchFieldHistory::AddEntry(OpSearchFieldHistoryEntry *item) 
{
	UINT32 i;

	for(i = 0; i < m_entries.GetCount(); i++)
	{
		OpSearchFieldHistoryEntry *old_item = m_entries.Get(i);
		if(old_item)
		{
			if(!uni_strcmp(old_item->GetSearchTerm(), item->GetSearchTerm()) && !uni_strcmp(old_item->GetID(), item->GetID()))
			{
				m_entries.Remove(i);
				OP_DELETE(old_item);
				break;
			}
		}
	}
	return m_entries.Insert(0, item); 
}

BOOL OpSearchFieldHistory::RemoveEntry(const uni_char* id, const uni_char *search_term)
{
	UINT32 i;

	for(i = 0; i < m_entries.GetCount(); i++)
	{
		OpSearchFieldHistoryEntry *item = m_entries.Get(i);
		if(item)
		{
			if(!uni_strcmp(item->GetSearchTerm(), search_term) && !uni_strcmp(item->GetID(), id))
			{
				m_entries.Remove(i);
				OP_DELETE(item);
				return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL OpSearchFieldHistory::RemoveEntries(const uni_char* id)
{
	BOOL removed = FALSE;

	UINT32 i = m_entries.GetCount();
	while (i > 0)
	{
		--i;
		OpSearchFieldHistoryEntry* item = m_entries.Get(i);
		if (item && !uni_strcmp(item->GetID(), id))
		{
			m_entries.Remove(i);
			OP_DELETE(item);
			removed = TRUE;
		}
	}

	return removed;
}

void OpSearchFieldHistory::DeleteAllItems()
{
	m_entries.DeleteAll();
}
