/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "simpletreemodel.h"
#include "adjunct/quick/managers/FavIconManager.h"

/***********************************************************************************
**
**	SimpleTreeModel
**
***********************************************************************************/

static const INT32 FIRST_AUTOMATIC_ID = 0x40000000;
static const INT32 LAST_AUTOMATIC_ID = 0x7fffffff;

/***********************************************************************************
*
*
*
***********************************************************************************/
SimpleTreeModel::SimpleTreeModel(INT32 column_count)
	: m_largest_used_id(FIRST_AUTOMATIC_ID-1) // start automatic ids at high numbers
#ifdef DEBUG_ENABLE_OPASSERT
	, m_last_seen_item_count(0)
#endif
{
	m_column_count = column_count;
	m_column_data  = OP_NEWA(ColumnData, column_count);
}

/***********************************************************************************
*
*
*
***********************************************************************************/
SimpleTreeModel::~SimpleTreeModel()
{
	/* If this assert triggers, an item has been added to the model
	 * outside the control of this class.  In particular, it means we
	 * have no control over what ids have been used.
	 */
	OP_ASSERT(m_last_seen_item_count == GetItemCount() || GetItemCount() == 0);
	OP_DELETEA(m_column_data);
}

/***********************************************************************************
*
*
*
***********************************************************************************/
void SimpleTreeModel::SetColumnData(INT32 column,
									const uni_char* string,
									const char* image,
									BOOL sort_by_string_first,
									BOOL matchable)
{
	if (!m_column_data)
		return;

	m_column_data[column].m_string.Set(string);
	m_column_data[column].m_image.Set(image);
	m_column_data[column].m_sort_by_string_first = sort_by_string_first;
	m_column_data[column].m_matchable = matchable;
}

/***********************************************************************************
*
*
*
***********************************************************************************/
INT32 SimpleTreeModel::AddItem(const uni_char* string,
							   const char* image,
							   INT32 sort_order,
							   INT32 parent,
							   void* user_data,
							   INT32 id,
							   OpTypedObject::Type type,
							   BOOL initially_checked)
{
	OP_ASSERT(id < FIRST_AUTOMATIC_ID);
	if (id == 0)
		id = MakeAutomaticID();

	SimpleTreeModelItem* item = OP_NEW(SimpleTreeModelItem, (this,
														user_data,
														id,
														type,
														initially_checked));
	if (!item)
		return -1;

	if (!item->m_item_data)
	{
		OP_DELETE(item);
		return -1;
	}

	item->SetItemData(0, string, image, sort_order);

	AboutToAddItem(item);
	return AddLast(item, parent);
}

/***********************************************************************************
*
*
*
***********************************************************************************/
INT32 SimpleTreeModel::AddSeparator(INT32 parent)
{
	SimpleTreeModelItem* item = OP_NEW(SimpleTreeModelItem, (this,
														0,
														MakeAutomaticID(),
														OpTypedObject::UNKNOWN_TYPE,
														FALSE,
														TRUE));
	if (!item)
		return -1;

	if (!item->m_item_data)
	{
		OP_DELETE(item);
		return -1;
	}

	AboutToAddItem(item);
	return AddLast(item, parent);
}

/***********************************************************************************
*
*
*
***********************************************************************************/
INT32 SimpleTreeModel::InsertBefore(INT32 sibling,
									const uni_char* string,
									const char* image,
									INT32 sort_order,
									void* user_data,
									INT32 id,
									OpTypedObject::Type type,
									BOOL initially_checked)
{
	OP_ASSERT(id < FIRST_AUTOMATIC_ID);
	if (id == 0)
		id = MakeAutomaticID();

	SimpleTreeModelItem* item = OP_NEW(SimpleTreeModelItem, (this,
														user_data,
														id,
														type,
														initially_checked));
	if (!item)
		return -1;

	if (!item->m_item_data)
	{
		OP_DELETE(item);
		return -1;
	}

	item->SetItemData(0, string, image, sort_order);

	AboutToAddItem(item);
	return GenericTreeModel::InsertBefore(item, sibling);
}

