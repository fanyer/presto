/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick/models/HistoryAutocompleteModel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeViewDropdown.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/desktop_util/skin/SkinUtils.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/hotlist/HotlistModelItem.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/models/HistoryModelItem.h"
#include "adjunct/quick/models/Bookmark.h"

#include "modules/history/OpHistoryModel.h"
#include "modules/widgets/OpButton.h"
#include "modules/search_engine/WordHighlighter.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/opdata/UniString.h"

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES


//////////////////////////////////////////////
// PageBasedAutocompleteItem
//////////////////////////////////////////////
#define ICON_COLUMN_INDENTATION 3
#define MINIMUM_BADGE_WIDTH 24

#define RANK_TIME_FACTOR				2.0
#define RANK_TIME_MAX_DOWNGRADE			-3.0
#define RANK_BOOKMARK_TITLE_FACTOR		2.0
#define RANK_BOOKMARK_NICKNAME_FACTOR	10.0
#define RANK_PAGE_TITLE_FACTOR			2.0
#define RANK_PAGE_URL_FACTOR			3.0

#define HISTORY_ITEM_URL_LIMIT			0.6
#define PAGE_CONTENT_URL_LIMIT			0.4
#define PAGE_CONTENT_TITLE_LMIT			0.3

#define AUTOCOMPLETE_ITEM_FORMAT		UNI_L("%s - %s")
#define SHORTENING						"..."
#define SHORTENING_VISIBLE_LENGTH		2
#define MAX_ADDRESSBAR_STR_LENG			1000

HistoryAutocompleteModelItem::HistoryAutocompleteModelItem():
	m_rank(-1),
	m_badge_width(0),
	m_use_favicons(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableAddressbarFavicons) == 1),
	m_search_type(UNKNOWN)
{}

OP_STATUS HistoryAutocompleteModelItem::ShortenUrl(OpString& text, const OpStringC& preserve, int max_length)
{
	if (max_length <= 0)
		return OpStatus::ERR;

	int source_length = text.Length();

	if (source_length > max_length)
	{
		uni_char* ret = ShortenUrlInternal(text, preserve, max_length, source_length);
		RETURN_OOM_IF_NULL(ret);
		text.TakeOver(ret);
	}
	return OpStatus::OK;
}


double HistoryAutocompleteModelItem::CalculateRank(const OpStringC& search_pattern, const OpStringC& text, const char* separator,double factor) const
{
	if (search_pattern.IsEmpty() || text.IsEmpty() || separator == NULL)
		return 0;

	double rank = 0;

	OpAutoVector<OpString> words;
	StringUtils::SplitString(words,search_pattern, UNI_L(' '));

	for(UINT32 i = 0; i < words.GetCount(); i++)
	{
		rank += CalculateRankPart(*words.Get(i),text,separator, factor);
	}

	return rank;
}

double HistoryAutocompleteModelItem::CalculateRankPart(const OpStringC& search_pattern, const OpStringC& text, const char* separator,double factor) const
{
	double rank = 0;
	int match_index = 0;
	int separator_index = 0;
	unsigned int depth_counter = 1;

	if ((match_index = text.FindI(search_pattern)) != KNotFound)
	{
		while ((separator_index = text.Find(separator, separator_index)) != KNotFound && separator_index < match_index)
		{
			separator_index++;
			depth_counter++;
		}
		rank = factor / depth_counter;
	}
	return rank;
}

