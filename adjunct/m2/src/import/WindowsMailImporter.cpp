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

// Importer based on account model as defined for Windows Mail in:
// http://msdn2.microsoft.com/en-us/library/ms715237.aspx

# include "adjunct/m2/src/engine/message.h"
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/engine/account.h"
# include "adjunct/m2/src/engine/accountmgr.h"
# include "adjunct/m2/src/util/str/m2tokenizer.h"
# include "adjunct/m2/src/import/ImporterModel.h"
# include "adjunct/m2/src/import/WindowsMailImporter.h"
# include "modules/locale/oplanguagemanager.h"
# include <math.h>

FILE* WindowsMailImporter::m_tmpFile = NULL;
BOOL WindowsMailImporter::m_blank = FALSE;
INT32 WindowsMailImporter::m_total = 0;

WindowsMailImporter::WindowsMailImporter()
:	m_version(0),
	m_prevVersion(0),
	m_raw(NULL),
	m_raw_length(0),
	m_raw_capacity(0)
{
	m_tmp_filename.Reserve(MAX_PATH);
}


WindowsMailImporter::~WindowsMailImporter()
{
	OP_DELETEA(m_raw);
	m_raw = NULL;

	if (m_tmpFile)
	{
		fclose(m_tmpFile);
		m_tmpFile = NULL;
	}

}


OP_STATUS WindowsMailImporter::Init()
{
	GetModel()->DeleteAll();

	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	if (!bu)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}


OP_STATUS WindowsMailImporter::ImportAccountSettings(const OpStringC& identityName,
														const OpStringC& account_reg_key,
														const OpStringC& signature)
{
	return OpStatus::OK;
}


BOOL WindowsMailImporter::ImportSignature(const OpStringC& signature_reg_key, OpString& signature)
{
	return FALSE;
}


OP_STATUS WindowsMailImporter::ImportSettings()
{
	ImporterModelItem* item = GetImportItem();

	if (!item)
		return OpStatus::ERR;

	if (item->GetType() == OpTypedObject::IMPORT_IDENTITY_TYPE)
	{
		OpString signature;
		ImportSignature(item->GetInfo(), signature);

		INT32 parent = GetModel()->GetIndexByItem(item);

		if (parent < 0)
			return OpStatus::ERR;

		INT32 sibling = GetModel()->GetChildIndex(parent);

		OP_STATUS ret = OpStatus::ERR;

		while (sibling != -1)
		{
			ImporterModelItem* item_sibling = (ImporterModelItem*)GetModel()->GetItemByPosition(sibling);
			if (item_sibling && item_sibling->GetType() == OpTypedObject::IMPORT_ACCOUNT_TYPE)
			{
				if (OpStatus::IsSuccess(ImportAccountSettings(item->GetName(), item_sibling->GetInfo(), signature)))
				{
					ret = OpStatus::OK;
				}
			}

			sibling = GetModel()->GetSiblingIndex(sibling);
		}
		return ret;
	}

	if (item->GetType() != OpTypedObject::IMPORT_ACCOUNT_TYPE)
		return OpStatus::ERR;

	INT32 pos = GetModel()->GetIndexByItem(item);

	if (pos < 0)
		return OpStatus::ERR;

	INT32 parent = GetModel()->GetItemParent(pos);

	if (parent < 0)
		return OpStatus::ERR;

	ImporterModelItem* parent_item = (ImporterModelItem*)GetModel()->GetItemByPosition(parent);

	if (!parent_item || parent_item->GetType() != OpTypedObject::IMPORT_IDENTITY_TYPE)
		return OpStatus::ERR;

	OpString signature;
	ImportSignature(parent_item->GetInfo(), signature);

	return ImportAccountSettings(parent_item->GetName(), item->GetInfo(), signature);
}


OP_STATUS WindowsMailImporter::ImportMessages()
{
	ImporterModelItem* item = GetImportItem();
	if (!item)
		return OpStatus::ERR;

	RETURN_IF_ERROR(GetModel()->MakeSequence(m_sequence, item));

	OpenPrefsFile();

	return StartImport();
}


