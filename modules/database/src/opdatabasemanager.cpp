/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#include "modules/database/src/opdatabase_base.h"

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

#include "modules/database/opdatabasemanager.h"
#include "modules/database/ps_commander.h"
#include "modules/database/prefscollectiondatabase.h"
#include "modules/database/opdatabase.h"
#include "modules/database/src/webstorage_data_abstraction_simple_impl.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opstrlst.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/formats/base64_decode.h"

#ifdef EXTENSION_SUPPORT
# include "modules/gadgets/OpGadgetManager.h"
#endif //  EXTENSION_SUPPORT

const uni_char g_ps_memory_file_name[] = {':','m','e','m','o','r','y',':',0};

PS_IndexIterator::PS_IndexIterator(PS_Manager* mgr, URL_CONTEXT_ID context_id,
		PS_ObjectType use_case_type, const uni_char* origin,
		BOOL only_persistent, IterationOrder order) :
	m_order_type(order),
	m_psobj_type(use_case_type),
	m_origin(origin),
	m_context_id(context_id),
	m_last_mod_count(0),
	m_1st_lvl_index(0),
	m_2nd_lvl_index(0),
	m_3rd_lvl_index(0),
	m_sorted_index(NULL),
	m_mgr(mgr)
{
	if (only_persistent)
		SetFlag(PERSISTENT_DB_ITERATOR);
	OP_ASSERT(mgr != NULL);
}

PS_IndexIterator::~PS_IndexIterator()
{
	UnsetFlag(OBJ_INITIALIZED | OBJ_INITIALIZED_ERROR);
	if (m_sorted_index != NULL)
	{
		OP_DELETE(m_sorted_index);
		m_sorted_index = NULL;
	}
	m_mgr = NULL;
}

void PS_IndexIterator::ResetL()
{
	OP_ASSERT(m_mgr != NULL);
	m_last_mod_count = m_mgr->GetModCount();

	if (IsOrdered())
		m_1st_lvl_index = 0;
	else
		m_1st_lvl_index = (m_psobj_type != KDBTypeStart ? m_psobj_type : KDBTypeStart + 1);
	m_2nd_lvl_index = 0;
	m_3rd_lvl_index = 0;
	if (PeekCurrentL() == NULL)
		MoveNextL();
}

void PS_IndexIterator::BuildL()
{
	DB_ASSERT_RET(!IsInitialized(),);

	SetFlag(OBJ_INITIALIZED);

	if (IsOrdered())
	{
		OP_ASSERT(m_mgr != NULL);
		OP_ASSERT(!m_sorted_index);
		//build sorted stuff
		OpStackAutoPtr<OpVector<PS_IndexEntry> > sorted_index_ptr(OP_NEW_L(OpVector<PS_IndexEntry>, ()));
		OpStackAutoPtr<PS_IndexIterator> unordered_iterator_ptr(m_mgr->GetIteratorL(
				m_context_id, m_psobj_type, GetFlag(PERSISTENT_DB_ITERATOR), UNORDERED));
		OP_ASSERT(unordered_iterator_ptr.get());

		OP_STATUS status;
		if (!unordered_iterator_ptr->AtEndL())
		{
			do {
				PS_IndexEntry* elem = unordered_iterator_ptr->GetItemL();
				OP_ASSERT(elem != NULL);
				if (GetFlag(PERSISTENT_DB_ITERATOR) && elem->IsMemoryOnly())
					continue;
				OP_ASSERT(m_context_id == DB_NULL_CONTEXT_ID || m_context_id == elem->GetUrlContextId());
				if (m_origin != NULL &&
					!OpDbUtils::StringsEqual(m_origin, elem->GetOrigin()))
					continue;

				status = sorted_index_ptr->Add(elem);
				if (OpStatus::IsError(status))
				{
					m_order_type = UNORDERED;//not ordered, collection is empty
					m_sorted_index = NULL;
					LEAVE(SIGNAL_OP_STATUS_ERROR(status));
				}
			} while(unordered_iterator_ptr->MoveNextL());

			sorted_index_ptr->QSort(m_order_type == ORDERED_ASCENDING ? PS_IndexEntry::CompareAsc : PS_IndexEntry::CompareDesc);
			m_sorted_index = sorted_index_ptr.release();
		}
		else
		{
			m_order_type = UNORDERED;
			m_1st_lvl_index = KDBTypeEnd;//optimization to prevent doing the full array loop
		}
		//else //no items ?? bah !
	}
	ResetL();
}

void PS_IndexIterator::ReSync()
{
	if (m_last_mod_count != m_mgr->GetModCount())
	{
		m_last_mod_count = m_mgr->GetModCount();
		if (PeekCurrentL() == NULL)
			MoveNextL();
	}
}

BOOL PS_IndexIterator::MoveNextL()
{
	DB_ASSERT_LEAVE(IsInitialized(), PS_Status::ERR_INVALID_STATE);

	//if the manager mutates, then the iterator becomes invalid
	if (IsDesynchronized())
		LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_OUT_OF_RANGE));

	if (AtEndL())
		return FALSE;

	if (IsOrdered())
	{
		do
		{
			m_1st_lvl_index++;
		} while(PeekCurrentL() == NULL && !AtEndL());
	}
	else
	{
		OP_ASSERT(m_mgr != NULL);
		PS_Manager::IndexByContext* mgr_data = m_mgr->GetIndex(m_context_id);
		if (mgr_data != NULL)
		{
			PS_Manager::IndexEntryByOriginHash** const* db_index = mgr_data->m_object_index;
			do {
				//assert ensures that the array cannot be empty, else it should have been deleted
				OP_ASSERT(db_index[m_1st_lvl_index] == NULL ||
						db_index[m_1st_lvl_index][m_2nd_lvl_index] == NULL ||
						db_index[m_1st_lvl_index][m_2nd_lvl_index]->m_objects.GetCount() != 0);

				if (db_index[m_1st_lvl_index] == NULL ||
					db_index[m_1st_lvl_index][m_2nd_lvl_index] == NULL ||
					db_index[m_1st_lvl_index][m_2nd_lvl_index]->m_objects.GetCount() == 0 ||
					db_index[m_1st_lvl_index][m_2nd_lvl_index]->m_objects.GetCount()-1 <= m_3rd_lvl_index)
				{
					if (PS_Manager::m_max_hash_amount - 1 <= m_2nd_lvl_index) {
						m_1st_lvl_index = (m_psobj_type!=KDBTypeStart ? (unsigned)KDBTypeEnd : m_1st_lvl_index + 1);
						m_2nd_lvl_index = 0;
					}
					else
					{
						m_2nd_lvl_index++;
					}
					m_3rd_lvl_index = 0;
				}
				else
				{
					m_3rd_lvl_index++;
				}
			} while(PeekCurrentL() == NULL && !AtEndL());
		}
	}
	return !AtEndL();
}

BOOL PS_IndexIterator::AtEndL() const
{
	DB_ASSERT_LEAVE(IsInitialized(), PS_Status::ERR_INVALID_STATE);

	//if the manager mutates, then the iterator becomes invalid
	if (IsDesynchronized())
		LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_OUT_OF_RANGE));

	return IsOrdered() ?
		m_sorted_index->GetCount() <= m_1st_lvl_index :
		m_1st_lvl_index == KDBTypeEnd;
}

PS_IndexEntry* PS_IndexIterator::GetItemL() const
{
	DB_ASSERT_LEAVE(IsInitialized(), PS_Status::ERR_INVALID_STATE);

	//if the manager mutates, then the iterator becomes invalid
	if (IsDesynchronized())
		LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_OUT_OF_RANGE));

	if (AtEndL())
		return NULL;

	if (IsOrdered())
	{
		return m_sorted_index->Get(m_1st_lvl_index);
	}
	else
	{
		PS_Manager::IndexByContext* mgr_data = m_mgr->GetIndex(m_context_id);
		OP_ASSERT(mgr_data != NULL);

		//this method is called outside of the object, so we must ensure
		//the iterator is pointing to a valid object, because
		//multiple calls to GetItem should be as performant as possible
		OP_ASSERT(mgr_data->m_object_index[m_1st_lvl_index] != NULL);
		OP_ASSERT(mgr_data->m_object_index[m_1st_lvl_index][m_2nd_lvl_index] != NULL);
		OP_ASSERT(mgr_data->m_object_index[m_1st_lvl_index][m_2nd_lvl_index]->m_objects.GetCount() > m_3rd_lvl_index);

		return mgr_data->m_object_index[m_1st_lvl_index][m_2nd_lvl_index]->m_objects.Get(m_3rd_lvl_index);
	}
}

BOOL PS_IndexIterator::IsDesynchronized() const
{
	OP_ASSERT(m_last_mod_count == m_mgr->GetModCount());//this is the sort of stuff that shouldn't happen
	return m_last_mod_count != m_mgr->GetModCount();
}

PS_IndexEntry* PS_IndexIterator::PeekCurrentL() const
{
	if (IsDesynchronized())
		LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_OUT_OF_RANGE));

	if (AtEndL())
		return NULL;

	if (IsOrdered())
	{
		return m_sorted_index->Get(m_1st_lvl_index);
	}
	else
	{
		//this method is called internally to check if the iterator is pointing to a valid
		//object, hence the extra checks

		PS_Manager::IndexByContext* mgr_data = m_mgr->GetIndex(m_context_id);
		if (mgr_data != NULL)
		{
			PS_Manager::IndexEntryByOriginHash** const* db_index = mgr_data->m_object_index;
			if (db_index[m_1st_lvl_index] != NULL)
				if (db_index[m_1st_lvl_index][m_2nd_lvl_index] != NULL)
					if (db_index[m_1st_lvl_index][m_2nd_lvl_index]->m_objects.GetCount() > m_3rd_lvl_index)
					{
						PS_IndexEntry* elem = db_index[m_1st_lvl_index][m_2nd_lvl_index]->m_objects.Get(m_3rd_lvl_index);
						if (GetFlag(PERSISTENT_DB_ITERATOR) && elem->IsMemoryOnly())
							return NULL;
						OP_ASSERT(m_context_id == DB_NULL_CONTEXT_ID || m_context_id == elem->GetUrlContextId());
						if (m_origin != NULL &&
							!OpDbUtils::StringsEqual(m_origin, elem->GetOrigin()))
							return NULL;
						return elem;
					}
		}
	}
	return NULL;
}



/*************
 * PS_DataFile
 *************/

#define ENSURE_PATHSEPCHAR(buffer) do { \
	if (buffer.Length()>0 && buffer.GetStorage()[buffer.Length()-1]!=PATHSEPCHAR) \
		RETURN_IF_ERROR(buffer.Append(PATHSEPCHAR)); \
} while(0)

OP_STATUS
PS_DataFile::MakeAbsFilePath()
{
	INTEGRITY_CHECK();
	// Already have an absolute path. Just quit.
	if (m_file_abs_path != NULL)
		return OpStatus::OK;

	OP_ASSERT(m_index_entry != NULL);

	const uni_char* profile_folder = m_index_entry->GetPolicyAttribute(PS_Policy::KMainFolderPath);
	RETURN_OOM_IF_NULL(profile_folder);
	if (*profile_folder == 0)
		return OpStatus::ERR_NO_ACCESS;

	unsigned profile_folder_length = profile_folder != NULL ? uni_strlen(profile_folder) : 0;

	uni_char *file_abs_path = NULL, *file_rel_path = NULL;
	unsigned file_path_length = 0;

	if (m_file_rel_path != NULL && OpDbUtils::IsFilePathAbsolute(m_file_rel_path))
	{
		// This file path is actually absolute, so we have everything we need. Skip ahead.
		file_path_length = uni_strlen(m_file_rel_path);
		file_abs_path = const_cast<uni_char*>(m_file_rel_path);
		file_rel_path = const_cast<uni_char*>(m_file_rel_path);
	}
	else
	{
		TempBuffer buffer;
		buffer.Clear();
		buffer.SetCachedLengthPolicy(TempBuffer::TRUSTED);

		uni_char folder_name[9] = {0}; /* ARRAY OK joaoe 2011-10-17 */
		unsigned folder_number;

		if (profile_folder != NULL)
		{
			RETURN_IF_ERROR(buffer.Append(profile_folder));
			ENSURE_PATHSEPCHAR(buffer);
		}

		if (m_file_rel_path != NULL)
		{
			// We already have a relative file path to the storage folder
			// which was read from the index file. Now we just need to resolve
			// it to an absolute path.
			RETURN_IF_ERROR(buffer.Append(m_file_rel_path));
			OP_DELETEA(const_cast<uni_char*>(m_file_rel_path));
			m_file_rel_path = NULL;
		}
		else
		{
			// No relative path means we need to look up a new file name
			// that isn't occupied using the the following form
			// * path_to_profile_folder/subfolder_if_any/xx/xx/xxxxxxxx
			// Verbose:
			// * path_to_profile_folder/subfolder_if_any/type_in_hex_2_chars/origin_hash_2_chars/data_file_name_8_chars_number_hex

			RETURN_IF_ERROR(buffer.Append(m_index_entry->GetPolicyAttribute(PS_Policy::KSubFolder)));
			ENSURE_PATHSEPCHAR(buffer);

			folder_number = 0xff & m_index_entry->GetType();
			uni_snprintf(folder_name, 9, UNI_L("%0.2X%c"), folder_number, PATHSEPCHAR);
			RETURN_IF_ERROR(buffer.Append(folder_name, 3));
			ENSURE_PATHSEPCHAR(buffer);

			folder_number = 0xff & PS_Manager::HashOrigin(m_index_entry->GetOrigin());
			uni_snprintf(folder_name, 9, UNI_L("%0.2X%c"), folder_number, PATHSEPCHAR);
			RETURN_IF_ERROR(buffer.Append(folder_name, 3));
			ENSURE_PATHSEPCHAR(buffer);

			//calculate new file name using sequential numbering
			unsigned current_length = buffer.Length();
			buffer.SetCachedLengthPolicy(TempBuffer::UNTRUSTED);
			OpFile test_file;
			BOOL file_exists = FALSE;

			RETURN_IF_ERROR(m_index_entry->GetPSManager()->MakeIndex(TRUE, m_index_entry->GetUrlContextId()));

			PS_Manager::IndexByContext* mgr_data = m_index_entry->GetIndex();
			OP_ASSERT(mgr_data != NULL);
			OP_ASSERT(mgr_data->m_object_index[m_index_entry->GetType()] != NULL);
			OP_ASSERT(mgr_data->m_object_index[m_index_entry->GetType()][PS_Manager::HashOrigin(m_index_entry->GetOrigin())] != NULL);

			unsigned& m_highest_filename_number = mgr_data->
				m_object_index[m_index_entry->GetType()][PS_Manager::HashOrigin(m_index_entry->GetOrigin())]->m_highest_filename_number;

			// n_bad_tries tells the number of failed file accesses -
			// like permissions failed or device not available
			// n_good_tries tells when the calculated file name was being
			// used by another file. However, in a normal situation this
			// is unlikely to matter much because the code keeps track of
			// the last file name and finds the one immediately after.
			// This is useful if there are leftover files on the file system
			// which are not in the index file.
			unsigned n_bad_tries = 0, n_bad_tries_limit = 10;
			unsigned n_good_tries = 0, n_good_tries_limit = 10000;
			BOOL ok;
			for (ok = TRUE; ok; ok = (n_bad_tries < n_bad_tries_limit && n_good_tries < n_good_tries_limit))
			{
				folder_number = m_highest_filename_number;
				uni_snprintf(folder_name, 9, UNI_L("%0.8X"), folder_number);
				RETURN_IF_ERROR(buffer.Append(folder_name));

				// This will rollover from 0xffffffff to 0 and that's intended.
				m_highest_filename_number++;
				RETURN_IF_ERROR(test_file.Construct(buffer.GetStorage()));
				if (OpStatus::IsSuccess(test_file.Exists(file_exists)))
				{
					DB_DBG(("File exists ? %s, %S\n", DB_DBG_BOOL(file_exists), buffer.GetStorage()));
					if (!file_exists)
						break;
				}
				else
				{
					n_bad_tries++;
					DB_DBG(("Failed to access file: %S\n", buffer.GetStorage()));
				}
				n_good_tries++;
				buffer.GetStorage()[current_length] = 0;
			}
			if (ok)
			{
				SetBogus(FALSE);
			}
			else
			{
				//whoops - can't access folder
				m_file_abs_path = NULL;
				m_file_rel_path = NULL;
				SetBogus(TRUE);
				return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_ACCESS);
			}
		}
		file_path_length = buffer.Length();

		file_abs_path = OP_NEWA(uni_char, file_path_length + 1);
		RETURN_OOM_IF_NULL(file_abs_path);

		uni_strncpy(file_abs_path, buffer.GetStorage(), file_path_length);
		file_abs_path[file_path_length] = 0;
		file_rel_path = file_abs_path;
	}
	OP_ASSERT(file_abs_path != NULL);
	OP_ASSERT(file_rel_path != NULL);

	if (profile_folder_length < file_path_length && profile_folder_length != 0)
	{
		if (uni_strncmp(file_abs_path, profile_folder, profile_folder_length) == 0)
		{
			file_rel_path = file_abs_path + profile_folder_length;
			if (file_rel_path[0] == PATHSEPCHAR)
				file_rel_path++;
		}
	}
	m_file_abs_path = file_abs_path;
	m_file_rel_path = file_rel_path;

	return InitFileObj();
}