uni_char* HistoryAutocompleteModelItem::ShortenUrlInternal(const OpStringC& input, const OpStringC& preserve, int max_length, int input_length)
{
	UniString uni_input;
	RETURN_VALUE_IF_ERROR(uni_input.SetConstData(input.CStr(),input_length),NULL);
	
	UniString uni_out;
	
	const uni_char* slash = UNI_L("/");

	if(uni_input.FindFirstOf(UNI_L("/?")) != OpDataNotFound)
	{
		bool add_delimiter = true;
		
		OpAutoPtr< OtlCountedList<UniString> > parts(uni_input.Split('/'));

		int count = parts->Count();
		for (OtlList<UniString>::Iterator part = parts->Begin(); part != parts->End(); ++part)
		{
			count--;
			if (part->IsEmpty())
			{
				if (count == 0)
					RETURN_VALUE_IF_ERROR(uni_out.AppendConstData(slash,1),NULL);
				continue;
			}

			if (max_length - uni_out.Length() - SHORTENING_VISIBLE_LENGTH <= 0)
				break;
			
			if (count == 0)
			{
				if ((input_length > max_length - int(uni_out.Length())) && part->FindFirstOf(UNI_L("?&")) != OpDataNotFound)
				{
					RETURN_VALUE_IF_ERROR(ShortenUrlQuery(*part, preserve, max_length - uni_out.Length()),NULL);
				}

				RETURN_VALUE_IF_ERROR((uni_out.AppendConstData(slash),1), NULL);
				RETURN_VALUE_IF_ERROR(uni_out.Append(*part), NULL);
				continue;
			}

			if ((!add_delimiter || part->Length() > SHORTENING_VISIBLE_LENGTH ) && 
				(!preserve.CStr() || part->FindFirst(preserve) == OpDataNotFound) && input_length > (max_length - (int)uni_out.Length()))
			{
				if (!add_delimiter)
				{
					input_length -= part->Length();
					continue;
				}

				RETURN_VALUE_IF_ERROR(uni_out.AppendConstData(slash,1), NULL);
				add_delimiter=false;
				input_length -= part->Length() + 1;
				RETURN_VALUE_IF_ERROR(uni_out.AppendConstData(UNI_L(SHORTENING),sizeof(SHORTENING)-1), NULL);
			}
			else
			{
				RETURN_VALUE_IF_ERROR(uni_out.AppendConstData(slash,1), NULL);
				RETURN_VALUE_IF_ERROR(uni_out.Append(*part), NULL);
				add_delimiter = true;
				input_length -= part->Length() + 1;
			}
		}
	}

	if (int(uni_out.Length()) > max_length)
	{
		if (max_length > SHORTENING_VISIBLE_LENGTH)
		{
			uni_out.Trunc(max_length - SHORTENING_VISIBLE_LENGTH);
			RETURN_VALUE_IF_ERROR(uni_out.AppendConstData(UNI_L(SHORTENING),sizeof(SHORTENING)-1), NULL);
		}
		else
		{
			uni_out.Trunc(max_length);
		}
	}

	uni_char* output = OP_NEWA(uni_char, uni_out.Length()+1);
	if (!output)
		return NULL;

	uni_out.CopyInto(output, uni_out.Length());
	output[uni_out.Length()] = '\0';
	return output;
}

OP_STATUS HistoryAutocompleteModelItem::ShortenUrlQuery(UniString& text, const OpStringC& preserve, int max_length)
{
	int input_length = text.Length();

	UniString uni_out;

	if (text.FindFirst(UNI_L("&")) == OpDataNotFound || max_length <= SHORTENING_VISIBLE_LENGTH)
	{
		if (max_length > SHORTENING_VISIBLE_LENGTH)
		{
			text.Trunc(max_length - SHORTENING_VISIBLE_LENGTH);
			RETURN_IF_ERROR(text.AppendConstData(UNI_L(SHORTENING),sizeof(SHORTENING)-1));
		}
		else
		{
			text.Trunc(max_length);
		}
	}

	bool add_delimiter = true;

	OpAutoPtr< OtlCountedList<UniString> > parts(text.Split('&'));

	int count = parts->Count();
	for (OtlList<UniString>::Iterator part = parts->Begin(); part != parts->End(); ++part)
	{
		count--;
		if (part->IsEmpty())
			continue;
			
		if (max_length - uni_out.Length() - SHORTENING_VISIBLE_LENGTH <= 0)
			break;

		if (count == 0)
		{
			if (add_delimiter)
				RETURN_IF_ERROR(uni_out.AppendConstData(UNI_L("&"),1));
			RETURN_IF_ERROR(uni_out.Append(*part));
			continue;
		}

		if ((!add_delimiter || part->Length() > SHORTENING_VISIBLE_LENGTH ) &&
			(!preserve.CStr() || part->FindFirst(preserve) == OpDataNotFound) && input_length > (max_length - (int)uni_out.Length()))
		{
			if (!add_delimiter)
			{
				input_length -= part->Length();
				continue;
			}

			RETURN_IF_ERROR(uni_out.AppendConstData(UNI_L(SHORTENING),sizeof(SHORTENING)-1));
			RETURN_IF_ERROR(uni_out.AppendConstData(UNI_L("&"),1));
			
			add_delimiter=false;
			input_length -= part->Length() + 1;
		}
		else
		{
			RETURN_IF_ERROR(uni_out.Append(*part));
			RETURN_IF_ERROR(uni_out.AppendConstData(UNI_L("&"),1));
			
			add_delimiter = true;
			input_length -= part->Length() + 1;
		}
	}

	text = uni_out;

	return OpStatus::OK;
}

