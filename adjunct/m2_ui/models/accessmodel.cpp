/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "accessmodel.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/quick_toolkit/widgets/OpUnreadBadge.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	AccessModelItem
**
***********************************************************************************/

OP_STATUS AccessModelItem::GetItemData(ItemData* item_data)
{
	// used for not prefetching indexes just for sorting text
	BOOL is_sorting = m_is_sorting;
	m_is_sorting = FALSE;

	// Can possibly be optimized a bit if needed:
	MessageEngine *engine = MessageEngine::GetInstance();
	Indexer *indexer = engine->GetIndexer();
	OpString languagestring;

	if (!indexer)
	{
		return OpStatus::ERR;
	}

	if (item_data->query_type == INIT_QUERY)
	{

		Index *index = indexer->GetIndexById(m_id);
		if (index)
		{
			if (index->GetExpanded())
				item_data->flags |= FLAG_INITIALLY_OPEN;

			if (index->IsReadOnly())
				item_data->flags |= FLAG_INITIALLY_DISABLED;
		}

		return OpStatus::OK;
	}

	if (item_data->query_type == MATCH_QUERY)
	{
		Index *index = IsCategory(m_id) ? NULL : indexer->GetIndexById(m_id);

		if (index)
		{
			BOOL match = TRUE;

			if (match && item_data->match_query_data.match_type == MATCH_FOLDERS)
			{
				match = (index->GetId() >= IndexTypes::FIRST_FOLDER && index->GetId() < IndexTypes::LAST_FOLDER) || index->GetId() == IndexTypes::SPAM;
			}

			if (match && item_data->match_query_data.match_text->HasContent())
			{
				OpString name;
				index->GetName(name);

				match = name.HasContent() && uni_stristr(name.CStr(), item_data->match_query_data.match_text->CStr());
			}

			if (match)
			{
				item_data->flags |= FLAG_MATCHED;
			}
		}

		return OpStatus::OK;
	}

	if (item_data->query_type == INFO_QUERY)
	{
		OpString desc, format;
		RETURN_IF_ERROR(GetItemDescription(desc));
		RETURN_IF_ERROR(item_data->info_query_data.info_text->SetStatusText(desc.CStr()));
		RETURN_IF_ERROR(desc.Append(" "));
		
		Index *m2index = indexer->GetIndexById(m_id);

		if (m_id == IndexTypes::UNREAD_UI)
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_UNREAD_MAIL_HEADER, format));
			RETURN_IF_ERROR(desc.AppendFormat(format.CStr(), m2index->UnreadCount()));
		}
		else if (m_id == IndexTypes::TRASH || m2index->GetSpecialUseType() == AccountTypes::FOLDER_TRASH)
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_UNREAD_AND_TOTAL_MAIL_HEADER, format));
			RETURN_IF_ERROR(desc.AppendFormat(format.CStr(), m2index->UnreadCount(), m2index->MessageCount()));
		}
		else
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_UNREAD_AND_TOTAL_MAIL_HEADER, format));
			RETURN_IF_ERROR(desc.AppendFormat(format.CStr(), m2index->UnreadCount(), m2index->MessageCount() - m2index->CommonCount(IndexTypes::TRASH) ));
		}

		RETURN_IF_ERROR(item_data->info_query_data.info_text->AddTooltipText(NULL, desc.CStr()));

		return OpStatus::OK;
	}

	if (item_data->query_type != COLUMN_QUERY && item_data->query_type != PREPAINT_QUERY)
	{
		return OpStatus::OK;
	}

	if (item_data->query_type == COLUMN_QUERY && item_data->column_query_data.column == 0)
	{
		switch (m_id)
		{
		case IndexTypes::UNREAD_UI:
			item_data->column_query_data.column_sort_order = 0;
			break;
		case IndexTypes::NEW_UI:
			item_data->column_query_data.column_sort_order = 1;
			break;
		case IndexTypes::RECEIVED:
			item_data->column_query_data.column_sort_order = 2;
			break;
		case IndexTypes::PIN_BOARD:
			item_data->column_query_data.column_sort_order = 3;
			break;
		case IndexTypes::OUTBOX:
			item_data->column_query_data.column_sort_order = 4;
			break;
		case IndexTypes::SENT:
			item_data->column_query_data.column_sort_order = 5;
			break;
		case IndexTypes::DRAFTS:
			item_data->column_query_data.column_sort_order = 6;
			break;
		case IndexTypes::SPAM:
			item_data->column_query_data.column_sort_order = 7;
			break;
		case IndexTypes::TRASH:
			item_data->column_query_data.column_sort_order = 8;
			break;
		case IndexTypes::UNREAD:
		case IndexTypes::CLIPBOARD:
		case IndexTypes::DOC_ATTACHMENT:
		case IndexTypes::IMAGE_ATTACHMENT:
		case IndexTypes::AUDIO_ATTACHMENT:
		case IndexTypes::VIDEO_ATTACHMENT:
		case IndexTypes::ZIP_ATTACHMENT:
			item_data->column_query_data.column_sort_order = m_id;
			break;

		default:
			if (m_id >= IndexTypes::FIRST_UNIONGROUP && m_id < IndexTypes::LAST_UNIONGROUP)
			{
				item_data->column_query_data.column_sort_order = -1;
			}
		}
	}
	Index *index = IsCategory(m_id) ? NULL : indexer->GetIndexById(m_id);

	if (!index)
	{
		if (item_data->query_type == PREPAINT_QUERY)
		{
			return OpStatus::OK;
		}
		item_data->column_query_data.column_text->Empty();
	}
	else
	{
		if (!is_sorting)
		{
			OpStatus::Ignore(index->DelayedPreFetch()); // make sure unread count is OK.
		}

		if (index->IsReadOnly())
			item_data->flags |= FLAG_INITIALLY_DISABLED | FLAG_DISABLED;

		// initial unread count can change model, so it must be done during PREPAINT_QUERY
		UINT32 unread_count = index->UnreadCount();

		if (item_data->query_type == PREPAINT_QUERY)
		{
			return OpStatus::OK;
		}

		UINT32 total_count = m_id == IndexTypes::TRASH ? index->MessageCount() : index->MessageCount() ? index->MessageCount() - index->CommonCount(IndexTypes::TRASH) : 0;

		OpString unread_count_string;
		unread_count_string.AppendFormat(UNI_L("%d"), unread_count);
		OpString unread_count_string_full;
		unread_count_string_full.AppendFormat(UNI_L(" (%d)"), unread_count);
		OpString total_count_string;
		total_count_string.AppendFormat(UNI_L("%d"), total_count);
		OpString total_count_string_full;
		total_count_string_full.AppendFormat(UNI_L(" (%d)"), total_count);

		if (unread_count > 0)
		{
			item_data->flags |= FLAG_BOLD;
		}

		switch (item_data->column_query_data.column)
		{
			case 0:		// case 0 is name
			case 5:		// case 5 is name + (unread count)
			{
				if (m_id < IndexTypes::FIRST_CONTACT || m_id >IndexTypes::LAST_CONTACT)
				{
					index->GetImages(item_data->column_query_data.column_image, item_data->column_bitmap);
				}

				RETURN_IF_ERROR(GetItemDescription(*item_data->column_query_data.column_text));
				if (item_data->column_query_data.column == 5 && unread_count > 0)
					item_data->column_query_data.column_text->Append(unread_count_string_full.CStr());

				break;
			}
			case 1:		// case 1 is unread count or total count for drafts, outbox and trash
			{
				OpString *label_text = NULL;
				if (unread_count && m_id != IndexTypes::OUTBOX && m_id != IndexTypes::DRAFTS && m_id != IndexTypes::TRASH)
				{
					label_text = &unread_count_string;
				}
				else if ((m_id == IndexTypes::OUTBOX || m_id == IndexTypes::DRAFTS || m_id == IndexTypes::TRASH) && total_count)
				{
					label_text = &total_count_string;
				}
				if (label_text)
				{
					OpUnreadBadge* label;
					if (item_data->column_query_data.custom_cell_widget == NULL)
					{
						RETURN_IF_ERROR(OpUnreadBadge::Construct(&label));
						item_data->column_query_data.custom_cell_widget = label;
						label->SetIgnoresMouse(TRUE);
						if (unread_count)
							label->GetBorderSkin()->SetImage("Mail Treeview Unread Badge");
						else
							label->GetBorderSkin()->SetImage("Mail Treeview Read Badge");
					}
					else
					{
						label = static_cast<OpUnreadBadge*>(item_data->column_query_data.custom_cell_widget);
					}

					item_data->flags |= FLAG_CUSTOM_CELL_WIDGET;
					item_data->flags |= FLAG_RIGHT_ALIGNED;
					RETURN_IF_ERROR(label->SetText(label_text->CStr()));
				}

				item_data->column_query_data.column_sort_order = unread_count;
				break;
			}

			case 2:		//total
				if (total_count)
					RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(total_count_string.CStr()));
				item_data->column_query_data.column_sort_order = total_count;
				item_data->flags |= FLAG_RIGHT_ALIGNED;
				break;

			case 3:		//size
			{
				UINT64 size = 0;

				for (INT32SetIterator it(index->GetIterator()); it; it++)
				{
					size += engine->GetStore()->GetMessageSize(it.GetData());
				}

				if (size)
				{
					item_data->column_query_data.column_sort_order = size;

					OpString bytestr;

					RETURN_IF_ERROR(FormatByteSize(bytestr, size));
					RETURN_IF_ERROR(item_data->column_query_data.column_text->Append(bytestr));
				}
				item_data->flags |= FLAG_RIGHT_ALIGNED;

				break;
			}

			case 4:		//info
			{
				if (index->GetKeyword())
					RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(index->GetKeyword()));
				else if (index->GetSearchCount() > 0)
				RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(index->GetSearch(0)->GetSearchText()));

				break;
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS AccessModelItem::GetItemDescription(OpString& description)
{
	description.Empty();

	Index *index = MessageEngine::GetInstance()->GetIndexById(m_id);

	if (!index)
		return OpStatus::OK;

	// Default name: name of index
	RETURN_IF_ERROR(index->GetName(description));

	// Special cases
	if (m_id >= IndexTypes::FIRST_CONTACT && m_id < IndexTypes::LAST_CONTACT)
	{
		BrowserUtils* browser_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
		OpString full_name;

		IndexSearch* search = index->GetSearch();
		if (search)
		{
			browser_utils->GetContactName(search->GetSearchText(), full_name);
			if (full_name.Compare(search->GetSearchText()) != 0)
				RETURN_IF_ERROR(description.Set(full_name));
		}
	}
	else if (m_id >= IndexTypes::FIRST_SEARCH && m_id < IndexTypes::LAST_SEARCH)
	{
		if (index->GetSearch()->GetCurrentId() > 0)
		{
			OpString search_for;
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_SEARCHING_FOR, search_for));
			RETURN_IF_ERROR(search_for.Append(" "));
			RETURN_IF_ERROR(description.Insert(0, search_for));
			RETURN_IF_ERROR(description.Append("..."));
		}
	}

	return OpStatus::OK;
}

