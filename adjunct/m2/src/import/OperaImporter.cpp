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

# include "adjunct/desktop_util/opfile/desktop_opfile.h"
# include "adjunct/m2/src/engine/account.h"
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/engine/message.h"
# include "adjunct/m2/src/import/ImporterModel.h"
# include "adjunct/m2/src/import/M1_Prefs.h"
# include "adjunct/m2/src/import/OperaImporter.h"
# include "adjunct/m2/src/util/qp.h"
# include "modules/locale/oplanguagemanager.h"
# include "modules/util/path.h"
# include "modules/util/filefun.h"

# ifdef USE_MAILDIR
typedef const char* MBID;
typedef UINT32 MBATTR;
typedef	unsigned char MBTYPE;
# else
typedef UINT32 MBID;
typedef UINT32 MBATTR;
typedef	unsigned char MBTYPE;
# endif // USE_MAILDIR

typedef UINT32 MBFolderStatus;
# define MBFS_CREATED_STANDARD			0
# define MBFS_OPEN_OK					0
# define MBFS_CREATED_EXISTS			1
# define MBFS_CREATED_STORAGE_SCANNED	2
# define MBFS_CREATED_ERROR				3
# define MBFS_OPEN_ERROR				3

typedef UINT32 itemAttr;
// These first two has special meaning!
# define ITEM_ATTR_SET			0x80000000
# define ITEM_ATTR_CLEAR		0x40000000

# define ITEM_ATTR_UNREAD		0x00000001
# define ITEM_ATTR_DRAFT		0x00000002
# define ITEM_ATTR_REPLIED		0x00000004
# define ITEM_ATTR_FORWARDED	0x00000008
# define ITEM_ATTR_REDIRECTED	0x00000010
# define ITEM_ATTR_SENT			0x00000020
# define ITEM_ATTR_QUEUED		0x00000040
# define ITEM_ATTR_PENDING		0x00000080
# define ITEM_ATTR_DELETED		0x00000100
# define ITEM_ATTR_ATTACHMENTS	0x00000200
# define ITEM_ATTR_UNSENT		0x00000400

typedef UINT32 Warnings;
# define WARN_ALL				0x00000001
# define WARN_UNREAD			0x00000002
# define WARN_QUEUED			0x00000004
# define WARN_ATTACHMENTS		0x00000008
# define WARN_FOLDERS			0x00000010
# define WARN_ENTIRE_TRASH		0x00000020
# define WARN_ACCOUNTS			0x00000040

typedef UINT32 itemType;
# define ITEM_TYPE_FOLDER	0
# define ITEM_TYPE_MAIL		1

typedef UINT32 MBFolderItemType;
# define MBFIT_FOLDER	0
# define MBFIT_MAIL		1
# define MBFIT_DATA		2

typedef UINT32 MBFolderType;
# define MBFT_FOLDER	0
# define MBFT_INBOX		1
# define MBFT_OUTBOX	2
# define MBFT_SENT		3
# define MBFT_TRASH		4
# define MBFT_LOCAL		5
# define MBFT_ACCOUNT	6
# define MBFT_SEARCH	7
# define MBFT_DRAFTS	8

typedef UINT32 MBInboxAttr;
# define MBIA_DELETE_MESSAGES		0x0001
# define MBIA_SAVE_PASSWORD			0x0002
# define MBIA_PASSWORD_SAVED		0x0004

typedef UINT32 MBFolderAttr;
# define	MBFA_FOLDER_EXPANDED	0x8000
# define MBFA_UPDATE_HEADER			0x4000

struct v3indexHeader
{
    // The following is identical to v2
	char	id[28];			// sign that it is a right file
	char	longname[64];	// Full name of folder
	UINT32	spaceUsed;
	UINT32	spaceFreed;
	UINT32	indexEnd;		// position for next index item
	UINT32	storeEnd;		// position for adding next mail
	UINT32	lastSize;		// timestamp for the storage file
	UINT32	version;		// version ID
	UINT32	count;			// how many mails are in the folder
	MBTYPE	foldertype;		// what type of folder is this?
	MBATTR	folderattr;		// attribytes for this folder
    // ------- end of v2 -------
    INT32	unread;         // number of unread mail in folder