OP_STATUS HistoryAutocompleteModelItem::JoinParts(OpStringC primary, OpStringC secondary, OpString& result)
{
	if (secondary.IsEmpty())
		return result.Set(primary);

	if (UiDirection::Get() == UiDirection::RTL)
		op_swap(primary, secondary);

	result.Empty();
	return result.AppendFormat(AUTOCOMPLETE_ITEM_FORMAT, primary.CStr(), secondary.CStr());
}


unsigned int PageBasedAutocompleteItem::m_font_width = 0;

PageBasedAutocompleteItem::PageBasedAutocompleteItem(HistoryModelPage* page)
  : m_page(page)
  , m_bookmark(NULL)
{
	m_packed_init = 0;
	OP_ASSERT(m_page);
	SetSearchType(HISTORY);
}

PageBasedAutocompleteItem::~PageBasedAutocompleteItem()
{
	if (m_bookmark)
	{
		m_bookmark->SetListener(NULL);
		m_bookmark = NULL; 
	}
}

OP_STATUS PageBasedAutocompleteItem::GetAddress(OpString& address) const
{
	return m_page->GetAddress(address);
}

BOOL PageBasedAutocompleteItem::HasDisplayAddress()
{
	return m_bookmark && m_bookmark->GetHasDisplayUrl(); 
}

OP_STATUS PageBasedAutocompleteItem::GetDisplayAddress(OpString& address) const
{
	if (m_search_type == BOOKMARK)
	{
		if (m_bookmark)
		{
			return address.Set(m_bookmark->GetDisplayUrl());
		}
		else
		{
			// If it is a bookmark and m_bookmark is not set
			// then it may be a nickname to a bookmark.
			// In this case real bookmark (and its address)
			// is kept in associated item.
		
			OpString nick;
			RETURN_IF_ERROR(m_page->GetAddress(nick));
			HistoryPage* bookmark_item = NULL;
			DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

			// Verify, that it really was a nick

			if (OpStatus::IsSuccess(history_model->GetBookmarkByNick(nick, bookmark_item)))
				return bookmark_item->GetAddress(address);
		}
	}
	return GetAddress(address);
}

OP_STATUS PageBasedAutocompleteItem::GetTitle(OpString& title)
{
	switch (m_search_type)
	{
		case BOOKMARK:
		{
			if (m_bookmark)
			{
				return title.Append(m_bookmark->GetName());
			}
			else
			{
				OpString nick;
				RETURN_IF_ERROR(m_page->GetAddress(nick));
				HistoryPage* bookmark_item = NULL;
				DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

				if (OpStatus::IsSuccess(history_model->GetBookmarkByNick(nick, bookmark_item)))
				{
					return title.Append(nick);
				}
				else
				{
					return title.Append(m_page->GetTitle());
				}
			}
			break;
		}
		case PAGE_CONTENT:
		case HISTORY:
		case OPERA_PAGE:
		{
			return title.Append(m_page->GetTitle());
			break;
		}
		default:
		{
			OP_ASSERT(!"Unknown search type");
			break;
		}
	}

	return OpStatus::OK;
}

