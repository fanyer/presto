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

# include "adjunct/desktop_util/file_utils/folder_recurse.h"
# include "adjunct/desktop_util/opfile/desktop_opfile.h"
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/engine/message.h"
# include "adjunct/m2/src/engine/store/mboxstore.h"
# include "adjunct/m2/src/import/ImporterModel.h"
# include "adjunct/m2/src/import/MboxImporter.h"
# include "modules/locale/locale-enum.h"
# include "modules/locale/oplanguagemanager.h"

MboxImporter::MboxImporter()
:	m_mboxFile(NULL),
	m_raw(NULL),
	m_raw_length(0),
	m_raw_capacity(0),
	m_one_line_ahead(NULL),
	m_two_lines_ahead(NULL),
	m_finished_reading(TRUE),
	m_found_start_of_message(FALSE),
	m_found_start_of_next_message(FALSE),
	m_found_valid_message(FALSE),
	m_folder_recursor(NULL)
{
}

MboxImporter::~MboxImporter()
{
	if (m_mboxFile)
		OP_DELETE(m_mboxFile);

	OP_DELETEA(m_raw);
	OP_DELETEA(m_one_line_ahead);
	OP_DELETEA(m_two_lines_ahead);
}

OP_STATUS MboxImporter::InitLookAheadBuffers()
{
	if (!m_one_line_ahead)
	{
		m_one_line_ahead = OP_NEWA(char, LOOKAHEAD_BUF_LEN+1);
		if (!m_one_line_ahead)
			return OpStatus::ERR_NO_MEMORY;
		m_one_line_ahead[0] = m_one_line_ahead[LOOKAHEAD_BUF_LEN] = 0;
	}

	if (!m_two_lines_ahead)
	{
		m_two_lines_ahead = OP_NEWA(char, LOOKAHEAD_BUF_LEN+1);
		if (!m_two_lines_ahead)
		{
			OP_DELETEA(m_one_line_ahead);
			return OpStatus::ERR_NO_MEMORY;
		}
		m_two_lines_ahead[0] = m_two_lines_ahead[LOOKAHEAD_BUF_LEN] = 0;
	}

	m_finished_reading = FALSE;

	return OpStatus::OK;
}

