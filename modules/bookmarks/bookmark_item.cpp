/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CORE_BOOKMARKS_SUPPORT

#ifdef SUPPORT_DATA_SYNC
#include "modules/bookmarks/bookmark_sync.h"
#endif // SUPPORT_DATA_SYNC
#include "modules/bookmarks/bookmark_item.h"
#include "modules/url/url_api.h"
#include "modules/url/url2.h"

BookmarkAttribute::BookmarkAttribute() :
	m_text_value(NULL),
	m_int_value(0),
	m_max_len(0)
{
	packed.m_attr_type = BOOKMARK_NONE;
	packed.m_owns_text_val = 1;
	packed.m_max_len_action = ACTION_FAIL;
#ifdef SUPPORT_DATA_SYNC
	packed.m_sync = 0;
#endif // SUPPORT_DATA_SYNC
}

BookmarkAttribute::~BookmarkAttribute()
{
	if (packed.m_owns_text_val)
		OP_DELETEA(m_text_value);
}

void BookmarkAttribute::SetAttributeType(BookmarkAttributeType attr_type)
{
	packed.m_attr_type = attr_type;
}

BookmarkAttributeType BookmarkAttribute::GetAttributeType() const
{
	return static_cast<BookmarkAttributeType>(packed.m_attr_type);
}

OP_STATUS BookmarkAttribute::SetValue(BookmarkAttribute *attr)
{
	packed.m_attr_type = attr->packed.m_attr_type;

	if (packed.m_owns_text_val)
		OP_DELETEA(m_text_value);
	if (attr->m_text_value)
	{
		m_text_value = OP_NEWA(uni_char, uni_strlen(attr->m_text_value)+1);
		if (!m_text_value)
			return OpStatus::ERR_NO_MEMORY;
		uni_strcpy(m_text_value, attr->m_text_value);
		packed.m_owns_text_val = 1;
	}
	else
		m_text_value = attr->m_text_value;
	m_int_value = attr->m_int_value;
	return OpStatus::OK;
}

OP_STATUS BookmarkAttribute::GetTextValue(OpString &value) const
{
	return value.Set(m_text_value);
}

uni_char* BookmarkAttribute::GetTextValue() const
{
	return m_text_value;
}

OP_STATUS BookmarkAttribute::SetTextValue(const uni_char *value)
{
	if (!value)
		return SetTextValue(value, 0);
	return SetTextValue(value, uni_strlen(value));
}

OP_STATUS BookmarkAttribute::SetTextValue(const uni_char *value, unsigned int len)
{
	if (packed.m_owns_text_val)
		OP_DELETEA(m_text_value);
	if (len > 0)
	{
		m_text_value = OP_NEWA(uni_char, len+1);
		if (!m_text_value)
			return OpStatus::ERR_NO_MEMORY;

		if (value)
			uni_strncpy(m_text_value, value, len);
		m_text_value[len] = 0;
		packed.m_owns_text_val = 1;
	}
	else
		m_text_value = NULL;
	return OpStatus::OK;
}

int BookmarkAttribute::GetIntValue() const
{
	return m_int_value;
}

void BookmarkAttribute::SetIntValue(int value)
{
	m_int_value = value;
}

UINT32 BookmarkAttribute::GetMaxLength() const
{
	return m_max_len;
}

OP_STATUS BookmarkAttribute::SetMaxLength(UINT32 value)
{
	m_max_len = value;
	return OpStatus::OK;
}

BookmarkActionType BookmarkAttribute::GetMaxLengthAction() const
{
	return static_cast<BookmarkActionType>(packed.m_max_len_action);
}

void BookmarkAttribute::SetMaxLengthAction(BookmarkActionType action)
{
	packed.m_max_len_action = action;
}

BOOL BookmarkAttribute::ViolatingMaxLength(BookmarkItem *bookmark)
{
	BookmarkAttribute attr;
	bookmark->GetAttribute(GetAttributeType(), &attr);

	attr.m_max_len = m_max_len;

	return ((attr.m_text_value ? uni_strlen(attr.m_text_value) : 0) > m_max_len);
}

