/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CORE_BOOKMARKS_SUPPORT

#include "modules/util/opfile/opsafefile.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/util/opstring.h"
#include "modules/formats/uri_escape.h"

#include "modules/bookmarks/bookmark_ini_storage.h"
#include "modules/bookmarks/bookmark_manager.h"

BookmarkIniStorage::BookmarkIniStorage(BookmarkManager *manager) :
	::BookmarkStorageProvider(manager),
	m_filename(NULL),
	m_folder(OPFILE_FOLDER_COUNT),
	m_bookmark_file(NULL),
	m_bookmark_prefs(NULL),
	m_current_section(0),
	m_index(0),
	m_listener(NULL)
{
}

BookmarkIniStorage::~BookmarkIniStorage()
{
	Close();
	op_free(m_filename);
	m_filename = NULL;
	BookmarkListElm *elm = static_cast<BookmarkListElm*>(m_pool.First());
	while (elm)
	{
		BookmarkItem *bookmark = elm->GetBookmark();
		OP_DELETE(bookmark);
		elm = static_cast<BookmarkListElm*>(elm->Suc());
	}
	m_pool.Clear();
}

OP_STATUS BookmarkIniStorage::UseFormat(BookmarkFormatType format)
{
	if (!(format == BOOKMARK_VERBOSE || format == BOOKMARK_BINARY || format == BOOKMARK_BINARY_COMPRESSED))
		return OpStatus::ERR;

	m_format = format;
	return OpStatus::OK;
}

