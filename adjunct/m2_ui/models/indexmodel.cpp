/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2_ui/models/indexmodel.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/skin/OpSkinManager.h"
#include <locale.h>

#include <time.h>

#define SMOOTH_M2_SORTING

double g_thread_checking = 0;

INT32 GenericIndexModelItem::s_item_id = -1;

///	---------------------------------------------------------------------------
///	HeaderModelItem
///	---------------------------------------------------------------------------

OP_STATUS HeaderModelItem::GetItemData(ItemData* item_data)
{
	switch (item_data->query_type)
	{
		case INIT_QUERY:
		{
			item_data->item_type = TREE_MODEL_ITEM_TYPE_HEADER;
			if (m_is_open)
			{
				item_data->flags |= FLAG_INITIALLY_OPEN;
			}
			return OpStatus::OK;
		}
		case SKIN_QUERY:
		{
			if (GetModel()->HasTwoLinedItems())
			{
				item_data->skin_query_data.skin = "Mail Treeview Header Skin";
			}
			else
			{
				item_data->skin_query_data.skin = "Mail Treeview Header Single Skin";
			}

			return OpStatus::OK;
		}
		case COLUMN_QUERY:
		{
			item_data->column_query_data.column_text->Empty();
			break;
		}
		default:
			return OpStatus::OK;
	}	
	
	g_skin_manager->GetTextColor("Mail Treeview Header Skin", &item_data->column_query_data.column_text_color);
	item_data->flags |= FLAG_BOLD | FLAG_VERTICAL_ALIGNED | FLAG_CENTER_ALIGNED;

	if (item_data->column_query_data.column == 0)
	{
		item_data->column_query_data.column_span = GetModel()->GetColumnCount();
		item_data->column_query_data.column_text->Set(m_name.CStr());
	}
	return OpStatus::OK;
}

int HeaderModelItem::DateHeaderData::CompareWith(GenericIndexModelItem& item, IndexModel& model)
{
	return m_end_time - item.GetDate();
}

bool HeaderModelItem::DateHeaderData::IsInGroup(GenericIndexModelItem& item, IndexModel& model)
{
	time_t time = item.GetDate();
	return m_begin_time <= time && time <= m_end_time;
}

time_t HeaderModelItem::GetDate()
{
	return m_data->GetDate();
}

int HeaderModelItem::CompareWith(GenericIndexModelItem& item)
{
	if (GetModel()->GetGroupingMethod() != IndexTypes::GROUP_BY_NONE || item.IsHeader())
		return m_data->CompareWith(item, *GetModel());
	else
		return -1;
}

HeaderModelItem* HeaderModelItem::ConstructDateHeader(Str::LocaleString name, time_t begin_time, time_t end_time, IndexTypes::GroupingMethods grouping_method)
{
	OpAutoPtr<DateHeaderData> data(OP_NEW(DateHeaderData, (begin_time, end_time)));
	RETURN_VALUE_IF_NULL(data.get(), NULL);
	OpAutoPtr<HeaderModelItem> item(OP_NEW(HeaderModelItem, (data.release(), grouping_method)));
	RETURN_VALUE_IF_NULL(item.get(), NULL);
	if (name == Str::NOT_A_STRING)
	{
		char year[5];
		struct tm * date = op_localtime(&end_time);
		op_itoa(1900 + date->tm_year, year, 10);
		item->m_name.Set(year);
	}
	else
	{
		RETURN_VALUE_IF_ERROR(g_languageManager->GetString(name, item->m_name), NULL);
	}
	return item.release();
}

HeaderModelItem* HeaderModelItem::ConstructFlagHeader(Str::LocaleString name, StoreMessage::Flags flag, BOOL is_set, BOOL is_main_header, IndexTypes::GroupingMethods grouping_method)
{
	OpAutoPtr<FlagHeaderData> data(OP_NEW(FlagHeaderData, (flag, is_set, is_main_header)));
	RETURN_VALUE_IF_NULL(data.get(), NULL);
	OpAutoPtr<HeaderModelItem> item(OP_NEW(HeaderModelItem, (data.release(), grouping_method)));
	RETURN_VALUE_IF_NULL(item.get(), NULL);
	RETURN_VALUE_IF_ERROR(g_languageManager->GetString(name, item->m_name), NULL);
	return item.release();
}

///	---------------------------------------------------------------------------
///	IndexModelItem
///	---------------------------------------------------------------------------
OP_STATUS IndexModelItem::GetItemData(ItemData* item_data)
{
	IndexModel* model = GetModel();

	switch (item_data->query_type)
	{
		case INIT_QUERY:
		{
			if (m_is_open)
			{
				item_data->flags |= FLAG_INITIALLY_OPEN;
			}
			if (model->HasItemTwoLines(this))
			{
				item_data->flags |= FLAG_ASSOCIATED_TEXT_ON_TOP;
			}

			return OpStatus::OK;
		}
		case MATCH_QUERY:
		{
			IndexModel* model = GetModel();
			if (GetParentItem() && GetParentItem()->IsHeader())
				item_data->flags |= FLAG_MATCHED_CHILD;
			else
				item_data->flags &= ~FLAG_MATCHED_CHILD;
			if (model->Matches(m_id))
				item_data->flags |= FLAG_MATCHED;

			return OpStatus::OK;
		}
		case ASSOCIATED_TEXT_QUERY:
		{
			if (!model->HasItemTwoLines(this))
				return OpStatus::OK;

			break;
		}
		case UNREAD_QUERY:
		{
			if (!MessageEngine::GetInstance()->GetStore()->GetMessageFlag(m_id, Message::IS_READ))
				item_data->flags |= FLAG_UNREAD;
			return OpStatus::OK;
		}
		case SKIN_QUERY:
		{
			if (!model->HasTwoLinedItems() || (item_data->flags & FLAG_FOCUSED || item_data->flags & FLAG_SELECTED))
				return OpStatus::OK;

			if (model->HasItemTwoLines(this))
				item_data->skin_query_data.skin = "Mail Treeview Two Line Parent Skin";
			else
				item_data->skin_query_data.skin = "Mail Treeview Two Line Skin";
			return OpStatus::OK;
		}
		case COLUMN_QUERY:
		{
			if (item_data->column_query_data.column == IndexModel::TwoLineFromSubjectColumn)
			{
				item_data->column_query_data.column = IndexModel::FromColumn;
				RETURN_IF_ERROR(GetItemData(item_data));
				item_data->column_query_data.column = IndexModel::TwoLineFromSubjectColumn;
				return OpStatus::OK;
			}
			if (m_is_fetching)
			{
				item_data->column_query_data.column_text->Empty();
				return OpStatus::OK;
			}
			else if (!m_is_fetched)
			{
				if (model->DelayedItemData(m_id))
				{
					item_data->column_query_data.column_text->Empty();
					m_is_fetching = TRUE;
					return OpStatus::OK;
				}
				else
				{
					m_is_fetched = TRUE;
				}
			}

			// reset string so we don't repeat the subject instead of showing missing date etc:
			item_data->column_query_data.column_text->Empty();
			break;
		}
		case INFO_QUERY:
			break;
		default:
			return OpStatus::OK;
	}

	MessageEngine *engine = MessageEngine::GetInstance();
	Store *store = engine->GetStore();

	if (!store)
		return OpStatus::ERR;

	Message message;

	if (OpStatus::IsError(store->GetMessageMinimalHeaders(message, m_id)))
	{
		if (store->HasFinishedLoading())
		{
			// Ghost, TODO: find out something clever to do here :p
			return OpStatus::OK; 
		}

		item_data->flags |= FLAG_DISABLED;

		// If we haven't finished loading yet, show 'loading' message
		if (item_data->column_query_data.column == 3)
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_LOADING_MESSAGE, *item_data->column_query_data.column_text));
		}

		return OpStatus::OK;
	}
	else
	{
		item_data->flags &= ~FLAG_DISABLED;
	}

	if (item_data->query_type == INFO_QUERY)
	{
		return GetInfo(message, item_data);
	}

	UINT32 unread_count = 0;
	BOOL display_needed = !(item_data->flags & FLAG_NO_PAINT);

	// We need more info if this is a resent message, fetch all data
	if (message.IsFlagSet(Message::IS_OUTGOING) && message.IsFlagSet(Message::IS_RESENT))
		OpStatus::Ignore(store->GetMessageData(message));

	BOOL read = message.IsFlagSet(Message::IS_READ);

	if (!read)
	{
		item_data->flags |= FLAG_BOLD;
		item_data->flags |= FLAG_UNREAD;
	}

	BOOL unseen_messages = FALSE;

	if (display_needed && !(item_data->flags & FLAG_OPEN) && item_data->query_type != ASSOCIATED_TEXT_QUERY)
	{
		unread_count = model->GetUnreadChildCount(GetIndex(), unseen_messages);
		if (unread_count && !read)
		{
			unread_count++;
		}
		else if (unread_count)
		{
			item_data->flags |= FLAG_BOLD;
		}
	}
	else if (model->HasItemTwoLines(this) && item_data->query_type == ASSOCIATED_TEXT_QUERY)
	{
		unread_count = model->GetUnreadChildCount(GetIndex(), unseen_messages);

		if (unread_count)
			item_data->flags |= FLAG_BOLD;
		unread_count = 0;
	}

	UINT32 text_color = OP_RGB(70, 70, 70), associated_text_color = OP_RGB(0,0,0);

	// blue/unvisited color if unseen
	if (unseen_messages || (!read && !message.IsFlagSet(Message::IS_SEEN)))
	{
		associated_text_color = text_color = OP_RGB(0x0, 0x59, 0xB3);
	}

	Index* in_thread = engine->GetIndexer()->GetThreadIndex(m_id);
	if (in_thread && in_thread->IsIgnored())
	{
		associated_text_color = text_color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
	}

	if (item_data->query_type == ASSOCIATED_TEXT_QUERY)
	{
		return GetAssociatedText(message, text_color, associated_text_color, unread_count, item_data);
	}

	item_data->column_query_data.column_text_color = text_color;

	switch (item_data->column_query_data.column)
	{
	case IndexModel::StatusColumn: // STATUS
	{
		GetStatusColumnData(message, item_data);
		break;
	}
	case IndexModel::FromColumn: // FROM
	{
		RETURN_IF_ERROR(GetFromColumnData(message, item_data));
		break;
	}
	case IndexModel::SubjectColumn: // SUBJ
	{
		RETURN_IF_ERROR(GetSubjectColumnData(in_thread, message, unread_count, item_data));
		break;
	}
	case IndexModel::LabelIconColumn:
	case IndexModel::LabelColumn: // Labels
	{
		RETURN_IF_ERROR(GetLabelColumnData(message, item_data));
		break;
	}
	case IndexModel::SizeColumn: // SIZE
	{
		RETURN_IF_ERROR(GetSizeColumnData(message, item_data));
		break;
	}
	case IndexModel::SentDateColumn: // Sent date
	{
		RETURN_IF_ERROR(GetSentDateColumnData(message, item_data));
		break;
	}
	break;
	}

	return OpStatus::OK;
}