BOOL BookmarkAttribute::Equal(BookmarkAttribute *attribute) const
{
	if (!m_text_value || !attribute->m_text_value)
	{
		return (!m_text_value || !*m_text_value) && (!attribute->m_text_value || !*attribute->m_text_value) && m_int_value == attribute->m_int_value;
	}

	return uni_strcmp(m_text_value, attribute->m_text_value) == 0;
}

BOOL BookmarkAttribute::Equal(OpString *text_value) const
{
	if (!m_text_value)
		return !text_value->HasContent();
	return uni_strcmp(m_text_value, text_value->CStr()) == 0;
}

OP_STATUS BookmarkAttribute::Cut(UINT32 len)
{
	unsigned int cur_len = m_text_value ? uni_strlen(m_text_value) : 0;
	if (len > cur_len)
		return OpStatus::OK;

	uni_char *new_val = OP_NEWA(uni_char, len+1);
	if (!new_val)
		return OpStatus::ERR_NO_MEMORY;

	uni_strncpy(new_val, m_text_value, len);
	new_val[len] = 0;
	if (packed.m_owns_text_val)
		OP_DELETEA(m_text_value);
	m_text_value = new_val;
	packed.m_owns_text_val = 1;
	return OpStatus::OK;
}

#ifdef SUPPORT_DATA_SYNC
BOOL BookmarkAttribute::ShouldSync() const
{
	return packed.m_sync;
}

void BookmarkAttribute::SetSync(BOOL value)
{
	packed.m_sync = !!value;
}
#endif // SUPPORT_DATA_SYNC

BOOL BookmarkAttribute::OwnsTextValue() const
{
	return packed.m_owns_text_val;
}

void BookmarkAttribute::SetOwnsTextValue(BOOL owns)
{
	packed.m_owns_text_val = !!owns;
}

BookmarkItem::BookmarkItem() :
	m_id(NULL),
	m_pid(NULL),
	m_count(0),
	m_max_count(~0u),
	m_listener(NULL)
{
	packed.m_folder_type = FOLDER_NO_FOLDER;
#ifdef SUPPORT_DATA_SYNC
	packed.m_is_modified = 0;
	packed.m_is_added = 0;
	packed.m_is_deleted = 0;
	packed.m_should_sync_parent = 0;
	packed.m_allowed_to_sync = 1;
#endif // SUPPORT_DATA_SYNC
	packed.m_move_is_copy = 0;
	packed.m_deletable = 1;
	packed.m_sub_folders_allowed = 1;
	packed.m_separators_allowed = 1;
}

BookmarkItem::~BookmarkItem()
{
	if (m_listener)
		m_listener->OnBookmarkDeleted();
 	OP_DELETEA(m_id);
 	OP_DELETEA(m_pid);
	m_attributes.DeleteAll();
}

uni_char* BookmarkItem::GetUniqueId() const
{
	return m_id;
}

void BookmarkItem::SetUniqueId(uni_char *uid)
{
	m_id = uid;
}

uni_char* BookmarkItem::GetParentUniqueId() const
{
	return m_pid;
}

void BookmarkItem::SetParentUniqueId(uni_char *uid)
{
#ifdef SUPPORT_DATA_SYNC
	packed.m_should_sync_parent = 1;
#endif // SUPPORT_DATA_SYNC

	m_pid = uid;
}

BookmarkItem* BookmarkItem::GetPreviousItem() const
{
	return static_cast<BookmarkItem*>(Pred());
}

BookmarkItem* BookmarkItem::GetNextItem() const
{
	return static_cast<BookmarkItem*>(Suc());
}

BookmarkAttribute* BookmarkItem::FindAttribute(BookmarkAttributeType attr_type) const
{
	unsigned int i, count = m_attributes.GetCount();
	for (i=0; i<count; i++)
	{
		BookmarkAttribute *attr = m_attributes.Get(i);
		if (attr->GetAttributeType() == attr_type)
			return attr;
	}

	return NULL;
}

OP_STATUS BookmarkItem::CreateAttribute(BookmarkAttributeType attr_type, BookmarkAttribute **attr)
{
	*attr = OP_NEW(BookmarkAttribute, ());
	if (!*attr)
		return OpStatus::ERR_NO_MEMORY;

	(*attr)->SetAttributeType(attr_type);

	OP_STATUS res = m_attributes.Add(*attr);
	if (OpStatus::IsError(res))
	{
		OP_DELETE(*attr);
		*attr = NULL;
	}
	return res;
}