    // The following are for future use
    UINT32	flags;			// reserved1;
    UINT32	reserved2;      // for future use
    UINT32	reserved3;      // for future use
    UINT32	reserved4;      // for future use
    UINT32	reserved5;      // for future use
    UINT32	reserved6;      // for future use
    UINT32	reserved7;      // for future use
    UINT32	reserved8;      // for future use
    UINT32	reserved9;      // for future use
    UINT32	reserved10;     // for future use
};

//********************************************************************

struct v2indexItem
{
	UINT32	start;	// point to start of item data
	UINT32	end;	// end of item data + 1
	MBATTR	attr;	// shows read or deleted state
};

//********************************************************************

struct v3indexItem
{
    // The following is identical to v2
	UINT32	start;	// point to start of item data
	UINT32	end;	// end of item data + 1
	MBATTR	attr;	// shows read or deleted state
    // ------- end of v2 -------

    UINT32	flags;  // Flags (like, has attachments)
    UINT32	date;
    char	state;
    char	pri;
    char	from[64];
    char	subject[64];

    UINT32	reserved1;
    UINT32	reserved2;
};

//********************************************************************

struct v4indexItem
{
	UINT32	start;	// point to start of item data
	UINT32	end;	// end of item data + 1
	MBATTR	attr;	// shows read or deleted state

    UINT32	flags;  // Flags (like, has attachments)
    UINT32	date;
    char	state;
    char	pri;
    char	from[256];
    char	subject[256];

    UINT32	reserved1;
    UINT32	reserved2;
};

//********************************************************************

typedef v3indexHeader indexHeader;
//typedef v4indexItem indexItem;
typedef v3indexItem	indexItemSmallArray;
typedef v4indexItem indexItemLargeArray;


struct indexItem
{
	UINT32	start;	// point to start of item data
	UINT32	end;	// end of item data + 1
	MBATTR	attr;	// shows read or deleted state
    UINT32	flags;  // Flags (like, has attachments)
};




OperaImporter::OperaImporter()
:	m_mboxFile(NULL),
	m_idxFile(NULL),
	m_accounts_ini(NULL),
	m_treePos(-1),
	m_inProgress(FALSE),
	m_version4(FALSE),
	m_isTrashFolder(FALSE)
{
}


OperaImporter::~OperaImporter()
{
	OP_DELETE(m_accounts_ini);

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	if (m_mboxFile)
	{
		m_mboxFile->Close();
		glue_factory->DeleteOpFile(m_mboxFile);
		m_mboxFile = NULL;
	}

	if (m_idxFile)
	{
		m_idxFile->Close();
		glue_factory->DeleteOpFile(m_idxFile);
		m_idxFile = NULL;
	}
}


OP_STATUS OperaImporter::ReadMessage(OpFileLength file_pos, UINT32 length, OpString8& message)
{
	BOOL result = FALSE;
	message.Empty();

	if (m_mboxFile && m_mboxFile->SetFilePos(file_pos)==OpStatus::OK)
	{
		message.Reserve(length + 1);

		OpFileLength readLength = 0;
		if (m_mboxFile->Read(message.CStr(), length, &readLength) == OpStatus::OK && readLength == length)
		{
			message.CStr()[length] = '\0';

			result = !(m_mboxFile->Eof());
			OP_ASSERT(result);
		}
	}

	return result;
}


# define MBFLAG_LARGE_ARRAYS	1	// uses index item version 4