OP_STATUS
PS_DataFile::EnsureDataFileFolder()
{
	INTEGRITY_CHECK();
	if (m_file_abs_path == NULL)
		RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(MakeAbsFilePath()));

	OP_ASSERT(m_file_abs_path != NULL);

	const uni_char* path_sep_needle = uni_strrchr(m_file_abs_path, PATHSEPCHAR);
	if (path_sep_needle != NULL)
	{
		uni_char file_abs_path_copy[_MAX_PATH+1]; /* ARRAY OK joaoe 2010-05-25 */
		unsigned file_abs_path_copy_length = (unsigned)MIN(path_sep_needle - m_file_abs_path, _MAX_PATH);

		uni_strncpy(file_abs_path_copy, m_file_abs_path, file_abs_path_copy_length);
		file_abs_path_copy[file_abs_path_copy_length] = 0;

		if (*file_abs_path_copy)
		{
			OpFile file;
			RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(file.Construct(file_abs_path_copy)));
			RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(file.MakeDirectory()));
		}
	}

	return OpStatus::OK;
}

PS_DataFile::PS_DataFile(PS_IndexEntry* index_entry,
			const uni_char* file_abs_path,
			const uni_char* file_rel_path)
		: m_index_entry(index_entry)
		, m_file_abs_path(file_abs_path)
		, m_file_rel_path(file_rel_path)
		, m_bogus_file(FALSE)
		, m_should_delete(FALSE)
{
	DB_DBG_CHK(index_entry, ("%p: PS_DataFile::PS_DataFile(" DB_DBG_ENTRY_STR "): %d\n", this, DB_DBG_ENTRY_O(index_entry), GetRefCount()))
}

PS_DataFile::~PS_DataFile()
{
	DB_DBG_CHK(m_index_entry, ("%p: PS_DataFile::~PS_DataFile(" DB_DBG_ENTRY_STR "): %d\n", this, DB_DBG_ENTRY_O(m_index_entry), GetRefCount()))
	INTEGRITY_CHECK();
	OP_ASSERT(GetRefCount() == 0);
	if (m_file_abs_path != NULL && m_file_abs_path != g_ps_memory_file_name)
	{
		OP_DELETEA(const_cast<uni_char*>(m_file_abs_path));
	}
	else if (m_file_rel_path != NULL && m_file_rel_path != g_ps_memory_file_name)
	{
		OP_DELETEA(const_cast<uni_char*>(m_file_rel_path));
	}
	m_file_abs_path = m_file_rel_path = NULL;
	if (m_index_entry != NULL && m_index_entry->m_data_file_name == this)
	{
		m_index_entry->m_data_file_name = NULL;
	}
}

OP_STATUS
PS_DataFile::InitFileObj()
{
	OP_ASSERT(m_file_abs_path == g_ps_memory_file_name ||
			uni_strstr(m_file_abs_path, g_ps_memory_file_name) == NULL);

	if (m_file_abs_path != NULL && m_file_abs_path != g_ps_memory_file_name)
		return m_file_obj.Construct(m_file_abs_path);

	return OpStatus::OK;
}

/*static*/
OP_STATUS
PS_DataFile::Create(PS_IndexEntry* index_entry, const uni_char* file_rel_path)
{
	OP_ASSERT(index_entry != NULL);
	OP_ASSERT(index_entry->m_data_file_name == NULL);

	PS_DataFile* new_dfn;

	if (index_entry->IsMemoryOnly())
	{
		new_dfn = &(index_entry->GetPSManager()->m_non_persistent_file_name_obj);
	}
	else
	{
		OP_ASSERT(file_rel_path == NULL || !OpDbUtils::StringsEqual(file_rel_path, g_ps_memory_file_name));

		uni_char* file_rel_path_dupe = NULL;
		if (file_rel_path !=  NULL)
		{
			file_rel_path_dupe = UniSetNewStr(file_rel_path);
			RETURN_OOM_IF_NULL(file_rel_path_dupe);
		}
		new_dfn = OP_NEW(PS_DataFile,(index_entry, NULL, file_rel_path_dupe));
		if (new_dfn == NULL)
		{
			OP_DELETEA(file_rel_path_dupe);
			return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);
		}
	}

	OP_ASSERT(index_entry->m_data_file_name == NULL);
	index_entry->m_data_file_name = new_dfn;
	new_dfn->IncRefCount();
	PS_DataFile_AddOwner(new_dfn, index_entry)

	return OpStatus::OK;
}

void
PS_DataFile::SafeDelete()
{
	DB_DBG_CHK(m_index_entry, ("%p: PS_DataFile::SafeDelete (" DB_DBG_ENTRY_STR "): %d\n", this, DB_DBG_ENTRY_O(m_index_entry), GetRefCount()))

	INTEGRITY_CHECK();
	if (GetRefCount() > 0)
		return;

#ifdef SELFTEST
	if (m_index_entry != NULL && m_index_entry->GetPSManager()->m_self_test_instance)
		SetWillBeDeleted();
#endif

	if (WillBeDeleted())
	{
		if (!m_file_abs_path && m_file_rel_path)
			OpDbUtils::ReportCondition(MakeAbsFilePath());
		if (GetOpFile() != NULL)
		{
			DB_DBG(("  - deleting file %S\n", m_file_abs_path));
			OpStatus::Ignore(GetOpFile()->Delete());
		}
		//TODO: delete folders if empty
	}
	DeleteSelf();
}

OP_STATUS
PS_DataFile::FileExists(BOOL *exists)
{
	OP_ASSERT(exists);
	RETURN_IF_ERROR(MakeAbsFilePath());
	if (OpFile* data_file_obj = GetOpFile())
		RETURN_IF_ERROR(data_file_obj->Exists(*exists));
	else
		*exists = FALSE;
	return OpStatus::OK;
}

void
PS_MgrPrefsListener::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (id != OpPrefsCollection::PersistentStorage)
		return;

	switch(pref)
	{
#ifdef DATABASE_STORAGE_SUPPORT
	case PrefsCollectionDatabase::DatabaseStorageQuotaExceededHandling:
		m_last_type = KWebDatabases;
		break;
#endif //DATABASE_STORAGE_SUPPORT
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	case PrefsCollectionDatabase::LocalStorageQuotaExceededHandling:
		m_last_type = KLocalStorage;
		break;
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case PrefsCollectionDatabase::WidgetPrefsQuotaExceededHandling:
		m_last_type = KWidgetPreferences;
		break;
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	default:
		m_last_type = KDBTypeStart;
	}
	//if this fails, there nothing much we can do...
	OpDbUtils::ReportCondition(m_mgr->EnumerateContextIds(this));
}

void
PS_MgrPrefsListener::PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC& newvalue)
{}

#ifdef PREFS_HOSTOVERRIDE
void
PS_MgrPrefsListener::HostOverrideChanged(OpPrefsCollection::Collections id, const uni_char* hostname)
{}
#endif

OP_STATUS
PS_MgrPrefsListener::HandleContextId(URL_CONTEXT_ID context_id)
{
	if (m_last_type == KDBTypeStart)
		return m_mgr->EnumerateTypes(this, context_id);
	else
		return m_mgr->EnumerateOrigins(this, context_id, m_last_type);
}


OP_STATUS
PS_MgrPrefsListener::HandleType(URL_CONTEXT_ID context_id, PS_ObjectType type)
{
	OP_ASSERT(m_last_type == KDBTypeStart);
	return m_mgr->EnumerateOrigins(this, context_id, type);
}

OP_STATUS
PS_MgrPrefsListener::HandleOrigin(URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin)
{
	m_mgr->InvalidateCachedDataSize(type, context_id, origin);
	return OpStatus::OK;
}

void
PS_MgrPrefsListener::Clear()
{
	if (m_current_listenee != NULL)
	{
		m_current_listenee->UnregisterListener(this);
		m_current_listenee = NULL;
	}
}

OP_STATUS
PS_MgrPrefsListener::Setup(OpPrefsCollection* listenee)
{
	Clear();
	if (listenee != NULL)
	{
		TRAP_AND_RETURN(status, listenee->RegisterListenerL(this));
		m_current_listenee = listenee;
	}
	return OpStatus::OK;
}

/**
 * repetition of the PS_ObjectType enumeration - because the
 * integer associated with each enum might change for some reason
 * it's easier to just write to the file the type as a human readable
 * string than an integer.
 * NOTE: the above means that once one adds a value to this array
 * it can no longer change after it's being used in public product,
 * else data files will apparently go missing for end users
 */

//the default one does not help much
#ifdef HAS_COMPLEX_GLOBALS
# define CONST_ARRAY_CLS(name, _, type) const type const name[] = {
#else
# define CONST_ARRAY_CLS(fn, name, type) void init_##fn () { const type* local = const_cast<const type*>(name); int i = 0;
#endif // HAS_COMPLEX_GLOBALS

CONST_ARRAY_CLS(database_module_mgr_psobj_types, g_database_manager->GetTypeDescTable(), uni_char*)
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	CONST_ENTRY(UNI_L("localstorage")), //KLocalStorage
	CONST_ENTRY(UNI_L("sessionstorage")), //KSessionStorage
#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND
#ifdef DATABASE_STORAGE_SUPPORT
	CONST_ENTRY(UNI_L("webdatabase")), //KWebDatabases
#endif //DATABASE_STORAGE_SUPPORT
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	CONST_ENTRY(UNI_L("widgetpreferences")), //KWidgetPreferences
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	CONST_ENTRY(UNI_L("userscript")), //KUserJsStorage
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	CONST_ENTRY(NULL)
#ifndef HAS_COMPLEX_GLOBALS
	;
#define database_module_mgr_psobj_types (g_database_manager->GetTypeDescTable())
#endif //HAS_COMPLEX_GLOBALS
CONST_END(database_module_mgr_psobj_types)

#ifdef SQLITE_SUPPORT
const OP_STATUS op_database_manager_sqlite_to_opstatus[27] =
{
	PS_Status::OK,                      //SQLITE_OK           0   /* Successful result */
	PS_Status::ERR_BAD_QUERY,           //SQLITE_ERROR        1   /* SQL error or missing database */
	PS_Status::ERR,                     //SQLITE_INTERNAL     2   /* Internal logic error in SQLite */
	PS_Status::ERR_NO_ACCESS,           //SQLITE_PERM         3   /* Access permission denied */
	PS_Status::ERR_TIMED_OUT,           //SQLITE_ABORT        4   /* Callback routine requested an abort */
	PS_Status::ERR_YIELD,               //SQLITE_BUSY         5   /* The ps_obj file is locked */
	PS_Status::ERR_YIELD,               //SQLITE_LOCKED       6   /* A table in the ps_obj is locked */
	PS_Status::ERR_NO_MEMORY,           //SQLITE_NOMEM        7   /* A malloc() failed */
	PS_Status::ERR_READ_ONLY,           //SQLITE_READONLY     8   /* Attempt to write a readonly ps_obj */
	PS_Status::ERR_TIMED_OUT,           //SQLITE_INTERRUPT    9   /* Operation terminated by sqlite3_interrupt()*/
	PS_Status::ERR_NO_ACCESS,           //SQLITE_IOERR       10   /* Some kind of disk I/O error occurred */
	PS_Status::ERR_CORRUPTED_FILE,      //SQLITE_CORRUPT     11   /* The ps_obj disk image is malformed */
	PS_Status::OK,                      //SQLITE_NOTFOUND    12   /* NOT USED. Table or record not found */
	PS_Status::ERR_QUOTA_EXCEEDED,      //SQLITE_FULL        13   /* Insertion failed because ps_obj is full */
	PS_Status::ERR_NO_ACCESS,           //SQLITE_CANTOPEN    14   /* Unable to open the ps_obj file */
	PS_Status::OK,                      //SQLITE_PROTOCOL    15   /* NOT USED. Database lock protocol error */
	PS_Status::ERR_BAD_QUERY,           //SQLITE_EMPTY       16   /* Database is empty */
	PS_Status::ERR_VERSION_MISMATCH,    //SQLITE_SCHEMA      17   /* The ps_obj schema changed */
	PS_Status::ERR_BAD_BIND_PARAMETERS, //SQLITE_TOOBIG      18   /* String or BLOB exceeds size limit */
	PS_Status::ERR_CONSTRAINT_FAILED,   //SQLITE_CONSTRAINT  19   /* Abort due to constraint violation */
	PS_Status::ERR_BAD_QUERY,           //SQLITE_MISMATCH    20   /* Data type mismatch */
	PS_Status::ERR,                     //SQLITE_MISUSE      21   /* Library used incorrectly */
	PS_Status::ERR_NOT_SUPPORTED,       //SQLITE_NOLFS       22   /* Uses OS features not supported on host */
	PS_Status::ERR_AUTHORIZATION,       //SQLITE_AUTH        23   /* Authorization denied */
	PS_Status::ERR,                     //SQLITE_FORMAT      24   /* Auxiliary ps_obj format error */
	PS_Status::ERR_BAD_BIND_PARAMETERS, //SQLITE_RANGE       25   /* 2nd parameter to sqlite3_bind out of range */
	PS_Status::ERR_CORRUPTED_FILE       //SQLITE_NOTADB      26   /* File opened that is not a ps_obj file */
};
#endif //SQLITE_SUPPORT