BOOL AccessModelItem::IsCategory(index_gid_t index_id)
{
	return IndexTypes::FIRST_CATEGORY <= index_id && index_id < IndexTypes::LAST_CATEGORY;
}

/***********************************************************************************
**
**	AccessModel
**
***********************************************************************************/
AccessModel::AccessModel(UINT32 category_id, Indexer* indexer)
	: m_category_id(category_id)
	, m_show_hidden_indexes(FALSE)
	, m_indexer(indexer)
{
}

AccessModel::~AccessModel()
{
	if (MessageEngine::GetInstance())
	{
		MessageEngine::GetInstance()->GetMasterProgress().RemoveListener(this);
		MessageEngine::GetInstance()->RemoveEngineListener(this);
		m_indexer->RemoveIndexerListener(this);
	}
}

void AccessModel::ReInit()
{
	ModelLock lock(this);

	DeleteAll();

	OpINT32Vector vector;
	if (OpStatus::IsSuccess(m_indexer->GetChildren(m_category_id, vector)))
	{
		for (UINT32 id = 0; id < vector.GetCount(); id ++)
		{
			if (!GetItemByID(vector.Get(id)))
			{
				AddFolderItem(m_indexer->GetIndexById(vector.Get(id)));
			}
			else
				OP_ASSERT(!"why does this happen");
		}
	}
	m_indexer->UpdateCategoryUnread(m_category_id);
}

