/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * George Refseth, rfz@opera.com
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/engine/accountmgr.h"
# include "adjunct/m2/src/import/ImporterModel.h"
# include "adjunct/m2/src/import/importer.h"

Importer::Importer()
:	m_accountId(0),
	m_stopImport(FALSE),
	m_totalCount((OpFileLength)-1),
	m_count((OpFileLength)-1),
	m_duplicates(0),
	m_grandTotal(0),
	m_id(EngineTypes::NONE),
	m_moveToSentItem(NULL),
	m_moveCurrentToSent(FALSE),
	m_sequence(OP_NEW(OpQueue<ImporterModelItem>, ())),
	m_model(OP_NEW(ImporterModel, ())),
	m_currentItem(NULL),
	m_loop(NULL),
	m_isImporting(FALSE),
	m_prefs(NULL),
	m_resumeCacheEnabled(FALSE)
{
	m_folder_path.Set(UNI_L("."));
}


Importer::~Importer()
{
	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	if (m_loop)
	{
		m_loop->SetTarget(NULL);
		glue_factory->DeleteMessageLoop(m_loop);
	}

	if (m_sequence)
	{
		m_model->EmptySequence(m_sequence);
		OP_DELETE(m_sequence);
		m_sequence = NULL;
	}

	OP_DELETE(m_model);
	m_model = NULL;

    glue_factory->DeletePrefsFile(m_prefs);
	m_prefs = NULL;
}

OP_STATUS Importer::AddImportFile(const OpStringC& path)
{
	OpString * filepath;
	filepath = OP_NEW(OpString, ());
	if (!filepath)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS sts = filepath->Set(path);
	if (sts != OpStatus::OK)
	{
		OP_DELETE(filepath);
		return sts;
	}
	sts = m_file_list.Add(filepath);
	if (sts != OpStatus::OK)
	{
		OP_DELETE(filepath);
	}
	return sts;
}

OP_STATUS Importer::AddImportFileList(const OpVector <OpString> * file_list)
{
	OpString * filepath;
	OP_STATUS sts = OpStatus::OK;
	for (UINT32 i = 0; i < file_list->GetCount(); i++)
	{
		filepath = OP_NEW(OpString, ());
		if (!filepath)
		{
			sts = OpStatus::ERR_NO_MEMORY;
			break;
		}
		sts = filepath->Set(file_list->Get(i)->CStr());
		if (sts != OpStatus::OK)
		{
			OP_DELETE(filepath);
			break;
		}
		sts = m_file_list.Add(filepath);
		if (sts != OpStatus::OK)
		{
			OP_DELETE(filepath);
			break;
		}
	}
	return sts;
}

OP_STATUS Importer::SetM2Account(UINT16 account_id)
{
	if (account_id == 0)
		return OpStatus::ERR;

	m_accountId = account_id;
	return OpStatus::OK;
}

OP_STATUS Importer::SetM2Account(const OpStringC& name)
{
    AccountManager* am = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
	if (am)
	{
		UINT16 account_id = am->FindAccountId(name);
		return SetM2Account(account_id);
	}

	return OpStatus::ERR;
}


OP_STATUS Importer::CreateImportAccount()
{
	OpString temp_acc_name;
	if (!m_currentItem)
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::MI_IDM_IMPORT_MAIL_PARENT, temp_acc_name));
	}
	else
	{
		ImporterModelItem* item = m_model->GetRootItem(m_currentItem);
		temp_acc_name.Set(item->GetName().CStr());
	}

	RETURN_IF_ERROR(MessageEngine::GetInstance()->InitMessageDatabaseIfNeeded());
    AccountManager* am = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
	if (!am)
		return OpStatus::ERR_NULL_POINTER;

	Account* account = NULL;
	OP_STATUS res = am->AddTemporaryAccount(account, AccountTypes::POP, UNI_L("localhost"), FALSE, 0, UNI_L(""), UNI_L("")); //Hardcode the new account to be POP, in lack of more information
	if (OpStatus::IsSuccess(res) && account)
	{
		account->SetIsTemporary(FALSE);
		account->SetAccountName(temp_acc_name);
		account->SaveSettings();
		m_accountId = account->GetAccountId();
	}

	return res;
}