BOOL OperaImporter::OnContinueImport()
{
	OP_NEW_DBG("OperaImporter::OnContinueImport", "m2");

	if (m_stopImport)
		return FALSE;

	if (!m_inProgress)
	{
		if (GetModel()->SequenceIsEmpty(m_sequence))
			return FALSE;

		ImporterModelItem* item = GetModel()->PullItem(m_sequence);
		if (NULL != item)
		{
			if (OpStatus::IsSuccess(OpenMailBox(item->GetPath())))
			{
				indexHeader head;
				OpFileLength bytes_read = 0;
				if (m_idxFile->Read(&head, sizeof(indexHeader), &bytes_read) == OpStatus::OK)
				{
					m_version4 = (head.flags & MBFLAG_LARGE_ARRAYS);
					m_isTrashFolder = (MBFT_TRASH == head.foldertype);

					OpString tmp;
					tmp.Set(item->GetVirtualPath());

					int pos = tmp.FindLastOf('/');
					if ((strlen(head.longname) > 0) && (pos != KNotFound))
					{
						tmp.CStr()[pos + 1] = '\0';		// get rid of short folder name

						char* p = NULL;
						if ((p = strstr(head.longname, "?B?")) != NULL)	// It's NOT Base64, it's QP damn it!
						{
							*(++p) = 'Q';
						}

						OpString name;
						QPDecode(head.longname, name);
						tmp.Append(name);	// use long name instead

						m_m2FolderPath.Set(tmp);
					}
					else	// fallback: use short name
					{
						m_m2FolderPath.Set(item->GetVirtualPath());
					}

					OpString acc;
					GetImportAccount(acc);

					m_currInfo.Empty();
					m_currInfo.Set(acc);
					m_currInfo.Append(UNI_L(" - "));
					m_currInfo.Append(item->GetName());

					if (InResumeCache(m_m2FolderPath))
					{
						m_idxFile->Close();
						GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
						glue_factory->DeleteOpFile(m_idxFile);
						m_idxFile = NULL;

						m_mboxFile->Close();
						glue_factory->DeleteOpFile(m_mboxFile);
						m_mboxFile = NULL;

						OpString progressInfo, already_imported;
						OpStatus::Ignore(progressInfo.Set(m_currInfo));
						OpStatus::Ignore(g_languageManager->GetString(Str::S_ALREADY_IMPORTED, already_imported));
						OpStatus::Ignore(already_imported.Insert(0, UNI_L(" ")));
						OpStatus::Ignore(progressInfo.Append(already_imported));

						MessageEngine::GetInstance()->OnImporterProgressChanged(this, progressInfo, 0, 0);

						return TRUE;
					}

					m_totalCount = head.count;
					m_count = 0;

					MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount);

					m_inProgress = TRUE;
				}
			}
		}
	}
	else
	{
		OP_DBG(("OperaImporter::OnContinueImport() msg# %u of %u", m_count, m_totalCount));

		BOOL eof = FALSE;
		//OP_STATUS ret =
		ImportSingle(eof);	// jant TODO: check return value and issue err msg
		if (eof)
		{
			m_inProgress = FALSE;
		}
	}

	return TRUE;
}


void OperaImporter::OnCancelImport()
{
	OP_NEW_DBG("OperaImporter::OnCancelImport", "m2");
	OP_DBG(("OperaImporter::OnCancelImport()"));

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	if (m_mboxFile)
	{
		if (m_mboxFile->Close() != OpStatus::OK)
		{
		}
		glue_factory->DeleteOpFile(m_mboxFile);
		m_mboxFile = NULL;
	}

	if (m_idxFile)
	{
		m_idxFile->Close();
		glue_factory->DeleteOpFile(m_idxFile);
		m_idxFile = NULL;
	}

	m_inProgress = FALSE;
}


OP_STATUS OperaImporter::OpenMailBox(const OpStringC& fullPath)
{
	OP_NEW_DBG("OperaImporter::OpenMailBox", "m2");
	OP_DBG(("OperaImporter::OpenMailBox( %S )", fullPath.CStr()));

	OpString tmp;
	tmp.Set(fullPath);
	tmp.Append(UNI_L(".MBS"));

	OP_DELETE(m_mboxFile);

	m_mboxFile = OP_NEW(OpFile, ());
	if (!m_mboxFile)
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_mboxFile->Construct(tmp.CStr()));

	RETURN_IF_ERROR(m_mboxFile->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE));

	tmp.Set(fullPath);
	tmp.Append(UNI_L(".IDX"));

	OP_DELETE(m_idxFile);

	m_idxFile = OP_NEW(OpFile, ());
	if (!m_idxFile)
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_idxFile->Construct(tmp.CStr()));

	return m_idxFile->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE);
}