void AccessModel::AddFolderItem(Index* index)
{
	if (!index || (!m_show_hidden_indexes && (!index->IsVisible() || IsHiddenAccount(index)))) // doesn't exist or is hidden
		return;

	UINT32 index_id = index->GetId();
	if (GetItemByID(index_id))	// already there
		return;

	// we have to add the parent index before adding this index
	UINT32 parent_id = m_indexer->GetIndexById(index_id)->GetParentId();
	AccessModelItem* item = OP_NEW(AccessModelItem, (index_id));
	if (!item)
		return;

	if (parent_id && parent_id != m_category_id)
	{
		AccessModelItem* parent = NULL;
		AddFolderItem(m_indexer->GetIndexById(parent_id));
		parent = GetItemByID(parent_id);
		if (parent)
			parent->AddChildLast(item);
		else
			AddLast(item);
	}
	else
	{
		AddLast(item);
	}
}

OP_STATUS AccessModel::Init()
{
	m_indexer->AddIndexerListener(this);

	MessageEngine::GetInstance()->AddEngineListener(this);

	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetMasterProgress().AddListener(this));

	ReInit();

	return OpStatus::OK;
}

INT32 AccessModel::GetColumnCount()
{
	return 6;
}

OP_STATUS AccessModel::GetColumnData(ColumnData* column_data)
{
	Str::LocaleString strings[] ={
		Str::LocaleString(Str::SI_IDSTR_CL_NAME_COL),
		Str::LocaleString(Str::DI_IDSTR_M2_FOLDER_IDX_UNREAD),
		Str::LocaleString(Str::S_PROGRESS_TYPE_TOTAL),
		Str::LocaleString(Str::SI_DIRECTORY_COLOUMN_SIZE),
		Str::LocaleString(Str::M_VIEW_HOTLIST_MENU_INFO)};

	g_languageManager->GetString(strings[column_data->column], column_data->text);

	return OpStatus::OK;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS AccessModel::GetTypeString(OpString& type_string)
{
	return g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_MAIL, type_string);
}
#endif