OP_STATUS WindowsMailImporter::ImportContacts()
{
	return OpStatus::OK;
}


void WindowsMailImporter::ContinueImport2()
{
	OP_NEW_DBG("WindowsMailImporter::ContinueImport2", "m2");

	const int BUF_LEN = 1024;
	static char buf[BUF_LEN];

	char* ret = fgets(buf, BUF_LEN, m_tmpFile);

	if ( m_raw_length > 14 && (('F' == buf[0] && strncmp(ret, "From importer@", 14) == 0) || feof(m_tmpFile)))
	{
		Message m2_message;
		if (OpStatus::IsSuccess(m2_message.Init(m_accountId)))
		{
			m_raw[m_raw_length] = '\0';
			m2_message.SetRawMessage(m_raw);
			m2_message.SetFlag(Message::IS_READ, TRUE);

			OP_STATUS ret = MessageEngine::GetInstance()->ImportMessage(&m2_message, m_m2FolderPath, m_moveCurrentToSent);
			if (OpStatus::IsError(ret))
			{
				Header::HeaderValue msgId, from;
				m2_message.GetMessageId(msgId);
				m2_message.GetFrom(from);
				OpString context;
				context.Set(UNI_L("OE Import WARNING"));

				OpString formatstring;

				RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_IMPORT_PROGRESS_FAIL, formatstring));

				OpString message;
				message.AppendFormat(formatstring.CStr(),
									m_currInfo.CStr(), (int)ret, msgId.CStr(), from.CStr());
				MessageEngine::GetInstance()->OnError(m_accountId, message, context);
			}
		}

		m_raw[0] = '\0';
		m_raw_length = 0;

		m_count++;
		m_grandTotal++;
		MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount);
	}

	INT32 buflen = strlen(buf);

	if (m_raw_length + buflen >= m_raw_capacity)	// brute force realloc
	{
		m_raw_capacity = (m_raw_length + buflen) * 2;
		char* _raw = OP_NEWA(char, m_raw_capacity);
		if (!_raw)
			return;

		memcpy(_raw, m_raw, m_raw_length);

		OP_DELETEA(m_raw);
		m_raw = _raw;

		OP_DBG(("WindowsMailImporter::ContinueImport2() REALLOCATION, m_raw_capacity=%u", m_raw_capacity));
	}

	memcpy(m_raw + m_raw_length, buf, buflen);
	m_raw_length += buflen;

	if (feof(m_tmpFile))
	{
		OP_DELETEA(m_raw);
		m_raw = NULL;

		fclose(m_tmpFile);
		m_tmpFile = NULL;
// TODO:		::DeleteFileA(m_tmp_filename.CStr());

		AddToResumeCache(m_m2FolderPath);

		OP_DBG(("WindowsMailImporter::ContinueImport2() m_count=%u, m_totalCount=%u", m_count, m_totalCount));
	}
}


BOOL WindowsMailImporter::OnContinueImport()
{
	OP_NEW_DBG("WindowsMailImporter::OnContinueImport", "m2");

	if (m_stopImport)
		return FALSE;

	if (m_tmpFile != NULL)
	{
		ContinueImport2();
		return TRUE;
	}

	if (GetModel()->SequenceIsEmpty(m_sequence))
		return FALSE;

	ImporterModelItem* item = GetModel()->PullItem(m_sequence);

	if (!item)
		return TRUE;

	m_moveCurrentToSent = m_moveToSentItem ? (item == m_moveToSentItem) : FALSE;

	if (item->GetType() != OpTypedObject::IMPORT_MAILBOX_TYPE)
		return TRUE;

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
		if (m_tmpFile)
		{
			fclose(m_tmpFile);
			m_tmpFile = NULL;
// TODO:			::DeleteFileA(m_tmp_filename.CStr());
		}

		OpString already_imported;

		g_languageManager->GetString(Str::S_IMPORT_ALREADY, already_imported);

		progressInfo.Append(already_imported);
		MessageEngine::GetInstance()->OnImporterProgressChanged(this, progressInfo, 0, 0);

		return TRUE;
	}