OP_STATUS IndexModelItem::GetInfo(Message& message, ItemData* item_data)
{
	Header::HeaderValue subject_header;
	RETURN_IF_ERROR(message.GetHeaderValue(Header::SUBJECT, subject_header));
	RETURN_IF_ERROR(item_data->info_query_data.info_text->AddTooltipText(NULL, subject_header.CStr()));

	OpString date, label;
	time_t t;
	Header* date_header = message.GetHeader(Header::DATE);
	if (date_header)
	{
		date_header->GetValue(t);
		RETURN_IF_ERROR(date_header->GetTranslatedName(label));

		// FIXME: %#c is nice on windows, but we need %c on other platforms
		OpString time_format;
#ifdef MSWIN
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_HEADER_TIME_FORMAT, time_format));
#else
		RETURN_IF_ERROR(time_format.Set(UNI_L("%c")));
#endif
		RETURN_IF_ERROR(FormatTime(date, time_format.CStr(), t));
		RETURN_IF_ERROR(item_data->info_query_data.info_text->AddTooltipText(label.CStr(), date.CStr()));
	}

	int size = message.GetMessageSize();
	OpString bytestr;
	RETURN_IF_ERROR(FormatByteSize(bytestr, size));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDL_SIZE, label));
	RETURN_IF_ERROR(item_data->info_query_data.info_text->AddTooltipText(label.CStr(), bytestr.CStr()));

	return OpStatus::OK;
}

OP_STATUS IndexModelItem::GetAssociatedText(Message& message, UINT32 text_color, UINT32 associated_text_color, UINT32 unread_count, ItemData* item_data)
{
	item_data->associated_text_query_data.text_color = text_color;

	Header::HeaderValue subject_header;
	RETURN_IF_ERROR(message.GetHeaderValue(Header::SUBJECT, subject_header));
	if (subject_header.HasContent())
		RETURN_IF_ERROR(item_data->associated_text_query_data.text->Set(subject_header));
	else
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NO_SUBJECT, *item_data->associated_text_query_data.text));

	if (unread_count)
	{
		uni_char buf[20];
		uni_sprintf(buf,UNI_L(" (%i)"),unread_count);
		RETURN_IF_ERROR(item_data->associated_text_query_data.text->Append(buf));
	}

	item_data->associated_text_query_data.text_color = associated_text_color;
	item_data->associated_text_query_data.associated_text_indentation = -2;

	return OpStatus::OK;
}

void IndexModelItem::GetStatusColumnData(Message& message, ItemData* item_data)
{
	BOOL read = message.IsFlagSet(Message::IS_READ);
	Index* spam = g_m2_engine->GetIndexById(IndexTypes::SPAM);
	Index* trash = g_m2_engine->GetIndexById(IndexTypes::TRASH);
	if (trash && trash->Contains(m_id))
		{ item_data->column_query_data.column_image = "Mail Bullet Trash"; }
	else if (spam && spam->Contains(m_id))
		{ item_data->column_query_data.column_image = "Mail Bullet Spam"; }
	else if (message.IsFlagSet(StoreMessage::IS_FLAGGED)) 
		{ item_data->column_query_data.column_image = "Mail Bullet Flagged"; }
	else if (message.IsFlagSet(Message::IS_OUTGOING) && !message.IsFlagSet(Message::IS_SENT))
		{ item_data->column_query_data.column_image = "Mail Bullet Draft"; }
	else if (message.IsFlagSet(Message::IS_RESENT)) 
		{ item_data->column_query_data.column_image = "Mail Bullet Redirected"; }
	else if (message.IsFlagSet(Message::IS_REPLIED) && !read) 
		{ item_data->column_query_data.column_image =  "Mail Bullet Replied Unread";}
	else if (message.IsFlagSet(Message::IS_REPLIED)) 
		{ item_data->column_query_data.column_image =  "Mail Bullet Replied";}
	else if (message.IsFlagSet(Message::IS_FORWARDED) && !read) 
		{ item_data->column_query_data.column_image =  "Mail Bullet Forwarded Unread";}
	else if (message.IsFlagSet(Message::IS_FORWARDED)) 
		{ item_data->column_query_data.column_image =  "Mail Bullet Forwarded";}
	else if (message.IsFlagSet(Message::IS_SENT) && !read) 
		{ item_data->column_query_data.column_image = "Mail Bullet Sent Unread";}
	else if (message.IsFlagSet(Message::IS_SENT)) 
		{ item_data->column_query_data.column_image = "Mail Bullet Sent";}
	else if (message.IsFlagSet(Message::IS_QUEUED)) 
		{ item_data->column_query_data.column_image = "Mail Bullet Queued";}
	else if (!read && !message.IsFlagSet(Message::IS_SEEN))
		{ item_data->column_query_data.column_image = "Mail Bullet Unseen";}
	else if (!read) 
		{ item_data->column_query_data.column_image = "Mail Bullet New";}
	else
		{ item_data->column_query_data.column_image = "Mail Bullet Read";}
}