OP_STATUS BookmarkItem::GetAttribute(BookmarkAttributeType attr_type, BookmarkAttribute *attr_val) const
{
	BookmarkAttribute *attr = FindAttribute(attr_type);
	if (attr)
		return attr_val->SetValue(attr);
	return attr_val->SetTextValue(UNI_L(""));
}

BookmarkAttribute* BookmarkItem::GetAttribute(BookmarkAttributeType attr_type) const
{
	return FindAttribute(attr_type);
}

OP_STATUS BookmarkItem::SetAttribute(BookmarkAttributeType attr_type, BookmarkAttribute *attr_val)
{
	BookmarkAttribute *attr = FindAttribute(attr_type);
	if (!attr && attr_val)
		RETURN_IF_ERROR(CreateAttribute(attr_type, &attr));
	else if (attr && !attr_val)
	{
		attr->SetIntValue(0);
		attr->SetTextValue(NULL);
#ifdef SUPPORT_DATA_SYNC
		SetModified(TRUE);
		attr->SetSync(TRUE);
#endif // SUPPORT_DATA_SYNC
		if (m_listener)
			m_listener->OnBookmarkChanged(attr_type, FALSE);
		return OpStatus::OK;
	}
	else if (!attr && !attr_val)
		return OpStatus::OK;

	if (attr_val->Equal(attr))
		return OpStatus::OK;

	if (attr_type == BOOKMARK_URL)
	{
		OpString fixed_url, value;
		RETURN_IF_ERROR(attr_val->GetTextValue(value));
		URL url = g_url_api->GetURL(value.CStr());
		if (!url.IsEmpty() && url.Type() != URL_MAILTO)
		{
			TRAPD(rc, g_url_api->ResolveUrlNameL(value, fixed_url));
			RETURN_IF_ERROR(rc);
			RETURN_IF_ERROR(attr->SetTextValue(fixed_url.CStr()));
		}
		else
			RETURN_IF_ERROR(attr->SetTextValue(value.CStr()));
	}
	else
	{
		attr_val->SetAttributeType(attr_type);
		RETURN_IF_ERROR(attr->SetValue(attr_val));
	}
#ifdef SUPPORT_DATA_SYNC
	SetModified(TRUE);
	attr->SetSync(TRUE);
#endif // SUPPORT_DATA_SYNC
	if (m_listener)
		m_listener->OnBookmarkChanged(attr_type, FALSE);
	return OpStatus::OK;
}

BookmarkFolderType BookmarkItem::GetFolderType() const
{
	return static_cast<BookmarkFolderType>(packed.m_folder_type);
}

void BookmarkItem::SetFolderType(BookmarkFolderType type)
{
	packed.m_folder_type = type;
}

OP_STATUS BookmarkItem::SetParentFolder(BookmarkItem *parent)
{
	if (!parent)
		return OpStatus::OK;

	if (parent->GetFolderType() == FOLDER_NO_FOLDER)
		return OpStatus::ERR;

	BookmarkItem *parent_folder = parent;
	for (; parent_folder; parent_folder = parent_folder->GetParentFolder())
		parent_folder->SetCount(parent_folder->GetCount() + GetCount() + 1);

	Under(parent);

#ifdef SUPPORT_DATA_SYNC
	packed.m_should_sync_parent = 1;
	packed.m_is_modified = 1;
#endif // SUPPORT_DATA_SYNC
	if (m_listener)
		m_listener->OnBookmarkChanged(BOOKMARK_NONE, TRUE);

	return OpStatus::OK;
}

BookmarkItem* BookmarkItem::GetParentFolder() const
{
	return (BookmarkItem*) Parent();
}

BookmarkItem* BookmarkItem::GetChildren() const
{
	return (BookmarkItem*) FirstChild();
}

UINT32 BookmarkItem::GetCount() const
{
	return m_count;
}

void BookmarkItem::SetCount(UINT32 count)
{
	m_count = count;
}

UINT32 BookmarkItem::GetMaxCount() const
{
	return m_max_count;
}

void BookmarkItem::SetMaxCount(UINT32 max_count)
{
	m_max_count = max_count;
}