/***********************************************************************************
**
**	IndexAdded
**
***********************************************************************************/

OP_STATUS AccessModel::IndexAdded(Indexer *indexer, UINT32 index_id)
{
	Index *index = indexer->GetIndexById(index_id);
	if (!index)
	{
		return OpStatus::ERR;
	}

	if ((!index->IsVisible() || IsHiddenAccount(index)) && !m_show_hidden_indexes)
	{
		return OpStatus::OK;
	}

	if (GetItemByID(index_id) || !BelongsToThisModel(index))
	{
		return OpStatus::OK;
	}

	AccessModelItem* item = OP_NEW(AccessModelItem, (index_id));
	if (!item)
		return OpStatus::ERR_NO_MEMORY;
	AccessModelItem* parent = GetItemByID(index->GetParentId());
	if (parent)
		parent->AddChildLast(item);
	else
	{
		AddLast(item);
	}
	m_indexer->UpdateCategoryUnread(m_category_id);
	return OpStatus::OK;
}

/***********************************************************************************
**
**	IndexVisibleChanged
**
***********************************************************************************/

OP_STATUS AccessModel::IndexVisibleChanged(Indexer *indexer, UINT32 index_id)
{
	Index* index = m_indexer->GetIndexById(index_id);
	if (index && BelongsToThisModel(index))
	{
		if (index->IsVisible())
		{
			RETURN_IF_ERROR(IndexAdded(indexer, index_id));
			OpINT32Vector children;
			RETURN_IF_ERROR(g_m2_engine->GetIndexer()->GetChildren(index_id, children));
			for (UINT32 i = 0; i < children.GetCount(); i++)
			{
				RETURN_IF_ERROR(IndexAdded(indexer, children.Get(i)));			
			}
			return OpStatus::OK;
		}
		else
		{
			return IndexRemoved(indexer, index_id);
		}
	}
	return OpStatus::OK;
}