BOOL Importer::GetImportAccount(OpString& account)
{
	ImporterModelItem* item = m_model->GetRootItem(m_currentItem);
	if (item != NULL &&
		(item->GetType() == OpTypedObject::IMPORT_ACCOUNT_TYPE ||
		 item->GetType() == OpTypedObject::IMPORT_IDENTITY_TYPE))
	{
		account.Set(item->GetName());
		return TRUE;
	}
	return FALSE;
}


BOOL Importer::GetSettingsFileName(OpString& name)
{
	ImporterModelItem* item = m_model->GetRootItem(m_currentItem);
	if (item != NULL && item->GetType() == OpTypedObject::IMPORT_ACCOUNT_TYPE)
	{
		name.Set(item->GetSettingsFileName());
		return TRUE;
	}
	return FALSE;
}


ImporterModelItem* Importer::GetRootItem() const
{
	return m_model->GetRootItem(m_currentItem);
}


const uni_char* SECTION_OPERA			= UNI_L("Opera");
const uni_char* SECTION_EUDORA			= UNI_L("Eudora");
const uni_char* SECTION_NETSCAPE		= UNI_L("Netscape");
const uni_char* SECTION_OUTLOOKEXPRESS	= UNI_L("OE");
const uni_char* SECTION_MBOX			= UNI_L("mbox");
const uni_char* SECTION_APPLE			= UNI_L("Apple");
const uni_char* SECTION_THUNDERBIRD		= UNI_L("Thunderbird");
const uni_char* SECTION_OPERA7			= UNI_L("Opera7");
const uni_char* KEY_LASTUSED			= UNI_L("LastUsedImportFolder");

void Importer::GetLastUsedImportFolder(EngineTypes::ImporterId id, OpString& path)
{
    if (!m_prefs)
	{
		if (OpStatus::IsError(OpenPrefsFile()))
			return;
	}

	OpString section;

	switch (id)
	{
		case EngineTypes::OPERA:
			section.Set(SECTION_OPERA);
			break;

		case EngineTypes::EUDORA:
			section.Set(SECTION_EUDORA);
			break;

		case EngineTypes::NETSCAPE:
			section.Set(SECTION_NETSCAPE);
			break;

		case EngineTypes::OE:
			section.Set(SECTION_OUTLOOKEXPRESS);
			break;

		case EngineTypes::MBOX:
			section.Set(SECTION_MBOX);
			break;

		case EngineTypes::APPLE:
			section.Set(SECTION_APPLE);
			break;

		case EngineTypes::THUNDERBIRD:
			section.Set(SECTION_THUNDERBIRD);
			break;

		case EngineTypes::OPERA7:
			section.Set(SECTION_OPERA7);
			break;

		default: return;
	}

	OpString tmpPath;
	TRAPD(err, m_prefs->ReadStringL(section, KEY_LASTUSED, tmpPath));

	// We want the path to be without enclosing quotes
	if (tmpPath.HasContent())
	{
		BOOL  stripOnce = FALSE, stripTwice= FALSE;
		tmpPath.Strip(TRUE, TRUE);
		int pathlength = tmpPath.Length();
		if (tmpPath[0] == '\"' && tmpPath[pathlength - 1] == '\"')
		{
			stripOnce = TRUE;
			if (tmpPath[1] == '\"' && tmpPath[pathlength - 2] == '\"')
				stripTwice = TRUE;
		}
		if (stripTwice)
			path.Set(tmpPath.CStr() + 2, pathlength - 4);
		else if (stripOnce)
			path.Set(tmpPath.CStr() + 1, pathlength - 2);
		else
			path.Set(tmpPath.CStr());
	}
}