OP_STATUS PageBasedAutocompleteItem::UpdateRank(const OpStringC& match_text)
{
	m_rank = 0;
	// title match
	OpString title;
	RETURN_IF_ERROR(GetTitle(title));
	if (m_search_type == PAGE_CONTENT)
		m_rank += CalculateRank(match_text, title, " ", RANK_PAGE_TITLE_FACTOR / 2);
	else
		m_rank += CalculateRank(match_text, title, " ", RANK_PAGE_TITLE_FACTOR);

	// url address match
	OpString address;
	RETURN_IF_ERROR(GetStrippedAddress(address));
	m_rank += CalculateRank(match_text, address, "/", RANK_PAGE_URL_FACTOR);

	if (m_search_type == BOOKMARK)
	{
		m_rank = m_rank * 2; // bookmark bonus;

		if (m_bookmark)
			m_rank += CalculateRank(match_text, m_bookmark->GetName(), " ", RANK_BOOKMARK_TITLE_FACTOR);

		// for bookmarks display and go to url address might be not the same thing, which is awesome;)
		OpString display_addr;
		if(HasDisplayAddress() && OpStatus::IsSuccess(GetDisplayAddress(display_addr)))
		{
			OpString domain;
			unsigned domain_offset = 0;
			RETURN_IF_ERROR(StringUtils::FindDomain(display_addr, domain, domain_offset));

			OpString cut_address;
			RETURN_IF_ERROR(cut_address.Set(display_addr.SubString(domain_offset).CStr()));
			m_rank += CalculateRank(match_text, cut_address, "/", RANK_PAGE_URL_FACTOR);
		}

		OpAutoVector<OpString> nick_urls;
		if (address.HasContent() && !uni_strpbrk(address.CStr(), UNI_L(".:/?\\")))
			g_hotlist_manager->ResolveNickname(address, nick_urls);

		// Check if it's a bookmark nickname
		if (nick_urls.GetCount() >= 1)
		{
			// address might have nick value, in case we deal with nicknamed bookmark
			if (address == match_text)
				m_rank += RANK_BOOKMARK_NICKNAME_FACTOR;
			else
				m_rank += CalculateRank(match_text, address, " ", RANK_PAGE_URL_FACTOR) * 2;
		}
	}

	// time ranking
	time_t two_months_earlier = g_timecache->CurrentTime() - 60*24*60*60;
	time_t access_time = GetAccessed();
	m_rank += max(RANK_TIME_FACTOR * (access_time - two_months_earlier) / (60*24*60*60) , RANK_TIME_MAX_DOWNGRADE);

	return OpStatus::OK;
}


OP_STATUS PageBasedAutocompleteItem::GetStrippedAddress(OpString & address)
{
	return m_page->GetStrippedAddress(address);
}

BOOL PageBasedAutocompleteItem::IsUserDeletable()
{
	return m_page && !m_bookmark && m_page->IsInHistory();
}

const OpStringC8 PageBasedAutocompleteItem::GetItemImage()
{
	return m_page->GetImage();
}

OP_STATUS PageBasedAutocompleteItem::GetItemData(OpTreeModelItem::ItemData* item_data)
{
	switch (item_data->query_type)
	{
		case INIT_QUERY:
		{
			SetColumnsWidths(item_data);
			RETURN_IF_ERROR(GetFlags(item_data));
			break;
		}
		case COLUMN_QUERY:
		{
			if (item_data->column_query_data.column == 0)
			{
				item_data->column_query_data.column_indentation = ICON_COLUMN_INDENTATION;
			}
			RETURN_IF_ERROR(GetContents(item_data));
			break;
		}
		case ASSOCIATED_IMAGE_QUERY:
		{
			if (m_search_type == HISTORY || m_search_type == PAGE_CONTENT)
			{
				RETURN_IF_ERROR(item_data->associated_image_query_data.image->Set("Speeddial Close"));
			}
			break;
		}
		case SKIN_QUERY:
		{
			item_data->skin_query_data.skin = "Address Bar Drop Down Item";
			break;
		}
	}
	return OpStatus::OK;
}

OP_STATUS PageBasedAutocompleteItem::GetFlags(OpTreeModelItem::ItemData* item_data)
{
	item_data->flags |= FLAG_FORMATTED_TEXT;
	return OpStatus::OK;
}

void PageBasedAutocompleteItem::SetColumnsWidths(OpTreeModelItem::ItemData* item_data)
{
	OP_ASSERT(item_data->treeview);
	if (item_data->treeview)
	{
		item_data->treeview->SetColumnFixedWidth(0, max(MINIMUM_BADGE_WIDTH,m_badge_width));
		item_data->treeview->SetColumnWeight(1, 1);
	}
}

void PageBasedAutocompleteItem::SetBookmark(Bookmark* bookmark) 
{ 
	if (!bookmark || m_bookmark)
		return;

	bookmark->SetListener(this);
	m_bookmark = bookmark;
}


OP_STATUS PageBasedAutocompleteItem::GetContents(OpTreeModelItem::ItemData* item_data)
{
	switch(item_data->column_query_data.column)
	{
		case 0:
			return GetIcon(item_data);
		case 1:
			return GetTextContent(item_data);
		default:
			OP_ASSERT(!"Unknown value");
	}

	return OpStatus::OK;
}