OP_STATUS MboxImporter::Init()
{
	OpFileInfo::Mode	test_for_dir_mode;
	OpFile*				test_for_dir_file = NULL;

	GetModel()->DeleteAll();
	
	GetModel()->SetSortListener(this);

	if (m_file_list.GetCount() == 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(InitLookAheadBuffers());

	for (UINT32 i = 0; i < m_file_list.GetCount(); i++)
	{
		test_for_dir_file = OP_NEW(OpFile, ());
		if(test_for_dir_file && OpStatus::IsSuccess(test_for_dir_file->Construct(m_file_list.Get(i)->CStr())) )
		{
			if( OpStatus::IsSuccess(test_for_dir_file->GetMode(test_for_dir_mode)) )
			{
				OP_DELETE(test_for_dir_file);
				if(test_for_dir_mode == OpFileInfo::DIRECTORY)
				{
					OpString rootString;
					rootString.Set(m_file_list.Get(i)->CStr());
					if (rootString.Length() > 1 && rootString[rootString.Length() - 1] == PATHSEPCHAR)
						rootString.Delete(rootString.Length() - 1); 

					SetRecursorRoot(rootString);

					while (m_folder_recursor)
					{
						OpFile* file_runner;
						OP_STATUS sts = m_folder_recursor->GetNextFile(file_runner);
						if (OpStatus::IsSuccess(sts))
						{
							if (file_runner)
							{
								OpString mbox;
								sts = mbox.Set(file_runner->GetFullPath());
								if (OpStatus::IsError(sts))
								{
									OP_DELETE(m_folder_recursor);
									m_folder_recursor = NULL;
									OP_DELETE(file_runner);
								}
								if (IsValidMboxFile(mbox.CStr()))
								{
									OpString virtual_path;
									INT32 index = GetOrCreateFolder(rootString, *file_runner, virtual_path);
									InitSingleMbox(mbox, virtual_path, index);
								}
								OP_DELETE(file_runner);
							}
							else
							{
								OP_DELETE(m_folder_recursor);
								m_folder_recursor = NULL;
							}
						}
						else
						{
							OP_DELETE(m_folder_recursor);
							m_folder_recursor = NULL;
						}
					}
					continue;
				}
			}
			else
				OP_DELETE(test_for_dir_file);
		}
		OpString empty;
		InitSingleMbox(*m_file_list.Get(i), empty);
	}
	return OpStatus::OK;
}

void MboxImporter::InitSingleMbox(const OpString& mbox, OpString& virtual_path, INT32 index /*=-1*/)
{
	if (IsValidMboxFile(mbox.CStr()))
	{
		int sep = mbox.FindLastOf(PATHSEPCHAR);
	
		if (KNotFound == sep)
			return;
	
		OpString name;
		name.Set(mbox.CStr() + sep + 1);
	
		OpString path;
		path.Set(mbox.CStr());
	
		if (virtual_path.IsEmpty())
			virtual_path.Set(name.CStr());

		OpString virtual_folder;
		virtual_folder.Set(virtual_path.CStr());
#ifdef MSWIN
		uni_char * backslash = virtual_folder.CStr();
		while((backslash = uni_strchr(backslash, PATHSEPCHAR)) != NULL)
		{
			*backslash = (uni_char)'/';
		}
#endif
	
/*		OpString imported;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_IMPORTED, imported));
		OpStatus::Ignore(imported.Insert(0, UNI_L(" (")));
		OpStatus::Ignore(imported.Append(UNI_L(")")));
		OpStatus::Ignore(virtual_path.Append(imported));
 RFZTODO: Discuss the need for this... 	
*/
		ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_MAILBOX_TYPE,
														name, path, virtual_folder, UNI_L("")));
		if (item)
			/*INT32 branch = */
			GetModel()->AddSorted(item, index);
	}
}

void MboxImporter::SetImportItems(const OpVector<ImporterModelItem>& items)
{
	GetModel()->EmptySequence(m_sequence);

	UINT32 count = items.GetCount();

	for (UINT32 i = 0; i < count; i++)
	{
		ImporterModelItem* item = items.Get(i);
		GetModel()->MakeSequence(m_sequence, item, TRUE);
	}
}

OP_STATUS MboxImporter::ImportMessages()
{
	OpenPrefsFile();
	return StartImport();
}

BOOL MboxImporter::OnContinueImport()
{
	OP_NEW_DBG("MboxImporter::OnContinueImport", "m2");

	if (m_stopImport)
		return FALSE;

	if (m_mboxFile)
	{
		ImportMboxAsync();
		return TRUE;
	}
	
	if (GetModel()->SequenceIsEmpty(m_sequence))
		return FALSE;

	ImporterModelItem* item = GetModel()->PullItem(m_sequence);

	if (!item)
		return FALSE;

	m_moveCurrentToSent = m_moveToSentItem ? (item == m_moveToSentItem) : FALSE;

	if (item->GetType() != OpTypedObject::IMPORT_MAILBOX_TYPE)
		return TRUE;

	OP_DBG(("MboxImporter::OnContinueImport() box = %S", item->GetName().CStr()));
	m_m2FolderPath.Set(item->GetVirtualPath());

	OpString acc;
	GetImportAccount(acc);
	m_currInfo.Empty();
	m_currInfo.Set(acc);
	m_currInfo.Append(UNI_L(" - "));
	m_currInfo.Append(item->GetName());

	OpString progressInfo;
	progressInfo.Set(m_currInfo);

	if (InResumeCache(m_m2FolderPath))
	{
		if (m_mboxFile)
		{
			OP_DELETE(m_mboxFile);
			m_mboxFile = NULL;
		}

		OpString already_imported;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ALREADY_IMPORTED, already_imported));
		OpStatus::Ignore(progressInfo.Append(UNI_L(" ")));
		OpStatus::Ignore(progressInfo.Append(already_imported));
		MessageEngine::GetInstance()->OnImporterProgressChanged(this, progressInfo, 0, 0, TRUE);

		return TRUE;
	}

	OP_ASSERT(!m_mboxFile);
	
	m_mboxFile = OP_NEW(OpFile, ());
	if (m_mboxFile && OpStatus::IsSuccess(m_mboxFile->Construct(item->GetPath().CStr())))
		InitMboxFile();
	else
	{
		OP_DELETE(m_mboxFile);
		m_mboxFile = NULL;
	}
	
	return TRUE;
}