void Importer::SetLastUsedImportFolder(EngineTypes::ImporterId id, const OpStringC& path)
{
    if (!m_prefs)
	{
		if (OpStatus::IsError(OpenPrefsFile()))
			return;
	}

	OpString section;

	switch (id)
	{
		case EngineTypes::OPERA:
			section.Set(SECTION_OPERA);
			break;

		case EngineTypes::EUDORA:
			section.Set(SECTION_EUDORA);
			break;

		case EngineTypes::NETSCAPE:
			section.Set(SECTION_NETSCAPE);
			break;

		case EngineTypes::OE:
			section.Set(SECTION_OUTLOOKEXPRESS);
			break;

		case EngineTypes::MBOX:
			section.Set(SECTION_MBOX);
			break;

		case EngineTypes::APPLE:
			section.Set(SECTION_APPLE);
			break;

		case EngineTypes::THUNDERBIRD:
			section.Set(SECTION_THUNDERBIRD);
			break;

		case EngineTypes::OPERA7:
			section.Set(SECTION_OPERA7);
			break;

		default: return;
	}

	TRAPD(err,	m_prefs->WriteStringL(section, KEY_LASTUSED, path); 
				m_prefs->CommitL(TRUE));
}


OP_STATUS Importer::StartImport()
{
	if (m_isImporting)
		return OpStatus::OK;

	if (m_stopImport)
		return OpStatus::ERR;

	m_grandTotal = 0;

	if (NULL == m_loop)
	{
		m_loop = MessageEngine::GetInstance()->GetGlueFactory()->CreateMessageLoop();
		if (NULL == m_loop)
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(m_loop->SetTarget(this));
	}

	m_isImporting = TRUE;
	m_stopImport = FALSE;

	return m_loop->Post(MSG_M2_CONTINUEIMPORT);
}


OP_STATUS Importer::Receive(OpMessage message)
{
	switch (message)
	{
		case MSG_M2_CONTINUEIMPORT:
			if (OnContinueImport())
			{
				return m_loop->Post(MSG_M2_CONTINUEIMPORT);
			}
			m_isImporting = FALSE;
			MessageEngine::GetInstance()->OnImporterFinished(this, m_error_message);
			break;

		case MSG_M2_CANCELIMPORT:
			if (m_isImporting)
			{
				OnCancelImport();
				m_isImporting = FALSE;
			}
			break;
	}

	return OpStatus::OK;
}


void Importer::CancelImport()
{
	m_stopImport = TRUE;

	if (m_loop)
	{
		m_loop->Send(MSG_M2_CANCELIMPORT);
	}
}


OP_STATUS Importer::OpenPrefsFile()
{
    OpString mailroot_dir;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	BrowserUtils* browser_utils = glue_factory->GetBrowserUtils();
    RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailroot_dir));

	OpString prefs_filename;

	browser_utils->PathDirFileCombine(prefs_filename, mailroot_dir, UNI_L("imp.dat"));

    if (m_prefs)
	{
		glue_factory->DeletePrefsFile(m_prefs);
	}

	m_prefs = glue_factory->CreatePrefsFile(prefs_filename);
    if (!m_prefs)
        return OpStatus::ERR_NO_MEMORY;

	GetImportAccount(m_cacheName);

	return OpStatus::OK;
}


OP_STATUS Importer::AddToResumeCache(const OpStringC& mailbox)
{
	if (!m_resumeCacheEnabled)
		return OpStatus::OK;

	if (m_cacheName.IsEmpty())
		return OpStatus::ERR;

	TRAPD(ret,	m_prefs->WriteIntL(m_cacheName, mailbox, 1); 
				m_prefs->CommitL(TRUE));

	return ret;
}


OP_STATUS Importer::ClearResumeCache()
{
	if (m_cacheName.IsEmpty())
		return OpStatus::ERR;

	TRAPD(ret,	m_prefs->DeleteSectionL(m_cacheName.CStr()); 
				m_prefs->CommitL(TRUE));

	return ret;
}


BOOL Importer::InResumeCache(const OpStringC& mailbox) const
{
	if (!m_resumeCacheEnabled)
		return FALSE;

	if (m_cacheName.IsEmpty())
		return FALSE;

	int flag;
	TRAPD(err, flag = m_prefs->ReadIntL(m_cacheName, mailbox));

	if (OpStatus::IsSuccess(err) && flag == 1)
		return TRUE;

	return FALSE;
}

#endif //M2_SUPPORT