#ifdef DEBUG_ENABLE_OPASSERT
BOOL op_database_manager_type_desc_validate(unsigned length)
{
	BOOL ok = TRUE;
	for(unsigned idx = 0; idx < length; idx++)
		ok = ok && database_module_mgr_psobj_types[idx] != NULL && *database_module_mgr_psobj_types[idx];
	return ok;
}
#endif // DEBUG_ENABLE_OPASSERT

#ifdef SQLITE_SUPPORT
OP_STATUS
PS_Manager::SqliteErrorCodeToOpStatus(SQLiteErrorCode err_code)
{
	unsigned u_err_code = 0x3f & (unsigned)op_abs(err_code);
	return ARRAY_SIZE(op_database_manager_sqlite_to_opstatus) <= u_err_code ?
			OpStatus::OK : op_database_manager_sqlite_to_opstatus[u_err_code];
}
#endif //SQLITE_SUPPORT

/// the manager

PS_Manager::PS_Manager() :
#ifdef SELFTEST
	m_self_test_instance(FALSE),
#endif //SELFTEST
	m_modification_counter(0),
	m_last_save_mod_counter(0),
	m_inside_enumerate_count(0),
	m_prefs_listener(this)
{}

#ifdef SELFTEST
PS_Manager* PS_Manager::NewForSelfTest()
{
	PS_Manager *mgr = OP_NEW(PS_Manager, ());
	if (mgr != NULL)
		mgr->m_self_test_instance = TRUE;
	return mgr;
}
#endif //SELFTEST

TempBuffer& PS_Manager::GetTempBuffer(BOOL clear)
{
	if (clear)
		m_global_buffer.Clear();
	return m_global_buffer;
}

OP_STATUS PS_Manager::EnsureInitialization()
{
	if (IsInitialized())
		return OpStatus::OK;

	OP_ASSERT(!GetFlag(BEING_DELETED) && IsOperaRunning() && GetMessageHandler() != NULL);

	RETURN_IF_ERROR(AddContext(DB_MAIN_CONTEXT_ID, OPFILE_PERSISTENT_STORAGE_FOLDER, false));

#ifdef SELFTEST
	if (!m_self_test_instance)
#endif //SELFTEST
	{
		OpMessage messages[2] = {MSG_DB_MANAGER_FLUSH_INDEX_ASYNC, MSG_DB_MODULE_DELAYED_DELETE_DATA};
		RETURN_IF_ERROR(GetMessageHandler()->SetCallBackList(this, GetMessageQueueId(), messages, ARRAY_SIZE(messages)));

		RETURN_IF_ERROR(m_prefs_listener.Setup(g_pcdatabase));
	}

	OP_ASSERT(m_inside_enumerate_count == 0);
	m_last_save_mod_counter = ++m_modification_counter;
	CONST_ARRAY_INIT(database_module_mgr_psobj_types);

	OP_ASSERT(op_database_manager_type_desc_validate(DATABASE_INDEX_LENGTH));//ensure it's well built

	SetFlag(OBJ_INITIALIZED);

	return OpStatus::OK;
}

OP_STATUS
PS_Manager::AddContext(URL_CONTEXT_ID context_id, OpFileFolder root_folder_id, bool extension_context)
{
	DB_DBG(("PS_Manager::AddContext(): %u, %u\n", context_id, root_folder_id));

	// Calling this multiple times is supported because the code flow in core is quite complex
	// so it should make stuff easier for the API user. However, the values passed need to be
	// consistent, else there is trouble.
#ifdef EXTENSION_SUPPORT
	OP_ASSERT(GetIndex(context_id) == NULL || !GetIndex(context_id)->m_bools.is_extension_context || extension_context);
#endif // EXTENSION_SUPPORT
	OP_ASSERT(GetIndex(context_id) == NULL || !GetIndex(context_id)->WasRegistered() || GetIndex(context_id)->m_context_folder_id == root_folder_id);

	// NOTE: cannot load file now. It would be useless overhead
	// because this call will happen during core initialization,
	// and because the folder id and the was_registered flags
	// are only set after.
	RETURN_IF_ERROR(MakeIndex(FALSE, context_id));
	IndexByContext *ctx = GetIndex(context_id);
	OP_ASSERT(ctx);
	ctx->m_context_folder_id = root_folder_id;
	ctx->m_bools.was_registered = true;
#ifdef EXTENSION_SUPPORT
	ctx->m_bools.is_extension_context = extension_context;
#endif // EXTENSION_SUPPORT
	return OpStatus::OK;
}

bool
PS_Manager::IsContextRegistered(URL_CONTEXT_ID context_id) const
{
	IndexByContext *ctx = GetIndex(context_id);
	return ctx != NULL && ctx->WasRegistered();
}

#ifdef EXTENSION_SUPPORT
bool
PS_Manager::IsContextExtension(URL_CONTEXT_ID context_id) const
{
	IndexByContext *ctx = GetIndex(context_id);
	return ctx != NULL && ctx->WasRegistered() && ctx->IsExtensionContext();
}
#endif // EXTENSION_SUPPORT

BOOL
PS_Manager::GetContextRootFolder(URL_CONTEXT_ID context_id, OpFileFolder* root_folder_id)
{
	OP_ASSERT(root_folder_id);

	IndexByContext *ctx = GetIndex(context_id);
	if (ctx == NULL)
		return FALSE;

	// The non-default context id needs to be registered using AddContext
	// Else the folder will be bogus.
	OP_ASSERT(ctx->WasRegistered());
	if (!ctx->WasRegistered())
		return FALSE;

	*root_folder_id = ctx->m_context_folder_id;
	return TRUE;
}

void
PS_Manager::RemoveContext(URL_CONTEXT_ID context_id)
{
	DropEntireIndex(context_id);
}

void PS_Manager::Destroy()
{
	OP_ASSERT(m_inside_enumerate_count == 0);

#ifdef DATABASE_MODULE_DEBUG
#ifdef SELFTEST
	if (!m_self_test_instance)
#endif //SELFTEST
	{
		TRAPD(status_dummy_ignore,DumpIndexL(DB_NULL_CONTEXT_ID, UNI_L("PS_Manager::~Destroy()")));
	}
#endif // DATABASE_MODULE_DEBUG

	SetFlag(BEING_DELETED);

	if (GetFlag(OBJ_INITIALIZED))
	{
		if (GetMessageHandler() != NULL)
			GetMessageHandler()->UnsetCallBacks(this);

		FlushDeleteQueue(FALSE);
		m_prefs_listener.Clear();
		DropEntireIndex();
	}
	m_object_index.DeleteAll();

	OP_ASSERT(m_object_index.GetCount() == 0);

	UnsetFlag(OBJ_INITIALIZED | OBJ_INITIALIZED_ERROR);

	m_last_save_mod_counter = ++m_modification_counter;
}

/*static*/ OP_STATUS
PS_Manager::CheckOrigin(PS_ObjectType type, const uni_char* origin, BOOL is_persistent)
{
	if (origin == NULL || *origin == 0)
	{
		OP_ASSERT(!"Origin may not be blank");
		return OpStatus::ERR_NULL_POINTER;
	}

	return OpStatus::OK;
}

OP_STATUS PS_Manager::GetObject(PS_ObjectType type, const uni_char* origin,
	const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id, PS_Object** ps_obj)
{
	OP_ASSERT(ValidateObjectType(type));

	if (!ValidateObjectType(type))
		return OpStatus::ERR_OUT_OF_RANGE;

	OP_ASSERT(ps_obj != NULL);

	unsigned access = g_database_policies->GetPolicyAttribute(type,
				PS_Policy::KAccessToObject, context_id, origin);
	if (access == PS_Policy::KAccessDeny)
		return OpStatus::ERR_NO_ACCESS;

	RETURN_IF_ERROR(CheckOrigin(type, origin, is_persistent));

	RETURN_IF_ERROR(EnsureInitialization());

	RETURN_IF_ERROR(MakeIndex(TRUE, context_id));

	OP_ASSERT(GetIndex(context_id) != NULL && GetIndex(context_id)->WasRegistered());
	if (!GetIndex(context_id)->WasRegistered())
		return OpStatus::ERR_NO_ACCESS;

	PS_IndexEntry* key = CheckObjectExists(type, origin,
			name, is_persistent, context_id);
	if (!key)
	{
		//before creating a new ps_obj, check that we're not passing the allowed limit
		unsigned max_allowed_dbs = g_database_policies->GetPolicyAttribute(type,
				PS_Policy::KMaxObjectsPerOrigin, context_id, origin);

		if (max_allowed_dbs != 0 && max_allowed_dbs < GetNumberOfObjects(context_id, type, origin))
		{
			RETURN_IF_ERROR(PS_Status::ERR_MAX_DBS_PER_ORIGIN);
		}

		RETURN_IF_ERROR(StoreObject(type, origin, name,
				NULL, NULL, is_persistent, context_id, &key));
	}

	if (key->m_ps_obj == NULL)
	{
		OP_ASSERT(key->GetRefCount() == 0);
		PS_Object* o = PS_Object::Create(key);
		RETURN_OOM_IF_NULL(o);
		key->m_ps_obj = o;
	}
	key->IncRefCount();
	key->UnmarkDeletion();

	*ps_obj = key->m_ps_obj;

	return OpStatus::OK;
}

OP_STATUS PS_Manager::DeleteObject(PS_ObjectType type,
		const uni_char* origin,
		const uni_char* name,
		BOOL is_persistent, URL_CONTEXT_ID context_id)
{
	RETURN_IF_ERROR(EnsureInitialization());
	RETURN_IF_ERROR(MakeIndex(TRUE, context_id));

	PS_IndexEntry* key = CheckObjectExists(type, origin, name, is_persistent, context_id);
	if (key)
	{
		key->Delete();
		return OpStatus::OK;
	}
	return OpStatus::ERR_OUT_OF_RANGE;
}

OP_STATUS
PS_Manager::DeleteObjects(PS_ObjectType type, URL_CONTEXT_ID context_id,
		const uni_char *origin, BOOL only_persistent)
{
	DB_DBG(("PS_Manager::DeleteObjects(): %s,%d,%S,%s\n", GetTypeDesc(type), context_id, origin, DB_DBG_BOOL(only_persistent)))

	class DeleteItr : public PS_MgrContentIterator {
	public:
		PS_Manager *m_mgr;
		BOOL m_only_persistent;
		OpVector<PS_IndexEntry> m_objs;
		DeleteItr(PS_Manager *mgr, BOOL only_persistent) : m_mgr(mgr), m_only_persistent(only_persistent){}
		virtual OP_STATUS HandleOrigin(URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin) {
			return m_mgr->EnumerateObjects(this, context_id, type, origin);
		}
		virtual OP_STATUS HandleObject(PS_IndexEntry* object_entry) {
			OP_ASSERT(object_entry);
			if (!m_only_persistent || object_entry->IsPersistent())
				return m_objs.Add(object_entry);
			return OpStatus::OK;
		}
	} itr(this, only_persistent);

	OP_STATUS status;
	if (origin == NULL || origin[0] == 0)
		status = EnumerateOrigins(&itr, context_id, type);
	else
		status = EnumerateObjects(&itr, context_id, type, origin);

	for (unsigned k = 0, m = itr.m_objs.GetCount(); k < m; k++)
		itr.m_objs.Get(k)->Delete();

	return status;
}

OP_STATUS
PS_Manager::DeleteObjects(PS_ObjectType type, URL_CONTEXT_ID context_id, BOOL only_persistent)
{
	return DeleteObjects(type, context_id, NULL, only_persistent);
}

OP_STATUS
PS_Manager::GetGlobalDataSize(OpFileLength* data_size, PS_ObjectType type,
	URL_CONTEXT_ID context_id, const uni_char* origin)
{
	OP_ASSERT(data_size != NULL);
	TRAPD(status, *data_size = GetGlobalDataSizeL(type, context_id, origin));
	return SIGNAL_OP_STATUS_ERROR(status);
}

OpFileLength
PS_Manager::GetGlobalDataSizeL(PS_ObjectType type, URL_CONTEXT_ID context_id, const uni_char* origin)
{
	OP_ASSERT(IsInitialized());

	LEAVE_IF_ERROR(MakeIndex(TRUE, context_id));
	IndexByContext* mgr_data = GetIndex(context_id);
	OP_ASSERT(mgr_data != NULL);

	if (origin == NULL || origin[0] == 0)
	{
		if (mgr_data->m_cached_data_size[type] != FILE_LENGTH_NONE)
			return mgr_data->m_cached_data_size[type];
	}

	IndexEntryByOriginHash* eoh = mgr_data->GetIndexEntryByOriginHash(type, origin);
	if (eoh != NULL)
	{
		if (eoh->HasCachedDataSize(origin))
			return eoh->GetCachedDataSize(origin);
		if (eoh->GetNumberOfDbs(origin) == 0)
			return 0;
	}

	PS_IndexIterator *iterator = GetIteratorL(context_id, type, origin,
			TRUE, PS_IndexIterator::UNORDERED);
	OP_ASSERT(iterator != NULL);
	OpStackAutoPtr<PS_IndexIterator> iterator_ptr(iterator);

	PS_IndexEntry *key;
	OpFileLength fize_size_one = 0, fize_size_total = 0;

	if (!iterator->AtEndL())
	{
		do
		{
			key = iterator->GetItemL();
			if (key->GetPolicyAttribute(PS_Policy::KOriginExceededHandling, NULL) != PS_Policy::KQuotaAllow)
			{
				LEAVE_IF_ERROR(key->GetDataFileSize(&fize_size_one));
				fize_size_total += fize_size_one;
			}
		} while(iterator->MoveNextL());
	}

	if (origin == NULL || origin[0] == 0)
	{
		mgr_data->m_cached_data_size[type] = fize_size_total;
	}
	else if (eoh != NULL)
	{
		LEAVE_IF_ERROR(eoh->SetCachedDataSize(origin, fize_size_total));
	}

	return fize_size_total;
}

void
PS_Manager::InvalidateCachedDataSize(PS_ObjectType type, URL_CONTEXT_ID context_id, const uni_char* origin)
{
	IndexByContext* mgr_data = GetIndex(context_id);
	if (mgr_data != NULL)
	{
		//global one
		mgr_data->m_cached_data_size[type] = FILE_LENGTH_NONE;

		IndexEntryByOriginHash *ci = mgr_data->GetIndexEntryByOriginHash(type, origin);
		if (ci != NULL)
			ci->UnsetCachedDataSize(origin);
	}
}

#define DBFILE_ENTRY_TYPE UNI_L("Type")
#define DBFILE_ENTRY_TYPE_SZ 4