/***********************************************************************************
*
*
*
***********************************************************************************/
INT32 SimpleTreeModel::InsertAfter(INT32 sibling,
								   const uni_char* string,
								   const char* image,
								   INT32 sort_order,
								   void* user_data,
								   INT32 id,
								   OpTypedObject::Type type,
								   BOOL initially_checked)
{
	OP_ASSERT(id < FIRST_AUTOMATIC_ID);
	if (id == 0)
		id = MakeAutomaticID();

	SimpleTreeModelItem* item = OP_NEW(SimpleTreeModelItem, (this,
														user_data,
														id,
														type,
														initially_checked));
	if (!item)
		return -1;

	if (!item->m_item_data)
	{
		OP_DELETE(item);
		return -1;
	}

	item->SetItemData(0, string, image, sort_order);

	AboutToAddItem(item);
	return GenericTreeModel::InsertAfter(item, sibling);
}

/***********************************************************************************
*
*
*
***********************************************************************************/
INT32 SimpleTreeModel::InsertAfter(INT32 sibling)
{
	SimpleTreeModelItem* item = OP_NEW(SimpleTreeModelItem, (this,
														0,
														MakeAutomaticID(),
														OpTypedObject::UNKNOWN_TYPE));
	if (!item)
		return -1;

	if (!item->m_item_data)
	{
		OP_DELETE(item);
		return -1;
	}

	AboutToAddItem(item);
	return GenericTreeModel::InsertAfter(item, sibling);
}

/***********************************************************************************
*
*
*
***********************************************************************************/
INT32 SimpleTreeModel::InsertBefore(INT32 sibling)
{
	SimpleTreeModelItem* item = OP_NEW(SimpleTreeModelItem, (this,
														0,
														MakeAutomaticID(),
														OpTypedObject::UNKNOWN_TYPE));
	if (!item)
		return -1;

	if (!item->m_item_data)
	{
		OP_DELETE(item);
		return -1;
	}

	AboutToAddItem(item);
	return GenericTreeModel::InsertBefore(item, sibling);
}

INT32 SimpleTreeModel::MakeAutomaticID()
{
	if (GetItemCount() == 0)
	{
#ifdef DEBUG_ENABLE_OPASSERT
		m_last_seen_item_count = 0;
#endif
		m_largest_used_id = FIRST_AUTOMATIC_ID - 1;
	};
	/* If this assert triggers, an item has been added to the model
	 * outside the control of this class.  In particular, it means we
	 * have no control over what ids have been used.
	 */
	OP_ASSERT(m_last_seen_item_count == GetItemCount());

	if (m_largest_used_id >= LAST_AUTOMATIC_ID)
	{
		m_largest_used_id = FIRST_AUTOMATIC_ID - 1;
		INT32 itemcount = GetItemCount();
		for (INT32 i = 0; i < itemcount; i++)
		{
			OpTreeModelItem * item = GetItemByPosition(i);
			INT32 id = item->GetID();
			if (id > m_largest_used_id)
				m_largest_used_id = id;
		};
		/* If this assert triggers, I suspect the performance
		 * characteristics of this code is likely to degrade
		 * significantly.  In that case, it may be about time to
		 * consider a different strategy.
		 */
		OP_ASSERT((m_largest_used_id - FIRST_AUTOMATIC_ID) < (LAST_AUTOMATIC_ID - FIRST_AUTOMATIC_ID) / 2);
	};

	if (m_largest_used_id < LAST_AUTOMATIC_ID)
	{
		INT32 use_id = m_largest_used_id + 1;
		if (use_id >= FIRST_AUTOMATIC_ID)
		{
			m_largest_used_id = use_id;
			return use_id;
		};
	};

	OP_ASSERT(!"Performance degradation: generating automatic ids will now show n^2 performance");

	return MakeAutomaticIDBySearching();
}