OP_STATUS AccessModel::IndexParentIdChanged(Indexer *indexer, UINT32 index_id,  UINT32 old_parent_id, UINT32 new_parent_id) 
{ 
	if (GetItemByID(index_id) || GetItemByID(new_parent_id) || new_parent_id == m_category_id)
	{
		RETURN_IF_ERROR(IndexRemoved(indexer, index_id));
		RETURN_IF_ERROR(IndexAdded(indexer, index_id));

		OpINT32Vector children;
		RETURN_IF_ERROR(indexer->GetChildren(index_id, children));
		for (UINT32 i = 0 ; i < children.GetCount(); i++)
		{
			Index* index = indexer->GetIndexById(children.Get(i));
			if (index)
				RETURN_IF_ERROR(IndexParentIdChanged(indexer, children.Get(i), 0, index->GetParentId()));
		}
	}
	return OpStatus::OK; 
}

/***********************************************************************************
**
**	IndexRemoved
**
***********************************************************************************/

OP_STATUS AccessModel::IndexRemoved(Indexer *indexer, UINT32 index_number)
{
	AccessModelItem *item = GetItemByID(index_number);

	if (item)
	{
		item->Delete();
		m_indexer->UpdateCategoryUnread(m_category_id);
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	IndexNameChanged
**
***********************************************************************************/

OP_STATUS AccessModel::IndexNameChanged(Indexer *indexer, UINT32 index_id)
{
	if (index_id)
	{
		AccessModelItem *item = GetItemByID(index_id);

		if (item)
			item->Change(TRUE);
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	OnIndexChanged
**
***********************************************************************************/
void AccessModel::OnIndexChanged(UINT32 index_number)
{
	if (index_number == UINT_MAX)
	{
		BeginChange();
		ChangeAll();
		EndChange();
	}
	else
	{
		AccessModelItem *item = GetItemByID(index_number);

		if (item)
		{
			item->Change();
		}
		else
		{
			// we need to add, the item probably used to be invisible
			IndexAdded(m_indexer, index_number);
		}
	}
	m_indexer->UpdateCategoryUnread(m_category_id);
}

/***********************************************************************************
**
**	OnActiveAccountChanged
**
***********************************************************************************/

void AccessModel::OnActiveAccountChanged()
{
	ReInit();
}

void AccessModel::OnStatusChanged(const ProgressInfo& progress)
{
	// for each top-level item i, change
	AccessModelItem* item = GetItemByIndex(0);

	while (item)
	{
		if (item->GetID() >= IndexTypes::FIRST_ACCOUNT && item->GetID() < IndexTypes::LAST_ACCOUNT)
			item->Change();
		item = item->GetSiblingItem();
	}
}

/***********************************************************************************
**
**	IsHiddenAccount
**
***********************************************************************************/

BOOL AccessModel::IsHiddenAccount(Index* index)
{
	UINT16 account_id = (UINT16) index->GetAccountId();
	if (account_id != 0)
	{
		return !MessageEngine::GetInstance()->GetAccountManager()->IsAccountActive(account_id);
	}
	return FALSE;
}

/***********************************************************************************
**
**	BelongsToThisModel
**
***********************************************************************************/

BOOL AccessModel::BelongsToThisModel(Index* index)
{
	UINT32 parent_id = index->GetParentId();

	while (parent_id)
	{
		if (GetItemByID(parent_id) || parent_id == m_category_id)
			return TRUE;

		Index * parent_index = m_indexer->GetIndexById(parent_id);
		if (parent_index && parent_index->GetParentId() != 0)
		{
			parent_id = parent_index->GetParentId();
		}
		else
		{
			parent_id = 0;
		}
	}
	return FALSE;
}

#endif //M2_SUPPORT
