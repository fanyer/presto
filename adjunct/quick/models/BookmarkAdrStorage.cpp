/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 *
 */

#include "core/pch.h"

#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/models/BookmarkAdrStorage.h"
#include "adjunct/quick/hotlist/hotlistfileio.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/hotlist/hotlistparser.h"
#include "adjunct/desktop_util/resources/ResourceSetup.h"

enum AdrItemType
{
	FOLDER = FOLDER_NORMAL_FOLDER,	
	SEPARATOR = FOLDER_SEPARATOR_FOLDER,
	BOOKMARK = FOLDER_NO_FOLDER,
	HIDDEN_BOOKMARK,
	UNKNOWN_ITEM,
	END_OF_FOLDER = -2,
};

enum BookmarkAttributeTypeExtension
{
    BOOKMARK_ID = BOOKMARK_NONE,
    BOOKMARK_TRASH,
    BOOKMARK_UID,
    BOOKMARK_MOVE_IS_COPY,
    BOOKMARK_DELETABLE,
    BOOKMARK_SUBFOLDER_ALLOWED,
    BOOKMARK_SEP_ALLOWED,
    BOOKMARK_MAX_ITEMS,
    BOOKMARK_UNKNOWN_ATTRIBUTE
};

struct KEYWORD_ATTRIBUTE
{
	const char* keyword;
	int attribute;
};

static KEYWORD_ATTRIBUTE keywords[] =
{
	// these are in core,and must be in the same order as in core
	{KEYWORD_URL,			BOOKMARK_URL},
	{KEYWORD_NAME,			BOOKMARK_TITLE},
	{KEYWORD_DESCRIPTION,BOOKMARK_DESCRIPTION},
	{KEYWORD_SHORTNAME,		BOOKMARK_SHORTNAME},
	{KEYWORD_THUMBNAIL,		BOOKMARK_THUMBNAIL_FILE},
	{KEYWORD_CREATED,		BOOKMARK_CREATED},
	{KEYWORD_VISITED,		BOOKMARK_VISITED},
	{KEYWORD_PANEL_POS,		BOOKMARK_PANEL_POS},
	{KEYWORD_IN_PANEL,		BOOKMARK_SHOW_IN_PANEL},
	{KEYWORD_PERSONALBAR_POS,BOOKMARK_PERSONALBAR_POS},
	{KEYWORD_ON_PERSONALBAR,BOOKMARK_SHOW_IN_PERSONAL_BAR},
	{KEYWORD_ACTIVE,		BOOKMARK_ACTIVE},
	{KEYWORD_TARGET,		BOOKMARK_TARGET},
	{KEYWORD_EXPANDED,		BOOKMARK_EXPANDED},
	{KEYWORD_SMALL_SCREEN,	BOOKMARK_SMALLSCREEN},
	{KEYWORD_PARTNER_ID,	BOOKMARK_PARTNER_ID},
	{KEYWORD_DISPLAY_URL,   BOOKMARK_DISPLAY_URL},
	// The rest are not normal core attributes 
	{KEYWORD_ID,			BOOKMARK_ID},
	{KEYWORD_TRASH_FOLDER,	BOOKMARK_TRASH},
	{KEYWORD_UNIQUEID,		BOOKMARK_UID},
	{KEYWORD_MOVE_IS_COPY,	BOOKMARK_MOVE_IS_COPY},
	{KEYWORD_DELETABLE,		BOOKMARK_DELETABLE},
	{KEYWORD_SUBFOLDER_ALLOWED,	BOOKMARK_SUBFOLDER_ALLOWED},
	{KEYWORD_SEPARATOR_ALLOWED,	BOOKMARK_SEP_ALLOWED},
	// Ignored attributes
	{KEYWORD_FAVICON,		BOOKMARK_UNKNOWN_ATTRIBUTE}
};

static const char* item_types[] = {KEYWORD_URLSECTION, /* bookmark */
									KEYWORD_SEPERATORSECTION, /* separator */
									KEYWORD_FOLDERSECTION, /* Folder*/
									KEYWORD_ENDOFFOLDER,
									KEYWORD_DELETED_BOOKMARK /* Last entry in the file, used to store uuids of deleted default bookmark */
									};


BookmarkAdrStorage::BookmarkAdrStorage(BookmarkManager *manager, const uni_char* path, BOOL sync, BOOL merge_trash) :
	BookmarkStorageProvider(manager),
	m_sync(sync),
	m_merge_trash(merge_trash),
	m_writer(NULL),
	m_reader(NULL),
	m_last_item(NULL),
	m_listener(NULL)
{
	m_filename.Set(path);
}