OP_STATUS MboxImporter::InitMboxFile()
{
	OP_STATUS sts;
	OP_ASSERT(m_mboxFile);
	
	sts = m_mboxFile->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE);
	if (OpStatus::IsError(sts))
	{
		OP_DELETE(m_mboxFile);
		m_mboxFile = NULL;
		return sts;
	}
	
	m_finished_reading = FALSE;
	m_one_line_ahead[0] = m_two_lines_ahead[0] = 0;
	struct stat buf;
	if (DesktopOpFileUtils::Stat(m_mboxFile, &buf) != OpStatus::OK)
		m_totalCount = 0;
	else
		m_totalCount = buf.st_size;

	m_count = 0;
	OP_DELETEA(m_raw);
	m_raw_capacity = min(IMPORT_CHUNK_CAPACITY, m_totalCount + 1);
	m_raw = OP_NEWA(char, m_raw_capacity);

	if (m_raw)
		m_raw[0] = '\0';
	else
		return OpStatus::ERR_NO_MEMORY;

	m_raw_length = 0;
	MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount, TRUE);
	return OpStatus::OK;
}

void MboxImporter::OnCancelImport()
{
	OP_NEW_DBG("MboxImporter::OnCancelImport", "m2");
	OP_DBG(("MboxImporter::OnCancelImport()"));

	OP_DELETEA(m_raw);
	m_raw = NULL;

	if (m_mboxFile)
	{
		OP_DELETE(m_mboxFile);
		m_mboxFile = NULL;
	}
}