OP_STATUS IndexModelItem::GetFromColumnData(Message& message, ItemData* item_data)
{
	Header *header = NULL;

	if (message.IsFlagSet(Message::IS_OUTGOING))
	{
		if (message.IsFlagSet(Message::IS_RESENT))
			header = message.GetHeader(Header::RESENTTO);
		if (!header)
			header = message.GetHeader(Header::TO);
	}
	else
	{
		header = message.GetHeader(Header::FROM);
	}

	if (header)
	{
		const Header::From* from = header->GetFirstAddress();
		if (from)
		{
			RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(from->GetAddress()));

			if (from->GetName().HasContent())
			{
				RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(from->GetName()));
				uni_char* tab = const_cast<uni_char*>(item_data->column_query_data.column_text->CStr());
				while ((tab=uni_strchr(tab, '\t')) != NULL) *tab=' ';
			}

			if (item_data->column_query_data.column_text->HasContent())
			{
				const char* image = NULL;
				g_m2_engine->GetGlueFactory()->GetBrowserUtils()->GetContactImage(from->GetAddress(), image);
				if (image && op_strcmp(image, "Contact Unknown") != 0 && op_strcmp(image, "Contact0") != 0)
				{
					item_data->column_query_data.column_image = image;
				}
			}

			IndexModel* model = GetModel();
			if (message.IsFlagSet(Message::IS_OUTGOING) &&
				model->GetIndexId() != IndexTypes::SENT &&
				model->GetIndexId() != IndexTypes::OUTBOX &&
				model->GetIndexId() != IndexTypes::DRAFTS && 
				model->GetIndex()->GetSpecialUseType() != AccountTypes::FOLDER_SENT)
			{
				OpString to_string;

				RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_TO, to_string));
				to_string.Append(UNI_L(" "));

				item_data->column_query_data.column_text->Insert(0, to_string); 
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS IndexModelItem::GetSubjectColumnData(Index* in_thread, Message& message, UINT32 unread_count, ItemData* item_data)
{
	if (!GetModel()->HasTwoLinedItems())
	{
		MessageEngine* engine = MessageEngine::GetInstance();
		BOOL complete = engine->GetStore()->GetMessageHasBody(m_id) && !message.IsFlagSet(Message::PARTIALLY_FETCHED);
		Index* trash = engine->GetIndexById(IndexTypes::TRASH);
		Index* spam = engine->GetIndexById(IndexTypes::SPAM);
		BOOL read = message.IsFlagSet(Message::IS_READ);

		if (message.GetAccountPtr() && message.GetAccountPtr()->IsScheduledForFetch(m_id))
		{
			item_data->column_query_data.column_image = "Mail Icon Get";
		}
		else if (trash && trash->Contains(m_id))
		{
			item_data->column_query_data.column_image = read ? "Mail Icon Trash" : "Mail Icon Trash";
		}
		else if (spam && spam->Contains(m_id))
		{
			item_data->column_query_data.column_image = "Mail Icon Spam";
		}
		else if (in_thread && in_thread->IsIgnored())
		{
			item_data->column_query_data.column_image = "Mail Icon Ignored";
		}
		else if (in_thread && in_thread->IsWatched())
		{
			if (complete)
				item_data->column_query_data.column_image = "Mail Icon Followed";
			else
				item_data->column_query_data.column_image = "Mail Icon Followed No Body";
		}
		else if (message.IsFlagSet(Message::IS_OUTGOING) && !message.IsFlagSet(Message::IS_SENT))
		{
			item_data->column_query_data.column_image = "Mail Icon Draft";
		}
		else if (message.IsFlagSet(Message::IS_NEWS_MESSAGE))
		{
			if (complete)
				item_data->column_query_data.column_image = read ? "Mail Icon News Read" : "Mail Icon News Unread";
			else
				item_data->column_query_data.column_image = "Mail Icon News No Body";
		}
		else if (message.IsFlagSet(Message::IS_NEWSFEED_MESSAGE))
		{
			item_data->column_query_data.column_image = read ? "Mail Icon Feed Read" : "Mail Icon Feed Unread";
		}
		else if (!message.IsFlagSet(Message::IS_SEEN))
		{
			item_data->column_query_data.column_image = "Mail Icon Unseen";
		}
		else if (complete)
		{
			item_data->column_query_data.column_image = read ? "Mail Icon Read" : "Mail Icon Unread";
		}
		else
		{
			item_data->column_query_data.column_image = read ?  "Mail Icon No Body" : "Mail Icon Unread No Body";
		}
	}

	Header::HeaderValue subject_header;
	RETURN_IF_ERROR(message.GetHeaderValue(Header::SUBJECT, subject_header));
	if (subject_header.HasContent())
		RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(subject_header));
	else
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NO_SUBJECT, *item_data->column_query_data.column_text));

	const uni_char* subject = item_data->column_query_data.column_text->CStr();

	if (subject)
	{
		uni_char* tab = const_cast<uni_char*>(subject);
		while ((tab=uni_strchr(tab, '\t')) != NULL) *tab=' ';

		if (uni_strnicmp(subject, UNI_L("Re: "), 4) == 0)
		{
			item_data->column_query_data.column_text_prefix = 4;
		}
		else if (uni_strnicmp(subject, UNI_L("Fwd: "), 5) == 0)
		{
			item_data->column_query_data.column_text_prefix = 5;
		}
	}

	if (unread_count)
	{
		uni_char buf[20];
		uni_sprintf(buf,UNI_L(" (%i)"),unread_count);
		RETURN_IF_ERROR(item_data->column_query_data.column_text->Append(buf));
	}

	return OpStatus::OK;
}

OP_STATUS IndexModelItem::GetLabelColumnData(Message& message, ItemData* item_data)
{
	OpString display_string;
	Index* index;

	for (INT32 it = -1; (index = g_m2_engine->GetIndexer()->GetRange(IndexTypes::FIRST_FOLDER, IndexTypes::LAST_FOLDER, it)) != NULL; )
	{
		if (index->Contains(message.GetId()))
		{
			if (display_string.IsEmpty())
			{
				RETURN_IF_ERROR(index->GetImages(item_data->column_query_data.column_image, item_data->column_bitmap));
				if (item_data->column_query_data.column == IndexModel::LabelIconColumn)
				{
					item_data->column_query_data.column_sort_order = index->GetId();
					break;
				}
				RETURN_IF_ERROR(display_string.Set(index->GetName()));
			}
			else
			{
				RETURN_IF_ERROR(display_string.AppendFormat(UNI_L(", %s"), index->GetName().CStr()));
			}
		}
	}

	if (message.GetPriority() != 3)
	{
		OpString priority;
		item_data->column_query_data.column_sort_order = message.GetPriority() + 1000;

		switch (message.GetPriority())
		{
		case 1:
			item_data->column_query_data.column_text_color = OP_RGB(0x33, 0x33, 0x99);
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_HIGHEST_PRIORITY, priority));
			break;
		case 2:
			item_data->column_query_data.column_text_color = OP_RGB(0x33, 0x33, 0x99);
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_HIGH_PRIORITY, priority));
			break;
		case 4:
			item_data->column_query_data.column_text_color = OP_RGB(0x33, 0x33, 0x33);
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_LOW_PRIORITY, priority));
			break;
		case 5:
			item_data->column_query_data.column_text_color = OP_RGB(0x33, 0x33, 0x33);
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_LOWEST_PRIORITY, priority));
			break;
		}
		if (display_string.IsEmpty())
		{
			RETURN_IF_ERROR(display_string.Set(priority));
			if (message.GetPriority() < 3)
				item_data->column_query_data.column_image = "Priority High";
			else
				item_data->column_query_data.column_image = "Priority Low";
		}
		else
		{
			RETURN_IF_ERROR(display_string.AppendFormat(UNI_L(", %s"), priority.CStr()));
		}
	}
	
	if (message.IsFlagSet(StoreMessage::IS_READ))
		item_data->column_query_data.column_text_color = OP_RGB(0X6D, 0X6D, 0X6D);

	if (item_data->flags & FLAG_FOCUSED && display_string.IsEmpty())
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NO_LABEL, display_string));
	}

	if (display_string.HasContent() && item_data->column_query_data.column == IndexModel::LabelColumn)
	{
		RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(display_string));
	}
	return OpStatus::OK;
}