OP_STATUS BookmarkIniStorage::OpenLoad(const uni_char *filename, OpFileFolder folder)
{
	OP_MEMORY_VAR OP_STATUS ret = OpStatus::OK;
	BOOL flag;

	m_sections.Resize(0);
	m_current_section = 0;

	if (m_filename && filename)
	{
		if (uni_strcmp(m_filename, filename) != 0)
		{
			op_free(m_filename);
			m_filename = uni_strdup(filename);
			if (!m_filename)
				return OpStatus::ERR_NO_MEMORY;
			m_folder = folder;
		}
		if (m_folder != folder)
			m_folder = folder;
	}
	else if (filename)
	{
		m_filename = uni_strdup(filename);
		if (!m_filename)
			return OpStatus::ERR_NO_MEMORY;
		m_folder = folder;
	}
	else if (!m_filename)
		return OpStatus::ERR_NULL_POINTER;

	Close();

	m_bookmark_file = OP_NEW(OpFile, ());
	if (!m_bookmark_file)
		return OpStatus::ERR_NO_MEMORY;

	ret = m_bookmark_file->Construct(filename, folder);
	if (ret != OpStatus::OK)
	{
		OP_DELETE(m_bookmark_file);
		m_bookmark_file = NULL;
		return ret;
	}

	m_bookmark_prefs = OP_NEW(PrefsFile, (PREFS_STD));
	if (!m_bookmark_prefs)
	{
		OP_DELETE(m_bookmark_file);
		m_bookmark_file = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	TRAP(ret, m_bookmark_prefs->ConstructL());
	if (ret != OpStatus::OK)
	{
		OP_DELETE(m_bookmark_file);
		OP_DELETE(m_bookmark_prefs);
		m_bookmark_file = NULL;
		m_bookmark_prefs = NULL;
		return ret;
	}

	m_bookmark_file->Exists(flag);
	if (!flag)
	{
		OP_DELETE(m_bookmark_file);
		OP_DELETE(m_bookmark_prefs);
		m_bookmark_file = NULL;
		m_bookmark_prefs = NULL;
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

	TRAP(ret, m_bookmark_prefs->SetFileL(m_bookmark_file));
	if (ret != OpStatus::OK)
	{
		OP_DELETE(m_bookmark_file);
		OP_DELETE(m_bookmark_prefs);
		m_bookmark_file = NULL;
		m_bookmark_prefs = NULL;
		return ret;
	}

	TRAP(ret, m_bookmark_prefs->LoadAllL());
	if (ret != OpStatus::OK)
	{
		OP_DELETE(m_bookmark_file);
		OP_DELETE(m_bookmark_prefs);
		m_bookmark_file = NULL;
		m_bookmark_prefs = NULL;
		return ret;
	}

	TRAP(ret, m_bookmark_prefs->ReadAllSectionsL(m_sections));
	if (ret != OpStatus::OK)
	{
		OP_DELETE(m_bookmark_file);
		OP_DELETE(m_bookmark_prefs);
		m_bookmark_file = NULL;
		m_bookmark_prefs = NULL;
		m_sections.Resize(0);
		return ret;
	}

	m_pool.Clear();

	return OpStatus::OK;
}

OP_STATUS BookmarkIniStorage::CheckIfExists(const uni_char *filename, OpFileFolder folder)
{
	OpFile bookmark_file;
	BOOL flag;

	RETURN_IF_ERROR(bookmark_file.Construct(filename, folder));
	RETURN_IF_ERROR(bookmark_file.Exists(flag));
	if (!flag)
	{
		RETURN_IF_ERROR(bookmark_file.Open(OPFILE_WRITE));
		bookmark_file.Close();
	}
	return OpStatus::OK;
}

OP_STATUS BookmarkIniStorage::OpenSave(const uni_char *filename, OpFileFolder folder)
{
	OP_STATUS ret;
	OP_MEMORY_VAR UINT32 i=0;
	UINT32 number_of_sections;
	uni_char section_name[24]; // ARRAY OK 2007-08-14 adame

	if (m_filename && filename)
	{
		if (uni_strcmp(m_filename, filename) != 0)
		{
			op_free(m_filename);
			m_filename = uni_strdup(filename);
			if (!m_filename)
				return OpStatus::ERR_NO_MEMORY;
			m_folder = folder;
		}
		if (m_folder != folder)
			m_folder = folder;
	}
	else if (filename)
	{
		m_filename = uni_strdup(filename);
		if (!m_filename)
			return OpStatus::ERR_NO_MEMORY;
		m_folder = folder;
	}
	else if (!m_filename)
		return OpStatus::ERR_NULL_POINTER;

	Close();

	RETURN_IF_ERROR(CheckIfExists(filename, folder));
	RETURN_IF_ERROR(OpenLoad(filename, folder));

	// Delete old bookmarks
	number_of_sections = m_sections.Count();
	for (i=0; i<number_of_sections; i++)
	{
		uni_snprintf(section_name, 24, UNI_L("Bookmark %d"), i);
		TRAP(ret, m_bookmark_prefs->DeleteSectionL(section_name));
		if (!OpStatus::IsSuccess(ret))
		{
			OP_DELETE(m_bookmark_file);
			OP_DELETE(m_bookmark_prefs);
			m_bookmark_file = NULL;
			m_bookmark_prefs = NULL;
			return ret;
		}
	}

	m_current_section = 0;
	m_sections.Resize(0);

	return OpStatus::OK;
}

OP_STATUS BookmarkIniStorage::Close()
{
	OP_STATUS ret = OpStatus::OK;

	if (m_bookmark_prefs)
	{
		TRAP(ret, m_bookmark_prefs->CommitL());
		if (OpStatus::IsError(ret))
			return ret;
		OP_DELETE(m_bookmark_prefs);
		m_bookmark_prefs = NULL;
		m_sections.Resize(0);
		m_index = 0;
	}

	if (m_bookmark_file)
	{
		ret = m_bookmark_file->Close();
		OP_DELETE(m_bookmark_file);
		m_bookmark_file = NULL;
	}

	return ret;
}

OP_STATUS BookmarkIniStorage::SaveAttribute(BookmarkAttributeType attribute, const uni_char *section_name, const uni_char *entry_name, BookmarkItem *bookmark)
{
	BookmarkAttribute attr;
	OpString tmp;
	OP_STATUS ret = OpStatus::OK;

	RETURN_IF_ERROR(bookmark->GetAttribute(attribute, &attr));
	RETURN_IF_ERROR(attr.GetTextValue(tmp));
	if (tmp.HasContent())
	{
		int len = UriEscape::GetEscapedLength(tmp.CStr(), UriEscape::Ctrl | UriEscape::Equals | UriEscape::Percent);
		uni_char *escaped = OP_NEWA(uni_char, len+1);
		if (!escaped)
			return OpStatus::ERR_NO_MEMORY;
		UriEscape::Escape(escaped, tmp.CStr(), UriEscape::Ctrl | UriEscape::Equals | UriEscape::Percent);
		TRAP(ret, m_bookmark_prefs->WriteStringL(section_name, entry_name, escaped));
		OP_DELETEA(escaped);
	}
	return ret;
}

OP_STATUS BookmarkIniStorage::SaveIntAttribute(BookmarkAttributeType attribute, const uni_char *section_name, const uni_char *entry_name, BookmarkItem *bookmark)
{
	BookmarkAttribute attr;
	OpString tmp;
	OP_STATUS ret = OpStatus::OK;

	RETURN_IF_ERROR(bookmark->GetAttribute(attribute, &attr));
	int int_val = attr.GetIntValue();
	RETURN_IF_ERROR(tmp.AppendFormat(UNI_L("%d"), int_val));
	TRAP(ret, m_bookmark_prefs->WriteStringL(section_name, entry_name, tmp.CStr()));
	return ret;
}

OP_STATUS BookmarkIniStorage::SaveBookmark(BookmarkItem *bookmark)
{
	OpString tmp;
	OP_STATUS ret;
	uni_char section_name[24]; // ARRAY OK 2007-06-26 adame

	if (!m_bookmark_prefs)
	{
		if (m_filename)
			RETURN_IF_ERROR(OpenSave(m_filename, m_folder));
		else
			return OpStatus::ERR;
	}

	if (!bookmark)
		return OpStatus::OK;

	uni_snprintf(section_name, 24, UNI_L("Bookmark %d"), m_current_section);
	m_current_section++;

	RETURN_IF_ERROR(SaveAttribute(BOOKMARK_URL, section_name, UNI_L("URL"), bookmark));
	RETURN_IF_ERROR(SaveAttribute(BOOKMARK_TITLE, section_name, UNI_L("Title"), bookmark));
	RETURN_IF_ERROR(SaveAttribute(BOOKMARK_DESCRIPTION, section_name, UNI_L("Description"), bookmark));
	RETURN_IF_ERROR(SaveAttribute(BOOKMARK_SHORTNAME, section_name, UNI_L("Shortname"), bookmark));
	RETURN_IF_ERROR(SaveAttribute(BOOKMARK_FAVICON_FILE, section_name, UNI_L("Favicon file"), bookmark));
	RETURN_IF_ERROR(SaveAttribute(BOOKMARK_THUMBNAIL_FILE, section_name, UNI_L("Thumbnail file"), bookmark));
	RETURN_IF_ERROR(SaveAttribute(BOOKMARK_CREATED, section_name, UNI_L("Created"), bookmark));
	RETURN_IF_ERROR(SaveAttribute(BOOKMARK_VISITED, section_name, UNI_L("Visited"), bookmark));
	RETURN_IF_ERROR(SaveAttribute(BOOKMARK_TARGET, section_name, UNI_L("Target"), bookmark));

	// int
	BookmarkAttribute* attr;

#ifdef BOOKMARKS_PERSONAL_BAR
	attr = bookmark->GetAttribute(BOOKMARK_PERSONALBAR_POS);
	if (attr && attr->GetIntValue() != -1)
		RETURN_IF_ERROR(SaveIntAttribute(BOOKMARK_PERSONALBAR_POS, section_name, UNI_L("Personalbar pos"), bookmark));

	attr = bookmark->GetAttribute(BOOKMARK_PANEL_POS);
	if (attr && attr->GetIntValue() != -1)
		RETURN_IF_ERROR(SaveIntAttribute(BOOKMARK_PANEL_POS, section_name, UNI_L("Panel pos"), bookmark));
#endif

	attr = bookmark->GetAttribute(BOOKMARK_ACTIVE);
	if (attr && attr->GetIntValue() != FALSE)
		RETURN_IF_ERROR(SaveIntAttribute(BOOKMARK_ACTIVE, section_name, UNI_L("Active"), bookmark));

	attr = bookmark->GetAttribute(BOOKMARK_EXPANDED);
	if (attr && attr->GetIntValue() != FALSE)
		RETURN_IF_ERROR(SaveIntAttribute(BOOKMARK_EXPANDED, section_name, UNI_L("Expanded"), bookmark));

	attr = bookmark->GetAttribute(BOOKMARK_SMALLSCREEN);
	if (attr && attr->GetIntValue() != FALSE)
		RETURN_IF_ERROR(SaveIntAttribute(BOOKMARK_SMALLSCREEN, section_name, UNI_L("Small screen"), bookmark));

#ifdef CORE_SPEED_DIAL_SUPPORT
	if (bookmark->GetParentFolder()->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER)
	{
		RETURN_IF_ERROR(SaveIntAttribute(BOOKMARK_SD_RELOAD_ENABLED, section_name, UNI_L("Reload enabled"), bookmark));
		RETURN_IF_ERROR(SaveIntAttribute(BOOKMARK_SD_RELOAD_INTERVAL, section_name, UNI_L("Reload interval"), bookmark));
		RETURN_IF_ERROR(SaveIntAttribute(BOOKMARK_SD_RELOAD_EXPIRED, section_name, UNI_L("Reload only if expired"), bookmark));
	}
#endif // CORE_SPEED_DIAL_SUPPORT
	if (bookmark->GetFolderType() != FOLDER_NO_FOLDER)
		RETURN_IF_ERROR(SaveTargetFolderValues(section_name, bookmark));

	RETURN_IF_ERROR(tmp.Set(bookmark->GetUniqueId()));
	TRAP(ret, m_bookmark_prefs->WriteStringL(section_name, UNI_L("UUID"), tmp));
	if (OpStatus::IsError(ret))
		return ret;

	switch (bookmark->GetFolderType())
	{
	case FOLDER_NORMAL_FOLDER:
		TRAP(ret, m_bookmark_prefs->WriteStringL(section_name, UNI_L("Folder type"), UNI_L("Normal")));
		if (OpStatus::IsError(ret))
			return ret;
		break;
	case FOLDER_TRASH_FOLDER:
		TRAP(ret, m_bookmark_prefs->WriteStringL(section_name, UNI_L("Folder type"), UNI_L("Trash")));
		if (OpStatus::IsError(ret))
			return ret;
		break;
#ifdef CORE_SPEED_DIAL_SUPPORT
	case FOLDER_SPEED_DIAL_FOLDER:
		TRAP(ret, m_bookmark_prefs->WriteStringL(section_name, UNI_L("Folder type"), UNI_L("SpeedDial")));
		if (OpStatus::IsError(ret))
			return ret;
		break;
#endif // CORE_SPEED_DIAL_SUPPORT
	case FOLDER_SEPARATOR_FOLDER:
		TRAP(ret, m_bookmark_prefs->WriteStringL(section_name, UNI_L("Folder type"), UNI_L("Separator")));
		if (OpStatus::IsError(ret))
			return ret;
		break;
	}

	if (bookmark->GetParentFolder() && bookmark->GetParentFolder() != m_manager->GetRootFolder())
	{
		RETURN_IF_ERROR(tmp.Set(bookmark->GetParentFolder()->GetUniqueId()));
		TRAP(ret, m_bookmark_prefs->WriteStringL(section_name, UNI_L("Parent folder"), tmp));
		if (OpStatus::IsError(ret))
			return ret;
	}

	if (m_listener)
		m_listener->BookmarkIsSaved(bookmark, OpStatus::OK);

	return OpStatus::OK;
}

OP_STATUS BookmarkIniStorage::ClearStorage()
{
	if (!m_bookmark_prefs)
		if (m_filename)
			RETURN_IF_ERROR(OpenSave(m_filename, m_folder));

	if (m_listener)
		m_listener->BookmarkIsSaved(NULL, OpStatus::OK);

	return OpStatus::OK;
}

OP_STATUS BookmarkIniStorage::LoadAttribute(BookmarkAttributeType attribute, const PrefsSection *section, const uni_char *entry, BookmarkItem *bookmark)
{
	BookmarkAttribute attr;
	const uni_char *value = NULL;

	value = section->Get(entry);
	if (value)
	{
		int len = uni_strlen(value);
		uni_char *unescaped = OP_NEWA(uni_char, len+1);
		if (!unescaped)
			return OpStatus::ERR_NO_MEMORY;
		UriUnescape::Unescape(unescaped, value, UriUnescape::All);

		OP_STATUS res = attr.SetTextValue(unescaped);
		OP_DELETEA(unescaped);
		RETURN_IF_ERROR(res);
		RETURN_IF_ERROR(bookmark->SetAttribute(attribute, &attr));
	}

	return OpStatus::OK;
}

OP_STATUS BookmarkIniStorage::LoadIntAttribute(BookmarkAttributeType attribute, const PrefsSection *section, const uni_char *entry, BookmarkItem *bookmark)
{
	BookmarkAttribute attr;
	const uni_char *value = NULL;

	value = section->Get(entry);
	if (value)
	{
		int int_val = uni_atoi(value);
		attr.SetIntValue(int_val);
		RETURN_IF_ERROR(bookmark->SetAttribute(attribute, &attr));
	}

	return OpStatus::OK;
}

OP_STATUS BookmarkIniStorage::LoadBookmark(BookmarkItem *bookmark)
{
	PrefsSection * OP_MEMORY_VAR section=NULL;
	OpStringC section_name;
	OpStringC parent_folder_id, folder_type, uuid;
	BookmarkAttribute attr;
	OP_STATUS ret;

	if (!m_bookmark_prefs)
	{
		if (m_filename)
			RETURN_IF_ERROR(OpenLoad(m_filename, m_folder));
		else
			return OpStatus::ERR;
	}

	if (m_current_section >= m_sections.Count())
		return OpStatus::ERR;
	section_name = m_sections.Item(m_current_section);
	m_current_section++;

	TRAP_AND_RETURN(ret, (section = m_bookmark_prefs->ReadSectionL(section_name.CStr())));
	OpAutoPtr<PrefsSection> ap_sec(section);

	if (section)
	{
		ret = LoadAttribute(BOOKMARK_URL, section, UNI_L("URL"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadAttribute(BOOKMARK_TITLE, section, UNI_L("Title"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadAttribute(BOOKMARK_DESCRIPTION, section, UNI_L("Description"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadAttribute(BOOKMARK_SHORTNAME, section, UNI_L("Shortname"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadAttribute(BOOKMARK_FAVICON_FILE, section, UNI_L("Favicon file"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadAttribute(BOOKMARK_THUMBNAIL_FILE, section, UNI_L("Thumbnail file"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadAttribute(BOOKMARK_CREATED, section, UNI_L("Created"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadAttribute(BOOKMARK_VISITED, section, UNI_L("Visited"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadAttribute(BOOKMARK_TARGET, section, UNI_L("Target"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
#ifdef BOOKMARKS_PERSONAL_BAR
		ret = LoadIntAttribute(BOOKMARK_PERSONALBAR_POS, section, UNI_L("Personalbar pos"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadIntAttribute(BOOKMARK_PANEL_POS, section, UNI_L("Panel pos"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
#endif
		ret = LoadIntAttribute(BOOKMARK_ACTIVE, section, UNI_L("Active"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadIntAttribute(BOOKMARK_EXPANDED, section, UNI_L("Expanded"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
		ret = LoadIntAttribute(BOOKMARK_SMALLSCREEN, section, UNI_L("Small screen"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);

#ifdef CORE_SPEED_DIAL_SUPPORT
		ret = LoadIntAttribute(BOOKMARK_SD_RELOAD_ENABLED, section, UNI_L("Reload enabled"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);

		ret = LoadIntAttribute(BOOKMARK_SD_RELOAD_INTERVAL, section, UNI_L("Reload interval"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);

		ret = LoadIntAttribute(BOOKMARK_SD_RELOAD_EXPIRED, section, UNI_L("Reload only if expired"), bookmark);
		if (ret == OpStatus::ERR_OUT_OF_RANGE)
			m_index++;
		RETURN_IF_ERROR(ret);
#endif // CORE_SPEED_DIAL_SUPPORT

		TRAP_AND_RETURN(ret, (uuid = m_bookmark_prefs->ReadStringL(section_name, UNI_L("UUID"), UNI_L(""))));

		if (uuid.CStr())
		{
			BOOL old_format = TRUE;
			const uni_char *tmp = uuid.CStr();
			// Check if uid is in format with dashes
			unsigned i;
			if (uni_strlen(tmp) != 36)
				old_format = FALSE;

			for (i=0; i<36 && old_format; i++)
			{
				if (i == 8 || i == 13 || i == 18 || i == 23)
				{
					if (tmp[i] != '-')
						old_format = FALSE;
				}
				else if (!uni_isxdigit(tmp[i]) || !uni_isdigit(tmp[i]) && !uni_islower(tmp[i]))
					old_format = FALSE;
			}

			uni_char *uid = OP_NEWA(uni_char, uuid.Length() + 1);
			if (!uid)
				return OpStatus::ERR_NO_MEMORY;

			uni_strcpy(uid, uuid.CStr());
			if (old_format)
			{
				// Remove all dashes and make all letters uppercase.
				int offset=0, i;
				for (i=0; i<36; i++)
				{
					uid[i-offset] = Unicode::ToUpper(uid[i]);
					if (uid[i] == '-')
						offset++;
				}
				uid[32] = 0;
			}
			bookmark->SetUniqueId(uid);
		}
		else
		{
			m_index++;
			return OpStatus::ERR_OUT_OF_RANGE;
		}

		TRAP_AND_RETURN(ret, (folder_type = m_bookmark_prefs->ReadStringL(section_name, UNI_L("Folder type"), UNI_L(""))));

		if (folder_type.HasContent())
		{
			if (uni_stricmp(folder_type.CStr(), "normal") == 0)
				bookmark->SetFolderType(FOLDER_NORMAL_FOLDER);
			else if (uni_stricmp(folder_type.CStr(), "trash") == 0)
				bookmark->SetFolderType(FOLDER_TRASH_FOLDER);
#ifdef CORE_SPEED_DIAL_SUPPORT
			else if (uni_stricmp(folder_type.CStr(), "speeddial") == 0)
				bookmark->SetFolderType(FOLDER_SPEED_DIAL_FOLDER);
#endif // CORE_SPEED_DIAL_SUPPORT
			else if (uni_stricmp(folder_type.CStr(), "separator") == 0)
				bookmark->SetFolderType(FOLDER_SEPARATOR_FOLDER);
			else
				bookmark->SetFolderType(FOLDER_NORMAL_FOLDER);

			LoadTargetFolderValues(section, bookmark);
		}
		else if (folder_type.CStr())
			bookmark->SetFolderType(FOLDER_NO_FOLDER);
		else
		{
			m_index++;
			return OpStatus::ERR_OUT_OF_RANGE;
		}


		TRAP_AND_RETURN(ret, (parent_folder_id = m_bookmark_prefs->ReadStringL(section_name, UNI_L("Parent folder"), UNI_L(""))));

		if (parent_folder_id.HasContent())
		{
			BOOL old_format = TRUE;
			const uni_char *tmp = parent_folder_id.CStr();
			// Check if uid is in format with dashes
			unsigned i;
			if (uni_strlen(tmp) != 36)
				old_format = FALSE;

			for (i=0; i<36 && old_format; i++)
			{
				if (i == 8 || i == 13 || i == 18 || i == 23)
				{
					if (tmp[i] != '-')
						old_format = FALSE;
				}
				else if (!uni_isxdigit(tmp[i]) || !uni_isdigit(tmp[i]) && !uni_islower(tmp[i]))
					old_format = FALSE;
			}

			uni_char *uid = OP_NEWA(uni_char, uni_strlen(parent_folder_id.CStr()) + 1);
			RETURN_OOM_IF_NULL(uid);
			uni_strcpy(uid, parent_folder_id.CStr());
			if (old_format)
			{
				// Remove all dashes and make all letters uppercase.
				int offset=0, i;
				for (i=0; i<36; i++)
				{
					uid[i-offset] = Unicode::ToUpper(uid[i]);
					if (uid[i] == '-')
						offset++;
				}
				uid[32] = 0;
			}

            // Find out which parent has which id.
			BookmarkItem *parent = m_manager->FindId(uid);
			BookmarkListElm *elm = FetchParentFromPool(uid);
			if (!parent && !elm)
			{
                // Parent has not been read in yet, put the child on hold.
				bookmark->SetParentUniqueId(uid);
				elm = OP_NEW(BookmarkListElm, ());
				if (!elm)
					return OpStatus::ERR_NO_MEMORY;

				elm->SetBookmark(bookmark);
				AddToPool(elm);
			}
			else if (!parent && elm)
			{
				OP_DELETEA(uid);

#ifdef SUPPORT_DATA_SYNC
				bookmark->SetAdded(TRUE);
#endif // SUPPORT_DATA_SYNC
				ret = m_manager->AddBookmark(bookmark, elm->GetBookmark());
				if (ret == OpStatus::ERR_OUT_OF_RANGE)
					m_index++;
				else if (ret == OpStatus::ERR)
					ret = OpStatus::OK; // Invalid bookmark, just ignore.
				RETURN_IF_ERROR(ret);

			}
			else
			{
				OP_DELETEA(uid);

#ifdef SUPPORT_DATA_SYNC
				bookmark->SetAdded(TRUE);
#endif // SUPPORT_DATA_SYNC
				ret = m_manager->AddBookmark(bookmark, parent);
				if (ret == OpStatus::ERR_OUT_OF_RANGE)
					m_index++;
				else if (ret == OpStatus::ERR)
					ret = OpStatus::OK; // Invalid bookmark, just ignore.
				RETURN_IF_ERROR(ret);

			}
		}
		else
		{
#ifdef SUPPORT_DATA_SYNC
			bookmark->SetAdded(TRUE);
#endif // SUPPORT_DATA_SYNC
			ret = m_manager->AddBookmark(bookmark, m_manager->GetRootFolder());
			if (ret == OpStatus::ERR_OUT_OF_RANGE)
				m_index++;
			else if (ret == OpStatus::ERR)
				ret = OpStatus::OK; // Invalid bookmark, just ignore.
			RETURN_IF_ERROR(ret);
		}

		if (bookmark->GetFolderType() != FOLDER_NO_FOLDER)
		{
			BookmarkListElm *elm;
			while ((elm = FetchFromPool(bookmark->GetUniqueId())))
			{
				BookmarkItem *item = elm->GetBookmark();
#ifdef SUPPORT_DATA_SYNC
				item->SetAdded(TRUE);
#endif // SUPPORT_DATA_SYNC
				ret = m_manager->AddBookmark(item, bookmark);
				RemoveFromPool(bookmark->GetUniqueId());
				if (OpStatus::IsError(ret))
				{
					OP_DELETE(item);
					if (ret != OpStatus::ERR_OUT_OF_RANGE)
						return ret;
				}
			}
		}
	}

	m_index++;
	if (m_listener)
		m_listener->BookmarkIsLoaded(bookmark, OpStatus::OK);

	return OpStatus::OK;
}

OP_STATUS BookmarkIniStorage::FolderBegin(BookmarkItem *folder)
{
	if (folder->GetFolderType() == FOLDER_NO_FOLDER)
		return OpStatus::ERR;

	m_current_folder = folder;

	return OpStatus::OK;
}

OP_STATUS BookmarkIniStorage::FolderEnd(BookmarkItem *folder)
{
	m_current_folder = folder->GetParentFolder();
	return OpStatus::OK;
}

BOOL BookmarkIniStorage::MoreBookmarks()
{
	if (!m_bookmark_prefs)
	{
		m_index = 0;
		if (m_filename)
			OpenLoad(m_filename, m_folder);
		else
			return FALSE;
	}

	return m_sections.Count() - m_index ? TRUE : FALSE;
}

void BookmarkIniStorage::RegisterListener(BookmarkStorageListener *l)
{
	m_listener = l;
}

void BookmarkIniStorage::UnRegisterListener(BookmarkStorageListener *l)
{
	if (m_listener != l)
		return;
	m_listener = NULL;
}

void BookmarkIniStorage::AddToPool(BookmarkListElm *elm)
{
	elm->Into(&m_pool);
}

void BookmarkIniStorage::RemoveFromPool(uni_char *uid)
{
	BookmarkListElm *elm = FetchFromPool(uid);
	if (elm)
	{
		elm->Out();
		OP_DELETE(elm);
	}
}

BookmarkListElm* BookmarkIniStorage::FetchFromPool(uni_char *uid)
{
	BookmarkListElm *elm;
	uni_char *puid;

	if (!uid)
		return NULL;

	for (elm = (BookmarkListElm*) m_pool.First(); elm; elm = (BookmarkListElm*) elm->Suc())
	{
		puid = elm->GetBookmark()->GetParentUniqueId();
		if (puid && uni_strcmp(puid, uid) == 0)
			return elm;
	}
	return NULL;
}

BookmarkListElm* BookmarkIniStorage::FetchParentFromPool(uni_char *puid)
{
	BookmarkListElm *elm;
	uni_char *uid;

	if (!puid)
		return NULL;

	for (elm = (BookmarkListElm*) m_pool.First(); elm; elm = (BookmarkListElm*) elm->Suc())
	{
		uid = elm->GetBookmark()->GetUniqueId();
		if (uid && uni_strcmp(uid, puid) == 0)
			return elm;
	}
	return NULL;
}

OP_STATUS BookmarkIniStorage::SaveTargetFolderValues(const uni_char *section_name, BookmarkItem *bookmark)
{
	OpString tmp;

	unsigned int max_count = bookmark->GetMaxCount();
	if (max_count != ~0u)
	{
		RETURN_IF_ERROR(tmp.AppendFormat(UNI_L("%u"), max_count));
		RETURN_IF_LEAVE(m_bookmark_prefs->WriteStringL(section_name, UNI_L("Max count"), tmp.CStr()));
		tmp.Empty();
	}

	BOOL flag = bookmark->SubFoldersAllowed();
	if (!flag)
	{
		RETURN_IF_ERROR(tmp.AppendFormat(UNI_L("%u"), flag));
		RETURN_IF_LEAVE(m_bookmark_prefs->WriteStringL(section_name, UNI_L("Subfolders allowed"), tmp.CStr()));
		tmp.Empty();
	}

	flag = bookmark->Deletable();
	if (!flag)
	{
		RETURN_IF_ERROR(tmp.AppendFormat(UNI_L("%u"), flag));
		RETURN_IF_LEAVE(m_bookmark_prefs->WriteStringL(section_name, UNI_L("Deletable"), tmp.CStr()));
		tmp.Empty();
	}

	flag = bookmark->MoveIsCopy();
	if (flag)
	{
		RETURN_IF_ERROR(tmp.AppendFormat(UNI_L("%u"), flag));
		RETURN_IF_LEAVE(m_bookmark_prefs->WriteStringL(section_name, UNI_L("Move is copy"), tmp.CStr()));
		tmp.Empty();
	}

	flag = bookmark->SeparatorsAllowed();
	if (!flag)
	{
		RETURN_IF_ERROR(tmp.AppendFormat(UNI_L("%u"), flag));
		RETURN_IF_LEAVE(m_bookmark_prefs->WriteStringL(section_name, UNI_L("Separators allowed"), tmp.CStr()));
		tmp.Empty();
	}

	return OpStatus::OK;
}

void BookmarkIniStorage::LoadTargetFolderValues(const PrefsSection *section, BookmarkItem *bookmark)
{
	BOOL flag;
	const uni_char *value;

	value = section->Get(UNI_L("Max count"));
	if (value)
	{
		unsigned int max_count = uni_atoi(value);
		bookmark->SetMaxCount(max_count);
	}

	value = section->Get(UNI_L("Subfolders allowed"));
	if (value)
	{
		flag = uni_atoi(value) != 0;
		bookmark->SetSubFoldersAllowed(flag);
	}

	value = section->Get(UNI_L("Deletable"));
	if (value)
	{
		flag = uni_atoi(value) != 0;
		bookmark->SetDeletable(flag);
	}

	value = section->Get(UNI_L("Move is copy"));
	if (value)
	{
		flag = uni_atoi(value) != 0;
		bookmark->SetMoveIsCopy(flag);
	}

	value = section->Get(UNI_L("Separators allowed"));
	if (value)
	{
		flag = uni_atoi(value) != 0;
		bookmark->SetSeparatorsAllowed(flag);
	}
}

#endif // CORE_BOOKMARKS_SUPPORT