INT32 SimpleTreeModel::MakeAutomaticIDBySearching()
{
	INT32 itemcount = GetItemCount();
	INT32 base = FIRST_AUTOMATIC_ID;
	const int BLOCK_COUNT = 16;
	UINT32 used[BLOCK_COUNT];

	while (base < LAST_AUTOMATIC_ID - BLOCK_COUNT * 32)
	{
		for (int i = 0; i < BLOCK_COUNT; i++)
			used[i] = 0;
		for (INT32 i = 0; i < itemcount; i++)
		{
			OpTreeModelItem * item = GetItemByPosition(i);

			INT32 id = item->GetID();
			OP_ASSERT(id != 0); // All items should have a valid ID
			if (id >= base)
			{
				INT32 offs = id - base;
				INT32 block = offs / 32;
				if (block < BLOCK_COUNT)
				{
					int bit = offs % 32;
					OP_ASSERT((used[block] & (1 << bit)) == 0);
					used[block] |= 1 << bit;
				};
			};
		}
		for (int block = 0; block < BLOCK_COUNT; block++)
		{
			if (used[block] != 0xffffffff)
			{
				for (int bit = 0; bit < 32; bit++)
					if ((used[block] & (1 << bit)) == 0)
						return base + block * 32 + bit;
			};
		};
		base += BLOCK_COUNT * 32;
	}
	OP_ASSERT(!"Failed to find unused item ID!");
	return 0;
}

/***********************************************************************************
*
*
*
***********************************************************************************/

#ifdef DEBUG_ENABLE_OPASSERT
void SimpleTreeModel::Delete(INT32 pos, BOOL all_children)
{
	/* If this assert triggers, an item has been added to the model
	 * outside the control of this class.  In particular, it means we
	 * have no control over what ids have been used.
	 */
	OP_ASSERT(m_last_seen_item_count == GetItemCount());
	m_last_seen_item_count -= 1;
	TreeModel<SimpleTreeModelItem>::Delete(pos, all_children);
};
#endif

/***********************************************************************************
*
*
*
***********************************************************************************/
void SimpleTreeModel::AboutToAddItem(SimpleTreeModelItem * item)
{
	if (GetItemCount() == 0)
	{
#ifdef DEBUG_ENABLE_OPASSERT
		m_last_seen_item_count = 0;
#endif
		m_largest_used_id = FIRST_AUTOMATIC_ID - 1;
	};
	/* If this assert triggers, an item has been added to the model
	 * outside the control of this class.  In particular, it means we
	 * have no control over what ids have been used.
	 */
	OP_ASSERT(m_last_seen_item_count == GetItemCount());
#ifdef DEBUG_ENABLE_OPASSERT
	m_last_seen_item_count += 1;
#endif
	INT32 id = item->GetID();
	if (id > m_largest_used_id)
		m_largest_used_id = id;
	return;
};

/***********************************************************************************
*
*
*
***********************************************************************************/
void SimpleTreeModel::SetItemData(INT32 pos,
								  INT32 column,
								  const uni_char* string,
								  const char* image,
								  INT32 sort_order)
{
	GetItemByIndex(pos)->SetItemData(column, string, image, sort_order);
}

/***********************************************************************************
*
*
*
***********************************************************************************/
void SimpleTreeModel::SetItemColor(INT32 pos,
								   INT32 column,
								   INT32 color)
{
	GetItemByIndex(pos)->SetItemColor(column, color);
}

/***********************************************************************************
*
*
*
***********************************************************************************/
OP_STATUS SimpleTreeModel::GetColumnData(OpTreeModel::ColumnData* column_data)
{
	if (!m_column_data)
		return OpStatus::ERR_NO_MEMORY;

	column_data->image                = m_column_data[column_data->column].m_image.CStr();
	column_data->sort_by_string_first = m_column_data[column_data->column].m_sort_by_string_first;
	column_data->matchable            = m_column_data[column_data->column].m_matchable;

	RETURN_IF_ERROR(column_data->text.Set(m_column_data[column_data->column].m_string.CStr()));

	return OpStatus::OK;
}