void MboxImporter::ImportMboxAsync()
{
	OP_NEW_DBG("MboxImporter::ImportMboxAsync", "m2");

	if (!m_one_line_ahead || !m_two_lines_ahead)
	{
		OP_ASSERT(0);
		return;
	}

	static INT32 old = 0;

	strcpy(m_one_line_ahead, m_two_lines_ahead);
	m_two_lines_ahead[0] = 0;

	if (m_finished_reading ||
		DesktopOpFileUtils::ReadLine(m_two_lines_ahead, LOOKAHEAD_BUF_LEN, m_mboxFile)!=OpStatus::OK)
	{
		m_two_lines_ahead[0] = 0;
	}

	//Check if this really is start of a new message, or simply an invalid mbox-file (where "From " is not space-stuffed)
	if (strlen(m_one_line_ahead) > 5 && ((m_one_line_ahead[0] == 'F' && strni_eq(m_one_line_ahead, "FROM ", 5))))
	{
		if (m_found_valid_message)
			m_found_start_of_next_message = TRUE;
		else
			m_found_start_of_message = TRUE;
	}

	//Check if the next line most likely is not a header
	if (m_found_start_of_message || m_found_start_of_next_message)
	{
		if (*m_two_lines_ahead &&
			(*m_two_lines_ahead=='<' ||
			 (*m_two_lines_ahead=='>' && !strni_eq(m_two_lines_ahead, ">FROM ", 6)) ||
			 strni_eq(m_two_lines_ahead, "HTTP:", 5)) )
		{
			if (m_found_start_of_message)
				m_found_start_of_message = FALSE;
			else
				m_found_start_of_next_message = FALSE;
		}
	}

	//Check if the next line is a header. If not, don't save it (we're in the middle of a message body)
	if (m_found_start_of_message || m_found_start_of_next_message)
	{
		char* header_name_ptr = m_two_lines_ahead;
		while (*header_name_ptr && *header_name_ptr!=':' && *header_name_ptr>=33 && *header_name_ptr<=126) header_name_ptr++;
		if ((*header_name_ptr==':' && header_name_ptr!=m_two_lines_ahead) ||
					   (*header_name_ptr==0 && header_name_ptr==m_two_lines_ahead) ||
					   strni_eq(m_two_lines_ahead, ">FROM ", 6)) //To avoid bug#141122
		{
			if (m_found_start_of_message)
			{
				m_found_start_of_message = FALSE;
				m_found_valid_message = TRUE;
			}
		}
		else if (m_found_valid_message)
			m_found_start_of_next_message = FALSE;
	}

	BOOL save_message = m_found_valid_message && (m_found_start_of_next_message || m_finished_reading);

	//Append line to buffer (make sure the last line of the mbox file is also appended)
	if (m_found_valid_message && (!save_message || m_finished_reading))
	{
		int space_stuffs = (*m_one_line_ahead==' ' && strni_eq(m_one_line_ahead+1, "FROM ", 5)) ? 1 : 0;
		INT32 buflen = strlen(m_one_line_ahead) - space_stuffs;

		if (m_raw_length + buflen >= m_raw_capacity)	// brute force realloc
		{
			m_raw_capacity = (m_raw_length + buflen) * 2;
			char* _raw = OP_NEWA(char, m_raw_capacity);
			if (_raw)
				op_memcpy(_raw, m_raw, m_raw_length);

			OP_DELETEA(m_raw);
			m_raw = _raw;

			OP_DBG(("MBoxImporter::ImportMboxAsync() REALLOCATION, m_raw_capacity=%u", m_raw_capacity));
		}

		if (m_raw)
		{
			op_memcpy(m_raw + m_raw_length, m_one_line_ahead + space_stuffs, buflen);
			m_raw_length += buflen;
		}
	}

	//Yes, we should save it. Really!
	if (m_raw && save_message)
	{
		Message m2_message;
		if (m2_message.Init(m_accountId) == OpStatus::OK)
		{
			if (!m_finished_reading && m_raw_length>0 && m_raw[m_raw_length-1]=='\n') m_raw_length--; //Skip linefeed preceding "From "
			if (!m_finished_reading && m_raw_length>0 && m_raw[m_raw_length-1]=='\r') m_raw_length--;

			m_raw[m_raw_length] = '\0';
			m2_message.SetRawMessage(m_raw);

			Header::HeaderValue header_value;
			OpString x_opera_status;
			BOOL skip_message = FALSE;

			BOOL opera_header_found = OpStatus::IsSuccess(m2_message.GetHeaderValue("X-Opera-Status", header_value));
			if (opera_header_found && header_value.HasContent())
			{
				RETURN_VOID_IF_ERROR(x_opera_status.Set(header_value));

				// cut out the message id from the status header
				x_opera_status.Delete(0, 10);
				x_opera_status[8] = '\0';

				skip_message = (x_opera_status.Compare(UNI_L("00000000")) == 0);	// this message has been marked as deleted, do not import
			}

			BOOL xuidl_header_found = OpStatus::IsSuccess(m2_message.GetHeaderValue("X-UIDL", header_value));
			if (xuidl_header_found && header_value.HasContent())
			{
				OpString8 loc;
				RETURN_VOID_IF_ERROR(loc.Set(header_value.CStr()));
				// Need to test if acount is POP IMAP or NNTP
				RETURN_VOID_IF_ERROR(m2_message.SetInternetLocation(loc));
			}

			BOOL ximap_header_found = OpStatus::IsSuccess(m2_message.GetHeaderValue("X-IMAP", header_value));
			if (ximap_header_found && header_value.HasContent())
			{
				OpString8 imap_id;
				RETURN_VOID_IF_ERROR(imap_id.Set(header_value.CStr()));
				OpString8 relpath;
				OpString8 loc;
				if (relpath.HasContent())
				{
					RETURN_VOID_IF_ERROR(loc.AppendFormat("%s:%s", relpath.CStr(), imap_id.CStr()));
					// Need to test if acount is POP IMAP or NNTP
					RETURN_VOID_IF_ERROR(m2_message.SetInternetLocation(loc));
				}
			}

			if (!skip_message)
			{
				m2_message.SetFlag(Message::IS_READ, TRUE);
				if (MessageEngine::GetInstance()->ImportMessage(&m2_message, m_m2FolderPath, m_moveCurrentToSent) != OpStatus::OK)
				{
					OP_ASSERT(0);
				}
			}
		}

		m_raw[0] = '\0';
		m_raw_length = 0;

		m_found_valid_message = m_found_start_of_next_message;
		m_found_start_of_next_message = FALSE;

		m_grandTotal++;
		MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount, TRUE);
	}

	OpFileLength file_pos;
	if (m_mboxFile->GetFilePos(file_pos) == OpStatus::OK &&
		file_pos > 0)
	{
		m_count = (INT32)file_pos;
	}
	else
	{
		OP_DBG(("MBoxImporter::ImportMboxAsync() GetFilePos() failed"));
	}

	if (m_count - old >= 1000)
	{
		MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount, TRUE);
		old = m_count;
	}

	if (m_finished_reading)
	{
		OP_DELETEA(m_raw);
		m_raw = NULL;
		OP_DELETE(m_mboxFile);
		m_mboxFile = NULL;
		AddToResumeCache(m_m2FolderPath);
		OP_DBG(("MBoxImporter::ImportMboxAsync() m_count=%u, m_totalCount=%u", m_count, m_totalCount));
	}
	else if (m_mboxFile->Eof())
	{
		m_finished_reading=TRUE;
	}
}