/************************************************************************
 *
 * OpenRead
 *
 * Reads adr file header
 *
 * @return - If returning OK reader was ok and m_last_line contains first
 *           non-header line of the file
 *
 *
 ************************************************************************/
OP_STATUS BookmarkAdrStorage::OpenRead()
{
	OP_ASSERT(!m_reader && !m_writer);
	OP_ASSERT(m_filename.HasContent());

	if (!m_reader)
	{
		m_reader = OP_NEW(HotlistFileReader,());
		if (!m_reader)
			return OpStatus::ERR_NO_MEMORY;

		if (!m_reader->Init(m_filename, HotlistFileIO::UTF8))
		{
			return OpStatus::ERR;
		}
	}

	return ParseHeader();
}

/**************************************************************************
 *
 * @pre - m_reader->Init has been called
 *
 * @return TRUE if m_last_line non-empty, FALSE if EOF
 *         if TRUE then m_last_line holds next non-empty line
 *
 **************************************************************************/

BOOL BookmarkAdrStorage::Advance()
{
	int line_length = 0;
	m_last_line.Empty();
	uni_char *line = m_reader->Readline(line_length);
	if (!line) 
		return FALSE;
	while (!line[0])
	{
		line = m_reader->Readline(line_length);
		if (!line) 
			return FALSE;
	}
	m_last_line.Set(line);
	m_last_line.Strip();
	return TRUE;
}


/************************************************************************
 *
 * LoadBookmark
 *
 * Loads one bookmark - should only be called after MoreBookmarks returned
 *                      true
 *
 * @param bookmark - allocated BookmarkItem that will hold fields of loaded
 *                   bookmark on return
 * @return
 *
 *
 * @pre  - MoreBookmarks() == true
 *
 *
 * @post -
 *
 ************************************************************************/