OP_STATUS PageBasedAutocompleteItem::GetIcon(OpTreeModelItem::ItemData* item_data)
{
	if (m_use_favicons)
	{
		OpString adr;
		RETURN_IF_ERROR(GetDisplayAddress(adr));
		Image img = g_favicon_manager->Get(adr.CStr());
		if (!img.IsEmpty())
		{
			item_data->column_bitmap = img;
			return OpStatus::OK;
		}
	}

	switch (m_search_type)
	{
		case BOOKMARK:
				item_data->column_query_data.column_image  = "Address Dropdown Bookmark Icon";
			break;
			
		case PAGE_CONTENT:
		case HISTORY:
			if (m_use_favicons || item_data->flags & OpTreeModelItem::FLAG_FOCUSED)
				item_data->column_query_data.column_image = "Address Dropdown History Icon";
			break;
			
		case OPERA_PAGE:
			if (m_use_favicons || item_data->flags & OpTreeModelItem::FLAG_FOCUSED)
				item_data->column_query_data.column_image  = m_page->GetImage();
			break;
			
		default:
			OP_ASSERT(!"Unknown search type");
			break;
	}
	return OpStatus::OK;
}

OP_STATUS PageBasedAutocompleteItem::GetTextContent(OpTreeModelItem::ItemData* item_data)
{
	if (item_data->flags & FLAG_NO_PAINT)
	{
		return OpStatus::OK;
	}

	OpString address, title;
	RETURN_IF_ERROR(GetDisplayAddress(address));
	RETURN_IF_ERROR(GetTitle(title));

	OP_ASSERT(m_font_width);

	if (address == title)
		title.Empty();

	int title_limit = MAX_ADDRESSBAR_STR_LENG;
	int url_limit = MAX_ADDRESSBAR_STR_LENG;

	if (item_data->treeview && m_font_width > 0)
	{
		OpRect view_size = item_data->treeview->GetRect();
		int number_of_char =  max(max(view_size.width - m_badge_width - 10, 25) / int(m_font_width) - SHORTENING_VISIBLE_LENGTH, 10);
		if (number_of_char > SHORTENING_VISIBLE_LENGTH)
		{
			if (m_search_type == PAGE_CONTENT)
			{
				if (title.Length() > number_of_char * PAGE_CONTENT_TITLE_LMIT)
				{
					title_limit = max((number_of_char - SHORTENING_VISIBLE_LENGTH) * PAGE_CONTENT_TITLE_LMIT, SHORTENING_VISIBLE_LENGTH+1);
				}
				if (address.Length() > (number_of_char - SHORTENING_VISIBLE_LENGTH) * PAGE_CONTENT_URL_LIMIT)
				{
					url_limit = max((number_of_char - SHORTENING_VISIBLE_LENGTH) * PAGE_CONTENT_URL_LIMIT, SHORTENING_VISIBLE_LENGTH+1);
				}
			}
			else if (address.Length() + title.Length() >= number_of_char)
			{
				if (title.IsEmpty())
					url_limit = max(number_of_char, SHORTENING_VISIBLE_LENGTH + 1);
				else
					url_limit = max(number_of_char * HISTORY_ITEM_URL_LIMIT, SHORTENING_VISIBLE_LENGTH + 1);
			}
		}
	}

	if (m_search_type != OPERA_PAGE)
	{
		if (!m_packed.no_unescaping)
		{
			RETURN_IF_ERROR(g_globalHistory->MakeUnescaped(address));
		}

		OpString domain;
		unsigned domain_offset = 0;
		RETURN_IF_ERROR(StringUtils::FindDomain(address, domain, domain_offset));

		OpString cut_address;
		RETURN_IF_ERROR(cut_address.Set(address.SubString(domain_offset).CStr()));

		OpString rest_url;
		RETURN_IF_ERROR(rest_url.Set(cut_address.SubString(domain.Length())));

		RETURN_IF_ERROR(StringUtils::AppendHighlight(domain, m_highlight_text));

		int max_rest_len = url_limit - domain.Length();
		if (rest_url.Length() > max_rest_len)
		{
			if (max_rest_len > 0)
				RETURN_IF_ERROR(ShortenUrl(rest_url, m_highlight_text, max_rest_len ));
			else
			{
				if (rest_url.Length() > SHORTENING_VISIBLE_LENGTH)
					RETURN_IF_ERROR(rest_url.Set(SHORTENING));
				else
					rest_url.Empty();
			}
		}

		RETURN_IF_ERROR(StringUtils::AppendHighlight(rest_url, m_highlight_text));

		if (title.Length() > title_limit)
		{
			title[max(title_limit - SHORTENING_VISIBLE_LENGTH, 0)] = '\0';
			RETURN_IF_ERROR(title.Append(SHORTENING));
		}

		if (m_search_type == PAGE_CONTENT)
		{
			OpString excerpt;
			RETURN_IF_ERROR(excerpt.Set("  |  <i>"));
			RETURN_IF_ERROR(excerpt.Append(m_associated_text));
			RETURN_IF_ERROR(excerpt.Append("</i>"));
			int color = SkinUtils::GetTextColor("Address DropDown Suggestion Label");
			if (color >= 0 && !(item_data->flags & FLAG_FOCUSED))
			{
				RETURN_IF_ERROR(StringUtils::AppendColor(excerpt,color,0,excerpt.Length()));
			}
			RETURN_IF_ERROR(title.Append(excerpt));
		}

		RETURN_IF_ERROR(StringUtils::AppendHighlight(title, m_highlight_text));
		if (!(item_data->flags & FLAG_FOCUSED))
		{
			COLORREF domain_color = OP_RGB(0,0,0);
			OpSkinElement* skin_element = g_skin_manager->GetSkinElement("Address DropDown URL Label");
			if (skin_element && OpStatus::IsSuccess(skin_element->GetLinkColor((UINT32*)&domain_color, 0)))
			{
				RETURN_IF_ERROR(StringUtils::AppendColor(domain, domain_color, 0, domain.Length()));
			}
			
			int label_color = SkinUtils::GetTextColor("Address DropDown URL Label");
			if (label_color  >= 0)
			{
				RETURN_IF_ERROR(StringUtils::AppendColor(rest_url, label_color , 0, rest_url.Length()));
			}
			
			int title_color = SkinUtils::GetTextColor("Address DropDown URL Title");
			if (title_color  >= 0)
				RETURN_IF_ERROR(StringUtils::AppendColor(title, title_color, 0, title.Length()));
			
		}
		OpString url;
		RETURN_IF_ERROR(url.Set(domain));
		RETURN_IF_ERROR(url.Append(rest_url));
		OpString entry;
		RETURN_IF_ERROR(JoinParts(url, title, entry));
		RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(entry));
	}
	else
	{
		RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(address));
	}

	return OpStatus::OK;
}