/***********************************************************************************
*
*
*
***********************************************************************************/
void* SimpleTreeModel::GetItemUserData(INT32 pos)
{
	SimpleTreeModelItem* item = GetItemByIndex(pos);

	return item ? item->m_user_data : NULL;
}

/***********************************************************************************
*
*
*
***********************************************************************************/
void* SimpleTreeModel::GetItemUserDataByID(INT32 id)
{
	SimpleTreeModelItem* item = GetItemByID(id);

	return item ? item->m_user_data : NULL;
}

/***********************************************************************************
*
*
*
***********************************************************************************/
INT32 SimpleTreeModel::FindItemUserData(void* user_data)
{
	for (INT32 i = 0; i < GetItemCount(); i++)
	{
		if (GetItemUserData(i) == user_data)
			return i;
	}

	return -1;
}

/***********************************************************************************
*
*
*
***********************************************************************************/
const uni_char*	SimpleTreeModel::GetItemString(INT32 pos, INT32 column)
{
	SimpleTreeModelItem* item = GetItemByIndex(pos);

	return item ? item->m_item_data[column].m_string.CStr() : NULL;
}

/***********************************************************************************
*
*
*
***********************************************************************************/
const uni_char*	SimpleTreeModel::GetItemStringByID(INT32 id, INT32 column)
{
	SimpleTreeModelItem* item = GetItemByID(id);

	return item ? item->m_item_data[column].m_string.CStr() : NULL;
}

/***********************************************************************************
**
**	SimpleTreeModelItem
**
***********************************************************************************/

/***********************************************************************************
*
*
*
***********************************************************************************/
SimpleTreeModelItem::SimpleTreeModelItem(OpTreeModel* tree_model,
										 void* user_data,
										 INT32 id,
										 Type type,
										 BOOL initially_checked,
										 BOOL separator)
{
	/* All items in a model SHOULD have unique, non-0 IDs.  It is
	 * likely that there is code that assumes this.
	 */
	OP_ASSERT(id != 0);
	m_tree_model         = tree_model;
	m_user_data          = user_data;
	m_id                 = id;
	m_type               = type;
	m_initially_checked  = initially_checked;
	m_item_data          = OP_NEWA(ItemColumnData, m_tree_model->GetColumnCount());
	m_item_data->m_flags = 0;
	m_item_data->m_flags = initially_checked ? FLAG_INITIALLY_CHECKED | FLAG_CHECKED : 0;
	m_separator          = separator;
	m_initially_disabled = FALSE;
	m_initially_open     = FALSE;
	m_is_text_separator  = FALSE;
	m_is_important       = FALSE;
	m_has_formatted_text = FALSE;
	m_clean_on_remove    = FALSE;
	m_associated_text    = NULL;
}

/***********************************************************************************
*
*
*
***********************************************************************************/
SimpleTreeModelItem::~SimpleTreeModelItem()
{
	OP_DELETEA(m_item_data);
	OP_DELETE(m_associated_text);
}

/***********************************************************************************
*
*
*
***********************************************************************************/
OP_STATUS SimpleTreeModelItem::SetItemData(INT32 column,
										   const uni_char* string,
										   const char* image,
										   INT32 sort_order)
{
	OP_ASSERT(column < m_tree_model->GetColumnCount());
	RETURN_IF_ERROR(m_item_data[column].m_string.Set(string));
	RETURN_IF_ERROR(m_item_data[column].m_image.Set(image));
	m_item_data[column].m_sort_order = sort_order;

	Change(TRUE);
	return OpStatus::OK;
}


/***********************************************************************************
*
*
*
***********************************************************************************/
Image SimpleTreeModelItem::GetFavIcon()
{
	return g_favicon_manager->Get(m_favicon_key.CStr());
}