UINT32 BookmarkItem::GetFolderDepth() const
{
	UINT32 folder_depth = 0;
	BookmarkItem *parent = GetParentFolder();
	for (; parent; parent = parent->GetParentFolder())
		folder_depth++;

	return folder_depth;
}

BOOL BookmarkItem::SubFoldersAllowed() const
{
	return packed.m_sub_folders_allowed;
}

void BookmarkItem::SetSubFoldersAllowed(BOOL value)
{
	packed.m_sub_folders_allowed = !!value;
}

BOOL BookmarkItem::Deletable() const
{
	return packed.m_deletable;
}

void BookmarkItem::SetDeletable(BOOL value)
{
	packed.m_deletable = !!value;
}

BOOL BookmarkItem::MoveIsCopy() const
{
	return packed.m_move_is_copy;
}

void BookmarkItem::SetMoveIsCopy(BOOL value)
{
	packed.m_move_is_copy = !!value;
}

BOOL BookmarkItem::SeparatorsAllowed() const
{
	return packed.m_separators_allowed;
}

void BookmarkItem::SetSeparatorsAllowed(BOOL value)
{
	packed.m_separators_allowed = !!value;
}

void BookmarkItem::Cut(BookmarkAttributeType attr_type, UINT32 len)
{
	BookmarkAttribute *attr = FindAttribute(attr_type);
	if (attr)
		attr->Cut(len);
}

void BookmarkItem::ClearAttributes()
{
	m_attributes.DeleteAll();
}

#ifdef SUPPORT_DATA_SYNC
void BookmarkItem::SetAllowedToSync(BOOL allowed_to_sync)
{
	packed.m_allowed_to_sync = !!allowed_to_sync;
}

BOOL BookmarkItem::IsAllowedToSync() const
{
	return packed.m_allowed_to_sync;
}

BOOL BookmarkItem::IsModified() const
{
	return packed.m_is_modified;
}

BOOL BookmarkItem::IsAdded() const
{
	return packed.m_is_added;
}

BOOL BookmarkItem::IsDeleted() const
{
	return packed.m_is_deleted;
}

void BookmarkItem::SetModified(BOOL is_modified)
{
	packed.m_is_modified = !!is_modified;
}

void BookmarkItem::SetAdded(BOOL is_added)
{
	packed.m_is_added = !!is_added;
}

void BookmarkItem::SetDeleted(BOOL is_deleted)
{
	packed.m_is_deleted = !!is_deleted;
}

BOOL BookmarkItem::ShouldSyncAttribute(BookmarkAttributeType attr_type) const
{
	BookmarkAttribute *attr = FindAttribute(attr_type);
	if (attr)
		return attr->ShouldSync();
	return FALSE;
}

void BookmarkItem::SetSyncAttribute(BookmarkAttributeType attr_type, BOOL val)
{
	BookmarkAttribute *attr = FindAttribute(attr_type);
	if (attr)
		attr->SetSync(TRUE);
}

BOOL BookmarkItem::ShouldSyncParent() const
{
	return packed.m_should_sync_parent;
}

void BookmarkItem::SetSyncParent(BOOL value)
{
	packed.m_should_sync_parent = !!value;
}

void BookmarkItem::ClearSyncFlags()
{
	unsigned int i, count = m_attributes.GetCount();
	for (i=0; i<count; i++)
	{
		BookmarkAttribute *attr = m_attributes.Get(i);
		attr->SetSync(FALSE);
	}
	packed.m_is_added = packed.m_is_deleted = packed.m_is_modified = packed.m_should_sync_parent = 0;
}

BOOL BookmarkItem::ShouldSync() const
{
	if (packed.m_is_added || packed.m_is_deleted || packed.m_is_modified || packed.m_should_sync_parent)
		return TRUE;

	unsigned int i, count = m_attributes.GetCount();
	for (i=0; i<count; i++)
	{
		BookmarkAttribute *attr = m_attributes.Get(i);
		if (attr->ShouldSync())
			return TRUE;
	}

	return FALSE;
}
#endif // SUPPORT_DATA_SYNC

void BookmarkItem::SetListener(BookmarkItemListener *listener)
{
	m_listener = listener;
}

#endif // CORE_BOOKMARKS_SUPPORT