BOOL MboxImporter::IsValidMboxFile(const uni_char* file_name)
{
	BOOL valid = FALSE;
	struct stat st;

	OpFile * mbox_file = OP_NEW(OpFile, ());
	if (!mbox_file || OpStatus::IsError(mbox_file->Construct(file_name)))
    {
        OP_DELETE(mbox_file);
        return FALSE;
    }

	if (DesktopOpFileUtils::Stat(mbox_file, &st) == OpStatus::OK &&
		st.st_size > 5)
	{
		const int BUF_LEN = 6;
		char buf[BUF_LEN];
		OpFileLength bytes_read = 0;
		if (mbox_file->Open(OPFILE_READ) != OpStatus::OK ||
			mbox_file->Read(buf, BUF_LEN, &bytes_read) != OpStatus::OK)
		{
			OP_DELETE(mbox_file);
			return FALSE;
		}

		if (op_strncmp(buf, "From ", 5) == 0)
		{
			valid = TRUE;
		}
	}

	OP_DELETE(mbox_file);
	return valid;
}

OP_STATUS MboxImporter::SetRecursorRoot(const OpString& root)
{
	// Define import store path
	OP_DELETE(m_folder_recursor);
	m_folder_recursor = OP_NEW(FolderRecursor, ());
	if (!m_folder_recursor) return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(m_folder_recursor->SetRootFolder(root)))
	{
		OP_DELETE(m_folder_recursor);
		m_folder_recursor = NULL;
	}

	return OpStatus::OK;
}