OP_STATUS OperaImporter::ImportSingle(BOOL& eof)
{
	OP_STATUS ret = OpStatus::OK;

	BOOL reallyEof = m_idxFile->Eof();

	if (!m_stopImport && !reallyEof)
	{
		eof = FALSE;

		indexItem item;

		OpFileLength file_pos;
		m_idxFile->GetFilePos(file_pos);
		OpFileLength bytesRead = 0;
		if (m_idxFile->Read(&item, sizeof(indexItem), &bytesRead) == OpStatus::OK)
		{
			if (bytesRead != sizeof(indexItem))
			{
				// This is where we really get to eof the unix way...
				// we have read beyond the end of the file..
				reallyEof = m_idxFile->Eof();
			}
			else
			{
				if (m_version4)
				{
					m_idxFile->SetFilePos(file_pos + sizeof(indexItemLargeArray));
				}
				else
				{
					m_idxFile->SetFilePos(file_pos + sizeof(indexItemSmallArray));
				}

				if (!(item.attr & ITEM_ATTR_DELETED))
				{
					OpString8 raw;

					if (item.start < item.end && ReadMessage(item.start, item.end - item.start, raw))
					{
						Message m2_message;

						ret = m2_message.Init(m_accountId);

						if (OpStatus::IsSuccess(ret))
						{
							m2_message.SetFlag(Message::IS_READ,	   (!(item.attr & ITEM_ATTR_UNREAD)) || (item.attr & ITEM_ATTR_SENT));
							m2_message.SetFlag(Message::IS_REPLIED,	 (item.attr & ITEM_ATTR_REPLIED));
							m2_message.SetFlag(Message::IS_FORWARDED,	 (item.attr & ITEM_ATTR_FORWARDED));
							m2_message.SetFlag(Message::IS_SENT,		 (item.attr & ITEM_ATTR_SENT));
							m2_message.SetFlag(Message::IS_RESENT,		 (item.attr & ITEM_ATTR_REDIRECTED));
							m2_message.SetFlag(Message::HAS_ATTACHMENT, (item.attr & ITEM_ATTR_ATTACHMENTS));

							// needs to be outgoing for outgoing flags to take effect:
							m2_message.SetFlag(Message::IS_OUTGOING, ((item.attr & ITEM_ATTR_SENT) || (item.attr & ITEM_ATTR_QUEUED)) );

							m_moveCurrentToSent = ((item.attr & ITEM_ATTR_SENT) || (item.attr & ITEM_ATTR_QUEUED));

							ret = m2_message.SetRawMessage(raw.CStr());
							if (OpStatus::IsSuccess(ret))
							{
								ret = MessageEngine::GetInstance()->ImportMessage(&m2_message, m_m2FolderPath, m_moveCurrentToSent);
							}
						}
					}
					else
					{
						ret = OpStatus::ERR;
					}
				}

				m_count++;
				m_grandTotal++;
				MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount);
			}
		}
		else
		{
			ret = OpStatus::ERR;
		}
	}

	if (m_stopImport || reallyEof)
	{
		m_idxFile->Close();
		GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
		glue_factory->DeleteOpFile(m_idxFile);
		m_idxFile = NULL;

		m_mboxFile->Close();
		glue_factory->DeleteOpFile(m_mboxFile);
		m_mboxFile = NULL;

		eof = TRUE;
	}

	if (reallyEof)
	{
		AddToResumeCache(m_m2FolderPath);
	}

	return ret;
}


OP_STATUS OperaImporter::ImportMessages()
{
	ImporterModelItem* item = GetImportItem();
	if (NULL == item)
		return OpStatus::ERR;

	GetModel()->MakeSequence(m_sequence, item);

	OpenPrefsFile();

	m_inProgress = FALSE;

	return StartImport();
}


