/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.
* It may not be distributed under any circumstances.
*
*/

#include "core/pch.h"

#include "adjunct/quick/models/NavigationItem.h"

OP_STATUS NavigationItem::GetItemData(ItemData* item_data)
{
	item_data->column_query_data.column_image = GetImage();

	if (item_data->query_type == INIT_QUERY)
	{
		if (IsSeparator())
		{
			item_data->flags |= FLAG_SEPARATOR;
			item_data->flags |= FLAG_DISABLED | FLAG_INITIALLY_DISABLED;
		}
		else if (IsFolder() && IsAllBookmarks())
		{
			item_data->flags |= FLAG_INITIALLY_OPEN;
		}

		return OpStatus::OK;
	}

	if (item_data->query_type == MATCH_QUERY)
	{
		BOOL match = FALSE;

		if (item_data->match_query_data.match_type == MATCH_FOLDERS)
		{
			match = IsFolder();
		}
		if (match)
		{
			item_data->flags |= FLAG_MATCHED;
		}
		return OpStatus::OK;
	}

	if (item_data->query_type == INFO_QUERY)
	{
		GetInfoText(*item_data->info_query_data.info_text);
		return OpStatus::OK;
	}

	if (item_data->query_type != COLUMN_QUERY)
	{
		return OpStatus::OK;
	}

	if (IsFolder() )
	{
		if( IsDeletedFolder() )
		{
			int index = GetIndex();
			if( index != -1 && GetChildItem())
			{
				item_data->flags |= FLAG_BOLD;
			}
		}
	}

	if (item_data->column_query_data.column == 0 || item_data->column_query_data.column == 1)
	{
		item_data->column_query_data.column_text->Set(GetName());
	}

	return OpStatus::OK;
}

void NavigationItem::GetInfoText(OpInfoText &text)
{
	text.SetStatusText(GetName().CStr());

	if (GetHotlistItem())
	{
		OpString str;
		g_languageManager->GetString(Str::DI_ID_EDITURL_SHORTNAMELABEL, str);
		text.AddTooltipText(str.CStr(), GetHotlistItem()->GetShortName().CStr());
		g_languageManager->GetString(Str::DI_IDLABEL_HL_CREATED, str);
		text.AddTooltipText(str.CStr(), GetHotlistItem()->GetCreatedString().CStr());
	}
}