OP_STATUS BookmarkAdrStorage::LoadBookmark(BookmarkItem *bookmark)
{
	BookmarkItemData item_data;
	RETURN_IF_ERROR(ReadBookmarkData(item_data));

	// don't add another trash folder when importing
	if (m_merge_trash)
	{
		BookmarkFolder* trash = g_hotlist_manager->GetBookmarksModel()->GetTrashFolder();
		if (item_data.type == FOLDER_TRASH_FOLDER && trash)
		{
			m_stack.Push(trash->GetCoreItem());
			return OpStatus::ERR_OUT_OF_RANGE;
		}
	}

	BookmarkItem* found = g_bookmark_manager->FindId(item_data.unique_id.CStr());
	if (found)
	{
		// If the folder already exist all children should go to the existing folder.
		if (item_data.type == FOLDER_NORMAL_FOLDER || item_data.type == FOLDER_TRASH_FOLDER)
		{
			m_stack.Push(found);
			m_last_item = 0;
		}
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	::SetDataFromItemData(&item_data, bookmark);
	RETURN_IF_ERROR(g_bookmark_manager->AddNewBookmark(bookmark, m_stack.Peek() ? m_stack.Peek() : g_bookmark_manager->GetRootFolder(), m_sync));

	if (item_data.type == FOLDER_NORMAL_FOLDER || item_data.type == FOLDER_TRASH_FOLDER)
	{
		m_stack.Push(bookmark);
		m_last_item = 0;
	}
	else
	{
		m_last_item = bookmark;
	}	

	if (m_listener)
		m_listener->BookmarkIsLoaded(bookmark, OpStatus::OK);

	return OpStatus::OK;
}

OP_STATUS BookmarkAdrStorage::ReadBookmarkData(BookmarkItemData& item_data)
{
	if (!m_reader)
	{
		RETURN_IF_ERROR(OpenRead());
    }

	INT32 item_type = GetItemType(m_last_line);
	m_last_line.Empty();
	OP_ASSERT(item_type >= 0 && item_type <= 3);

	switch (item_type)
	{
	case FOLDER:
		item_data.type = FOLDER_NORMAL_FOLDER;
		break;
	case SEPARATOR:
		item_data.type = FOLDER_SEPARATOR_FOLDER;
		break;
	case BOOKMARK:
		item_data.type = FOLDER_NO_FOLDER;
		break;
	default:
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	while (GetNextField(item_data))
		;

	return OpStatus::OK;
}

/**************************************************************
 *
 * ParseHeader
 *
 *
 *
 **************************************************************/

OP_STATUS BookmarkAdrStorage::ParseHeader()
{
	int line_length;
	uni_char *line;
	
	do
	{
		line = m_reader->Readline(line_length);
		if (!line) 
			return OpStatus::ERR;
    } while (!line[0]);

	OpStringC hline(line);
	if (hline.FindI(UNI_L("Hotlist version 2.0")) == KNotFound)
		return OpStatus::ERR;

	if (!Advance())
		return OpStatus::ERR; // EOF

	if (m_last_line.FindI(UNI_L("Options:")) != KNotFound)
	{
		// TODO: Handle Option to not sync model?
		if (!Advance())
			return OpStatus::ERR;
	}
	else if (m_last_line.Find(UNI_L("#")) == 0)
	{
		return OpStatus::OK;
	}

	return OpStatus::OK;
}


/************************************************************************
 *
 * IsKey
 *
 *
 ************************************************************************/

int BookmarkAdrStorage::IsKey(const uni_char* candidate,
							  int candidateLen,
							  const char* key,
							  int keyLen)
{
	if (candidateLen >= keyLen)
	{
		for (int i=0; i<keyLen; i++)
		{
			if (candidate[i] != (uni_char)key[i])
				return -1;
		}
		return keyLen;
    }

	return -1;
}



/************************************************************************
 *
 * GetItemType
 *
 * @param OpString line -
 *
 * @return ItemType if line spesicifes an itemtype, -2 if line
 *         specifies end-of-folder, else -1
 *
 ************************************************************************/

INT32 BookmarkAdrStorage::GetItemType(const OpString& line)
{
	OP_ASSERT(line.HasContent());

	if (line.Compare(UNI_L("-")) == 0)
		return END_OF_FOLDER;

	INT32 offset = -1;
	INT32 line_length = line.Length();
	unsigned int i = 0;

	while (i < ARRAY_SIZE(item_types) &&
			(offset = IsKey(line.CStr(), line_length, item_types[i], strlen(item_types[i])) == -1 ))
	{
		i++;
	}

	switch(i)
	{
	case 0: return BOOKMARK;
	case 1: return SEPARATOR;
	case 2: return FOLDER;
	case 3: return END_OF_FOLDER;
	case 4: return HIDDEN_BOOKMARK;
	default:
		{
			OP_ASSERT(FALSE); return UNKNOWN_ITEM; //
		}
	}
}


/************************************************************************
 *
 * GetItemField
 *
 * @param value - contains value of item field attribute
 *
 * @return attribute type
 *
 ************************************************************************/
INT32 BookmarkAdrStorage::GetItemField(const OpString& line, OpString& value)
{
	INT32 offset = -1;
	unsigned int i = 0;
	int line_length = line.Length();
	while (i < ARRAY_SIZE(keywords) && (offset = IsKey(line.CStr(), line_length, keywords[i].keyword, strlen(keywords[i].keyword)-1)) == -1)
	{
		i++;
	}

	if (i < ARRAY_SIZE(keywords))
	{
		value.Set((line.CStr() + strlen(keywords[i].keyword)));
		return keywords[i].attribute;
	}
	else
	{
		OP_ASSERT(FALSE);
		return BOOKMARK_UNKNOWN_ATTRIBUTE;
	}
}

/************************************************************************
 *
 * GetNextField
 *
 * @param item_data - holds attribute value of this field on return
 *
 * @return TRUE if current item has a next field, FALSE if EOF or
 *         new item reached
 *
 * @pre m_last_line is empty
 * @post m_last_line contains
 *
 ************************************************************************/
BOOL BookmarkAdrStorage::GetNextField(BookmarkItemData& item_data)
{
	if (!Advance()) return FALSE;

	// End of folder or new item -> Done
	if (m_last_line[0] == '#' || m_last_line.Compare(UNI_L("-")) == 0)
	{
		return FALSE;
    }

	OpString value;
	INT32 attribute = GetItemField(m_last_line, value);

	// Ignore unknown attribute
	if (attribute == BOOKMARK_UNKNOWN_ATTRIBUTE)
		return TRUE;

	switch(attribute)
	{
    case BOOKMARK_URL:
		item_data.url.Set(value);
		break;
    case BOOKMARK_TITLE :
		item_data.name.Set(value);
		break;
    case BOOKMARK_DESCRIPTION:
		{
			OpString tmp;
			HotlistParser::ConvertToNewline(tmp, value.CStr());
			item_data.description.Set(tmp);
		}
		break;
    case BOOKMARK_SHORTNAME:
		item_data.shortname.Set(value);
		break;
    case BOOKMARK_THUMBNAIL_FILE:
		OP_ASSERT(FALSE);
		break;
    case BOOKMARK_CREATED:
		{
			OpString date;
			SyncUtil::CreateRFC3339Date(uni_atof(value.CStr()), date);
			item_data.created.Set(date);
		}
		break;
    case BOOKMARK_VISITED:
		{
			OpString date;
			SyncUtil::CreateRFC3339Date(uni_atof(value.CStr()), date);
			item_data.visited.Set(date);
		}
		break;
    case BOOKMARK_PANEL_POS:
		item_data.panel_position = uni_atoi(value.CStr());
		break;
    case BOOKMARK_PERSONALBAR_POS:
		item_data.personalbar_position = uni_atoi(value.CStr());
		break;
    case BOOKMARK_ID:
		// ignored
		break;
    case BOOKMARK_ACTIVE:
		item_data.active = IsYes(value.CStr());
		break;
    case BOOKMARK_EXPANDED:
		item_data.expanded = IsYes(value.CStr());
		break;
    case BOOKMARK_SMALLSCREEN:
		item_data.small_screen = IsYes(value.CStr());
		break;
	case BOOKMARK_PARTNER_ID:
		item_data.partner_id.Set(value.CStr());
		break;
    case BOOKMARK_TRASH:
		item_data.type = FOLDER_TRASH_FOLDER;
		break;
    case BOOKMARK_UID:
		item_data.unique_id.Set(value.CStr());
		break;
    case BOOKMARK_TARGET:
		item_data.target.Set(value);
		break;
    case BOOKMARK_MOVE_IS_COPY:
		item_data.move_is_copy = IsYes(value);
		break;
    case BOOKMARK_DELETABLE:
		item_data.deletable = IsYes(value);
		break;
    case BOOKMARK_SUBFOLDER_ALLOWED:
		item_data.subfolders_allowed = IsYes(value);
		break;
    case BOOKMARK_SEP_ALLOWED:
		item_data.separators_allowed = IsYes(value);
		break;
	case BOOKMARK_MAX_ITEMS:
		item_data.max_count = uni_atoi(value.CStr());
		// for some reasons there are "MAX_ITEM=0" in some old bookmark files.
		// 10.1 take this as no limit so we do the same here.
		if (item_data.max_count == 0)
			item_data.max_count = -1;
		break;
    case BOOKMARK_DISPLAY_URL:
		item_data.display_url.Set(value);
		break;
    default: ;
    }

	return TRUE;
}

OP_STATUS BookmarkAdrStorage::DecodeUniqueId(uni_char *uid, char *new_uid)
{
	UINT32 i, offset = 0;

	new_uid[8] = '-';
	new_uid[13] = '-';
	new_uid[18] = '-';
	new_uid[23] = '-';

	for (i=0; i<36; i++)
	{
		if (i != 8 && i != 13 && i != 18 && i != 23)
			new_uid[i] = Unicode::ToLower(uid[i-offset]);
		else
			offset++;
	}

	new_uid[36] = 0;

	return OpStatus::OK;
}

BOOL BookmarkAdrStorage::MoreBookmarks()
{
	OP_ASSERT(!m_writer);

	if (!m_reader)
	{
		if (OpStatus::IsError(OpenRead()))
			return FALSE;
    }

	if (!m_last_line.HasContent())
	{
		// Empty stack in case of adr files with missing end_of_folder markers
		m_stack.Empty();
		return FALSE;
	}

	INT32 item_type = GetItemType(m_last_line);
	while (item_type == END_OF_FOLDER || item_type == UNKNOWN_ITEM)
	{
		if (item_type == END_OF_FOLDER)
		{
			if (m_stack.Peek())
			{
				m_last_item = m_stack.Pop();
			}
		}

		// Get next item and continue ---
		if (!Advance())
		{
			// Empty stack in case of adr files with missing end_of_folder markers
			m_stack.Empty();
			return FALSE;
		}

		item_type = GetItemType(m_last_line);
    }

	if (item_type == HIDDEN_BOOKMARK)
	{
		// Read the deleted partner bookmarks uuids, this should be the last item in the file
		while (Advance())
		{
			OpString partner_id;
			INT32 attribute = GetItemField(m_last_line, partner_id);
			if (attribute == BOOKMARK_PARTNER_ID)
			{
				g_desktop_bookmark_manager->AddDeletedDefaultBookmark(partner_id);
			}
		}
		return FALSE;
	}
	else
	{
		return m_last_line.HasContent();
	}
}

OP_STATUS BookmarkAdrStorage::OpenSave()
{
	OP_ASSERT(!m_reader && !m_writer);

	if (!m_writer)
	{
		m_writer = OP_NEW(HotlistFileWriter,());
		if (!m_writer)
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(m_writer->Open(m_filename, TRUE));
		RETURN_IF_ERROR(m_writer->WriteHeader());
	}
	return OpStatus::OK;
}

OP_STATUS BookmarkAdrStorage::Close()
{
	OP_ASSERT(!(m_reader && m_writer));
	OP_DELETE(m_reader);
	m_reader = NULL;
	
	OP_STATUS ret = OpStatus::OK;
	if (m_writer)
	{
		// Append the *Hidden* section which stores uuids of deleted default bookmarks
		HotlistParser::WriteDeletedDefaultBookmark(*m_writer);
		ret = m_writer->Close();
	}

	OP_DELETE(m_writer);
	m_writer = NULL;
	return ret;
}

OP_STATUS BookmarkAdrStorage::ClearStorage()
{
	RETURN_IF_ERROR(Close());
	RETURN_IF_ERROR(OpenSave());

	if (m_listener)
		m_listener->BookmarkIsSaved(NULL, OpStatus::OK);

	return OpStatus::OK;
}

OP_STATUS BookmarkAdrStorage::UseFormat(BookmarkFormatType format)
{
	if (format != BOOKMARK_ADR)
		return OpStatus::ERR_NOT_SUPPORTED;

	return OpStatus::OK;
}

OP_STATUS BookmarkAdrStorage::SaveBookmark(BookmarkItem *bookmark)
{
	if (!m_writer)
		RETURN_IF_ERROR(OpenSave());

	HotlistModelItem* item = g_hotlist_manager->GetBookmarksModel()->GetByUniqueGUID(bookmark->GetUniqueId());

	if (item)
	{
		if (item->IsFolder())
			RETURN_IF_ERROR(HotlistParser::WriteFolderNode(*m_writer, *item, *g_hotlist_manager->GetBookmarksModel()));
		else if (item->IsSeparator())
			RETURN_IF_ERROR(HotlistParser::WriteSeparatorNode(*m_writer, *item));
		else
			RETURN_IF_ERROR(HotlistParser::WriteBookmarkNode(*m_writer, *item));
	}
	else
	{
		// These are the ghost trash folders
		if (bookmark->GetFolderType() == FOLDER_TRASH_FOLDER)
			HotlistParser::WriteTrashFolderNode(*m_writer, bookmark);
		else
			OP_ASSERT(FALSE);
	}

	if (m_listener)
		m_listener->BookmarkIsSaved(bookmark, OpStatus::OK);

	return OpStatus::OK;
}

OP_STATUS BookmarkAdrStorage::FolderBegin(BookmarkItem *folder)
{
	return OpStatus::OK;
}

OP_STATUS BookmarkAdrStorage::FolderEnd(BookmarkItem *folder)
{
	if (!m_writer)
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_writer->WriteAscii("-\n\n"));
	return OpStatus::OK;
}

void BookmarkAdrStorage::RegisterListener(BookmarkStorageListener *l)
{
	m_listener = l;
}

void BookmarkAdrStorage::UnRegisterListener(BookmarkStorageListener *l)
{
	if (m_listener == l)
		m_listener = NULL;
}


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

DefaultBookmarkUpgrader::DefaultBookmarkUpgrader()
	:BookmarkAdrStorage(NULL, NULL)
{
	OpFile	default_file;
	RETURN_VOID_IF_ERROR(ResourceSetup::GetDefaultPrefsFile(DESKTOP_RES_PACKAGE_BOOKMARK, default_file));
	SetFilename(default_file.GetFullPath());
}

OP_STATUS DefaultBookmarkUpgrader::Run()
{
	while(MoreBookmarks())
	{
		BookmarkItemData item;

		RETURN_IF_ERROR(ReadBookmarkData(item));
		RETURN_IF_ERROR(UpgradeBookmark(item));
	}

	// Remove empty folders we just added
	for (INT i = m_added_folders.GetCount() - 1; i>=0; i--)
	{
		HotlistModelItem* item = g_hotlist_manager->GetBookmarksModel()->GetByUniqueGUID(m_added_folders.Get(i)->CStr());
		OP_ASSERT(item);
		if (item && item->GetChildCount() == 0 && BookmarkWrapper(item))
			BookmarkWrapper(item)->RemoveBookmark(TRUE, TRUE);
	}

	return OpStatus::OK;
}

OP_STATUS DefaultBookmarkUpgrader::UpgradeBookmark(BookmarkItemData& item)
{
	if (item.type == FOLDER_TRASH_FOLDER	// don't add another trash folder.
		|| g_desktop_bookmark_manager->IsDeletedDefaultBookmark(item.partner_id))
		return OpStatus::OK;

	HotlistModelItem* found = g_desktop_bookmark_manager->FindDefaultBookmark(item.partner_id);
	if (found)
	{
		BookmarkItem* core_item = BookmarkWrapper(found)->GetCoreItem();
		return UpdateExistingBookmark(core_item, item);
	}
	else
	{
		// Update the existing bookmark if the uuid match
		BookmarkItem* found = g_bookmark_manager->FindId(item.unique_id);
		if (found)
		{
			return UpdateExistingBookmark(found, item);
		}
		else // Find by name and url in the current folder
		{
			BookmarkItem* child = m_stack.Peek() ? m_stack.Peek()->GetChildren() : g_bookmark_manager->GetRootFolder()->GetChildren();
			while (child)
			{
				BookmarkAttribute attr;
				OpString url, name;
				RETURN_IF_ERROR(child->GetAttribute(BOOKMARK_URL, &attr));
				RETURN_IF_ERROR(attr.GetTextValue(url));
				RETURN_IF_ERROR(child->GetAttribute(BOOKMARK_TITLE, &attr));
				RETURN_IF_ERROR(attr.GetTextValue(name));

				if (url.Compare(item.url) == 0 && name.Compare(item.name) == 0)
					return UpdateExistingBookmark(child, item);

				child = child->GetNextItem();
			}
		}
	}

	// Only add new bookmark which has a partner id
	if (item.partner_id.IsEmpty())
		return OpStatus::OK;

	BookmarkItem* bookmark = OP_NEW(BookmarkItem,());
	RETURN_OOM_IF_NULL(bookmark);

	::SetDataFromItemData(&item, bookmark);
	
	// Add item if parent folder is not inside trash
	HotlistModelItem* folder = m_stack.Peek() ? g_hotlist_manager->GetBookmarksModel()->GetByUniqueGUID(m_stack.Peek()->GetUniqueId()) : NULL;
	if (!folder || !folder->GetIsInsideTrashFolder())
		RETURN_IF_ERROR(g_bookmark_manager->AddNewBookmark(bookmark, m_stack.Peek() ? m_stack.Peek() : g_bookmark_manager->GetRootFolder(), m_sync));

	if (item.type == FOLDER_NORMAL_FOLDER || item.type == FOLDER_TRASH_FOLDER)
	{
		OpString* uid = OP_NEW(OpString, ());
		if (uid)
		{
			uid->Set(bookmark->GetUniqueId());
			m_added_folders.Add(uid);
		}

		m_stack.Push(bookmark);
		m_last_item = 0;
	}
	else
	{
		m_last_item = bookmark;
	}	

	return OpStatus::OK;
}

OP_STATUS DefaultBookmarkUpgrader::UpdateExistingBookmark(BookmarkItem* item, BookmarkItemData& item_data)
{
	// Ignore if type doesn't match
	if (item_data.type != item->GetFolderType())
	{
		return OpStatus::OK;
	}

	// Update the attributes
	::SetAttribute(item, BOOKMARK_PARTNER_ID,	item_data.partner_id);
	::SetAttribute(item, BOOKMARK_URL,			item_data.url);
	::SetAttribute(item, BOOKMARK_TITLE,		item_data.name);
	::SetAttribute(item, BOOKMARK_DESCRIPTION,	item_data.description);
	::SetAttribute(item, BOOKMARK_DISPLAY_URL,	item_data.display_url);

	// Bookmark might have moved to a different folder
	if (item->GetParentFolder() != m_stack.Peek() && (item->GetParentFolder() != g_bookmark_manager->GetRootFolder() || m_stack.Peek() != NULL))
	{
		BookmarkItem* new_parent = m_stack.Peek();
		if (!new_parent)
			new_parent = g_bookmark_manager->GetRootFolder();
		OpStatus::Ignore(g_bookmark_manager->MoveBookmark(item, new_parent));
	}

	// If the folder already exist all children should go to the existing folder.
	if (item->GetFolderType() == FOLDER_NORMAL_FOLDER)
	{
		m_stack.Push(item);
		m_last_item = 0;
	}
	return OpStatus::OK;
}