OP_STATUS OperaImporter::ImportSettings()
{
	OpString accountIni;

	if (GetSettingsFileName(accountIni))
	{
		OP_STATUS ret = OpStatus::ERR;

		M1_Preferences account_ini;
		if (account_ini.SetFile(accountIni))
		{
			Account* account = MessageEngine::GetInstance()->CreateAccount();
			if (account)
			{
				account->SetIncomingProtocol(AccountTypes::POP);
				account->SetOutgoingProtocol(AccountTypes::SMTP);
				account->SetDefaults();

				OpString8 str8;
				OpString str;
				int val = 0;

				if (account_ini.GetStringValue("SETTINGS", "FullName", str8))
				{
					QPDecode(str8, str);
					account->SetRealname(str);
				}

				if (account_ini.GetStringValue("SETTINGS", "Organization", str8))
				{
					QPDecode(str8, str);
					account->SetOrganization(str);
				}

				if (account_ini.GetStringValue("SETTINGS", "EmailAddress", str8))
				{
					QPDecode(str8, str);
					account->SetEmail(str);
				}

				if (account_ini.GetStringValue("SETTINGS", "AccountName", str8))
				{
					QPDecode(str8, str);
					account->SetAccountName(str);
				}

				if (account_ini.GetStringValue("SETTINGS", "ReplyTo", str8))
				{
					QPDecode(str8, str);
					account->SetReplyTo(str);
				}

				if (account_ini.GetStringValue("INCOMING", "Login", str8))
				{
					QPDecode(str8, str);
					account->SetIncomingUsername(str);
				}

				if (account_ini.GetStringValue("INCOMING", "Address", str8))
				{
					QPDecode(str8, str);
					account->SetIncomingServername(str);
				}

				if (account_ini.GetIntegerValue("INCOMING", "Port", val))
				{
					account->SetIncomingPort(val);
				}

				if (account_ini.GetIntegerValue("INCOMING", "DeleteFromServer", val))
				{
					account->SetLeaveOnServer(!(BOOL)val);
				}

				if (account_ini.GetIntegerValue("INCOMING", "CheckInterval", val))
				{
					account->SetPollInterval(val * 60);
				}

				if (account_ini.GetIntegerValue("INCOMING", "CheckAutomation", val))
				{
					if (0 == val)
					{
						account->SetPollInterval(0);	// quick and dirty...
					}
				}

				if (account_ini.GetIntegerValue("INCOMING", "TLS", val))
				{
					account->SetUseSecureConnectionIn((BOOL)val);
				}

				if (account_ini.GetIntegerValue("INCOMING", "PlaySound", val))
				{
					account->SetSoundEnabled((BOOL)val);
				}

				if (account_ini.GetStringValue("INCOMING", "SoundFile", str8))
				{
					QPDecode(str8, str);
					account->SetSoundFile(str);
				}

				if (account_ini.GetStringValue("OUTGOING", "Address", str8))
				{
					QPDecode(str8, str);
					account->SetOutgoingServername(str);
				}

				if (account_ini.GetIntegerValue("OUTGOING", "Port", val))
				{
					account->SetOutgoingPort(val);
				}

				if (account_ini.GetIntegerValue("OUTGOING", "QueueMail", val))
				{
					account->SetQueueOutgoing((BOOL)val);
				}

				if (account_ini.GetIntegerValue("OUTGOING", "TLS", val))
				{
					account->SetUseSecureConnectionOut((BOOL)val);
				}

				ret = account->Init();

				if (OpStatus::IsSuccess(ret))
				{
					if (account_ini.GetStringValue("OUTGOING", "SignatureFile", str8))
					{
						QPDecode(str8, str);

						GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
						OpFile* sig_file = glue_factory->CreateOpFile(str);


						if (sig_file)
						{
							OP_STATUS err = sig_file->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE);
							if (OpStatus::IsError(err))
							{
								// Try by using filename (leaf) for signature file and look in
								// 1: folder for account and at
								// 2: mail root
								int i = str.FindLastOf('\\');
								if (i != KNotFound)
								{
									OpString sig_path;
									OpStringC filename(str.CStr()+i+1);
									OpPathDirFileCombine(sig_path, m_folder_path, filename);

									glue_factory->DeleteOpFile(sig_file);
									sig_file = glue_factory->CreateOpFile(sig_path);
									err = sig_file ? sig_file->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE) : OpStatus::ERR_NO_MEMORY;
								}
							}
							if (OpStatus::IsSuccess(err))
							{
								OpFileLength length;
								sig_file->GetFileLength(length);

								if (length > 0)
								{
									char* buf = OP_NEWA(char, length + 1);
									OpFileLength bytes_read = 0;
									if (buf && sig_file->Read(buf, length, &bytes_read)==OpStatus::OK)
									{
										OpString signature;
										int invalid_inp = 0;
										TRAPD(rc, invalid_inp = SetFromEncodingL(&signature, g_op_system_info->GetSystemEncodingL(), buf, length));
										if (rc == OpStatus::OK && invalid_inp == 0)
											account->SetSignature(signature,FALSE);

										OP_DELETEA(buf);
									}
								}
								sig_file->Close();
							}
							glue_factory->DeleteOpFile(sig_file);
						}
					}

					account->SaveSettings();
					m_accountId = account->GetAccountId();
				}
			}
		}

		return ret;
	}

	return OpStatus::ERR;
}