#define DBFILE_ENTRY_ORIGIN UNI_L("Origin")
#define DBFILE_ENTRY_ORIGIN_SZ 6

#define DBFILE_ENTRY_NAME UNI_L("Name")
#define DBFILE_ENTRY_NAME_SZ 4

#define DBFILE_ENTRY_FILE UNI_L("DataFile")
#define DBFILE_ENTRY_FILE_SZ 8

#define DBFILE_ENTRY_VERSION UNI_L("Version")
#define DBFILE_ENTRY_VERSION_SZ 7

void PS_Manager::ReadIndexFromFileL(const OpFile& file, URL_CONTEXT_ID context_id)
{
	OP_ASSERT(!GetFlag(BEING_DELETED) && IsOperaRunning());

#ifdef SELFTEST
	if (m_self_test_instance) return;
#endif //SELFTEST
	m_modification_counter++;
	PrefsFile prefs_file(PREFS_XML);
	ANCHOR(PrefsFile, prefs_file);

	prefs_file.ConstructL();
	prefs_file.SetFileL(&file);
	prefs_file.LoadAllL();

	OpString entry_type, entry_origin, entry_name, entry_datafile, entry_db_version;
	ANCHOR(OpString, entry_type);
	ANCHOR(OpString, entry_origin);
	ANCHOR(OpString, entry_name);
	ANCHOR(OpString, entry_datafile);
	ANCHOR(OpString, entry_db_version);

	const uni_char *entry_name_str = NULL, *entry_db_version_str = NULL;

	TempBuffer& buf = GetTempBuffer();

	OP_STATUS status = OpStatus::OK;
	OpString_list sections;
	ANCHOR(OpString_list, sections);
	prefs_file.ReadAllSectionsL(sections);
	unsigned i, nr_of_sections = sections.Count();
	for (i = 0; i < nr_of_sections; i++)
	{
		const OpString& current_section = sections.Item(i);

		prefs_file.ReadStringL(current_section, DBFILE_ENTRY_TYPE, entry_type);

		PS_ObjectType entry_type_number = GetDescType(entry_type.CStr());
		if (!ValidateObjectType(entry_type_number))
			continue;

		prefs_file.ReadStringL(current_section, DBFILE_ENTRY_FILE, entry_datafile);
		if (entry_datafile.IsEmpty())
			continue;

		prefs_file.ReadStringL(current_section, DBFILE_ENTRY_ORIGIN, entry_origin);

		entry_name_str = NULL;

		if (prefs_file.IsKey(current_section.CStr(), DBFILE_ENTRY_NAME))
		{
			prefs_file.ReadStringL(current_section, DBFILE_ENTRY_NAME, entry_name);

			if (!entry_name.IsEmpty())
			{
				unsigned entry_name_length = entry_name.Length();
				entry_name_str = entry_name.CStr();
				unsigned long readpos;
				BOOL warning;

				buf.ExpandL(entry_name_length / 4*3);
				OP_ASSERT((entry_name_length & 0x3) == 0);//multiple of four, else file is bogus
				entry_name_length = (entry_name_length / 4) * 4;
				make_singlebyte_in_place((char*)entry_name_str);
				unsigned new_size_name = GeneralDecodeBase64((const unsigned char*)entry_name_str,
						entry_name_length, readpos, (unsigned char*)buf.GetStorage(), warning, 0, NULL, NULL);

				OP_ASSERT(!warning);
				OP_ASSERT((unsigned)entry_name_length <= readpos);
				OP_ASSERT((new_size_name & 0x1) == 0);
				entry_name_length = new_size_name / 2;
				// This copy is safe. entry_name is holding the b64 string which is 33% bigger.
				op_memcpy(entry_name.CStr(), buf.GetStorage(), new_size_name);
				entry_name.CStr()[entry_name_length] = 0;
			}
			else
			{
				entry_name_str = UNI_L("");
			}
		}

		entry_db_version_str = NULL;

		if (prefs_file.IsKey(current_section.CStr(), DBFILE_ENTRY_VERSION))
		{
			prefs_file.ReadStringL(current_section, DBFILE_ENTRY_VERSION, entry_db_version);

			if (!entry_db_version.IsEmpty())
			{
				unsigned entry_db_version_length = entry_db_version.Length();
				entry_db_version_str = entry_db_version.CStr();
				unsigned long readpos;
				BOOL warning;

				buf.ExpandL(entry_db_version_length / 4 * 3);
				OP_ASSERT((entry_db_version_length & 0x3) == 0);//multiple of four, else file is bogus
				entry_db_version_length = (entry_db_version_length / 4) * 4;
				make_singlebyte_in_place((char*)entry_db_version_str);
				unsigned new_size_db_version = GeneralDecodeBase64((const unsigned char*)entry_db_version_str,
						entry_db_version_length, readpos, (unsigned char*)buf.GetStorage(), warning, 0, NULL, NULL);

				OP_ASSERT(!warning);
				OP_ASSERT((unsigned)entry_db_version_length <= readpos);
				OP_ASSERT((new_size_db_version & 0x1) == 0);
				entry_db_version_length = new_size_db_version / 2;
				// This copy is safe. entry_db_version is holding the b64 string which is 33% bigger.
				op_memcpy(entry_db_version.CStr(), buf.GetStorage(), new_size_db_version);
				entry_db_version.CStr()[entry_db_version_length] = 0;
			}
			else
			{
				entry_db_version_str = UNI_L("");
			}
		}
		LEAVE_IF_ERROR(MakeIndex(FALSE, context_id));
		PS_IndexEntry *key;
		status = StoreObject(entry_type_number, entry_origin.CStr(),
				entry_name_str, entry_datafile.CStr(),
				entry_db_version_str, TRUE, context_id, &key);
		LEAVE_IF_ERROR(status);
		OP_ASSERT(key != NULL);
		OP_ASSERT(CheckObjectExists(entry_type_number, entry_origin.CStr(),
				entry_name_str, TRUE, context_id) == key);

		// Update first file name number counter.
		ParseFileNameNumber(context_id, entry_datafile.CStr());

		if (entry_origin.Compare("opera:blank") == 0)
		{
			//delete opera:blank dbs -> these are generated for non-html docs
			//but were leftover from previous versions
			key->DeleteDataFile();
			key->SetFlag(PS_IndexEntry::MARKED_FOR_DELETION | PS_IndexEntry::MEMORY_ONLY_DB);
		}
	}
}

static void MakeSHAInBuffer(CryptoHash* sha1_maker, TempBuffer& buffer)
{
	if (sha1_maker == NULL) //if oom, just continue gracefully
		return;

	const uni_char *hex_digits = UNI_L("0123456789ABCDEF");
	unsigned hash_size = sha1_maker->Size();

	//make enough space
	RETURN_VOID_IF_ERROR(buffer.Expand(hash_size * 2 + 1));
	RETURN_VOID_IF_ERROR(sha1_maker->InitHash());

	buffer.SetCachedLengthPolicy(TempBuffer::UNTRUSTED);

	UINT8* data = reinterpret_cast<UINT8*>(buffer.GetStorage());
	uni_char* string = buffer.GetStorage();

	sha1_maker->CalculateHash(data, UNICODE_SIZE(buffer.Length()));
	sha1_maker->ExtractHash(data);

	for(unsigned j = hash_size; j > 0; j--)
	{
		string[j * 2 - 1] = hex_digits[(data[j - 1]     ) & 0xf];
		string[j * 2 - 2] = hex_digits[(data[j - 1] >> 4) & 0xf];
	}

	string[hash_size * 2] = 0;
	buffer.SetCachedLengthPolicy(TempBuffer::TRUSTED);
}

OP_STATUS PS_Manager::FlushIndexToFile(URL_CONTEXT_ID context_id)
{
	TRAPD(status, {
		FlushIndexToFileL(context_id);
	});
	return status;
}

void PS_Manager::FlushIndexToFileL(URL_CONTEXT_ID context_id)
{
#ifdef SELFTEST
	if (m_self_test_instance) return;
#endif

	if (context_id == DB_NULL_CONTEXT_ID)
	{
		if (m_object_index.GetCount() == 0)
			return;

		OpHashIterator* itr = m_object_index.GetIterator();
		LEAVE_IF_NULL(itr);
		OP_STATUS status = OpStatus::OK;
		if (OpStatus::IsSuccess(itr->First()))
		{
			do {
				URL_CONTEXT_ID ctx_id = reinterpret_cast<URL_CONTEXT_ID>(itr->GetKey());
				OP_ASSERT(ctx_id != DB_NULL_CONTEXT_ID);
				status = OpDbUtils::GetOpStatusError(FlushIndexToFile(ctx_id), status);
			} while (OpStatus::IsSuccess(itr->Next()));
		}
		OP_DELETE(itr);
		LEAVE_IF_ERROR(status);
		return;
	}

	if (!IsInitialized())
		return;

	IndexByContext* mgr_data = GetIndex(context_id);
	if (mgr_data == NULL || mgr_data->GetIsFlushing() || !mgr_data->WasRegistered() || !mgr_data->HasReadFile())
		return;

	mgr_data->SetIsFlushing(TRUE);
	IndexByContext::UnsetFlushHelper mgr_data_unset(mgr_data);
	ANCHOR(IndexByContext::UnsetFlushHelper, mgr_data_unset);

	DB_DBG(("Flushing index %d, %S\n", context_id, mgr_data->GetFileFd().GetFullPath()))

	PrefsFile prefs_file(PREFS_XML);
	ANCHOR(PrefsFile, prefs_file);

	prefs_file.ConstructL();
	prefs_file.SetFileL(&mgr_data->GetFileFd());

	OpStackAutoPtr<PS_IndexIterator> iterator(GetIteratorL(context_id, KDBTypeStart));
	OP_ASSERT(iterator.get());

	if (!iterator->AtEndL())
	{
		CryptoHash* sha1_maker = CryptoHash::CreateSHA1();
		OpStackAutoPtr<CryptoHash> sha1_maker_ptr(sha1_maker);

		TempBuffer& buf = GetTempBuffer();
		TempBuffer b64buf;
		ANCHOR(TempBuffer, b64buf);

		PS_IndexEntry *key;
		while(!iterator->AtEndL())
		{
			key = iterator->GetItemL();
			OP_ASSERT(key != NULL);

			// During shutdown, the objects must be pre-deleted, else
			// this assert triggers, which means that DropEntireIndex()
			// is not traversing all objects.
			OP_ASSERT(key->GetPSManager()->IsOperaRunning() || key->GetObject() == NULL);

			//skip entries without file IF they are not going to be deleted
			if (!key->HasWritableData() && !key->GetFlag(PS_IndexEntry::MARKED_FOR_DELETION))
			{
				iterator->MoveNextL();
				continue;
			}
			//skip entries with bogus file
			if (key->GetFileNameObj() != NULL && key->GetFileNameObj()->IsBogus())
			{
				iterator->MoveNextL();
				continue;
			}

			//key -> ps_obj type origin name
			buf.Clear();
			buf.AppendL(GetTypeDesc(key->GetType()));
			if (key->GetOrigin() != NULL)
			{
				buf.AppendL(' ');
				buf.AppendL(key->GetOrigin());
			}
			if (key->GetName() != NULL)
			{
				buf.AppendL(' ');
				buf.AppendL(key->GetName());
			}

			if (key->GetFlag(PS_IndexEntry::MARKED_FOR_DELETION))
			{
				prefs_file.DeleteSectionL(buf.GetStorage());
				MakeSHAInBuffer(sha1_maker, buf);
				prefs_file.DeleteSectionL(buf.GetStorage());

				if (key->GetFlag(PS_IndexEntry::PURGE_ENTRY) &&
					!IsBeingDestroyed())
				{
					key->UnsetFlag(PS_IndexEntry::MARKED_FOR_DELETION);
					DeleteEntryNow(key->GetType(), key->GetOrigin(), key->GetName(),
							!key->IsMemoryOnly(), key->GetUrlContextId());
					iterator->ReSync();
					continue;
				}
			}
			else
			{
				prefs_file.DeleteSectionL(buf.GetStorage());
				MakeSHAInBuffer(sha1_maker, buf);
				prefs_file.WriteStringL(buf.GetStorage(), DBFILE_ENTRY_TYPE, key->GetTypeDesc());
				if (key->GetOrigin() != NULL)
					prefs_file.WriteStringL(buf.GetStorage(), DBFILE_ENTRY_ORIGIN, key->GetOrigin());
				prefs_file.WriteStringL(buf.GetStorage(), DBFILE_ENTRY_FILE, key->GetFileRelPath());

				//the following two are specified by webpages, so there should be escaped
				if (key->GetName() != NULL)
				{
					unsigned bsize = UNICODE_SIZE(uni_strlen(key->GetName()));
					b64buf.ExpandL((bsize + 2) / 3 * 4 + 1);
					char *b64 = NULL;
					int b64sz = 0;

					switch (MIME_Encode_SetStr(b64, b64sz, (const char*)key->GetName(),
							bsize, NULL, GEN_BASE64_ONELINE))
					{
					case MIME_NO_ERROR:
						break;
					case MIME_FAILURE:
						LEAVE(OpStatus::ERR_NO_MEMORY);
						break;
					default:
						LEAVE(OpStatus::ERR);
					}

					OP_ASSERT((unsigned)b64sz <= b64buf.GetCapacity());
					b64buf.ExpandL(b64sz + 1);
					make_doublebyte_in_buffer(b64, b64sz, b64buf.GetStorage(), b64sz + 1);
					OP_DELETEA(b64);
					prefs_file.WriteStringL(buf.GetStorage(), DBFILE_ENTRY_NAME, b64buf.GetStorage());
					b64buf.Clear();
				}
				else
				{
					prefs_file.DeleteKeyL(buf.GetStorage(), DBFILE_ENTRY_NAME);
				}

				if (key->GetVersion() != NULL)
				{
					unsigned bsize = UNICODE_SIZE(uni_strlen(key->GetVersion()));
					b64buf.ExpandL((bsize + 2) / 3 * 4 + 1);
					char *b64 = NULL;
					int b64sz = 0;

					switch (MIME_Encode_SetStr(b64, b64sz, (const char*)key->m_db_version,
							bsize, NULL, GEN_BASE64_ONELINE))
					{
					case MIME_NO_ERROR:
						break;
					case MIME_FAILURE:
						LEAVE(OpStatus::ERR_NO_MEMORY);
						break;
					default:
						LEAVE(OpStatus::ERR);
					}

					OP_ASSERT((unsigned)b64sz <= b64buf.GetCapacity());
					b64buf.ExpandL(b64sz + 1);
					make_doublebyte_in_buffer(b64, b64sz, b64buf.GetStorage(), b64sz + 1);
					OP_DELETEA(b64);
					prefs_file.WriteStringL(buf.GetStorage(), DBFILE_ENTRY_VERSION, b64buf.GetStorage());
					b64buf.Clear();
				}
				else
				{
					prefs_file.DeleteKeyL(buf.GetStorage(), DBFILE_ENTRY_VERSION);
				}
			}
			iterator->MoveNextL();
		}
	}
	prefs_file.CommitL(TRUE, TRUE);
	m_last_save_mod_counter = m_modification_counter;
}

