/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * psmaas - Patricia Aas Oct 2005
 */
#include "core/pch.h"

#include "adjunct/quick/models/HistoryModelItem.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/models/HistoryAutocompleteModel.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/desktop_util/skin/SkinUtils.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpLocale.h"

/*___________________________________________________________________________*/
/*                          HISTORY MODEL ITEM                               */
/*___________________________________________________________________________*/
/***********************************************************************************
 ** HistoryModelItem - Constructor
 **
 ** @param
 **
 **
 ***********************************************************************************/
HistoryModelItem::HistoryModelItem()
{
    m_id            = OpTypedObject::GetUniqueID();
	m_siteFolder    = 0;
	m_prefixFolder  = 0;
	m_moving        = FALSE;
	m_matched       = FALSE;
	m_old_parent_id = 0;
}


/***********************************************************************************
 ** HistoryModelItem - Destructor
 **
 **
 **
 **
 ***********************************************************************************/
HistoryModelItem::~HistoryModelItem()
{
    if(m_siteFolder)
		m_siteFolder->RemoveChild();

	//HistoryModelItem should not be in the UI
	OP_ASSERT(!GetModel());

	if(GetModel())
		Remove();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryModelPage::HistoryModelPage(HistoryPage* page)
	: m_core_page(page),
	  m_open_in_a_tab(FALSE)
{
	if(m_core_page)
		m_core_page->AddListener(this);

	m_simple_item = OP_NEW(PageBasedAutocompleteItem, (this));
	if (m_simple_item)
	{
		if (page->IsNick())
			m_simple_item->SetNoUnescaping(TRUE);
		UpdateSimpleItem();
	}
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryModelPage::~HistoryModelPage()
{
	RemoveCorePage();

	if (m_simple_item)
	{
		OP_DELETE(m_simple_item);
	}
}

HistoryAutocompleteModelItem * HistoryModelPage::GetHistoryAutocompleteModelItem()
{
	return m_simple_item;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModelPage::RemoveCoreItem()
{
	RemoveCorePage();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModelPage::RemoveCorePage()
{
	if(m_core_page)
		m_core_page->RemoveListener(this);

	m_core_page = NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModelPage::UpdateSimpleItem()
{
	// Update the url and title :
 	//--------------------
	m_simple_item->SetHighlightText(NULL);
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModelItem::UnMatch()
{
	m_matched = FALSE;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModelItem::Match(const OpStringC & excerpt)
{
	m_matched = TRUE;
	m_excerpt.Empty();
	m_excerpt.Set(excerpt.CStr());
}


/***********************************************************************************
 ** Supports three querys: MATCH_QUERY, INFO_QUERY and COLUMN_QUERY.
 **
 ** Will set FLAG_MATCHED in a MATCH_QUERY if this item has match_text as either
 ** GetAddress() or GetTitle().
 **
 ** Will call info_text->AddTooltipText with GetTitle() and GetAddress() in an INFO_QUERY,
 ** if GetAddress() contains an @ the info_text->SetStatusText is supplied with the url
 ** using UniName(PASSWORD_HIDDEN) otherwise it will be supplied with GetAddress().
 **
 ** Will for all columns call column_text->Set with GetTitle(), GetAddress(), GetAccessed() and
 ** popularity, respectivly. For column 0, sets column_bitmap and possibly column_
 ** image to UNI_L("Bookmark Visited"). For column 2 sets column_sort_order to
 ** GetAccessed(). For column 3 sets column_sort_order.
 **
 ** HistoryModelItem::GetItemData
 ** @param item_data
 **
 ** @return OP_STATUS (OK)
 ***********************************************************************************/
OP_STATUS HistoryModelPage::GetItemData(ItemData* item_data)
{
	OpString address;
	OpString title;

	GetAddress(address);
	GetTitle(title);

	if (item_data->query_type == INIT_QUERY)
	{
		item_data->flags |= FLAG_FORMATTED_TEXT;

		return OpStatus::OK;
	}

    if (item_data->query_type ==MATCH_QUERY)
    {
		if(item_data->match_query_data.match_text->IsEmpty())
		{
			// Remove any previous excerpt:
			// -------------------------------
			m_excerpt.Empty();
			return OpStatus::OK;
		}

		BOOL matched = FALSE;

		if(!m_matched)
		{
			// Remove any previous excerpt - we are in a new match now:
			// -------------------------------
			m_excerpt.Empty();

			// Check if we match in the url or title:
			// -------------------------------
			uni_char* text = item_data->match_query_data.match_text->CStr();

			OpString needles;
			needles.Set(text);

			OpString heystack;

			heystack.Set("");
			heystack.Append(title.CStr());
			heystack.Append(" ");
			heystack.Append(address.CStr());

			matched = StringUtils::ContainsAll(needles, heystack);
		}

		if(matched || m_matched)
		{
			item_data->flags |= FLAG_MATCHED;
		}

		UnMatch();

		return OpStatus::OK;
	}

    if (item_data->query_type == INFO_QUERY)
    {
		OpString title_text, address_text;

		g_languageManager->GetString(Str::SI_IDSTR_HL_TREE_TITLE, title_text);
		g_languageManager->GetString(Str::SI_LOCATION_TEXT, address_text);

		item_data->info_query_data.info_text->AddTooltipText(title_text.CStr(), title.CStr());

		if( address.CStr() && uni_strchr( address.CStr(), '@' ) != 0 )
		{
			URL	url = g_url_api->GetURL(address.CStr());
			OpString url_string;
			url.GetAttribute(URL::KUniName_Username_Password_Hidden, url_string);
			item_data->info_query_data.info_text->SetStatusText(url_string.CStr());
			item_data->info_query_data.info_text->AddTooltipText(address_text.CStr(), address.CStr());
		}
		else
		{
			item_data->info_query_data.info_text->SetStatusText(address.CStr());
			item_data->info_query_data.info_text->AddTooltipText(address_text.CStr(), address.CStr());
		}
		return OpStatus::OK;
    }

	if(GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
	{
		if(IsOpen())
		{
			item_data->flags |= FLAG_BOLD;
		}
	}

	if (item_data->query_type == ASSOCIATED_TEXT_QUERY)
	{
		if(m_excerpt.HasContent())
			return item_data->associated_text_query_data.text->Set(m_excerpt.CStr());
		return OpStatus::OK;
	}

    if (item_data->query_type != COLUMN_QUERY)
    {
		return OpStatus::OK;
    }

    switch (item_data->column_query_data.column)
    {
		case 0:
		{
			item_data->column_query_data.column_text->Set(title.CStr());

			// Get image if necessary
			if (!(item_data->flags & FLAG_NO_PAINT))
			{
				if(GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
				{
					item_data->column_bitmap = g_favicon_manager->Get(address.CStr());
				}
				if ( item_data->column_bitmap.IsEmpty() )
					item_data->column_query_data.column_image = GetImage();
			}

			// item_data->column_query_data.column_text_color = OP_RGB(0x99, 0x00, 0x00); // Title is Blue

			break;
		}
		case 1:
		{
			if(GetType() == FOLDER_TYPE)
				break;

			item_data->column_query_data.column_text->Set(address.CStr());
			// item_data->column_query_data.column_text_color = OP_RGB(0x00, 0x77, 0x00);  // Url is Green
			break;
		}
		case 2:
		{
			if(GetType() == FOLDER_TYPE)
				break;

			time_t time = GetAccessed();

			struct tm *tm = op_localtime(&time);

			OpString s;
			s.Reserve(128);

			g_oplocale->op_strftime( s.CStr(), s.Capacity(), UNI_L("%x %X"), tm );

			item_data->column_query_data.column_text->Set(s);
			item_data->column_query_data.column_sort_order = GetAccessed();
			break;
		}
		case 3:
		{

			if(GetType() == FOLDER_TYPE)
				break;

			time_t popularity = GetPopularity();
			INT32 value;

			if (popularity == 0)
			{
				value = 0;
			}
			else if (popularity < 21600)
			{
				value = 5;
			}
			else if (popularity < 86400)
			{
				value = 4;
			}
			else if (popularity < 604800)
			{
				value = 3;
			}
			else if (popularity < 2419200)
			{
				value = 2;
			}
			else
			{
				value = 1;
			}

			OpString16 s;
			s.AppendFormat(UNI_L("%i"), value);

			item_data->column_query_data.column_text->Set(s);
			item_data->column_query_data.column_sort_order = popularity ? INT_MAX - popularity : GetAccessed();
		}
    }

    return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryModelFolder::HistoryModelFolder(HistoryFolder* folder)
	: m_core_folder(folder)
{
	if(m_core_folder)
		m_core_folder->AddListener(this);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryModelFolder::~HistoryModelFolder()
{
	RemoveCoreFolder();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModelFolder::RemoveCoreItem()
{
	RemoveCoreFolder();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModelFolder::RemoveCoreFolder()
{
	if(m_core_folder)
		m_core_folder->RemoveListener(this);

	m_core_folder = NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModelFolder::GetItemData(ItemData* item_data)
{
	OpString address;
	OpString title;

	GetAddress(address);
	GetTitle(title);

    if (item_data->query_type ==MATCH_QUERY)
    {
		uni_char* text = item_data->match_query_data.match_text->CStr();

		OpString needles;
		needles.Set(text);

		OpString heystack;

		heystack.Set("");
		heystack.Append(title.CStr());
		heystack.Append(" ");
		heystack.Append(address.CStr());

		BOOL matched = StringUtils::ContainsAll(needles, heystack);

		if(matched || m_matched)
		{
			item_data->flags |= FLAG_MATCHED;
		}

		UnMatch();

		return OpStatus::OK;
	}

    if (item_data->query_type == INFO_QUERY)
    {
		OpString title_text, address_text;

		g_languageManager->GetString(Str::SI_IDSTR_HL_TREE_TITLE, title_text);
		g_languageManager->GetString(Str::SI_LOCATION_TEXT, address_text);

		item_data->info_query_data.info_text->AddTooltipText(title_text.CStr(), title.CStr());

		if( address.CStr() && uni_strchr( address.CStr(), '@' ) != 0 )
		{
			URL	url = g_url_api->GetURL(address.CStr());
			OpString url_string;
			url.GetAttribute(URL::KUniName_Username_Password_Hidden, url_string);
			item_data->info_query_data.info_text->SetStatusText(url_string.CStr());
			item_data->info_query_data.info_text->AddTooltipText(address_text.CStr(), address.CStr());
		}
		else
		{
			item_data->info_query_data.info_text->SetStatusText(address.CStr());
			item_data->info_query_data.info_text->AddTooltipText(address_text.CStr(), address.CStr());
		}
		return OpStatus::OK;
    }

    if (item_data->query_type != COLUMN_QUERY)
    {
		return OpStatus::OK;
    }

    switch (item_data->column_query_data.column)
    {
		case 0:
		{
			item_data->column_query_data.column_text->Set(title.CStr());

			// Get images if necessary
			if (!(item_data->flags & FLAG_NO_PAINT))
			{
				if( GetChildItem() )
				{
					HistoryModelItem* item = GetChildItem();
					if( item->GetType() == HISTORY_ELEMENT_TYPE )
					{
						OpString address;
						item->GetAddress(address);
						item_data->column_bitmap = g_favicon_manager->Get(address.CStr());
					}
				}
				if ( item_data->column_bitmap.IsEmpty() )
					item_data->column_query_data.column_image = GetImage();
			}
			break;
		}
		case 1:
		{
			break;
		}
		case 2:
		{
			break;
		}
		case 3:
		{
			break;
		}
    }

    return OpStatus::OK;
}


/***********************************************************************************
 ** SetSiteFolder
 **
 ** @param
 **
 **
 ***********************************************************************************/
void HistoryModelItem::SetSiteFolder(HistoryModelSiteFolder* folder) {

    if(m_siteFolder)
		m_siteFolder->RemoveChild();

    m_siteFolder = folder;

    if(m_siteFolder)
		m_siteFolder->AddChild();
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryItem::Listener* HistoryModelItem::GetListener(HistoryItem* core_item)
{
	if(!core_item)
		return 0;

	return core_item->GetListenerByType(core_item->GetType());
}


/***********************************************************************************
 ** GetSiteFolder
 **
 ** @param
 **
 ** @return
 ***********************************************************************************/
HistoryModelSiteFolder* HistoryModelItem::GetSiteFolder(BOOL create_folder)
{
	if(!create_folder)
		return m_siteFolder;

	if(m_siteFolder)
		return m_siteFolder;

	if(GetCoreItem())
	{
		HistorySiteFolder* folder = GetCoreItem()->GetSiteFolder();

		if(!folder)
			return 0;

		HistoryItem::Listener* listener = GetListener(folder);

		if(!listener)
			SetSiteFolder(OP_NEW(HistoryModelSiteFolder, (folder)));
		else
			SetSiteFolder((HistoryModelSiteFolder*) listener);

		return m_siteFolder;
	}
	return 0;
}


/***********************************************************************************
 ** GetPrefixFolder
 **
 **
 **
 ** @return
 ***********************************************************************************/
HistoryModelPrefixFolder* HistoryModelItem::GetPrefixFolder()
{
	if(m_prefixFolder)
		return m_prefixFolder;

	if(GetCoreItem())
	{
		HistoryPrefixFolder* folder = GetCoreItem()->GetPrefixFolder();

		if(!folder)
			return 0;

		if(folder->GetContained())
			folder = folder->GetPrefixFolder();

		if(!folder)
			return 0;

		HistoryItem::Listener* listener = GetListener(folder);

		if(!listener)
			m_prefixFolder = OP_NEW(HistoryModelPrefixFolder, (folder));
		else
			m_prefixFolder = (HistoryModelPrefixFolder*) listener;

		return m_prefixFolder;
	}
	return 0;
}



/*___________________________________________________________________________*/
/*                         HISTORY MODEL SITE FOLDER                         */
/*___________________________________________________________________________*/

/***********************************************************************************
 ** HistoryModelSiteFolder - Constructor
 **
 ** @param
 **
 **
 ***********************************************************************************/
HistoryModelSiteFolder::HistoryModelSiteFolder(HistorySiteFolder* folder)
	: HistoryModelFolder(folder)
{
	m_num_children      = 0;

	for(int i = 0; i < NUM_TIME_PERIODS; i++)
		m_subfolders[i] = 0;
}


/***********************************************************************************
 ** HistoryModelSiteFolder - Destructor
 **
 **
 **
 **
 ***********************************************************************************/
HistoryModelSiteFolder::~HistoryModelSiteFolder()
{
 	for(int i = 0; i < NUM_TIME_PERIODS; i++)
 		OP_DELETE(m_subfolders[i]);
}


/***********************************************************************************
 ** GetSubFolder
 **
 ** @param
 **
 ** @return
 ***********************************************************************************/
HistoryModelSiteSubFolder* HistoryModelSiteFolder::GetSubFolder(TimePeriod period)
{
	HistoryItem* core_item = GetCoreItem();

    //Site folder must have core item
	OP_ASSERT(core_item);

	if(!core_item)
		return 0;

    //Core item must be a folder
	OP_ASSERT(core_item->GetType() == OpTypedObject::FOLDER_TYPE);

	if(core_item->GetType() != OpTypedObject::FOLDER_TYPE)
		return 0;

    //Site folder must have subsite folders
	OP_ASSERT(m_subfolders);

	if(!m_subfolders)
		return 0;

	if(m_subfolders[period])
		return m_subfolders[period];

	HistorySiteSubFolder*  folder   = &((HistorySiteFolder*) core_item)->GetSubFolder(period);

    //Core item should have site sub folder
	OP_ASSERT(folder);

	if(!folder)
		return 0;

	HistoryItem::Listener* listener = GetListener(folder);

	if(!listener)
		m_subfolders[period] = OP_NEW(HistoryModelSiteSubFolder, (folder));
	else
		m_subfolders[period] = (HistoryModelSiteSubFolder*) listener;

	return m_subfolders[period];
}