OP_STATUS IndexModelItem::GetSizeColumnData(Message& message, ItemData* item_data)
{
	int size = message.GetMessageSize();
	if (message.IsFlagSet(StoreMessage::IS_READ))
		item_data->column_query_data.column_text_color = OP_RGB(0X6D, 0X6D, 0X6D);
	item_data->flags &= (~FLAG_BOLD);
	item_data->flags |= FLAG_RIGHT_ALIGNED;

	OpString bytestr;

	RETURN_IF_ERROR(FormatByteSize(bytestr, size));
	RETURN_IF_ERROR(item_data->column_query_data.column_text->Append(bytestr));
	return OpStatus::OK;
}

OP_STATUS IndexModelItem::GetSentDateColumnData(Message& message, ItemData* item_data)
{
	if (message.IsFlagSet(StoreMessage::HAS_ATTACHMENT))
		item_data->column_query_data.column_image = "Mail Attachments Mini";
	time_t date 		 = 0;
	if (message.IsFlagSet(Message::IS_OUTGOING) && message.IsFlagSet(Message::IS_RESENT))
		message.GetDateHeaderValue(Header::RESENTDATE, date);
	if (!date)
		date = m_date;

	if (date)
	{
		RETURN_IF_ERROR(FormatTimeStandard(*item_data->column_query_data.column_text, date, GetModel()->GetTimeFormat(date)));
		if (message.IsFlagSet(StoreMessage::IS_READ))
			item_data->column_query_data.column_text_color = OP_RGB(0X6D, 0X6D, 0X6D);
		item_data->flags |= FLAG_RIGHT_ALIGNED;
		item_data->flags &= (~FLAG_BOLD);
	}
	return OpStatus::OK;
}

int	IndexModelItem::GetNumLines() 
{ 
	return GetModel()->HasItemTwoLines(this)? 2: 1;
}

int IndexModelItem::CompareWith(GenericIndexModelItem& item)
{
	if (item.s_item_id < item.GetID() && item.GetID() <= -1)
	{
		if (GetModel()->GetGroupingMethod() != IndexTypes::GROUP_BY_NONE)
		{
			return - item.CompareWith(*this);
		}
		return 1;
	}
	return MessageEngine::GetInstance()->GetStore()->CompareMessages(GetID(), item.GetID(), GetModel()->SortBy());
}

IndexModelItem::~IndexModelItem()
{
}

// ---------------------------------------------------------------------------------
// IndexModel class
// ---------------------------------------------------------------------------------

IndexModel::IndexModel() :
	m_index(NULL),
	m_trash(NULL),
	m_refcount(0),
	m_start_pos(-1),
	m_model_flags(0),
	m_grouping_method(IndexTypes::GROUP_BY_NONE),
	m_model_type(IndexTypes::MODEL_TYPE_FLAT),
	m_model_age(IndexTypes::MODEL_AGE_FOREVER),
	m_model_sort(IndexTypes::MODEL_SORT_BY_NONE),
	m_model_sort_ascending(false),
	m_is_waiting_for_delayed_message(false),
	m_two_lined_items(false),
	m_threaded(false),
	m_delayed_item_check(0),
	m_delayed_item_count(0),
	m_sort_by(Store::SORT_BY_ID),
	m_begin_of_last_week(0),
	m_begin_of_this_week(0),
	m_begin_of_this_day(0)
{
	// Initialize messages
	g_main_message_handler->SetCallBack(this, MSG_M2_DELAYED_CLOSE,  (MH_PARAM_1)(this));
	m_timer.SetTimerListener(this);
}

IndexModel::~IndexModel()
{
	g_main_message_handler->UnsetCallBacks(this); // Cleanup message handler
	BeginChange(); // do this to make sure the emptying is not broadcasted
	Empty();
	EndChange();
}

void IndexModel::Empty()
{
	if (!m_index)
		return;

	DeleteAll();

	if (m_index)
	{
		m_index->RemoveObserver(this);
	}

	if (m_trash && m_trash != m_index)
	{
		m_trash->RemoveObserver(this);
	}

	MessageEngine::GetInstance()->RemoveMessageListener(this);

	m_index = NULL;
	m_start_pos = -1;
	m_threaded = FALSE;
	m_copy.Clear();
	m_groups_map.DeleteAll();
}

BOOL IndexModel::IndexHidden(index_gid_t index_id)
{
	if (index_id == IndexTypes::TRASH && (m_model_flags&(1<<IndexTypes::MODEL_FLAG_TRASH))==0)
	{
		return TRUE;
	}
	if (index_id == IndexTypes::SPAM && (m_model_flags&(1<<IndexTypes::MODEL_FLAG_SPAM))==0)
	{
		return TRUE;
	}
	if (index_id == IndexTypes::RECEIVED_NEWS && (m_model_flags&(1<<IndexTypes::MODEL_FLAG_NEWSGROUPS))==0)
	{
		return TRUE;
	}
	if (index_id == MessageEngine::GetInstance()->GetIndexer()->GetRSSAccountIndexId() && (m_model_flags&(1<<IndexTypes::MODEL_FLAG_NEWSFEEDS))==0)
	{
		return TRUE;
	}
	if (index_id == IndexTypes::RECEIVED_LIST && (m_model_flags&(1<<IndexTypes::MODEL_FLAG_MAILING_LISTS))==0)
	{
		return TRUE;
	}
	if (index_id == IndexTypes::HIDDEN && (m_model_flags&(1<<IndexTypes::MODEL_FLAG_HIDDEN))==0)
	{
		return TRUE;
	}

	return FALSE;
}


BOOL IndexModel::HideableIndex(index_gid_t index_id)
{
	return (index_id == IndexTypes::TRASH || index_id == IndexTypes::SPAM || index_id == IndexTypes::RECEIVED_NEWS || index_id == IndexTypes::RECEIVED_LIST || index_id == IndexTypes::HIDDEN);
}


OP_STATUS IndexModel::ReInit(Index* index)
{
	if (!index)	// if NULL, reinit with same index
		index = m_index;

	if(m_timer.IsStarted())
		m_timer.Stop();

	ModelLock lock(this);

	Empty();

	if (!index)
	{
		return OpStatus::OK; // nothing else to do, really.
	}

	m_index = index;
	m_index->AddObserver(this);
	MessageEngine::GetInstance()->AddMessageListener(this);

	message_gid_t model_selected_message;
	MessageEngine::GetInstance()->GetIndexModelType(m_index->GetId(), m_model_type, m_model_age, m_model_flags, m_model_sort, m_model_sort_ascending, m_grouping_method, model_selected_message);
	if (m_model_type == IndexTypes::MODEL_TYPE_THREADED)
	{
		m_threaded = TRUE;
	}

	m_trash = MessageEngine::GetInstance()->GetIndexer()->GetIndexById(IndexTypes::TRASH);
	Index* unread = MessageEngine::GetInstance()->GetIndexer()->GetIndexById(IndexTypes::UNREAD);

	if (!m_trash || !unread)
	{
		return OpStatus::ERR;
	}
	if (m_trash != m_index)
	{
		m_trash->AddObserver(this);
	}

	if (m_index == MessageEngine::GetInstance()->GetIndexer()->GetIndexById(IndexTypes::UNREAD_UI) && m_grouping_method == IndexTypes::GROUP_BY_READ)
	{
		m_grouping_method = IndexTypes::GROUP_BY_NONE;
	}

	if (!m_threaded)
	{
		// flat view
		for (INT32SetIterator it(m_index->GetIterator()); it; it++)
		{
			message_gid_t id = it.GetData();
			if (id)
			{
				// not add items that are in trash
				if (m_index->MessageHidden(id))
				{
					continue;
				}

				IndexModelItem *item = OP_NEW(IndexModelItem, (id));
				if (!item)
					return OpStatus::ERR_NO_MEMORY;

				m_copy.Insert(item);
				AddLast(item);
			}
		}
	}
	else
	{
		// threaded view

		for (INT32SetIterator it(m_index->GetIterator()); it; it++)
		{
			message_gid_t id = it.GetData();
			if (id)
			{
				INT32 got_tree_index = -1;
				FindThreadedMessages(id, got_tree_index, -1);
			}
		}
	}

	ManageHeaders();
	CacheGroupIds();
	return OpStatus::OK;
}