OP_STATUS OperaImporter::Init()
{
	OP_STATUS ret = OpStatus::ERR;

	GetModel()->DeleteAll();

	OpString8 account8;
	int i = 0;

	while (m_accounts_ini && m_accounts_ini->GetEntry("ACCOUNTS", account8, i++) && !account8.IsEmpty())
	{
		OpString account;
		QPDecode(account8, account);
		// TODO: Strip account, it contains garbage.

		OpString fullPath;
		OpPathDirFileCombine(fullPath, m_folder_path, account);

		OpString accountIni;
		OpStringC accountFileName(UNI_L("ACCOUNT.INI"));
		OpPathDirFileCombine(accountIni, fullPath, accountFileName);

		M1_Preferences account_ini;
		if (account_ini.SetFile(accountIni))
		{
			OpString8 str8;
			if (account_ini.GetStringValue("SETTINGS", "AccountName", str8))
			{
				OpString accountName;
				QPDecode(str8, accountName);

				OpString m2FolderPath, imported;
				m2FolderPath.Set(accountName);
				OpStatus::Ignore(g_languageManager->GetString(Str::S_IMPORTED, imported));
				OpStatus::Ignore(imported.Insert(0, UNI_L(" (")));
				OpStatus::Ignore(imported.Append(UNI_L(")")));
				OpStatus::Ignore(m2FolderPath.Append(imported));


				ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_ACCOUNT_TYPE, accountName, *m_file_list.Get(0), m2FolderPath, UNI_L("")));
				if (item)
				{
					item->SetSettingsFileName(accountIni);
					INT32 branch = GetModel()->AddLast(item);

					OpPathDirFileCombine(fullPath, m_folder_path, account);

					ret = InsertMailBoxes(fullPath, m2FolderPath, branch);
				}
			}
		}
	}

	return ret;
}


OP_STATUS OperaImporter::InsertMailBoxes(const OpStringC& basePath, const OpStringC& m2FolderPath, INT32 parentIndex)
{
	OpString folderIni;
	OpStringC folderFileName(UNI_L("FOLDER.INI"));
	OpPathDirFileCombine(folderIni, basePath, folderFileName);

	M1_Preferences folder_ini;
	if (folder_ini.SetFile(folderIni))
	{
		OpString8 boxName8;
		int i = 0;

		while (folder_ini.GetEntry("MAILBOXES", boxName8, i++) && !boxName8.IsEmpty())
		{
			OpString boxName;
			QPDecode(boxName8, boxName);

			OpString virtualFolder;
			virtualFolder.Set(m2FolderPath);
			virtualFolder.Append(UNI_L("/"));
			virtualFolder.Append(boxName);

			OpString fullPath;
			OpPathDirFileCombine(fullPath, basePath, boxName);

			ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_MAILBOX_TYPE, boxName, fullPath, virtualFolder, UNI_L("")));
			if (item)
				GetModel()->AddLast(item, parentIndex);
		}

		OpString8 subFolder8;
		i = 0;

		while (folder_ini.GetEntry("SUB FOLDERS", subFolder8, i++) && !subFolder8.IsEmpty())
		{
			OpString subFolder;
			subFolder.Set(subFolder8);

			OpString fullPath;
			OpPathDirFileCombine(fullPath, basePath, subFolder);

			InsertSubFolder(fullPath, subFolder, m2FolderPath, parentIndex);
		}
	}
	return OpStatus::OK;
}


OP_STATUS OperaImporter::InsertSubFolder(const OpStringC& basePath,
										 const OpStringC& folderName,
										 const OpStringC& m2FolderPath,
										 INT32 parentIndex)
{
	OpString folderIni;
	OpStringC folderFileName(UNI_L("FOLDER.INI"));
	OpPathDirFileCombine(folderIni, basePath, folderFileName);

	M1_Preferences folder_ini;
	if (folder_ini.SetFile(folderIni))
	{
		OpString8 name8;
		OpString name;

		OpString virtualFolder;
		virtualFolder.Set(m2FolderPath);

		if (folder_ini.GetStringValue("SETTINGS", "FolderName", name8) && !name8.IsEmpty())
		{
			QPDecode(name8, name);
			virtualFolder.Append("/");
			virtualFolder.Append(name);
		}

		OpString fullPath;
		OpPathDirFileCombine(fullPath, basePath, folderName);

		ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_FOLDER_TYPE,
														(name.IsEmpty()) ? UNI_L("<folder>") : name.CStr(),
														fullPath, virtualFolder, UNI_L("")));
		if (item)
		{
			INT32 branch = GetModel()->AddLast(item, parentIndex);

			InsertMailBoxes(basePath, virtualFolder, branch);
		}
	}
	return OpStatus::OK;
}