INT32 MboxImporter::GetOrCreateFolder(const OpString& root, OpFile& mbox, OpString& virtual_path)
{
	OpString mboxDir;
	OpString name;
	INT32 index = -1;
	OpFile sub_dirs;

	OP_STATUS sts = mbox.GetDirectory(mboxDir);
	if (OpStatus::IsError(sts))
	{
		return index;
	}
	
	if (mboxDir.Length() > 1 && mboxDir[mboxDir.Length() - 1] == PATHSEPCHAR)
	{
		mboxDir.Delete(mboxDir.Length() - 1); 
	}

	sts = sub_dirs.Construct(mboxDir.CStr());
	if (OpStatus::IsError(sts))
	{
		return index;
	}
	sts = name.Set(sub_dirs.GetName());
	if (OpStatus::IsError(sts))
	{
		return index;
	}
	
	if (root.Compare(mboxDir) == 0)
	{
		virtual_path.Set(name.CStr());
		// Find match on top level or
		// add root and
		// set index on match or new entry
		int first_index = 0;
		ImporterModelItem * item = GetModel()->GetItemByIndex(first_index);
		while (item)
		{
			if (name.Compare(item->GetName()) == 0)
			{
				if (index == -1)
					index = first_index;
				break;
			}
			else
			{
				index = item->GetSiblingIndex();
				if (index >= 0)
					item = GetModel()->GetItemByIndex(index);
				else
					item = NULL;
			}
		}
		if (!item || index == -1)
		{
			// Define name, path and virtual_path
			OpString virtual_folder;
			virtual_folder.Set(mboxDir.CStr());
#ifdef MSWIN
			uni_char * backslash = virtual_folder.CStr();
			while((backslash = uni_strchr(backslash, PATHSEPCHAR)) != NULL)
			{
				*backslash = (uni_char)'/';
			}
#endif
			item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_FOLDER_TYPE,
										 name, mboxDir, virtual_folder, UNI_L("")));
			if (item)
			{
				/*INT32 branch = */
				index = GetModel()->AddSorted(item);
			}
			else
			{ 
				// RFZTODO: OOM
			}
		}
	}
	else
	{
		index = GetOrCreateFolder(root, sub_dirs, virtual_path);
		// define folder by picking out mbox->GetName();
		// see if there is a match among index children if
		// not add and put it into index
		// set new element index
		virtual_path.Append(PATHSEP);
		virtual_path.Append(name);
		if (GetModel()->GetItemByIndex(index) == NULL)
		{
			OP_ASSERT(!"There are no items available to test for children of.., should not GetOrCreateFolder always return an index?");
		}
		else
		{
			INT32 t_child_index = GetModel()->GetItemByIndex(index)->GetChildIndex();
			for (	int i = 0;
					i < GetModel()->GetItemByIndex(index)->GetChildCount();
					i++)
			{
				ImporterModelItem * item = GetModel()->GetItemByIndex(t_child_index);
				if (name.Compare(item->GetName()) == 0)
				{
					return t_child_index;
				}
				t_child_index = item->GetSiblingIndex();
			}
			if (!GetModel()->GetItemByIndex(index)->GetChildCount() || t_child_index == -1)
			{
				// Define name, path and virtual_path
				OpString virtual_folder;
				virtual_folder.Set(mboxDir.CStr());
#ifdef MSWIN
				uni_char * backslash = virtual_folder.CStr();
				while((backslash = uni_strchr(backslash, PATHSEPCHAR)) != NULL)
				{
					*backslash = (uni_char)'/';
				}
#endif
				ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_FOLDER_TYPE,
																name, mboxDir, virtual_folder, UNI_L("")));
				if (item)
					/*INT32 branch = */
					index = GetModel()->AddSorted(item, index);
			}
			else
				index = t_child_index;
		}
	}
	return index;
}

INT32 MboxImporter::OnCompareItems(OpTreeModel* tree_model, OpTreeModelItem* item0, OpTreeModelItem* item1)
{
	return static_cast<ImporterModelItem*>(item0)->GetName().Compare(static_cast<ImporterModelItem*>(item1)->GetName());
}
#endif //M2_SUPPORT