message_gid_t IndexModel::FindVisibleMessage(message_gid_t id)
{
	const DuplicateMessage* dupe = MessageEngine::GetInstance()->GetStore()->GetMessageDuplicates(id);

	if (!dupe)
	{
		if (m_index->Contains(id) && !m_index->MessageHidden(id))
			return id;
	}
		
	while (dupe)
	{
		id = dupe->m2_id;
			
		if (m_index->Contains(id) && !m_index->MessageHidden(id))
			return id;

		dupe = dupe->next;
	}

	return 0;
}


OP_STATUS IndexModel::FindThreadedMessages(message_gid_t id, INT32 &got_tree_index, INT32 insert_before)
{
	if (!m_index || !m_index->Contains(id) || m_index->MessageHidden(id))
	{
		return OpStatus::OK;
	}

	// not add same item twice
	IndexModelItem *current_item = OP_NEW(IndexModelItem, (id));
	if (!current_item)
		return OpStatus::ERR_NO_MEMORY;

	if (m_copy.Contains(current_item))
	{
		OP_DELETE(current_item);
		return OpStatus::OK;
	}

	Index* unread = MessageEngine::GetInstance()->GetIndexer()->GetIndexById(IndexTypes::UNREAD);
	BOOL is_unread = FALSE;

	if (unread)
		is_unread = unread->Contains(id);

	// reset value:
	BOOL found_tree_index = FALSE;

	// avoid inserting several times
	m_copy.Insert(current_item);

	message_gid_t parent_id = 0, parent_to_check = MessageEngine::GetInstance()->GetStore()->GetParentId(id);

	for (int i = 0; i < 3 && parent_to_check; i++)
	{
		parent_id = FindVisibleMessage(parent_to_check);

		if (parent_id != 0)
		{
			break;
		}
		parent_to_check = MessageEngine::GetInstance()->GetStore()->GetParentId(parent_to_check);
	}

	// first, add all that are available through parent/siblings

	if (parent_id && parent_id != id)
	{
		int i = GetPosition(parent_id);

		if (i != -1)
		{
			if (is_unread && !(MessageEngine::GetInstance()->GetStore()->GetMessageFlags(id) & Message::IS_SEEN))
			{
				GenericIndexModelItem *parent_item = GetItemByIndex(i);
				while (parent_item)
				{
					parent_item->m_is_open = TRUE;
					parent_item = parent_item->GetParentItem();
				}
			}
			got_tree_index = i;
			found_tree_index = TRUE;
		}

		// if not, add parents recursively
		if (!found_tree_index)
		{
			FindThreadedMessages(parent_id, got_tree_index, insert_before);
		}
	}

	current_item->m_groups.Add(m_grouping_method, reinterpret_cast<void*>(FindGroup(current_item, m_groups_map.Get(m_grouping_method))));
	if (insert_before != -1 && got_tree_index == -1)
	{
		got_tree_index = InsertBefore(current_item, insert_before);
	}
	else
	{
		if(got_tree_index != -1 && current_item->GetGroup(m_grouping_method) < GetItemByIndex(got_tree_index)->GetGroup(m_grouping_method))
			GetItemByIndex(got_tree_index)->m_groups.Add(m_grouping_method, reinterpret_cast<void*>(current_item->GetGroup(m_grouping_method)));
		got_tree_index = AddLast(current_item, got_tree_index);
	}
	CacheGroupIdsForItem(current_item);

	if ((m_start_pos == -1 || m_start_pos > got_tree_index) && is_unread)
	{
		m_start_pos = got_tree_index;
	}
	else if (m_start_pos != -1 && m_start_pos >= got_tree_index)
	{
		m_start_pos++;
	}
	return OpStatus::OK;
}

UINT32 IndexModel::GetUnreadChildCount(INT32 pos, BOOL& unseen_messages)
{
	unseen_messages = FALSE;

	if (m_threaded)
	{
		GenericIndexModelItem *item;
		UINT32 counter = 0;

		INT32 count = GetSubtreeSize(pos);
		pos++;

		for (INT32 i = 0; i < count; i++)
		{
			item = GetItemByIndex(i + pos);

			if (item)
			{
				UINT64 flags = MessageEngine::GetInstance()->GetStore()->GetMessageFlags(item->GetID());
				if (!(flags & (1 << Message::IS_READ)))
				{
					counter++;
					if (!(flags & (1 << Message::IS_SEEN)))
					{
						unseen_messages = TRUE;
					}
				}
			}
		}
		return counter;
	}
	return 0;
}

OP_STATUS IndexModel::GetColumnData(ColumnData* column_data)
{
	column_data->custom_sort = TRUE;

	// set image and string
	OpString string;
	switch (column_data->column)
	{
		case StatusColumn: // Status
			column_data->image = "Mail Bullet New";
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_COL_STATUS, string));
			break;
		case FromColumn: // From or To
			if (	m_index && (
					m_index->GetId() == IndexTypes::OUTBOX ||
					m_index->GetId() == IndexTypes::DRAFTS || 
					m_index->GetId() == IndexTypes::SENT) ||
					m_index->GetSpecialUseType() == AccountTypes::FOLDER_SENT)
				RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_TO, string));
			else
				RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_FROM, string));
			
			break;

		case SubjectColumn: // Subject 
		case TwoLineFromSubjectColumn: // Combined Subject and From for two lined items
			RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_FROMSUBJECT, string));
			break;

		case LabelColumn: // Label
		case LabelIconColumn:
			RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_LABELS, string));
			column_data->custom_sort = FALSE;
			column_data->sort_by_string_first = TRUE;
			break;
		case SizeColumn: // Size
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_M2_COL_SIZE, string));
			column_data->right_aligned = TRUE;
			break;

		case SentDateColumn: // Sent (mail) or Published (feeds)
			{
				column_data->right_aligned = TRUE;
				// if it's a feed account then use the published string
				if (m_index && g_m2_engine->IsIndexNewsfeed(m_index->GetId()))
					RETURN_IF_ERROR(g_languageManager->GetString(Str::S_FEED_PUBLISHED, string));
				else
					RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_TOSENT, string));
				break;
			}
	}

	return column_data->text.Set(string);
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS IndexModel::GetTypeString(OpString& type_string)
{
	return g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_MAIL, type_string);
}
#endif

INT32 IndexModel::CompareItems(INT32 column, OpTreeModelItem* item1, OpTreeModelItem* item2)
{
	if (column == 1 && m_index == NULL)
		return 0;

	m_sort_by = Store::SORT_BY_ID;

	switch (column)
	{
	case StatusColumn:
		m_sort_by = Store::SORT_BY_STATUS;
		break;
	case FromColumn:
		{
			if (m_index == NULL)
				return 0;

			switch (m_index->GetId())
			{
			case IndexTypes::SENT:
			case IndexTypes::OUTBOX:
			case IndexTypes::DRAFTS:
				m_sort_by = Store::SORT_BY_PREV_TO;
				break;
			default:
				// exception for IMAP sent folders
				if (m_index->GetSpecialUseType() == AccountTypes::FOLDER_SENT)
					m_sort_by = Store::SORT_BY_PREV_TO;
				else
					m_sort_by = Store::SORT_BY_PREV_FROM;
				break;
			}
		}
		break;
	case SubjectColumn:
	case TwoLineFromSubjectColumn:
		m_sort_by = Store::SORT_BY_PREV_SUBJECT;
		break;
	case LabelColumn:
		OP_ASSERT(!"This should be handled in OpTreeView::OnCompareItems");
		// fall through
	case LabelIconColumn:
		m_sort_by = Store::SORT_BY_LABEL;
		break;
	case SizeColumn:
		m_sort_by = Store::SORT_BY_SIZE;
		break;
	case SentDateColumn:
	{
		m_sort_by = m_threaded ? (m_model_sort_ascending ? Store::SORT_BY_THREADED_SENT_DATE : Store::SORT_BY_THREADED_SENT_DATE_DESCENDING) : Store::SORT_BY_SENT_DATE;
		break;
	}
	default:
		//OP_ASSERT(0);
		break;
	}
	return static_cast<GenericIndexModelItem*>(item1)->CompareWith(*static_cast<GenericIndexModelItem*>(item2));
}