OP_STATUS PS_Manager::FlushIndexToFileAsync(URL_CONTEXT_ID context_id)
{
#ifdef SELFTEST
	if (m_self_test_instance)
		return OpStatus::OK;
#endif //SELFTEST

	BOOL has_posted_msg = GetIndex(context_id) != NULL && GetIndex(context_id)->HasPostedFlushMessage();
	if (!IsInitialized() || has_posted_msg || GetMessageHandler() == NULL)
		return OpStatus::OK;

	if (GetMessageHandler()->PostMessage(MSG_DB_MANAGER_FLUSH_INDEX_ASYNC,
		GetMessageQueueId(), context_id, 0))
	{
		if (GetIndex(context_id) != NULL)
			GetIndex(context_id)->SetPostedFlushMessage();
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

PS_IndexIterator*
PS_Manager::GetIteratorL(URL_CONTEXT_ID context_id, PS_ObjectType use_case_type,
		const uni_char* origin, BOOL only_persistent, PS_IndexIterator::IterationOrder order)
{
	LEAVE_IF_ERROR(EnsureInitialization());
	LEAVE_IF_ERROR(MakeIndex(TRUE, context_id));
	OP_ASSERT(NULL != GetIndex(context_id));

	OpStackAutoPtr<PS_IndexIterator> itr_ptr(OP_NEW_L(PS_IndexIterator, (this, context_id, use_case_type, origin, only_persistent, order)));
	itr_ptr->BuildL();
	return itr_ptr.release();
}

OP_STATUS
PS_Manager::EnumerateContextIds(PS_MgrContentIterator* enumerator)
{
	DB_ASSERT_RET(enumerator != NULL, OpStatus::ERR_OUT_OF_RANGE);

	if (m_object_index.GetCount() == 0)
		return OpStatus::OK;

	class ThisItr: public OpHashTableForEachListener {
	public:
		PS_MgrContentIterator *m_enumerator;
		OP_STATUS m_status;
		ThisItr(PS_MgrContentIterator *enumerator) : m_enumerator(enumerator), m_status(OpStatus::OK){}
		virtual void HandleKeyData(const void* key, void* data)
		{
			IndexByContext * ctx = static_cast<IndexByContext* >(data);
			if (!ctx->WasRegistered())
				// Context not registered therefore should not be enumerated.
				return;

			URL_CONTEXT_ID ctx_id = reinterpret_cast<URL_CONTEXT_ID>(key);
			OP_ASSERT(ctx_id != DB_NULL_CONTEXT_ID);
			m_status = OpDbUtils::GetOpStatusError(m_status, m_enumerator->HandleContextId(ctx_id));
		}
	} itr(enumerator);

	m_inside_enumerate_count++;
	m_object_index.ForEach(&itr);
	m_inside_enumerate_count--;

	return itr.m_status;
}

OP_STATUS
PS_Manager::EnumerateTypes(PS_MgrContentIterator* enumerator, URL_CONTEXT_ID context_id)
{
	DB_ASSERT_RET(enumerator != NULL && context_id != DB_NULL_CONTEXT_ID, OpStatus::ERR_OUT_OF_RANGE);

	RETURN_IF_ERROR(EnsureInitialization());
	RETURN_IF_ERROR(MakeIndex(TRUE, context_id));

	IndexByContext* object_index = GetIndex(context_id);
	OP_STATUS status = OpStatus::OK;

	if (object_index != NULL)
	{
		m_inside_enumerate_count++;
		union { int k; PS_ObjectType db_type; };
		for (k = KDBTypeStart + 1; k < KDBTypeEnd; k++)
			if (object_index->m_object_index[db_type] != NULL)
				status = OpDbUtils::GetOpStatusError(status, enumerator->HandleType(context_id, db_type));
		m_inside_enumerate_count--;
	}
	return status;
}

OP_STATUS
PS_Manager::EnumerateOrigins(PS_MgrContentIterator* enumerator, URL_CONTEXT_ID context_id, PS_ObjectType type)
{
	DB_ASSERT_RET(enumerator != NULL &&
		context_id != DB_NULL_CONTEXT_ID &&
		ValidateObjectType(type), OpStatus::ERR_OUT_OF_RANGE);

	RETURN_IF_ERROR(EnsureInitialization());
	RETURN_IF_ERROR(MakeIndex(TRUE, context_id));

	IndexByContext* object_index = GetIndex(context_id);

	if (object_index != NULL)
	{
		IndexEntryByOriginHash** origin_table = object_index->m_object_index[type];
		if (origin_table != NULL)
		{
			class ThisItr : public OpHashTableForEachListener {
			public:
				PS_MgrContentIterator *m_enumerator;
				OP_STATUS m_status;
				URL_CONTEXT_ID m_context_id;
				PS_ObjectType m_type;

				ThisItr(PS_MgrContentIterator *enumerator, PS_ObjectType type, URL_CONTEXT_ID context_id) :
					m_enumerator(enumerator), m_status(OpStatus::OK), m_context_id(context_id), m_type(type) {}

				virtual void HandleKeyData(const void* key, void* data){
					const uni_char* origin = reinterpret_cast<const uni_char*>(key);
					m_status = OpDbUtils::GetOpStatusError(m_status,
							m_enumerator->HandleOrigin(m_context_id, m_type, origin));
				}
			} itr(enumerator, type, context_id);

			m_inside_enumerate_count++;

			for(unsigned k = 0; k < m_max_hash_amount; k++)
			{
				if (origin_table[k] != NULL)
					origin_table[k]->m_origin_cached_info.ForEach(&itr);
			}

			m_inside_enumerate_count--;

			return itr.m_status;
		}
	}
	return OpStatus::OK;
}


OP_STATUS
PS_Manager::EnumerateObjects(PS_MgrContentIterator* enumerator,
	URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin)
{
	DB_ASSERT_RET(enumerator != NULL &&
		origin != NULL && origin[0] != 0 &&
		context_id != DB_NULL_CONTEXT_ID &&
		ValidateObjectType(type), OpStatus::ERR_OUT_OF_RANGE);

	RETURN_IF_ERROR(EnsureInitialization());
	RETURN_IF_ERROR(MakeIndex(TRUE, context_id));

	IndexByContext* object_index = GetIndex(context_id);
	if (object_index != NULL)
	{
		IndexEntryByOriginHash** origins_table = object_index->m_object_index[type];
		if (origins_table != NULL)
		{
			IndexEntryByOriginHash* origins_hm = origins_table[HashOrigin(origin)];
			if (origins_hm)
			{
				m_inside_enumerate_count++;

				for (unsigned k = 0, m = origins_hm->m_objects.GetCount(); k < m; k++)
				{
					PS_IndexEntry *entry = origins_hm->m_objects.Get(k);
					if (uni_str_eq(entry->GetOrigin(), origin))
					{
						enumerator->HandleObject(entry);
					}
				}

				m_inside_enumerate_count--;
			}
		}
	}
	return OpStatus::OK;
}


PS_IndexEntry* PS_Manager::CheckObjectExists(PS_ObjectType type,
		const uni_char* origin, const uni_char* name,
		BOOL is_persistent, URL_CONTEXT_ID context_id) const
{
	DB_ASSERT_RET(IsInitialized() && ValidateObjectType(type), NULL);

	RETURN_VALUE_IF_ERROR(CheckOrigin(type, origin, is_persistent), NULL);

	IndexByContext* object_index = GetIndex(context_id);
	if (object_index != NULL)
	{
		IndexEntryByOriginHash** origin_table = object_index->m_object_index[type];
		if (origin_table != NULL)
		{
			unsigned hashed_origin = HashOrigin(origin);
			OP_ASSERT(hashed_origin < m_max_hash_amount);
			IndexEntryByOriginHash* key_table = origin_table[hashed_origin];
			if (key_table != NULL)
			{
				UINT32 index, limit = key_table->m_objects.GetCount();
				for(index = 0; index < limit; index++)
				{
					PS_IndexEntry *object_index = key_table->m_objects.Get(index);
					OP_ASSERT(object_index != NULL);//vector cannot have empty items
					if (object_index &&
						object_index->IsEqual(type, origin, name, is_persistent, context_id))
						return object_index;
				}
			}
		}
	}
	return NULL;
}

unsigned PS_Manager::GetNumberOfObjects(URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin) const
{
	IndexEntryByOriginHash *ci = GetIndexEntryByOriginHash(context_id, type, origin);
	return ci != NULL ? ci->GetNumberOfDbs(origin): 0;
}

OP_STATUS PS_Manager::StoreObject(PS_ObjectType type, const uni_char* origin,
		const uni_char* name, const uni_char* data_file_name,
		const uni_char* db_version, BOOL is_persistent, URL_CONTEXT_ID context_id,
		PS_IndexEntry** created_entry)
{
	DB_ASSERT_RET(IsInitialized() && ValidateObjectType(type) && m_inside_enumerate_count == 0, PS_Status::ERR_INVALID_STATE);

	//OP_ASSERT this should not exist in our builds !
	PS_IndexEntry* key = CheckObjectExists(type, origin, name, is_persistent, context_id);
	OP_ASSERT(key == NULL);
	if (key != NULL)
	{
		//we can't have entries being rewritten so ignore duplicate entries
		//in case some user feels like fiddling with the index file
		if (created_entry != NULL)
			*created_entry = key;
		return OpStatus::OK;
	}

	//sanity checks
	OP_ASSERT(is_persistent || data_file_name == NULL || uni_str_eq(g_ps_memory_file_name, data_file_name));

	//the callee must have initialized it !
	IndexByContext* mgr_data = GetIndex(context_id);
	OP_ASSERT(mgr_data != NULL);

	IndexEntryByOriginHash** origin_table = NULL;
	if (mgr_data->m_object_index[type] == NULL)
	{
		RETURN_OOM_IF_NULL(origin_table = OP_NEWA(IndexEntryByOriginHash*, m_max_hash_amount));
		op_memset(origin_table, 0, sizeof(IndexEntryByOriginHash*)*m_max_hash_amount);
		mgr_data->m_object_index[type] = origin_table;
	}
	else
	{
		origin_table = mgr_data->m_object_index[type];
	}
	OP_ASSERT(origin_table != NULL);

	unsigned hashed_origin = HashOrigin(origin);
	OP_ASSERT(hashed_origin < m_max_hash_amount);
	IndexEntryByOriginHash*  key_table = NULL;
	if (origin_table[hashed_origin] == NULL)
	{
		RETURN_OOM_IF_NULL(key_table = OP_NEW(IndexEntryByOriginHash, ()));
		origin_table[hashed_origin] = key_table;
	}
	else
	{
		key_table = origin_table[hashed_origin];
	}

	OP_STATUS status;
	PS_IndexEntry *datafile_entry = PS_IndexEntry::Create(this, context_id,
			type, origin, name, data_file_name, db_version, is_persistent, NULL);
	if (datafile_entry != NULL)
	{
		if (OpStatus::IsSuccess(status = key_table->m_objects.Add(datafile_entry)))
		{
			if (OpStatus::IsSuccess(status = key_table->IncNumberOfDbs(origin)))
			{
				//Success !
				if (created_entry != NULL)
					*created_entry = datafile_entry;

				m_modification_counter++;

				return OpStatus::OK;
			}
			key_table->m_objects.Remove(key_table->m_objects.GetCount() - 1);
		}
		OP_DELETE(datafile_entry);
	}
	else
		status = OpStatus::ERR_NO_MEMORY;

	if (origin_table[hashed_origin] &&
		origin_table[hashed_origin]->m_objects.GetCount() == 0)
	{
		OP_DELETE(origin_table[hashed_origin]);
		origin_table[hashed_origin] = NULL;
	}

	OP_ASSERT(origin_table[hashed_origin] == NULL || origin_table[hashed_origin]->m_objects.GetCount() != 0);

	return  status;
}

BOOL PS_Manager::DeleteEntryNow(PS_ObjectType type,
		const uni_char* origin, const uni_char* name,
		BOOL is_persistent, URL_CONTEXT_ID context_id)
{
	DB_ASSERT_RET(IsInitialized() && ValidateObjectType(type) && m_inside_enumerate_count == 0, FALSE);

	RETURN_VALUE_IF_ERROR(CheckOrigin(type, origin, is_persistent), FALSE);

	BOOL found_something = FALSE;

	//although the asserts validates the types, release builds don't have asserts
	//which could cause data to be written in places it shouldn't
	OP_ASSERT(ValidateObjectType(type));
	if (!ValidateObjectType(type))
		return found_something;

	IndexByContext* mgr_data = GetIndex(context_id);
	if (mgr_data != NULL)
	{

		IndexEntryByOriginHash** origin_table = mgr_data->m_object_index[type];
		if (origin_table != NULL)
		{
			unsigned hashed_origin = HashOrigin(origin);
			OP_ASSERT(hashed_origin < m_max_hash_amount);
			IndexEntryByOriginHash* key_table = origin_table[hashed_origin];
			if (key_table != NULL)
			{
				UINT32 index, limit = key_table->m_objects.GetCount();
				for(index = 0; index < limit; index++)
				{
					PS_IndexEntry *object_index = key_table->m_objects.Get(index);
					OP_ASSERT(object_index != NULL); // Vector cannot have empty items.

					if (object_index &&
						object_index->IsEqual(type, origin, name, is_persistent, context_id))
					{
						if (object_index->GetFlag(PS_IndexEntry::BEING_DELETED))
						{
							if (!object_index->IsMemoryOnly() && object_index->GetFlag(PS_IndexEntry::MARKED_FOR_DELETION))
							{
								OpStatus::Ignore(FlushIndexToFile(object_index->GetUrlContextId()));
							}
						}
						else
						{
							if (!object_index->IsMemoryOnly() && object_index->GetFlag(PS_IndexEntry::MARKED_FOR_DELETION))
							{
								OpStatus::Ignore(FlushIndexToFileAsync(object_index->GetUrlContextId()));
							}
							else
							{
#ifdef DATABASE_MODULE_DEBUG
								dbg_printf("Deleting ps_obj (%d,%S,%S,%s)\n", type,
										origin, name, is_persistent ? "persistent" : "memory");
#endif //DATABASE_MODULE_DEBUG
								key_table->m_objects.Remove(index--);
								key_table->DecNumberOfDbs(origin);
								OP_DELETE(object_index);
								m_modification_counter++;
							}
						}
						found_something = TRUE;
						break;
					}
				}
				if (origin_table[hashed_origin] &&
					origin_table[hashed_origin]->m_objects.GetCount() == 0)
				{
					OP_DELETE(origin_table[hashed_origin]);
					origin_table[hashed_origin] = NULL;
				}
			}
		}
	}
	return found_something;
}

PS_Manager::IndexByContext::IndexByContext(PS_Manager* mgr, URL_CONTEXT_ID context_id)
	: m_mgr(mgr)
	, m_context_id(context_id)
	, m_context_folder_id(OPFILE_ABSOLUTE_FOLDER)
	, m_bools_init(0)
{
	OP_ASSERT(mgr != NULL);
	op_memset(m_object_index, 0, sizeof(m_object_index));

	for(unsigned k = 0; k < DATABASE_INDEX_LENGTH; k++)
		m_cached_data_size[k] = FILE_LENGTH_NONE;
}

PS_Manager::IndexByContext::~IndexByContext()
{
	m_bools.being_deleted = TRUE;

	for (unsigned k = 0; k < DATABASE_INDEX_LENGTH; k++)
	{
		if (m_object_index[k] != NULL)
		{
			for (unsigned m = 0; m < m_max_hash_amount; m++)
			{
				if (m_object_index[k][m] != NULL)
				{
					OP_DELETE(m_object_index[k][m]);
					m_object_index[k][m] = NULL;
				}
			}
			OP_DELETEA(m_object_index[k]);
			m_object_index[k] = NULL;
		}
	}
}

PS_Manager::IndexEntryByOriginHash*
PS_Manager::IndexByContext::GetIndexEntryByOriginHash(PS_ObjectTypes::PS_ObjectType type, const uni_char* origin) const
{
	return m_object_index[type] != NULL && origin != NULL ? m_object_index[type][HashOrigin(origin)] : NULL;
}

OP_STATUS
PS_Manager::IndexByContext::InitializeFile()
{
	if (HasReadFile())
		return OpStatus::OK;

	PS_Policy* def_policy = g_database_policies->GetPolicy(PS_ObjectTypes::KDBTypeStart);
	OP_ASSERT(def_policy != NULL);
	TempBuffer &buf = m_mgr->GetTempBuffer(TRUE);

	const uni_char *profile_folder = def_policy->GetAttribute(PS_Policy::KMainFolderPath, m_context_id);
	RETURN_OOM_IF_NULL(profile_folder);
	if (*profile_folder == 0)
		return OpStatus::ERR_NO_ACCESS;

	RETURN_IF_ERROR(buf.Append(profile_folder));
	ENSURE_PATHSEPCHAR(buf);
	RETURN_IF_ERROR(buf.Append(def_policy->GetAttribute(PS_Policy::KSubFolder, m_context_id)));
	ENSURE_PATHSEPCHAR(buf);
	RETURN_IF_ERROR(buf.Append(DATABASE_INTERNAL_PERSISTENT_INDEX_FILENAME));

	RETURN_IF_ERROR(m_index_file_fd.Construct(buf.GetStorage()));

#ifdef DATABASE_MODULE_DEBUG
	dbg_printf("Reading ps_obj index: %d %S\n", m_context_id, m_index_file_fd.GetFullPath());
#endif // DATABASE_MODULE_DEBUG

	TRAPD(status,m_mgr->ReadIndexFromFileL(m_index_file_fd, m_context_id));
	if (OpStatus::IsSuccess(status))
		m_bools.file_was_read = true;

#ifdef DATABASE_MODULE_DEBUG
	dbg_printf(" - %d\n", status);
	TRAPD(________status,m_mgr->DumpIndexL(m_context_id, UNI_L("After loading")));
#endif // DATABASE_MODULE_DEBUG

	return status;
}

OP_STATUS PS_Manager::MakeIndex(BOOL load_file, URL_CONTEXT_ID context_id)
{
	IndexByContext* index = GetIndex(context_id);

	if (index == NULL)
	{
		index = OP_NEW(IndexByContext, (this, context_id));
		RETURN_OOM_IF_NULL(index);
		OP_STATUS status = m_object_index.Add(INT_TO_PTR(context_id), index);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(index);
			return status;
		}
	}

	if (load_file && index->WasRegistered())
	{
		OP_ASSERT(IsInitialized());
		RETURN_IF_MEMORY_ERROR(index->InitializeFile());
	}

	return OpStatus::OK;
}

PS_Manager::IndexEntryByOriginHash*
PS_Manager::GetIndexEntryByOriginHash(URL_CONTEXT_ID context_id, PS_ObjectTypes::PS_ObjectType type, const uni_char* origin) const
{
	if (origin == NULL || origin[0] == 0)
		return NULL;
	IndexByContext* index = GetIndex(context_id);
	return index != NULL ? index->GetIndexEntryByOriginHash(type, origin) : NULL;
}

void
PS_Manager::DropEntireIndex(URL_CONTEXT_ID context_id)
{
	if (!IsInitialized())
		return;

	/**
	 * First we explicitly delete only the ps_obj objects.
	 * Some will flush data and the index to disk for instance,
	 * so we should ensure that the index is intact while the
	 * ps_obj objects are deleted, else the iterators become
	 * invalid and may crash
	 */
	struct DbItr : public PS_MgrContentIterator
	{
		DbItr(PS_Manager* mgr) : m_mgr(mgr) {}

		virtual OP_STATUS HandleContextId(URL_CONTEXT_ID context_id)
		{
			PS_Manager::IndexByContext *ctx = m_mgr->GetIndex(context_id);
			if (ctx->HasReadFile())
			{
				ctx->SetIndexBeingDeleted();
				return m_mgr->EnumerateTypes(this, context_id);
			}
			else
				return OpStatus::OK;
		}
		virtual OP_STATUS HandleType(URL_CONTEXT_ID context_id, PS_ObjectType type)
		{
			return m_mgr->EnumerateOrigins(this, context_id, type);
		}
		virtual OP_STATUS HandleOrigin(URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin)
		{
			return m_mgr->EnumerateObjects(this, context_id, type, origin);
		}
		virtual OP_STATUS HandleObject(PS_IndexEntry* object_key)
		{
			if (object_key->GetObject() != NULL)
			{
				INTEGRITY_CHECK_P(object_key->GetObject());
				OP_ASSERT(m_dbs.Find(object_key->GetObject()) == -1);
				RETURN_IF_ERROR(m_dbs.Add(object_key->GetObject()));
			}
			return OpStatus::OK;
		}
		PS_Manager* m_mgr;
		OpVector<PS_Object> m_dbs;
	} delete_enumerator(this);

	if (context_id != DB_NULL_CONTEXT_ID)
	{
		PS_Manager::IndexByContext *ctx = GetIndex(context_id);
		if (ctx != NULL && ctx->HasReadFile())
			ctx->SetIndexBeingDeleted();
		else
			return;
	}

	OP_STATUS status  = OpStatus::OK;
	do {
		//loop ensures that everything is cleanup even if there is OOM
		//while populating the OpVector

		if (context_id == DB_NULL_CONTEXT_ID)
			status = EnumerateContextIds(&delete_enumerator);
		else
			status = EnumerateTypes(&delete_enumerator, context_id);

		if (delete_enumerator.m_dbs.GetCount() == 0)
			break;

		for(int k = delete_enumerator.m_dbs.GetCount() - 1; k >= 0; k--)
		{
			PS_Object *o = delete_enumerator.m_dbs.Get(k);
			INTEGRITY_CHECK_P(o);
			OP_ASSERT(delete_enumerator.m_dbs.Find(o) == k);
			OP_DELETE(o);
		}
		delete_enumerator.m_dbs.Empty();

	} while(OpStatus::IsMemoryError(status));

	ReportCondition(status);

	OpStatus::Ignore(FlushIndexToFile(context_id));

	if (context_id != DB_NULL_CONTEXT_ID)
	{
		IndexByContext* index = NULL;
		if (OpStatus::IsSuccess(m_object_index.Remove(INT_TO_PTR(context_id), &index)))
			OP_DELETE(index);
	}
}

unsigned
PS_Manager::HashOrigin(const uni_char* origin)
{
	unsigned calc_hash = 0;
	if (origin != NULL)
	{
		for(unsigned idx = 0; idx < m_max_hash_chars && origin[idx] != 0; idx++)
		{
			calc_hash = calc_hash ^ origin[idx];
		}
	}
	return calc_hash & m_hash_mask;
}

const uni_char* PS_Manager::GetTypeDesc(PS_ObjectType type) const {
	//although the asserts validates the types, release builds don't have asserts
	//which could cause data to be written in places it shouldn't
	OP_ASSERT(ValidateObjectType(type));
	if (!ValidateObjectType(type))
		return NULL;
	return database_module_mgr_psobj_types[type];
}

PS_ObjectTypes::PS_ObjectType PS_Manager::GetDescType(const uni_char* desc) const
{
	union { PS_ObjectType type; int x; };
	type  = KDBTypeStart;
	if (desc != NULL) {
		for (unsigned idx = KDBTypeStart + 1; idx < KDBTypeEnd && database_module_mgr_psobj_types[idx] != NULL; idx++)
		{
			if (desc == database_module_mgr_psobj_types[idx] ||
				uni_str_eq(desc, database_module_mgr_psobj_types[idx]))
			{
				x = idx;
				break;
			}
		}
	}
	OP_ASSERT(KDBTypeStart <= type && type < KDBTypeEnd);
	return type;
}

/*static*/ PS_Manager::DeleteDataInfo*
PS_Manager::DeleteDataInfo::Create(URL_CONTEXT_ID context_id, unsigned type_flags, bool is_extension, const uni_char* origin)
{
	void *p = op_malloc(sizeof(DeleteDataInfo) + (origin ? UNICODE_SIZE(uni_strlen(origin)) : 0));
	RETURN_VALUE_IF_NULL(p, NULL);

	DeleteDataInfo *o = new (p) DeleteDataInfo();
	if (origin != NULL)
		uni_strcpy(o->m_origin, origin);
	else
		o->m_origin[0] = 0;
	o->m_context_id = context_id;
	o->m_type_flags = type_flags;
#ifdef EXTENSION_SUPPORT
	o->m_is_extension = is_extension;
#endif // EXTENSION_SUPPORT
	return o;
}

void
PS_Manager::PostDeleteDataMessage(URL_CONTEXT_ID context_id, unsigned type_flags, const uni_char* origin, bool is_extension)
{
	if (type_flags && GetMessageHandler() != NULL)
	{
		OP_STATUS status = EnsureInitialization();
		if (OpStatus::IsError(status))
			ReportCondition(status);
		else
		{
			DeleteDataInfo* o = DeleteDataInfo::Create(context_id, type_flags, is_extension, origin);
			if (o == NULL)
				ReportCondition(OpStatus::ERR_NO_MEMORY);
			else
			{
				o->Into(&m_to_delete);
				if (!GetMessageHandler()->PostMessage(MSG_DB_MODULE_DELAYED_DELETE_DATA, GetMessageQueueId(), 0, 0))
					ReportCondition(OpStatus::ERR_NO_MEMORY);
			}
		}
	}
}

void
PS_Manager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(par1 == GetMessageQueueId());

	switch(msg)
	{
	case MSG_DB_MANAGER_FLUSH_INDEX_ASYNC:
	{
		//during module destruction messages are no longer flushed
		IndexByContext* index = GetIndex(static_cast<URL_CONTEXT_ID>(par2));
		if (index != NULL)
			index->UnsetPostedFlushMessage();

		ReportCondition(FlushIndexToFile(static_cast<URL_CONTEXT_ID>(par2)));
		break;
	}
	case MSG_DB_MODULE_DELAYED_DELETE_DATA:
	{
		ReportCondition(FlushDeleteQueue(TRUE));
		break;
	}
	default:
		OP_ASSERT(!"Warning: unhandled message received");
	}
}

