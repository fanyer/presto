/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

# include "modules/hardcore/base/op_types.h"
# include "modules/hardcore/base/opstatus.h"
# include "modules/debug/debug.h"
# include "adjunct/m2/src/import/ImporterModel.h"
# include "adjunct/m2/src/engine/engine.h"
# include "modules/locale/locale-enum.h"
# include "modules/locale/oplanguagemanager.h"
# include "modules/prefs/prefsmanager/prefstypes.h"


///////////////////////////////////////////////////////////////////////////////////
//	ImporterModelItem class
///////////////////////////////////////////////////////////////////////////////////

ImporterModelItem::ImporterModelItem()
: m_type(IMPORT_ACCOUNT_TYPE)
{

}


ImporterModelItem::ImporterModelItem(const Type type,
									 const OpStringC& name,
									 const OpStringC& path,
									 const OpStringC& virtualPath,
									 const OpStringC& info)
{
	OP_NEW_DBG("ImporterModelItem::ImporterModelItem", "m2");

	m_type = type;
	m_name.Set(name);
	m_path.Set(path);
	m_virtualPath.Set(virtualPath);
	m_info.Set(info);
	OP_DBG(("ImporterModelItem constructed:\n m_type=%i\n m_name=%S\n m_path=%S\n m_virtualPath=%S\n m_info=%S\n",
			  (int)m_type, m_name.CStr(), m_path.CStr(), m_virtualPath.CStr(), m_info.CStr()));
}


ImporterModelItem::ImporterModelItem(const Type type,
									 const char* name,
									 const char* path,
									 const char* virtualPath,
									 const char* info)
{
	OP_NEW_DBG("ImporterModelItem::ImporterModelItem", "m2");
	m_type = type;
	m_name.Set(name);
	m_path.Set(path);
	m_virtualPath.Set(virtualPath);
	m_info.Set(info);
	OP_DBG(("ImporterModelItem constructed:\n m_type=%i\n m_name=%S\n m_path=%S\n m_virtualPath=%S\n m_info=%S\n",
			  (int)m_type, m_name.CStr(), m_path.CStr(), m_virtualPath.CStr(), m_info.CStr()));
}


ImporterModelItem::ImporterModelItem(const ImporterModelItem& val)
    : TreeModelItem<ImporterModelItem, ImporterModel>()
	, OpQueueItem<ImporterModelItem>()
    , m_type(val.m_type)
{
	m_name.Set(val.m_name);
	m_path.Set(val.m_path);
	m_virtualPath.Set(val.m_virtualPath);
	m_info.Set(val.m_info);
}


ImporterModelItem& ImporterModelItem::operator=(const ImporterModelItem& val)
{
	if (this == &val)
		return *this;

	m_type = val.m_type;
	m_name.Set(val.m_name);
	m_path.Set(val.m_path);
	m_virtualPath.Set(val.m_virtualPath);
	m_info.Set(val.m_info);

	return *this;
}


OP_STATUS ImporterModelItem::GetItemData(ItemData* item_data)
{
	if (COLUMN_QUERY == item_data->query_type)
	{
		item_data->column_query_data.column_text->Set(m_name);
		switch (m_type)
		{
		case IMPORT_ACCOUNT_TYPE:
			item_data->column_query_data.column_image = "Folder";
			break;

		case IMPORT_FOLDER_TYPE:
			item_data->column_query_data.column_image = "Folder";
			break;

		case IMPORT_MAILBOX_TYPE:
			item_data->column_query_data.column_image = "Mail Inbox";
			break;

		default:
			break;
		}
	}
	return OpStatus::OK;
}


///////////////////////////////////////////////////////////////////////////////////
//	ImporterModel class
///////////////////////////////////////////////////////////////////////////////////

ImporterModel::ImporterModel()
{
}


ImporterModel::~ImporterModel()
{
}


OP_STATUS ImporterModel::Init()
{
	return OpStatus::OK;
}


INT32 ImporterModel::GetColumnCount()
{
	return 1;
}


OP_STATUS ImporterModel::GetColumnData(ColumnData* column_data)
{
	return g_languageManager->GetString(Str::S_IMPORT_ITEM, column_data->text);
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS ImporterModel::GetTypeString(OpString& type_string)
{
	return g_languageManager->GetString(Str::S_IMPORT_ITEM, type_string);
}
#endif


INT32 ImporterModel::GetRootBranch(ImporterModelItem* item)
{
	if (NULL == item)
		return -1;

	INT32 index = GetIndexByItem(item);
	if (-1 == index)
		return -1;	// item not found

	INT32 branch = -1;
	INT32 root = 0;

	while (root != -1)
	{
		branch = index;
		root = GetItemParent(index);
		index = root;
	}

	return branch;
}


ImporterModelItem* ImporterModel::GetRootItem(ImporterModelItem* item)
{
	ImporterModelItem* root = NULL;
	INT32 rootBranch = GetRootBranch(item);
	if (rootBranch != -1)
	{
		root = GetItemByIndex(rootBranch);
	}
	return root;
}

void ImporterModel::EmptySequence(OpQueue<ImporterModelItem>* sequence)
{
	if (!sequence->IsEmpty())
	{
		while (sequence->Dequeue() != NULL)
		{
		}
	}
}


OP_STATUS ImporterModel::MakeSequence(OpQueue<ImporterModelItem>* sequence, ImporterModelItem* parent_item, BOOL add /* = FALSE */)
{
	OP_STATUS ret = OpStatus::OK;

	if (!add)
		EmptySequence(sequence);

	INT32 parent_pos = GetIndexByItem(parent_item);
	if (-1 == parent_pos) return OpStatus::ERR;

	switch (parent_item->GetType())
	{
	case OpTypedObject::IMPORT_IDENTITY_TYPE:
	case OpTypedObject::IMPORT_ACCOUNT_TYPE:
		FillSequence(sequence, parent_pos);
		break;

	case OpTypedObject::IMPORT_FOLDER_TYPE:
		ret = PushItem(sequence, parent_item);	// add this folder
		FillSequence(sequence, parent_pos);
		break;

	case OpTypedObject::IMPORT_MAILBOX_TYPE:
		ret = PushItem(sequence, parent_item);	// add only this mailbox
		break;

	default:
		break;
	}
	return ret;
}

OP_STATUS ImporterModel::MakeRootSequence(OpQueue<ImporterModelItem>* sequence)
{
	for (int i = 0; i < GetCount(); i++)
	{
		RETURN_IF_ERROR(PushItem(sequence, GetItemByIndex(i)));
	}
	return OpStatus::OK;
}

OP_STATUS ImporterModel::FillSequence(OpQueue<ImporterModelItem>* sequence, INT32 parent)
{
	OP_STATUS ret = OpStatus::OK;

	INT32 sibling = GetChildIndex(parent);

	while (sibling != -1)
	{
		FillSequence(sequence, sibling);

		ImporterModelItem* item = GetItemByIndex(sibling);
		if (item != NULL)
		{
			ret = PushItem(sequence, item);
		}

		sibling = GetSiblingIndex(sibling);
	}
	return ret;
}

#endif //M2_SUPPORT