// Functions implementing the IndexTypes::Observer interface
OP_STATUS IndexModel::MessageAdded(Index* index, message_gid_t message, BOOL setting_keyword)
{
	if (!m_index || MessageEngine::GetInstance()->PerformingMultipleMessageChanges())
	{
		return OpStatus::OK;
	}

	if (index == m_index)
	{
		int parent = -1;
		if (m_threaded)
		{
			INT32 got_tree_index = -1;

			RETURN_IF_ERROR( FindThreadedMessages(message, got_tree_index, -1) );

			parent = GetItemParent(got_tree_index);
			
			while (parent != -1)
			{
				// Signal changed parents
				int grandparent = GetItemParent(parent);
				Change(parent, grandparent == -1);
				parent = grandparent;
			}
		}

		if (!m_threaded || parent == -1)
		{
			if (!m_index->MessageHidden(message) && GetPosition(message) == -1)
			{
				IndexModelItem *item = OP_NEW(IndexModelItem, (message));
				if (!item)
					return OpStatus::ERR_NO_MEMORY;

				m_copy.Insert(item);
				item->m_groups.Add(m_grouping_method, reinterpret_cast<void*>(FindGroup(item, m_groups_map.Get(m_grouping_method))));
				AddLast(item, parent);
				CacheGroupIdsForItem(item);
			}
		}
		return OpStatus::OK;
	}
	else if (IndexHidden(index->GetId()))
	{
		// added to trash, remove from view

		return MessageRemoved(m_index,message);
	}

	return OpStatus::ERR;
}

OP_STATUS IndexModel::MessageRemoved(Index* index, message_gid_t message, BOOL setting_keyword)
{
	if (!m_index || MessageEngine::GetInstance()->PerformingMultipleMessageChanges())
	{
		return OpStatus::OK;
	}

	if (index == m_index)
	{
		INT32 i = GetPosition(message);

		if (i != -1)
		{
			IndexModelItem *item = static_cast<IndexModelItem*>(GetItemByIndex(i));
			m_copy.Remove(item);
			GenericIndexModelItem* child = !item->GetParentItem() ? item->GetChildItem() : NULL;
			
			CacheGroupIdsForItem(item, true);
			Delete(i, FALSE);
			if (child)
			{
				Change(child->GetIndex(), TRUE);
			}
		}
	}
	else if (IndexHidden(index->GetId())) // removed from trash, add to view if in index
	{
		if (m_index->Contains(message))
		{
			if (m_threaded)
			{
				INT32 got_tree_index = -1;

				RETURN_IF_ERROR( FindThreadedMessages(message, got_tree_index) );
			}
			else if (!m_index->MessageHidden(message))
			{
				IndexModelItem *item = OP_NEW(IndexModelItem, (message));
				if (!item)
					return OpStatus::ERR_NO_MEMORY;

				m_copy.Insert(item);
				AddLast(item);
			}
		}
	}

	return OpStatus::OK;
}

// Function implementing the MessageEngine::MessageListener

void IndexModel::OnMessageChanged(message_gid_t message_id)
{
	if (!m_index)
	{
		return;
	}

	if (message_id == UINT_MAX)
	{
		ModelLock lock(this);

		// remove items that have become hidden (reverse order is faster)
		for (INT32 i = GetItemCount() - 1; i >= 0; i--)
		{
			GenericIndexModelItem* item = GetItemByIndex(i);

			message_gid_t id = item->GetID();

			if (!m_index->Contains(id) || m_index->MessageHidden(id))
			{
				MessageRemoved(m_index, id);
			}
		}

		// and add items that are now visible

		for (INT32SetIterator it(m_index->GetIterator()); it; it++)
		{
			message_gid_t current_id = it.GetData();
			IndexModelItem item(current_id);

			if (!m_copy.Contains(&item) && !m_index->MessageHidden(current_id))
			{
				MessageAdded(m_index, current_id);
			}
		}

		m_index->ResetUnreadCount(); // OK, doesn't need optimization
		ChangeAll();
		return;
	}

	if (!m_index->Contains(message_id))
	{
		MessageRemoved(m_index, message_id);
		return;
	}

	MessageEngine::GetInstance()->OnIndexChanged(m_index->GetId());

	BOOL message_hidden = m_index->MessageHidden(message_id);
	if (message_hidden)
	{
		MessageRemoved(m_index, message_id);
	}

	BOOL item_found = FALSE;

	INT32 i = GetPosition(message_id);

	if (i != -1)
	{
		item_found = TRUE;
		CacheGroupIdsForItem(GetItemByID(message_id));

		while (i != -1)
		{
			Change(i);
			i = GetItemParent(i);
		}
	}

	if (!item_found && !message_hidden)
	{
		MessageAdded(m_index, message_id);
	}
}

INT32 IndexModel::GetPosition(message_gid_t message_id)
{
	IndexModelItem dummy(message_id);
	IndexModelItem *copy = m_copy.FindSimilar(&dummy);

	if (copy == NULL)
	{
		return -1;
	}

	return GetIndexByItem(copy);
}


static OP_STATUS IndexModel_GetIds(GenericIndexModelItem* item, OpINT32Vector& thread_ids)
{
	const INT32 item_id = item->GetID();

	if (thread_ids.Find(item_id) != -1)
		return OpStatus::OK;

	RETURN_IF_ERROR(thread_ids.Add(item_id));

	GenericIndexModelItem* child = item->GetChildItem();
	while (child != 0)
	{
		IndexModel_GetIds(child, thread_ids);
		child = child->GetChildItem();
	}

	GenericIndexModelItem* sibling = item->GetSiblingItem();
	while (sibling != 0)
	{
		IndexModel_GetIds(sibling, thread_ids);
		sibling = sibling->GetSiblingItem();
	}

	return OpStatus::OK;
}


OP_STATUS IndexModel::GetThreadIds(message_gid_t id, OpINT32Vector& thread_ids) const
{
	IndexModel* non_const_this = (IndexModel *)(this);
	OP_ASSERT(non_const_this != 0);

	// First we find the root item for this thread.
	GenericIndexModelItem* root = non_const_this->GetItemByID(id);
	OP_ASSERT(root != 0);

	if(!root)
		return OpStatus::OK;

	while (root->GetParentItem() != 0)
		root = root->GetParentItem();

	RETURN_IF_ERROR(thread_ids.Add(root->GetID()));

	// Then we work our way from there.
	GenericIndexModelItem* first_child = root->GetChildItem();
	if (first_child != 0)
		RETURN_IF_ERROR(IndexModel_GetIds(first_child, thread_ids));

	return OpStatus::OK;
}


GenericIndexModelItem* IndexModel::GetLastInThread(GenericIndexModelItem* root, GenericIndexModelItem* last)
{
	if (!root)
		return NULL;

	if (!last)
		last = root;

	GenericIndexModelItem* child_item;

	for (child_item = root->GetChildItem(); child_item; child_item = child_item->GetSiblingItem())
	{
		if (child_item->GetID() > last->GetID())
			last = child_item;
		last = GetLastInThread(child_item, last);
	}

	return last;
}