OP_STATUS PS_Manager::FlushDeleteQueue(BOOL apply)
{
	if (!apply)
	{
		DeleteDataInfo* o;
		while((o = m_to_delete.First()) != NULL)
		{
			o->Out();
			op_free(o);
		}

		return OpStatus::OK;
	}

	class GetIdsItr : public PS_MgrContentIterator {
	public:
		GetIdsItr() : has_read_all(FALSE) {}
		BOOL has_read_all;
		OpINT32Set ids;
		virtual OP_STATUS HandleContextId(URL_CONTEXT_ID context_id)
		{
			return ids.Add(static_cast<INT32>(context_id));
		}
	} ids;

	class DeleteDataItr : public OpINT32Set::ForEachIterator {
	public:
		DeleteDataItr(PS_Manager *mgr)
			: m_mgr(mgr)
			, m_purge_orphans(false)
#ifdef EXTENSION_SUPPORT
			, m_is_extension(false)
#endif // EXTENSION_SUPPORT
			, m_status(OpStatus::OK) {}
		PS_Manager *m_mgr;
		bool m_purge_orphans;
#ifdef EXTENSION_SUPPORT
		bool m_is_extension;
#endif // EXTENSION_SUPPORT
		OP_STATUS m_status;
		PS_ObjectType m_type;
		const uni_char *m_origin;
		virtual void HandleInteger(INT32 context_id)
		{
			if (m_purge_orphans)
				m_status = OpDbUtils::GetOpStatusError(m_status, m_mgr->DeleteOrphanFiles(static_cast<URL_CONTEXT_ID>(context_id)));
			else
			{
				if (IndexByContext *idx = m_mgr->GetIndex(static_cast<URL_CONTEXT_ID>(context_id)))
				{
#ifdef EXTENSION_SUPPORT
					if (idx->IsExtensionContext() != m_is_extension)
						return;
#endif // EXTENSION_SUPPORT

					m_status = OpDbUtils::GetOpStatusError(m_status, m_mgr->DeleteObjects(m_type, static_cast<URL_CONTEXT_ID>(context_id), m_origin, FALSE));

#if defined EXTENSION_SUPPORT && defined WEBSTORAGE_WIDGET_PREFS_SUPPORT
					if (m_is_extension && m_type == KWidgetPreferences)
						g_gadget_manager->OnExtensionStorageDataClear(context_id, OpGadget::PREFS_APPLY_DATA_CLEARED);
#endif // defined EXTENSION_SUPPORT && defined WEBSTORAGE_WIDGET_PREFS_SUPPORT
				}
			}
		}
	} deldata(this);

	OP_STATUS status = OpStatus::OK;
	for (DeleteDataInfo* o = m_to_delete.First(); o != NULL; o = o->Suc())
	{
		//to understand what this cycle is doing, check PersistentStorageCommander::*
		for (unsigned k = KDBTypeStart + 1, mask = 1; k < KDBTypeEnd; k++, mask <<=1)
		{
			if (mask & o->m_type_flags)
			{
				deldata.m_type = static_cast<PS_ObjectType>(k);
				deldata.m_origin = o->m_origin;
#ifdef EXTENSION_SUPPORT
				deldata.m_is_extension = o->m_is_extension;
#endif // EXTENSION_SUPPORT

				if (o->m_context_id == PersistentStorageCommander::ALL_CONTEXT_IDS)
				{
					if (!ids.has_read_all)
					{
						status = OpDbUtils::GetOpStatusError(status, EnumerateContextIds(&ids));
						ids.has_read_all = TRUE;
					}
					ids.ids.ForEach(&deldata);
				}
				else
				{
					if (!ids.has_read_all)
						status = OpDbUtils::GetOpStatusError(status, ids.ids.Add(o->m_context_id));
					deldata.HandleInteger(static_cast<INT32>(o->m_context_id));
				}
			}
		}
	}

	deldata.m_purge_orphans = TRUE;
	ids.ids.ForEach(&deldata);

	FlushDeleteQueue(FALSE);

	return status;
}