//	char tmp_path[MAX_PATH];

// TODO:	::GetTempPathA(MAX_PATH, tmp_path);
// TODO: 	::GetTempFileNameA(tmp_path, "imp", 0, m_tmp_filename.CStr());

	m_tmpFile = fopen(m_tmp_filename.CStr(), "w+");
	if (!m_tmpFile)
		return TRUE;

	m_total = 0;
	m_blank = TRUE;

	OpString converting;
	g_languageManager->GetString(Str::S_IMPORT_CONVERTING, converting);

	progressInfo.Append(converting);

	MessageEngine::GetInstance()->OnImporterProgressChanged(this, progressInfo, 0, 0);

	const uni_char* filename = item->GetPath().CStr();

	if (filename)
	{
// TODO:		oe_data* data = oe_readbox(filename, getmboxstream);
// TODO:		delete data;
	}

	INT32 size = ftell(m_tmpFile);

	if (0 == size)
	{
		OpString context;
		OpString skipped_empty;
		g_languageManager->GetString(Str::S_IMPORT_OE_WARNING, context);
		g_languageManager->GetString(Str::S_IMPORT_EMPTYBOX_SKIPPED, skipped_empty);
		OpString message;
		message.AppendFormat(skipped_empty.CStr(), m_currInfo.CStr());
		MessageEngine::GetInstance()->OnError(m_accountId, message, context);
	}

	fflush(m_tmpFile);
	rewind(m_tmpFile);

	m_count = 0;
	m_totalCount = m_total;

	OP_DELETEA(m_raw);

// TODO:	m_raw_capacity = min(CAPACITY, size + 1);
	if ((m_raw = OP_NEWA(char, m_raw_capacity)) == NULL)
	{
		m_raw_capacity = 0;
	}
	else
		m_raw[0] = '\0';

	m_raw_length = 0;

	MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount);

	return TRUE;
}


void WindowsMailImporter::OnCancelImport()
{
	OP_NEW_DBG("WindowsMailImporter::OnCancelImport", "m2");
	OP_DBG(("WindowsMailImporter::OnCancelImport()"));

	OP_DELETEA(m_raw);
	m_raw = NULL;

	if (m_tmpFile)
	{
		fclose(m_tmpFile);
		m_tmpFile = NULL;
// TODO:		::DeleteFileA(m_tmp_filename.CStr());
	}
}


// ****************** Private ********************

BOOL WindowsMailImporter::GetIdentities()
{
	return FALSE;
}


OP_STATUS WindowsMailImporter::GetMailBoxNames(const OpStringC& basePath, OpVector<OpString>& mailboxes)
{
	if (OpFolderLister* folder_lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*.dbx"), basePath.CStr()))
	{
		BOOL more = folder_lister->Next();
		while (more)
		{
			const uni_char* f_name = folder_lister->GetFileName();
			if (NULL == f_name || 0 == *f_name)
				break;

			if (*f_name != '.' &&
				uni_stricmp(f_name, UNI_L("Pop3uidl.dbx")) &&
				uni_stricmp(f_name, UNI_L("Folders.dbx")))	//don't do these files, but do the rest
			{
				OpString* mailbox = OP_NEW(OpString, ());
				if (mailbox)
				{
					mailbox->Set(f_name);
					mailboxes.Add(mailbox);
				}
			}

			more = folder_lister->Next();
		}

		OP_DELETE(folder_lister);
	}

	return OpStatus::OK;
}


void WindowsMailImporter::getmboxstream(char* s)
{
	if (m_tmpFile)
	{
		if (m_blank && ('F' == s[0]) && strncmp(s, "From importer@", 14) == 0)
		{
			m_total++;
		}
		m_blank = ('\n' == s[0]);

		fputs(s, m_tmpFile);
	}
}

#endif //M2_SUPPORT