OP_STATUS OperaImporter::AddImportFile(const OpStringC& path)
{
	RETURN_IF_ERROR(Importer::AddImportFile(path));

    OP_DELETE(m_accounts_ini);
    m_accounts_ini = NULL;

	OpStackAutoPtr<M1_Preferences> prefs(OP_NEW(M1_Preferences, ()));
	RETURN_OOM_IF_NULL(prefs.get());
	if (!prefs->SetFile(path))
		return OpStatus::ERR;
	m_accounts_ini = prefs.release();

	m_accounts_ini->GetStringValue("Settings", "DefaultEncoding", m_defaultCharset);
	// Define folder path since it is used abundantly
	OpString dir;
	TRAPD(err, SplitFilenameIntoComponentsL(path, &dir, NULL, NULL));
	RETURN_IF_ERROR(err);

	return SetImportFolderPath(dir);
}

void OperaImporter::GetDefaultImportFolder(OpString& path)
{
	OpString iniPath;
	if (GetOperaIni(iniPath))
	{
		M1_Preferences opera_ini;
		opera_ini.SetFile(iniPath);
		OpString8 mailRoot8;
		if (opera_ini.GetStringValue("Mail", "Mail Root Directory", mailRoot8) && !mailRoot8.IsEmpty())
		{
			OpString mailRoot;
			mailRoot.Set(mailRoot8);
			OpStringC iniName(UNI_L("ACCOUNTS.INI"));
			OpPathDirFileCombine(path, mailRoot, iniName);
		}
	}
}


BOOL OperaImporter::GetOperaIni(OpString& fileName)
{
	BOOL exists = FALSE;
#ifdef MSWIN
	unsigned long size = 1024;
	OpString path;
	path.Reserve(size);

	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	if (bu->OpRegReadStrValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"),
		UNI_L("Last CommandLine"), path.CStr(), &size) != OpStatus::OK)
	{
		return FALSE;
	}

	OP_STATUS ret = OpStatus::OK;
	// check if we have a custom commandline with .ini file as parameter
	int first = path.FindFirstOf('"');
	if (KNotFound != first)
	{
		int last = path.FindLastOf('"');
		OpString iniPath;
		path.Set(path.CStr() + first + 1, last - (first + 1));

		ret = DesktopOpFileUtils::Exists(path, exists);
	}
	else
	{
		first = path.FindI(UNI_L("\\opera.exe"));
		if (KNotFound != first)
		{
			path.CStr()[first] = 0;	// get rid of exe-filename

			OpString keep;
			keep.Set(path);

			path.Append(UNI_L("\\opera6.ini"));

			ret = DesktopOpFileUtils::Exists(path, exists);

			if (ret==OpStatus::OK && !exists)	// try older version
			{
				path.Set(keep);
				path.Append(UNI_L("\\opera5.ini"));

				ret = DesktopOpFileUtils::Exists(path, exists);
			}
		}
	}
	fileName.Set(path);
#endif // MSWIN, TODO: find ini file for other platforms
	return exists;
}


BOOL OperaImporter::GetContactsFileName(OpString& fileName)
{
	if (m_folder_path.IsEmpty())
		return FALSE;

	fileName.Set(m_folder_path);

	int sep = fileName.FindLastOf(PATHSEPCHAR);
	if (sep != KNotFound)
	{
		fileName.CStr()[sep + 1] = 0;
		fileName.Append("contacts.adr");

		BOOL exists;
		if (DesktopOpFileUtils::Exists(fileName, exists) == OpStatus::OK &&
			exists)
		{
			return TRUE;
		}
	}

	return FALSE;
}


void OperaImporter::QPDecode(const OpStringC8& src, OpString& decoded)
{
	if (src.IsEmpty())
		return;

	const int qp_max_len_decode = 512;

	BOOL warn = FALSE;
	BOOL err = FALSE;

	OpQP::Decode(src, decoded, m_defaultCharset, warn, err);

	if (err)
	{
		// try to convert using native conversion
		char* pos = NULL;
		if ((pos = (char *)strstr(src.CStr(), "?Q?")) != NULL)
		{
			// get rid of qp-garbage
			char* str = pos + 3;

			if ((pos = strstr(str, "?")) != NULL)
			{
				*(pos) = '\0';
				decoded.Reserve(qp_max_len_decode + 1);
				TRAPD(rc, SetFromEncodingL(&decoded, g_op_system_info->GetSystemEncodingL(), str, qp_max_len_decode));
			}
		}
		else
		{
			decoded.Set(src);	// give up, use raw
		}
	}
}

#endif //M2_SUPPORT