//////////////////////////////////////////////
// SimpleAutocompleteItem
//////////////////////////////////////////////

/* FIXME: It is somewhat broken to not use a unique ID for each item
 * in a single model, since it is likely that there is some code that
 * assumes unique, non-0 IDs.  However, I don't want to spend the
 * effort on fixing the issue here, and I still want the assert to
 * trigger if anyone writes new code that makes this error.  So here
 * I'll just use 1 to silence the assert.
 */
static const INT32 SIMPLEAUTOCOMPLETEITEM_COMMON_ID = 1;
SimpleAutocompleteItem::SimpleAutocompleteItem(OpTreeModel* tree_model)
  : m_simpleitem(tree_model, 0, SIMPLEAUTOCOMPLETEITEM_COMMON_ID, OpTypedObject::UNKNOWN_TYPE)
{
	m_simpleitem.SetCleanOnRemove(TRUE);
	m_simpleitem.SetHasFormattedText(TRUE);
	m_packed_init = 0;
}

OP_STATUS SimpleAutocompleteItem::GetItemData(OpTreeModelItem::ItemData* item_data)
{
	switch (item_data->query_type)
	{
		case INIT_QUERY:
		{
			item_data->treeview->SetColumnFixedWidth(0, max(MINIMUM_BADGE_WIDTH, m_badge_width));
			item_data->treeview->SetColumnWeight(1, 1);
			if(m_packed.is_tips)
			{
				item_data->flags |= FLAG_NO_SELECT;
			}
			break;
		}
		case COLUMN_QUERY:
		{
			if (item_data->column_query_data.column == 0)
			{
				item_data->column_query_data.column_indentation = ICON_COLUMN_INDENTATION;

				if (m_use_favicons)
				{
					OpString adr;
					RETURN_IF_ERROR(GetDisplayAddress(adr));
					Image img = g_favicon_manager->Get(adr.CStr());

					if (!img.IsEmpty())
					{
						item_data->column_bitmap = img;
						return OpStatus::OK;
					}
				}

				if ((m_use_favicons ||item_data->flags & FLAG_FOCUSED) && (!m_packed.is_show_all)) 
				{
					item_data->column_query_data.column_image = "Address Dropdown Search Icon";
				}

				return OpStatus::OK;
			}
			break;
		}
		case ASSOCIATED_IMAGE_QUERY:
		{
			if (IsUserDeletable())
			{
				RETURN_IF_ERROR(item_data->associated_image_query_data.image->Set("Search Close History Item"));
			}
			break;
		}
		case SKIN_QUERY:
		{
			item_data->skin_query_data.skin = "Address Bar Drop Down Item";
			return OpStatus::OK;
		}
	}
	return m_simpleitem.GetItemData(item_data);
}