void IndexModel::SetOpenState(message_gid_t id, BOOL open)
{
	GenericIndexModelItem* item = GetItemByID(id);
	if (item)
		item->m_is_open = open;
}


void IndexModel::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
#ifdef SMOOTH_M2_SORTING
		case MSG_M2_DELAYED_CLOSE:
			for (int i = 0; i < 20 && m_delayed_items.GetCount() > 0; i++)
			{
				// update the last visited items first (last in the queue)
				message_gid_t id = m_delayed_items.Remove(m_delayed_items.GetCount()-1);
				int pos = GetPosition(id);

				if (pos >= 0)
				{
					IndexModelItem* item = (IndexModelItem*)GetItemByIndex(pos);
					item->SetIsFetched(true);

					BroadcastItemChanged(pos /*,m_delayed_items.GetCount() == 0*/);
				}
			}

			// clear old items to avoid sluggish performance
			while (m_delayed_items.GetCount() > 100)
			{
				message_gid_t id = m_delayed_items.Remove(0);
				int pos = GetPosition(id);

				if (pos >= 0)
				{
					IndexModelItem* item = (IndexModelItem*)GetItemByIndex(pos);
					item->SetIsFetched(true); // reset m_is_fetching to FALSE
					item->SetIsFetched(false); // reset m_is_fetched to FALSE

					// must broadcast that the item must be refetched if displayed again
					BroadcastItemChanged(pos /*,m_delayed_items.GetCount() == 0*/);
				}
			}

			if (m_delayed_items.GetCount())
			{
				g_main_message_handler->PostMessage(MSG_M2_DELAYED_CLOSE, (MH_PARAM_1)this, 0);
			}
			else
			{
				m_is_waiting_for_delayed_message = FALSE;
			}
			break;
#endif // SMOOTH_M2_SORTING
		default:
			OP_ASSERT(!"Unexpected message for IndexModel");
			break;
	}
}

void IndexModel::SetTwoLinedItems(bool two_lined_items)
{ 
	 if (m_two_lined_items != two_lined_items) 
	 {
		 m_two_lined_items = two_lined_items; 
		 ReInit();
	 } 
}

bool IndexModel::HasItemTwoLines(GenericIndexModelItem* item)
{
	if (!m_two_lined_items)
		return false;
	if (item->GetParentItem() && !item->GetParentItem()->IsHeader())
		return false;
	return true;
}

BOOL IndexModel::DelayedItemData(message_gid_t message_id)
{
#ifdef SMOOTH_M2_SORTING
	BOOL post_message = FALSE;

	// count number of times this function is called within short time
	m_delayed_item_count++;

	time_t now = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTime();
	if (m_delayed_item_check == 0)
	{
		// initial call to this function
		m_delayed_item_check = now;
	}
	if (m_delayed_item_check == now)
	{
		if (m_delayed_item_count > 50)
		{
			// this function has been called x times in less than a second
			if (!m_is_waiting_for_delayed_message)
			{
				post_message = TRUE;
			}
		}
	}
	if (m_delayed_item_check != now)
	{
		// new second, start counting again...
		m_delayed_item_check = now;
		m_delayed_item_count = 0;
	}

	// check that we don't have items in the queue while not waiting for a POST. Contact johan@opera.com if this triggers
	OP_ASSERT(m_delayed_items.GetCount() == 0 || m_is_waiting_for_delayed_message);

	if (post_message || m_is_waiting_for_delayed_message)
	{
		m_delayed_items.Add(message_id);

		if (m_is_waiting_for_delayed_message)
		{
			// no need to double post
			return TRUE;
		}
	}

	if (post_message)
	{
		m_is_waiting_for_delayed_message = TRUE;
		g_main_message_handler->PostMessage(MSG_M2_DELAYED_CLOSE, (MH_PARAM_1)this, 0);

		return TRUE;
	}
#endif // SMOOTH_M2_SORTING
	return FALSE;
}

void IndexModel::ManageHeaders()
{
	OpVector<HeaderModelItem>* header;

	RETURN_VALUE_IF_NULL((header = OP_NEW(OpVector<HeaderModelItem>,())),);
	m_groups_map.Add(header);

	AddHeaderToGroup(HeaderModelItem::ConstructFlagHeader(Str::S_MAIL_GROUP_HEADER_UNREAD, StoreMessage::IS_READ, FALSE, TRUE, IndexTypes::GROUP_BY_READ));
	AddHeaderToGroup(HeaderModelItem::ConstructFlagHeader(Str::S_MAIL_GROUP_HEADER_OTHER, StoreMessage::IS_READ, TRUE, FALSE, IndexTypes::GROUP_BY_READ));

	RETURN_VALUE_IF_NULL((header = OP_NEW(OpVector<HeaderModelItem>,())),);
	m_groups_map.Add(header);

	AddHeaderToGroup(HeaderModelItem::ConstructFlagHeader(Str::S_MAIL_GROUP_HEADER_FLAGGED, StoreMessage::IS_FLAGGED, TRUE, TRUE, IndexTypes::GROUP_BY_FLAG));
	AddHeaderToGroup(HeaderModelItem::ConstructFlagHeader(Str::S_MAIL_GROUP_HEADER_OTHER, StoreMessage::IS_FLAGGED, FALSE, FALSE, IndexTypes::GROUP_BY_FLAG));

	CreateHeadersForGroupingByDate();
}