static
OP_STATUS DeleteOrphanFilesAux(const uni_char *folder_path, unsigned base_folder_path_length,
	unsigned depth, const OpConstStringHashSet &existing_file_set, BOOL &folder_is_empty)
{
	OpFolderLister *lister;
	RETURN_IF_ERROR(OpFolderLister::Create(&lister));
	OpAutoPtr<OpFolderLister> lister_ptr(lister);
	RETURN_IF_ERROR(lister->Construct(folder_path, UNI_L("*")));
	OpString rel_name_nojournal;

	folder_is_empty = TRUE;

	while (lister->Next())
	{
		if (uni_str_eq(lister->GetFileName(), ".")  ||
			uni_str_eq(lister->GetFileName(), ".."))
			continue;

		if (depth == 0 && uni_str_eq(lister->GetFileName(), DATABASE_INTERNAL_PERSISTENT_INDEX_FILENAME))
		{
			folder_is_empty = FALSE;
			continue;
		}

		if (lister->IsFolder())
		{
			BOOL sub_folder_is_empty = FALSE;
			RETURN_IF_ERROR(DeleteOrphanFilesAux(lister->GetFullPath(), base_folder_path_length, depth + 1, existing_file_set, sub_folder_is_empty));
			if (sub_folder_is_empty)
			{
				DB_DBG(("DELETING ORPHAN FOLDER: %S\n", lister->GetFullPath()));
				OpFile orphan_folder;
				RETURN_IF_MEMORY_ERROR(orphan_folder.Construct(lister->GetFullPath()));
				RETURN_IF_MEMORY_ERROR(orphan_folder.Delete());
			}
			else
				folder_is_empty = FALSE;
		}
		else
		{
			const uni_char *rel_name = lister->GetFullPath() + base_folder_path_length;

			BOOL not_found = !existing_file_set.Contains(rel_name);
#ifdef SQLITE_SUPPORT
			if (not_found)
			{
				// Check if this is a sqlite journal file and if so, strip the-journal suffix
				// and check if the main file exists.
				unsigned rel_name_length = uni_strlen(rel_name);
				if (8 < rel_name_length && uni_str_eq(rel_name + rel_name_length - 8, "-journal"))
				{
					RETURN_IF_MEMORY_ERROR(rel_name_nojournal.Set(rel_name, rel_name_length - 8));
					not_found = !existing_file_set.Contains(rel_name_nojournal.CStr());
				}
			}
#endif // SQLITE_SUPPORT
			if (not_found)
			{
				DB_DBG(("DELETING ORPHAN FILE: %S\n", lister->GetFullPath()));
				OpFile orphan_file;
				RETURN_IF_MEMORY_ERROR(orphan_file.Construct(lister->GetFullPath()));
				RETURN_IF_MEMORY_ERROR(orphan_file.Delete());
			}
			else
				folder_is_empty = FALSE;
		}
	}

	return OpStatus::OK;
}

OP_STATUS PS_Manager::DeleteOrphanFiles(URL_CONTEXT_ID context_id)
{
	DB_DBG(("PS_Manager::DeleteOrphanFiles(%u)\n", context_id));

	PS_Policy* def_policy = g_database_policies->GetPolicy(PS_ObjectTypes::KDBTypeStart);
	OP_ASSERT(def_policy != NULL);

	const uni_char* profile_folder = def_policy->GetAttribute(PS_Policy::KMainFolderPath, context_id);
	RETURN_OOM_IF_NULL(profile_folder);
	if (*profile_folder == 0)
		return OpStatus::ERR_NO_ACCESS;

	const uni_char* storage_folder = def_policy->GetAttribute(PS_Policy::KSubFolder, context_id);
	OP_ASSERT(storage_folder && *storage_folder);

	TempBuffer folder_path;
	RETURN_IF_ERROR(folder_path.Append(profile_folder));
	ENSURE_PATHSEPCHAR(folder_path);
	unsigned base_folder_path_length = folder_path.Length();
	RETURN_IF_ERROR(folder_path.Append(storage_folder));

	class FileItr : public PS_MgrContentIterator {
	public:
		PS_Manager *m_mgr;
		OpConstStringHashSet m_file_name_set;
		FileItr(PS_Manager *mgr) : m_mgr(mgr){}
		~FileItr() {}
		virtual OP_STATUS HandleType(URL_CONTEXT_ID context_id, PS_ObjectType type) {
			return m_mgr->EnumerateOrigins(this, context_id, type);
		}
		virtual OP_STATUS HandleOrigin(URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin) {
			return m_mgr->EnumerateObjects(this, context_id, type, origin);
		}
		virtual OP_STATUS HandleObject(PS_IndexEntry* object_entry) {
			if (object_entry->IsPersistent() &&
				object_entry->GetFileRelPath() != NULL &&
				!OpDbUtils::IsFilePathAbsolute(object_entry->GetFileRelPath()))
			{
				RETURN_IF_MEMORY_ERROR(m_file_name_set.Add(object_entry->GetFileRelPath()));
			}
			return OpStatus::OK;
		}
	} itr(this);

	RETURN_IF_ERROR(EnumerateTypes(&itr, context_id));

	BOOL folder_is_empty; // This is unused.

	// 1st arg, folder to search for. First calls searches the pstorage folder
	// 2nd arg, length of full path of the profile folder incl leading slash, because the relative path names are relative to it
	return DeleteOrphanFilesAux(folder_path.GetStorage(), base_folder_path_length, 0, itr.m_file_name_set, folder_is_empty);
}

void
PS_Manager::ParseFileNameNumber(URL_CONTEXT_ID context_id, const uni_char* filename)
{
	if (filename == NULL)
		return;

	unsigned filename_length = uni_strlen(filename);
	// Generated file names have format .*/xx/xx/xxxxxxxx.
	// Everything else would therefore have been added manually
	// to the index, so we ignore those. We only need to check
	// for collisions against the files that have the proper format.
	if (filename_length < 15)
		return;
	for(unsigned ofs = filename_length - 15, k = 0; k < 15; k++){
		if (k == 0 || k == 3 || k == 6) {
			if (filename[ofs + k] != PATHSEPCHAR)
				return;
		}
		else {
			if (!uni_isxdigit(filename[ofs + k]))
				return;
		}
	}

	unsigned type_nr = uni_strtoul(filename + filename_length - 14, NULL, 16);
	unsigned hash_nr = uni_strtoul(filename + filename_length - 11, NULL, 16);
	unsigned file_nr = uni_strtoul(filename + filename_length -  8, NULL, 16);

	// Check if the numbers are out of range indexes.
	if (PS_ObjectTypes::KDBTypeEnd <= type_nr || m_max_hash_amount <= hash_nr)
		return;

	// If checks exists because the file path might have been edited manually
	// and therefore unsyched with the values that would have been generated.
	if (IndexByContext* data_0 = GetIndex(context_id))
		if (IndexEntryByOriginHash** data_1 = data_0->m_object_index[type_nr])
			if (IndexEntryByOriginHash* data_2 = data_1[hash_nr])
				if (data_2->m_highest_filename_number <= file_nr)
					// Might rollover, but it's really not a problem.
					data_2->m_highest_filename_number = file_nr + 1;
	DB_DBG(("PS_Manager::ParseFileNameNumber(%S): (0x%x), (0x%x), (0x%x)\n", filename, type_nr, hash_nr, file_nr));
}

PS_Manager::IndexEntryByOriginHash::~IndexEntryByOriginHash()
{
	m_objects.DeleteAll(); // To be safe
	m_origin_cached_info.DeleteAll();
}

unsigned
PS_Manager::IndexEntryByOriginHash::GetNumberOfDbs(const uni_char* origin) const
{
	OriginCachedInfo* ci = GetCachedEntry(origin);
	if (ci != NULL)
	{
		return ci->m_number_of_dbs;
	}
	return 0;
}

void
PS_Manager::IndexEntryByOriginHash::DecNumberOfDbs(const uni_char* origin)
{
	OP_ASSERT(GetNumberOfDbs(origin)>0 || !m_origin_cached_info.Contains(origin));
	OriginCachedInfo* ci = GetCachedEntry(origin);
	if (ci != NULL)
		ci->m_number_of_dbs--;
}

OP_STATUS
PS_Manager::IndexEntryByOriginHash::SetNumberOfDbs(const uni_char* origin, unsigned number_of_dbs)
{
	if (origin == NULL || origin[0] == 0) return OpStatus::OK;
	OriginCachedInfo* ci;
	RETURN_IF_ERROR(MakeCachedEntry(origin, &ci));
	OP_ASSERT(ci != NULL);
	ci->m_number_of_dbs = number_of_dbs;
	return OpStatus::OK;
}

OP_STATUS
PS_Manager::IndexEntryByOriginHash::SetCachedDataSize(const uni_char* origin, OpFileLength new_size)
{
	if (origin == NULL || origin[0] == 0) return OpStatus::OK;
	OriginCachedInfo* ci;
	RETURN_IF_ERROR(MakeCachedEntry(origin, &ci));
	OP_ASSERT(ci != NULL);
	ci->m_cached_data_size = new_size;
	return OpStatus::OK;
}

void
PS_Manager::IndexEntryByOriginHash::UnsetCachedDataSize(const uni_char* origin)
{
	OriginCachedInfo* ci = GetCachedEntry(origin);
	if (ci != NULL)
	{
		ci->m_cached_data_size = FILE_LENGTH_NONE;
	}
}

BOOL
PS_Manager::IndexEntryByOriginHash::HasCachedDataSize(const uni_char* origin) const
{
	OriginCachedInfo* ci = GetCachedEntry(origin);
	if (ci != NULL)
	{
		return ci->m_cached_data_size != FILE_LENGTH_NONE;
	}
	return FALSE;
}

OpFileLength
PS_Manager::IndexEntryByOriginHash::GetCachedDataSize(const uni_char* origin) const
{
	OriginCachedInfo* ci = GetCachedEntry(origin);
	if (ci != NULL)
	{
		return ci->m_cached_data_size;
	}
	return FILE_LENGTH_NONE;
}

PS_Manager::IndexEntryByOriginHash::OriginCachedInfo*
PS_Manager::IndexEntryByOriginHash::GetCachedEntry(const uni_char* origin) const
{
	OriginCachedInfo* ci;
	if (origin != NULL && origin[0] && OpStatus::IsSuccess(m_origin_cached_info.GetData(origin, &ci)))
		return ci;
	return NULL;
}

OP_STATUS
PS_Manager::IndexEntryByOriginHash::MakeCachedEntry(const uni_char* origin, OriginCachedInfo** ci)
{
	if (origin != NULL && origin[0] && (*ci = GetCachedEntry(origin)) == NULL)
	{
		*ci = (OriginCachedInfo*) OP_NEWA(byte, (sizeof(OriginCachedInfo) + UNICODE_SIZE(uni_strlen(origin))));
		RETURN_OOM_IF_NULL(*ci);
		uni_strcpy((*ci)->m_origin, origin);
		(*ci)->m_number_of_dbs = 0;
		(*ci)->m_cached_data_size = FILE_LENGTH_NONE;
		OP_STATUS status = m_origin_cached_info.Add((*ci)->m_origin, *ci);
		if (OpStatus::IsError(status))
		{
			OP_DELETEA(*ci);
			return status;
		}
	}
	return OpStatus::OK;
}

#ifdef DATABASE_MODULE_DEBUG
void PS_Manager::DumpIndexL(URL_CONTEXT_ID context_id, const uni_char* msg)
{
	if (!IsInitialized())
		return;

	if (context_id == DB_NULL_CONTEXT_ID && m_object_index.GetCount() != 0)
	{
		OpHashIterator* itr = m_object_index.GetIterator();
		LEAVE_IF_NULL(itr);
		OP_STATUS status = OpStatus::OK, status2 = OpStatus::OK;
		if (OpStatus::IsSuccess(itr->First()))
		{
			do {
				URL_CONTEXT_ID ctx_id = reinterpret_cast<URL_CONTEXT_ID>(itr->GetKey());
				OP_ASSERT(ctx_id != DB_NULL_CONTEXT_ID);
				TRAP(status2, DumpIndexL(ctx_id, msg));
				status = OpDbUtils::GetOpStatusError(status, status2);
			} while (OpStatus::IsSuccess(itr->Next()));
		}
		OP_DELETE(itr);
		LEAVE_IF_ERROR(status);
		return;
	}

	int output_index = 1;

	PS_IndexIterator *itr = GetIteratorL(context_id, KDBTypeStart, FALSE, PS_IndexIterator::ORDERED_DESCENDING);
	OP_ASSERT(itr != NULL);
	dbg_printf("PS_Manager::DumpIndexL(%d, %S)\n", context_id, msg);
	if (!itr->AtEndL())
	{
		do {
			const PS_IndexEntry* key = itr->GetItemL();
			OP_ASSERT(key);
			dbg_printf(" Database   %d\n", output_index++);
			dbg_printf("  Ctx:     %x\n", key->GetUrlContextId());
			dbg_printf("  Type:    %S\n", GetTypeDesc(key->GetType()));
			dbg_printf("  Origin:  %S\n", key->GetOrigin());
			dbg_printf("  Domain:  %S\n", key->GetDomain());
			dbg_printf("  Name:    %S\n", key->GetName());
			dbg_printf("  Memory:  %s\n", key->IsMemoryOnly()?"yes":"no");
			dbg_printf("  File:    %S\n", key->GetFileRelPath());
			dbg_printf("  Path:    %S\n", key->GetFileAbsPath());
			dbg_printf("  Version: %S\n", key->GetVersion());
			dbg_printf("  To del : %s\n", DB_DBG_BOOL(key->GetFlag(PS_IndexEntry::MARKED_FOR_DELETION)));
			dbg_printf("  Has obj: %s\n", DB_DBG_BOOL(key->GetObject() != NULL));

			dbg_printf("\n");
		} while(itr->MoveNextL());
	}
	else
	{
		dbg_printf("  no items\n");
	}
	OP_DELETE(itr);
}
#endif //DATABASE_MODULE_DEBUG