/***********************************************************************************
*
*
*
***********************************************************************************/
OP_STATUS SimpleTreeModelItem::SetAssociatedText(const OpStringC & associated_text)
{
	if(associated_text.IsEmpty())
		return OpStatus::ERR;

	if(!m_associated_text)
	{
		m_associated_text = OP_NEW(OpString, ());

		if(!m_associated_text)
			return OpStatus::ERR_NO_MEMORY;
	}

	return m_associated_text->Set(associated_text.CStr());
}

/***********************************************************************************
*
*
*
***********************************************************************************/
void SimpleTreeModelItem::OnRemoved()
{
	if(m_clean_on_remove)
	{
		if(m_associated_text)
			m_associated_text->Empty();
		SetHasFormattedText(FALSE);
	}
}

/***********************************************************************************
*
*
*
***********************************************************************************/
OP_STATUS SimpleTreeModelItem::GetItemData(ItemData* item_data)
{
	if (item_data->query_type == INIT_QUERY)
	{
		if (m_initially_checked)
			item_data->flags |= FLAG_INITIALLY_CHECKED;

		if (m_separator)
			item_data->flags |= FLAG_SEPARATOR | FLAG_INITIALLY_DISABLED;

		if (m_initially_disabled)
			item_data->flags |= FLAG_INITIALLY_DISABLED;

		if (m_initially_open)
			item_data->flags |= FLAG_INITIALLY_OPEN;

		if (m_is_text_separator)
			item_data->flags |= FLAG_TEXT_SEPARATOR;

		if (m_has_formatted_text)
			item_data->flags |= FLAG_FORMATTED_TEXT;

		return OpStatus::OK;
	}

	if (item_data->query_type == ASSOCIATED_TEXT_QUERY)
	{
		if(m_associated_text)
			return item_data->associated_text_query_data.text->Set(m_associated_text->CStr());
		return OpStatus::OK;
	}

	if (item_data->query_type == MATCH_QUERY)
	{
		if (item_data->match_query_data.match_type == MATCH_IMPORTANT && !m_is_important)
			return OpStatus::OK;

		if (item_data->match_query_data.match_type == MATCH_ALL || item_data->match_query_data.match_text->IsEmpty())
		{
			item_data->flags |= FLAG_MATCHED;
			return OpStatus::OK;
		}

		for (INT32 i = 0; i < m_tree_model->GetColumnCount(); i++)
		{
			OpTreeModel::ColumnData column_data(i);
			m_tree_model->GetColumnData(&column_data);
			if (column_data.matchable && m_item_data[i].m_string.FindI(*item_data->match_query_data.match_text) != KNotFound)
			{
				item_data->flags |= FLAG_MATCHED;
				return OpStatus::OK;
			}
		}

		return OpStatus::OK;
	}

	if (item_data->query_type != COLUMN_QUERY)
	{
		return OpStatus::OK;
	}

	item_data->column_query_data.column_text->Set(m_item_data[item_data->column_query_data.column].m_string.CStr());

	item_data->column_query_data.column_sort_order = m_item_data[item_data->column_query_data.column].m_sort_order;

	// Get visual cues if we're painting
	if (!(item_data->flags & FLAG_NO_PAINT))
	{
		item_data->column_query_data.column_image      = m_item_data[item_data->column_query_data.column].m_image.CStr();
		item_data->column_query_data.column_text_color = m_item_data[item_data->column_query_data.column].m_color;

		if(item_data->column_query_data.column == 0 && m_favicon_key.HasContent())
			item_data->column_bitmap = GetFavIcon();
	}

	// Formatting can be changed in an exsiting item. Typically when changing text
	if (m_has_formatted_text)
		item_data->flags |= FLAG_FORMATTED_TEXT;
	else
		item_data->flags &= ~FLAG_FORMATTED_TEXT;

	m_item_data[item_data->column_query_data.column].m_flags = item_data->flags;

	return OpStatus::OK;
}