void IndexModel::CreateHeadersForGroupingByDate()
{
	Str::LocaleString day_of_week[] = { 
										Str::S_MAIL_GROUP_HEADER_SUNDAY,
										Str::S_MAIL_GROUP_HEADER_MONDAY,
										Str::S_MAIL_GROUP_HEADER_TUESDAY,
										Str::S_MAIL_GROUP_HEADER_WEDNESDAY,
										Str::S_MAIL_GROUP_HEADER_THURSDAY,
										Str::S_MAIL_GROUP_HEADER_FRIDAY,
										Str::S_MAIL_GROUP_HEADER_SATURDAY
									  };
	Str::LocaleString months[] = { Str::S_MAIL_GROUP_HEADER_JANUARY,
										Str::S_MAIL_GROUP_HEADER_FEBRUARY,
										Str::S_MAIL_GROUP_HEADER_MARCH,
										Str::S_MAIL_GROUP_HEADER_APRIL,
										Str::S_MAIL_GROUP_HEADER_MAY,
										Str::S_MAIL_GROUP_HEADER_JUNE,
										Str::S_MAIL_GROUP_HEADER_JULY,
										Str::S_MAIL_GROUP_HEADER_AUGUST,
										Str::S_MAIL_GROUP_HEADER_SEPTEMBER,
										Str::S_MAIL_GROUP_HEADER_OCTOBER,
										Str::S_MAIL_GROUP_HEADER_NOVEMBER,
										Str::S_MAIL_GROUP_HEADER_DECEMBER,
									  };

	time_t current_time = op_time(NULL);
	struct tm * date = op_localtime(&current_time);
	
	int first_day_of_week = -1;
	OP_STATUS status = g_oplocale->GetFirstDayOfWeek(first_day_of_week);
	int current_day_in_week = date->tm_wday;
	int current_month = date->tm_mon;
	int current_year = date->tm_year;

	date->tm_sec = 59;
	date->tm_min = 59;
	date->tm_hour = 23;
	time_t end_time = op_mktime(date);
	time_t begin_time = end_time;

	//DSK-364253: Added extra 10 seconds because mac and linux don't treat 00:00:00 as a begining of new day
	//or system clocks are slower then timer.
	m_timer.Start((end_time - current_time + 10) * 1000);

	OpVector<HeaderModelItem>* header;
	RETURN_VALUE_IF_NULL((header = OP_NEW(OpVector<HeaderModelItem>,())),);
	m_groups_map.Add(header);

	//Create headers for today's and yesterday's mails
	AddDays(begin_time, -1);
	AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(Str::S_MAIL_GROUP_HEADER_TODAY, begin_time + 1, end_time, IndexTypes::GROUP_BY_DATE));
	end_time = begin_time;
	m_begin_of_this_day = begin_time + 1;
	AddDays(begin_time, -1);
	AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(Str::S_MAIL_GROUP_HEADER_YESTERDAY, begin_time + 1, end_time, IndexTypes::GROUP_BY_DATE));
	end_time = begin_time;
	m_begin_of_this_week = begin_time + 1;
	//Create headers for rest days in current week

	if (OpStatus::IsSuccess(status) && first_day_of_week > -1)
	{
		int day = current_day_in_week - 2;
		if (current_day_in_week == 0 && first_day_of_week != 0)
			day = 5;
		for (; day >= first_day_of_week; day--)
		{
			AddDays(begin_time, -1);
			AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(	day_of_week[day], 
																	begin_time + 1, 
																	end_time, 
																	IndexTypes::GROUP_BY_DATE));
			end_time = begin_time;
		}
		m_begin_of_this_week = begin_time + 1;
		//Last week and rest of month
		if (current_day_in_week == first_day_of_week)
		{
			AddDays(begin_time, -6);
			AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(Str::S_MAIL_GROUP_HEADER_LAST_WEEK, begin_time + 1, end_time, IndexTypes::GROUP_BY_DATE));
			end_time = begin_time;
			m_begin_of_last_week = begin_time + 1;
		}
		else
		{
			AddDays(begin_time, -7);
			AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(Str::S_MAIL_GROUP_HEADER_LAST_WEEK, begin_time + 1, end_time, IndexTypes::GROUP_BY_DATE));
			end_time = begin_time;
			m_begin_of_last_week = begin_time + 1;
		}
	}

	date = op_localtime(&end_time);
	if (current_month == date->tm_mon)
	{
		AddDays(begin_time, -date->tm_mday);
		AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(Str::S_MAIL_GROUP_HEADER_EARLIER_THIS_MONTH, begin_time + 1, end_time, IndexTypes::GROUP_BY_DATE));
		end_time = begin_time;
	}
	else if (current_month > date->tm_mon)
	{
		int month = date->tm_mon;
		AddDays(begin_time, -date->tm_mday);
		AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(months[month], begin_time + 1, end_time, IndexTypes::GROUP_BY_DATE));
		end_time = begin_time;
	}

	// months before in this year, last a few years, older
	date = op_localtime(&end_time);
	if (current_month > date->tm_mon)
	{
		for (int month = date->tm_mon; month >= 0 ; month--)
		{
			AddDays(begin_time, -date->tm_mday);
			AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(months[month], begin_time + 1, end_time, IndexTypes::GROUP_BY_DATE));
			end_time = begin_time;
			date = op_localtime(&end_time);
		}
	} else
	{
		AddDays(begin_time, -(date->tm_yday + 1));
		AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(Str::NOT_A_STRING, begin_time + 1, end_time, IndexTypes::GROUP_BY_DATE));
		end_time = begin_time;
	}
	date = op_localtime(&end_time);
	for (int year = date->tm_year; year >= current_year - 3; year--)
	{
		AddDays(begin_time, -(date->tm_yday + 1));
		AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(Str::NOT_A_STRING, begin_time + 1, end_time, IndexTypes::GROUP_BY_DATE));
		end_time = begin_time;
		date = op_localtime(&end_time);
	}

	AddHeaderToGroup(HeaderModelItem::ConstructDateHeader(Str::S_MAIL_GROUP_HEADER_OLDER, 0, end_time, IndexTypes::GROUP_BY_DATE));
}

void IndexModel::AddDays(time_t& date, int num_of_days) //this method works only if hour is 23 
{
	//TODO: Make this method independent on hours
	struct tm* time = op_localtime(&date);
	int hour = time->tm_hour > 0 ? time->tm_hour : 24;
	date = date + num_of_days * HeaderModelItem::DayDuration;
	time = op_localtime(&date);
	if ((time->tm_hour > 0 ? time->tm_hour : 24) - hour > 0)
	{
		date -= ((time->tm_hour > 0 ? time->tm_hour : 24) - hour) * 3600;
	}
	else if ( time->tm_hour - hour < 0)
	{
		date += (hour - time->tm_hour) * 3600;
	}
}

void IndexModel::AddHeaderToGroup(HeaderModelItem* item)
{
	RETURN_VALUE_IF_NULL(item, );
	if (AddLast(static_cast<GenericIndexModelItem*>(item)) == -1)
	{
		OP_DELETE(item);
		return;
	}
	m_groups_map.Get(item->GetGroupingMethod())->Add(item);
}

INTPTR IndexModel::FindGroupForItem(GenericIndexModelItem* parent, OpVector<HeaderModelItem>* groups, GenericIndexModelItem* exclude_item, BOOL exclude_tree)
{
	int header_id = groups->GetCount();
	int last_child_id = parent->GetIndex() + parent->GetSubtreeSize();
	for(int i = parent->GetIndex(); i <= last_child_id; i++)
	{
		GenericIndexModelItem* item = GetItemByIndex(i);
		if (item == exclude_item)
		{
			i += exclude_tree ? item->GetSubtreeSize() : 0;
			continue;
		}
		INT32 h_id = FindGroup(item, groups);
		if (h_id != -1 && header_id > h_id)
		{
			header_id = h_id;
			if (header_id == 0)
				break;
		}
	}
	return header_id;
}

INTPTR IndexModel::FindGroup(GenericIndexModelItem* item, OpVector<HeaderModelItem>* groups)
{
	RETURN_VALUE_IF_NULL(groups, -1);
	for(int i = groups->GetCount() - 1; i > 0; i--)
	{
		if(groups->Get(i)->IsHeaderForItem(*item))
			return i;
	}
	return 0;
}

INT32 IndexModel::GetGroupForItem(const OpTreeModelItem* item) const
{
	if (GetGroupingMethod() == -1 || !item)
		return 0;

	const GenericIndexModelItem * gim_item = static_cast<const GenericIndexModelItem*>(item);

	if (gim_item && gim_item->IsHeader())
	{
		return -1;
	}

	while (gim_item->GetParentItem())
		gim_item = gim_item->GetParentItem();

	return gim_item->GetGroup(GetGroupingMethod());
}

BOOL IndexModel::IsVisibleHeader(const OpTreeModelItem* header) const
{
	const HeaderModelItem* item = static_cast<const HeaderModelItem*>(header);
	if(item && item->IsHeader() && item->GetGroupingMethod() == GetGroupingMethod())
		return TRUE;
	return FALSE;
}

void IndexModel::CacheGroupIds()
{
	for(UINT32 index = 0, count = GetCount(); index < count; index++)
	{
		GenericIndexModelItem * item = GetItemByIndex(index);
		CacheGroupIdsForItem(item);
		index += item->GetSubtreeSize();
	}
}

void IndexModel::CacheGroupIdsForItem(GenericIndexModelItem* item, BOOL exclude_item, BOOL exclude_item_subtree)
{
	if (!item || !m_groups_map.GetCount())
		return;
	GenericIndexModelItem* exclude = item;
	while (item->GetParentItem())
		item = item->GetParentItem();
	item->m_groups.RemoveAll();
	for (UINT32 i = 0, count = m_groups_map.GetCount(); i < count; i++)
	{
		OpVector<HeaderModelItem> *groups = m_groups_map.Get(i);
		item->m_groups.Add(i, reinterpret_cast<void*>(FindGroupForItem(item, groups, exclude_item ? exclude : NULL, exclude_item_subtree)));
	}
}

int IndexModel::GetTimeFormat(time_t date)
{
	if (m_grouping_method != IndexTypes::GROUP_BY_DATE)
		return TIME_FORMAT_DEFAULT;

	if (date >= m_begin_of_this_day)
		return TIME_FORMAT_ONLY_TIME;
	if (date >= m_begin_of_this_week)
		return TIME_FORMAT_WEEKDAY_AND_TIME;
	if (date >= m_begin_of_last_week)
		return TIME_FORMAT_WEEKDAY_AND_DATE;
	return TIME_FORMAT_UNIX_FORMAT;
}
#endif //M2_SUPPORT