PS_IndexEntry*
PS_IndexEntry::Create(PS_Manager* mgr, URL_CONTEXT_ID context_id,
			PS_ObjectType type, const uni_char* origin,
			const uni_char* name, const uni_char* data_file_name,
			const uni_char* db_version, BOOL is_persistent,
			PS_Object* ps_obj)
{
	PS_IndexEntry* index = OP_NEW(PS_IndexEntry,(mgr));
	RETURN_VALUE_IF_NULL(index, NULL);

	ANCHOR_PTR(PS_IndexEntry, index);

	index->m_type = type;
	index->m_context_id = context_id;

#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
# ifdef DEBUG_ENABLE_OPASSERT
	if (type == KLocalStorage
		|| type == KSessionStorage
#  ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
		|| type == KWidgetPreferences
#  endif //defined WEBSTORAGE_WIDGET_PREFS_SUPPORT
# ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
		|| type == KUserJsStorage
# endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
		)
	{
		OP_ASSERT(name == NULL);
	}
	else
	{
		OP_ASSERT(name != NULL);
	}
# endif // DEBUG_ENABLE_OPASSERT
#endif // WEBSTORAGE_ENABLE_SIMPLE_BACKEND

	if (origin != NULL && origin[0])
	{
		index->m_origin = UniSetNewStr(origin);
		RETURN_VALUE_IF_NULL(index->m_origin, NULL);

#if defined WEBSTORAGE_ENABLE_SIMPLE_BACKEND && defined WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
		if (type != KUserJsStorage)
#endif // defined WEBSTORAGE_ENABLE_SIMPLE_BACKEND && defined WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
		{
			unsigned origin_length = uni_strlen(index->m_origin);

			//parse domain
			unsigned domain_start = 0, domain_end = origin_length, k = 0;
			for(; k < origin_length; k++)
			{
				if (index->m_origin[k] == ':' &&
					index->m_origin[k + 1] == '/')
				{
					do {
						k++;
					}
					while (!op_isalnum(index->m_origin[k]) && k < origin_length);
					if (k < origin_length)
						domain_start = k;
					break;
				}
				else if (!uni_isalpha(index->m_origin[k]))
					// Found junk before finding ':/' so it's serverless.
					break;
			}

			if (domain_start != 0)
			{
				for(; k < origin_length; k++)
					if (index->m_origin[k] == ':' || index->m_origin[k] == '/')
					{
						domain_end = k;
						break;
					}
				domain_end = domain_end - domain_start;
				uni_char* domain = OP_NEWA(uni_char, domain_end + 1);
				RETURN_VALUE_IF_NULL(domain, NULL);
				op_memcpy(domain, index->m_origin + domain_start, domain_end * sizeof(uni_char));
				domain[domain_end] = 0;
				index->m_domain = domain;
			}
		}
	}

	//after having the origin and the type, creating the file name will work
	if (data_file_name != NULL)
	{
		RETURN_VALUE_IF_ERROR(PS_DataFile::Create(index, data_file_name), NULL);
		OP_ASSERT(index->m_data_file_name != NULL);
	}

	if (name != NULL)
	{
		index->m_name = UniSetNewStr(name);
		RETURN_VALUE_IF_NULL(index->m_name, NULL);
	}
	if (db_version != NULL)
	{
		index->m_db_version = UniSetNewStr(db_version);
		RETURN_VALUE_IF_NULL(index->m_db_version, NULL);
	}

	index->m_ps_obj = ps_obj;
	index->SetFlag(OBJ_INITIALIZED);
	if (!is_persistent)
		index->SetFlag(MEMORY_ONLY_DB);

	ANCHOR_PTR_RELEASE(index);

	return index;
}

PS_IndexEntry::PS_IndexEntry(PS_Manager* mgr) :
	m_cached_file_size(0),
	m_manager(mgr),
	m_type(PS_ObjectTypes::KDBTypeStart),
	m_origin(NULL),
	m_name(NULL),
	m_db_version(NULL),
	m_context_id(DB_NULL_CONTEXT_ID),
	m_domain(NULL),
	m_data_file_name(NULL),
	m_ps_obj(NULL),
	m_res_policy(NULL),
	m_quota_status(QS_DEFAULT)
{}

void PS_IndexEntry::Destroy()
{
	if (m_ps_obj != NULL && !m_ps_obj->GetFlag(PS_Object::BEING_DELETED))
	{
		OP_DELETE(m_ps_obj);
		//The deconstructor on PS_Object sets the m_index_entry.m_object property
		//to NULL, so that's a sanity check making sure that happens.
		//There are multiple cases where either the index entry or the
		//ps_obj might be deleted, and these null assignments are
		//there to make sure nothing keeps deleted pointers.
		OP_ASSERT(m_ps_obj == NULL);
		m_ps_obj = NULL;
	}

	if (m_data_file_name != NULL)
	{
		OP_ASSERT(m_data_file_name == GetPSManager()->GetNonPersistentFileName() ||
				m_data_file_name->GetRefCount() == 1);
		PS_DataFile_ReleaseOwner(m_data_file_name, this)
		m_data_file_name->Release();
		m_data_file_name = NULL;
	}
	OP_ASSERT(GetRefCount() == 0);

	InvalidateCachedDataSize();
	UnsetFlag(OBJ_INITIALIZED | OBJ_INITIALIZED_ERROR);
	m_type = PS_ObjectTypes::KDBTypeStart;

	if (m_origin != m_domain)
		OP_DELETEA(m_domain);
	m_domain = NULL;

	OP_DELETEA(m_origin);
	m_origin = NULL;

	OP_DELETEA(m_name);
	m_name = NULL;

	OP_DELETEA(m_db_version);
	m_db_version = NULL;

}

BOOL PS_IndexEntry::CompareVersion(const uni_char* other) const
{
	return OpDbUtils::StringsEqual(
			GetVersion() != NULL && *GetVersion() ? GetVersion() : NULL,
			other      != NULL && *other        ? other        : NULL);
}

OP_STATUS PS_IndexEntry::MakeAbsFilePath()
{
	if (m_data_file_name == NULL)
	{
		RETURN_IF_ERROR(PS_DataFile::Create(this, NULL));
	}
	OP_ASSERT(m_data_file_name != NULL);
	return m_data_file_name->MakeAbsFilePath();
}

BOOL PS_IndexEntry::Delete()
{
	DB_DBG(("%p: PS_IndexEntry::Delete(" DB_DBG_ENTRY_STR "): %d, %p\n", this, DB_DBG_ENTRY_O(this), GetRefCount(), m_data_file_name))

	SetFlag(MARKED_FOR_DELETION);
	DeleteDataFile();
	if (!IsIndexBeingDeleted() &&
		!GetFlag(BEING_DELETED))
	{
		if (GetObject() != NULL)
		{
			OP_ASSERT(GetRefCount() != 0 || GetObject()->HasPendingActions());
			return GetObject()->PreClearData();
		}
		else
		{
			OP_ASSERT(GetRefCount() == 0);
			SetFlag(PURGE_ENTRY);
#ifdef SELFTEST
			//this flag is unset when flushing stuff to disk is which is something done async
			//for tests we want it to happen right away
			if (m_manager->m_self_test_instance) UnsetFlag(MARKED_FOR_DELETION);
#endif //SELFTEST
			return GetPSManager()->DeleteEntryNow(m_type, m_origin, m_name, !IsMemoryOnly(), m_context_id);
		}
	}
	return FALSE;
}

void PS_IndexEntry::HandleObjectShutdown(BOOL should_delete)
{
	DB_DBG(("%p: PS_IndexEntry::HandleObjectShutdown(" DB_DBG_ENTRY_STR "): %d, del: %s\n", this, DB_DBG_ENTRY_O(this), GetRefCount(), DB_DBG_BOOL(should_delete)))

	OP_ASSERT(GetRefCount()==0);
	m_ps_obj = NULL;
	if ( should_delete || GetFlag(MARKED_FOR_DELETION) || IsMemoryOnly() || (GetFileRelPath() == NULL))
	{
		Delete();
	}
}

OP_STATUS
PS_IndexEntry::GetDataFileSize(OpFileLength* file_size)
{
	DB_ASSERT_RET(IsInitialized(), PS_Status::ERR_INVALID_STATE);

	// Many steps involved in getting the data file size, read on
	OP_ASSERT(file_size != NULL);
	if (!GetFlag(HAS_CACHED_DATA_SIZE))
	{
		OpFileLength new_size = 0;
		BOOL has_size = FALSE;

		//1. evaluate the amount of data in memory
		PS_Object *obj = GetObject();
		if (obj != NULL && obj->IsInitialized())
		{
			OP_STATUS status = obj->EvalDataSizeSync(&new_size);
			if (OpStatus::ERR_YIELD != status)
			{
				RETURN_IF_ERROR(status);
				has_size = TRUE;
			}
		}

		//2 .if we don't have the ps_obj object in memory, query file size directly
		if (!has_size)
		{
			BOOL file_exists = FALSE;
			RETURN_IF_ERROR(FileExists(&file_exists));
			if (file_exists)
			{
				OP_ASSERT(GetOpFile() != NULL);
				RETURN_IF_ERROR(GetOpFile()->GetFileLength(new_size));
				has_size = TRUE;
			}
		}

		if (!has_size)
			new_size = 0;
		m_cached_file_size = new_size;
		SetFlag(HAS_CACHED_DATA_SIZE);
	}
	*file_size = m_cached_file_size;
	return OpStatus::OK;
}

BOOL
PS_IndexEntry::DeleteDataFile()
{
	if (m_data_file_name == NULL || IsMemoryOnly())
		return FALSE;

	if (GetQuotaHandlingStatus() != QS_WAITING_FOR_USER)
		SetQuotaHandlingStatus(QS_DEFAULT);

	//this will tag that the file needs to be deleted
	PS_DataFile_ReleaseOwner(m_data_file_name, this)
	m_data_file_name->SetWillBeDeleted();
	m_data_file_name->Release();
	m_data_file_name = NULL;

	//after deleting the data file, the version no longer applies
	OP_DELETEA(m_db_version);
	m_db_version = NULL;

	return TRUE;
}

void PS_IndexEntry::SetDataFileSize(const OpFileLength& file_size)
{
	if (!GetFlag(HAS_CACHED_DATA_SIZE) || m_cached_file_size != file_size)
	{
		GetPSManager()->InvalidateCachedDataSize(GetType(), GetUrlContextId(), GetOrigin());
		m_cached_file_size = file_size;
		SetFlag(HAS_CACHED_DATA_SIZE);
	}
}

PS_Policy* PS_IndexEntry::GetPolicy() const
{
	return m_res_policy != NULL ? m_res_policy : g_database_policies->GetPolicy(m_type);
}

BOOL PS_IndexEntry::IsEqual(const PS_IndexEntry& rhs) const
{
	return &rhs == this || IsEqual(rhs.m_type, rhs.m_origin, rhs.m_name, !rhs.IsMemoryOnly(), m_context_id);
}

BOOL PS_IndexEntry::IsEqual(PS_ObjectType type, const uni_char* origin,
		const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id) const
{
	DB_ASSERT_RET(IsInitialized(), FALSE);

	is_persistent = !!is_persistent;

	return IsMemoryOnly() != is_persistent &&
		m_type == type &&
		m_context_id == context_id &&
		OpDbUtils::StringsEqual(m_origin, origin) &&
		OpDbUtils::StringsEqual(m_name, name);
}

#undef RETURN_SIGN_IF_NON_0
#define RETURN_SIGN_IF_NON_0(x) do {int y=static_cast<int>(x);if(y!=0)return y<0?order:-order;} while(0)

int PS_IndexEntry::Compare(const PS_IndexEntry *lhs, const PS_IndexEntry *rhs, int order)
{
	OP_ASSERT(lhs && rhs);
	OP_ASSERT(order == -1 || order == 1);

	RETURN_SIGN_IF_NON_0(rhs->m_context_id - lhs->m_context_id);
	RETURN_SIGN_IF_NON_0(rhs->m_type - lhs->m_type);

	if (rhs->m_origin != NULL && lhs->m_origin != NULL)
		RETURN_SIGN_IF_NON_0(uni_stricmp(rhs->m_origin, lhs->m_origin));
	else
		RETURN_SIGN_IF_NON_0(PTR_TO_INT(rhs->m_origin) - PTR_TO_INT(lhs->m_origin));

	if (rhs->m_name != NULL && lhs->m_name != NULL)
		RETURN_SIGN_IF_NON_0(uni_stricmp(rhs->m_name, lhs->m_name));
	else
		RETURN_SIGN_IF_NON_0(PTR_TO_INT(rhs->m_name) - PTR_TO_INT(lhs->m_name));

	return 0;
}
#undef RETURN_SIGN_IF_NON_0

/*static*/PS_Object*
PS_Object::Create(PS_IndexEntry* key)
{
	switch(key->GetType())
	{
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	case PS_ObjectTypes::KSessionStorage:
	case PS_ObjectTypes::KLocalStorage:
		return WebStorageBackend_SimpleImpl::Create(key);
#endif // WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#ifdef DATABASE_STORAGE_SUPPORT
	case PS_ObjectTypes::KWebDatabases:
		return WSD_Database::Create(key);
#endif
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case PS_ObjectTypes::KWidgetPreferences:
		return WebStorageBackend_SimpleImpl::Create(key);
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	case PS_ObjectTypes::KUserJsStorage:
		return WebStorageBackend_SimpleImpl::Create(key);
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	}

	OP_ASSERT(!"Bad type !");
	return NULL;
}

/*static*/OP_STATUS
PS_Object::DeleteInstance(PS_ObjectType type, const uni_char* origin,
		const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id)
{
	return g_database_manager->DeleteObject(type, origin, name, is_persistent, context_id);
}
/*static*/OP_STATUS
PS_Object::DeleteInstances(PS_ObjectType type, URL_CONTEXT_ID context_id, const uni_char *origin, BOOL only_persistent)
{
	return g_database_manager->DeleteObjects(type, context_id, origin, only_persistent);
}
/*static*/OP_STATUS
PS_Object::DeleteInstances(PS_ObjectType type, URL_CONTEXT_ID context_id, BOOL only_persistent)
{
	return g_database_manager->DeleteObjects(type, context_id, only_persistent);
}

/*static*/OP_STATUS
PS_Object::GetInstance(PS_ObjectType type, const uni_char* origin,
		const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id, PS_Object** object)
{
	return g_database_manager->GetObject(type, origin, name, is_persistent, context_id, object);
}

PS_Object::PS_Object(PS_IndexEntry* entry) : m_index_entry(entry)
{
	OP_ASSERT(entry != NULL);
	entry->m_ps_obj = this;
}

#ifdef OPSTORAGE_SUPPORT

PS_ObjectTypes::PS_ObjectType WebStorageTypeToPSObjectType(WebStorageType ws_t)
{
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	switch(ws_t)
	{
	case WEB_STORAGE_SESSION:
		return PS_ObjectTypes::KSessionStorage;

	case WEB_STORAGE_LOCAL:
		return PS_ObjectTypes::KLocalStorage;

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case WEB_STORAGE_WGT_PREFS:
		return PS_ObjectTypes::KWidgetPreferences;
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	case WEB_STORAGE_USERJS:
		return PS_ObjectTypes::KUserJsStorage;
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	}
#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND

	OP_ASSERT(!"Missing storage type");
	return PS_ObjectTypes::KDBTypeStart;
}

WebStorageType PSObjectTypeToWebStorageType(PS_ObjectTypes::PS_ObjectType db_t)
{
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	switch(db_t)
	{
	case PS_ObjectTypes::KSessionStorage:
		return WEB_STORAGE_SESSION;

	case PS_ObjectTypes::KLocalStorage:
		return WEB_STORAGE_LOCAL;

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case PS_ObjectTypes::KWidgetPreferences:
		return WEB_STORAGE_WGT_PREFS;
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	case PS_ObjectTypes::KUserJsStorage:
		return WEB_STORAGE_USERJS;
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	}
#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND

	OP_ASSERT(!"Missing storage type");
	return WEB_STORAGE_START;
}

#endif // OPSTORAGE_SUPPORT

#endif	//DATABASE_MODULE_MANAGER_SUPPORT