//////////////////////////////////////////////
// SearchSuggestionAutocompleteItem
//////////////////////////////////////////////

OP_STATUS SearchSuggestionAutocompleteItem::GetItemData(OpTreeModelItem::ItemData *item_data)
{
	switch(item_data->query_type)
	{
		case INIT_QUERY:
		{
			OP_ASSERT(item_data->treeview);
			if (item_data->treeview)
			{
				item_data->treeview->SetColumnFixedWidth(0, m_badge_width);
			}
			item_data->flags |= FLAG_FORMATTED_TEXT;
			break;
		}
		case COLUMN_QUERY:
		{
			return GetColumnData(item_data);
		}
	}
	return SimpleAutocompleteItem::GetItemData(item_data);
}

OP_STATUS SearchSuggestionAutocompleteItem::GetColumnData(OpTreeModelItem::ItemData *item_data)
{
	switch (item_data->column_query_data.column)
	{
		case 0:
		{
			return SimpleAutocompleteItem::GetItemData(item_data);
		}
		case 1:
		{
			item_data->column_query_data.column_indentation = 0;
			SearchTemplate* search_template = g_searchEngineManager->GetByUniqueGUID(GetSearchProvider());
			if (search_template)
			{
				OpString result;
				RETURN_IF_ERROR(result.Set(m_search_suggestion));
				RETURN_IF_ERROR(StringUtils::AppendHighlight(result,m_highlight_text));

				int suggestion_color = SkinUtils::GetTextColor("Address DropDown URL Title");
				if (suggestion_color >= 0 && !(item_data->flags & FLAG_FOCUSED))
				{
					RETURN_IF_ERROR(StringUtils::AppendColor(result, suggestion_color, 0, result.Length()));
				}

				OpString search_engine_string;
				if (m_show_search_engine || IsSearchWith())
				{
					RETURN_IF_ERROR(search_template->GetEngineName(search_engine_string));

					int color = SkinUtils::GetTextColor("Address DropDown Suggestion Label");
					if (color >= 0 && !(item_data->flags & FLAG_FOCUSED))
						RETURN_IF_ERROR(StringUtils::AppendColor(search_engine_string, color, 0, search_engine_string.Length()));
				}
				OpString primary;
				RETURN_IF_ERROR(primary.Set(result));
				RETURN_IF_ERROR(JoinParts(primary, search_engine_string, result));
				RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(result.CStr()));
			}
			break;
		}
	}
	return OpStatus::OK;
}

OP_STATUS SearchSuggestionAutocompleteItem::GetDisplayAddress(OpString& address) const
{
	OpString search_engine;
	SearchTemplate* search_template = g_searchEngineManager->GetByUniqueGUID(GetSearchProvider());
	if (search_template)
		RETURN_IF_ERROR(search_template->GetEngineName(search_engine));

	return JoinParts(GetSearchSuggestion(), search_engine, address);
}

OP_STATUS SearchSuggestionAutocompleteItem::GetAddress(OpString& address) const
{
	SearchTemplate* search_template = g_searchEngineManager->GetByUniqueGUID(GetSearchProvider());
	if (!search_template)
		return OpStatus::ERR;

	RETURN_IF_ERROR(address.Set(search_template->GetKey()));
	RETURN_IF_ERROR(address.Append(" "));

	return address.Append(GetSearchSuggestion());
}
