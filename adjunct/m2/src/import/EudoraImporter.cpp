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

# include "adjunct/m2/src/engine/message.h"
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/engine/account.h"
# include "adjunct/m2/src/util/str/m2tokenizer.h"
# include "adjunct/m2/src/import/ImporterModel.h"
# include "adjunct/m2/src/import/EudoraImporter.h"
# ifdef MSWIN
#  include "adjunct/m2/src/import/MSWinUtil.h"
# endif
# include "modules/locale/oplanguagemanager.h"
# include "modules/prefsfile/prefsfile.h"
# include "modules/util/path.h"
# include "modules/util/filefun.h"

// mailbox types
# define MBT_IN		0
# define MBT_OUT	1
# define MBT_TRASH	2
# define MBT_USER	3

// message types
# define MT_UNREAD		0
# define MT_READ		1
# define MT_REPLIED		2
# define MT_FORWARDED	3
# define MT_REDIRECTED	4
# define MT_READ_ALT	5
# define MT_UNSENT		6
# define MT_QUEUED		7
# define MT_SENT		8
# define MT_INVALID		9
# define MT_TIMEQUEUED	10

// flags (1)
// ...
# define MF_TEXT_ATTACHMENT_IN_BODY 0x20	// bit 5 on = text att. in body

// flags (2)
// ...
# define MF_ATTACHMENT_PRESENT 0x80	// bit 7 on = attachment present

# pragma pack(2)

struct TocFileHeader		// total size = 104 bytes
{
	char			notUsed0[8];
	char			mailboxName[32];
	INT16			mailboxType;
	char			notUsed1[60];
	UINT16			noOfMessages;
};

struct TocMessageHeader		// total size = 218 bytes
{
	INT32	pos;		// pos in MBX file
	INT32	len;		// length of message
	INT32	time;		// message date and time
	INT16	type;		// message type
	char	flag1;		// bit flags
	char	flag2;		// bit flags
	char	notUsed[202];	// not used
};

// Leaving it uncompensated messes up PGO and concatenated builds
# pragma pack()

# define MAX_PERSONA 100
const uni_char* DOMINANT_PERSONALITY = UNI_L("Settings");
const char* X_FLOWED_PATT = "\r\n<x-flowed>";
const char* X_FLOWED_END_PATT = "\r\n</x-flowed>";
const char* X_HTML_PATT = "\r\n<x-html>";
const char* X_HTML_END_PATT = "\r\n</x-html>";
const char* CONTENT_TYPE_PATT = "\r\nCONTENT-TYPE: ";
const char* BOUNDARY_PATT = "BOUNDARY=\"";

EudoraImporter::EudoraImporter()
: m_mboxFile(NULL),
  m_tocFile(NULL),
  m_eudora_ini(NULL),
  m_inProgress(FALSE),
  m_mailboxType(-1)
{
}


EudoraImporter::~EudoraImporter()
{
	OP_DELETE(m_eudora_ini);

	if (m_mboxFile)
	{
		m_mboxFile->Close();
		OP_DELETE(m_mboxFile);
		m_mboxFile = NULL;
	}

	if (m_tocFile)
	{
		m_tocFile->Close();
		OP_DELETE(m_tocFile);
		m_tocFile = NULL;
	}
}

OP_STATUS EudoraImporter::ReadIniString(const OpStringC &section, const OpStringC &key, OpString &result)
{
	if (!m_eudora_ini)
		return OpStatus::ERR_NULL_POINTER;

	TRAPD(err, m_eudora_ini->ReadStringL(section, key, result));
	return err;
}

OP_STATUS EudoraImporter::Init()
{
	if (!m_eudora_ini)
		return OpStatus::ERR;

	GetModel()->DeleteAll();

	OpString account;

	RETURN_IF_ERROR(ReadIniString(DOMINANT_PERSONALITY, UNI_L("POPAccount"), account));

	if (account.IsEmpty())
		return OpStatus::ERR;

	//AutoReceiveAttachmentsDirectory might have been defined, which means that
	// if we are not importing in situ but a transported folder we have to guess at the
	// right path since the attachments folder is absolute
	OpString att_folder;
	ReadIniString(DOMINANT_PERSONALITY, UNI_L("AutoReceiveAttachmentsDirectory"), att_folder);

	if (att_folder.IsEmpty())
		m_attachments_folder_name.Set(UNI_L("attach"));
	else
	{
		// Find leaf folder name and use that as attachments folder name
		int pos = att_folder.FindLastOf('\\');	// find last path separator
		if (pos != KNotFound)
			m_attachments_folder_name.Set(att_folder.CStr() + pos + 1);	// strip path
	}

	OpString imported;
	OpStatus::Ignore(g_languageManager->GetString(Str::S_IMPORTED, imported));
	OpStatus::Ignore(imported.Insert(0, UNI_L(" (")));
	OpStatus::Ignore(imported.Append(UNI_L(")")));

	OpString m2FolderPath;
	m2FolderPath.Set(account);
	m2FolderPath.Append(imported);

	INT32 branch;

	ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_ACCOUNT_TYPE, account, *m_file_list.Get(0), m2FolderPath, UNI_L("")));
	if (item)
	{
		branch = GetModel()->AddLast(item); // add the dominant account as first entry

		InsertMailBoxes(m_folder_path, m2FolderPath, branch);
	}

	for (int persona = 0; persona < MAX_PERSONA; persona++)
	{
		uni_char buf[5];
		OpString str;
		str.Set(UNI_L("Persona"));
		str.Append(uni_itoa(persona, buf, 10));

		OpString entry;
		ReadIniString(UNI_L("Personalities"), str, entry);
		if (entry.IsEmpty())
			break;

		ReadIniString(entry, UNI_L("POPAccount"), account);

		if (account.HasContent())
		{
			OpString imported;
			m2FolderPath.Set(account);
			OpStatus::Ignore(g_languageManager->GetString(Str::S_IMPORTED, imported));
			OpStatus::Ignore(imported.Insert(0, UNI_L(" (")));
			OpStatus::Ignore(imported.Append(UNI_L(")")));
			OpStatus::Ignore(m2FolderPath.Append(imported));

			item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_ACCOUNT_TYPE, account, *m_file_list.Get(0), m2FolderPath, UNI_L("")));
			if (item)
			{
				branch = GetModel()->AddLast(item);

				InsertMailBoxes(m_folder_path, m2FolderPath, branch);
			}
		}
	}
	return OpStatus::OK;
}


BOOL EudoraImporter::OnContinueImport()
{
	OP_NEW_DBG("EudoraImporter::OnContinueImport", "m2");
	if (m_stopImport)
		return FALSE;

	if (!m_inProgress)
	{
		if (GetModel()->SequenceIsEmpty(m_sequence))
			return FALSE;

		ImporterModelItem* item = GetModel()->PullItem(m_sequence);
		if (item)
		{
			if (OpStatus::IsSuccess(OpenMailBox(item->GetPath())))
			{
				TocFileHeader head;
				OpFileLength bytes_read = 0;
				if (!m_stopImport && OpStatus::IsSuccess(m_tocFile->Read(&head, sizeof(TocFileHeader), &bytes_read)))
				{
					m_m2FolderPath.Set(item->GetVirtualPath());

					OpString acc;
					GetImportAccount(acc);

					m_currInfo.Empty();
					m_currInfo.Set(acc);
					m_currInfo.Append(UNI_L(" - "));
					m_currInfo.Append(item->GetName());

					if (InResumeCache(m_m2FolderPath))
					{
						m_tocFile->Close();
						OP_DELETE(m_tocFile);
						m_tocFile = NULL;

						m_mboxFile->Close();
						OP_DELETE(m_mboxFile);
						m_mboxFile = NULL;

						OpString progressInfo, already_imported;
						OpStatus::Ignore(progressInfo.Set(m_currInfo));
						OpStatus::Ignore(g_languageManager->GetString(Str::S_ALREADY_IMPORTED, already_imported));
						OpStatus::Ignore(already_imported.Insert(0, UNI_L(" (")));
						OpStatus::Ignore(already_imported.Append(UNI_L(")")));
						progressInfo.Append(already_imported);

						MessageEngine::GetInstance()->OnImporterProgressChanged(this, progressInfo, 0, 0);

						return TRUE;
					}

					if (!FindPersonality(acc, m_currPersonality))
					{
						if (m_currPersonality.CompareI(DOMINANT_PERSONALITY))
						{
							m_currPersonality.Empty();
						}
					}
					else
					{
						int pos = m_currPersonality.FindFirstOf('-');
						if (pos != KNotFound)
						{
							OpString tmp;
							tmp.Set(m_currPersonality.CStr() + pos + 1);
							m_currPersonality.Set("<");
							m_currPersonality.Append(tmp);
							m_currPersonality.Append(">");
						}
						else
						{
							m_currPersonality.Empty();
						}
					}

					m_mailboxType = head.mailboxType;
					m_totalCount = head.noOfMessages;
					m_count = 0;

					m_inProgress = TRUE;

					MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount);
				}
			}
		}
	}
	else
	{
		OP_DBG(("EudoraImporter::OnContinueImport() msg# %u of %u", m_count, m_totalCount));

		BOOL eof = FALSE;
		ImportSingle(eof);
		if (eof)
		{
			m_inProgress = FALSE;
		}
	}

	return TRUE;
}


void EudoraImporter::OnCancelImport()
{
	OP_NEW_DBG("EudoraImporter::OnCancelImport", "m2");
	OP_DBG(("EudoraImporter::OnCancelImport()"));
	if (m_mboxFile)
	{
		m_mboxFile->Close();
		OP_DELETE(m_mboxFile);
		m_mboxFile = NULL;
	}

	if (m_tocFile)
	{
		m_tocFile->Close();
		OP_DELETE(m_tocFile);
		m_tocFile = NULL;
	}

	m_inProgress = FALSE;
}


OP_STATUS EudoraImporter::OpenMailBox(const OpStringC& fullPath)
{
	OP_NEW_DBG("EudoraImporter::OpenMailBox", "m2");
	OP_DBG(("EudoraImporter::OpenMailBox( fullPath = %S )", fullPath.CStr()));

	OP_STATUS ret = OpStatus::ERR;

	OpString tocFullPath;
	tocFullPath.Set(fullPath);

	int dot = tocFullPath.FindLastOf('.');
	if (dot != KNotFound)
	{
		tocFullPath.CStr()[dot] = 0;
		tocFullPath.Append(UNI_L(".toc"));
	}
	else
	{
		return OpStatus::ERR;
	}

//	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	m_mboxFile = OP_NEW(OpFile, ());
	if (m_mboxFile)
	{
		m_mboxFile->Construct(fullPath.CStr());
		ret = m_mboxFile->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE);
		if (OpStatus::IsSuccess(ret))
		{
			m_tocFile = OP_NEW(OpFile, ());
			if (m_tocFile)
			{
				m_tocFile->Construct(tocFullPath.CStr());
				ret = m_tocFile->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE);
			}
			else ret = OpStatus::ERR_NO_MEMORY;
		}
	}
	else
		return OpStatus::ERR_NO_MEMORY;

	if (!OpStatus::IsSuccess(ret))
	{
		if (m_mboxFile)
		{
			m_mboxFile->Close();
			OP_DELETE(m_mboxFile);
			m_mboxFile = NULL;
		}

		if (m_tocFile)
		{
			m_tocFile->Close();
			OP_DELETE(m_tocFile);
			m_tocFile = NULL;
		}
	}

	return ret;
}


OP_STATUS EudoraImporter::ImportSingle(BOOL& eof)
{
	OP_STATUS ret = OpStatus::ERR;

	if (!m_stopImport && !m_tocFile->Eof())
	{
		eof = FALSE;

		TocMessageHeader msg_head;
        OpFileLength bytes_read;

		if (OpStatus::IsSuccess(m_tocFile->Read(&msg_head, sizeof(TocMessageHeader), &bytes_read)) &&
			bytes_read == sizeof(TocMessageHeader))
		{
			OpString8 raw;

			if (msg_head.len > 0 && OpStatus::IsSuccess(ReadMessage(msg_head.pos, msg_head.len, raw)))
			{
				Message m2_message;

				if (OpStatus::IsSuccess(m2_message.Init(m_accountId)))
				{
					OpVector<OpString>	attachmentNames;
					if (msg_head.flag2 & MF_ATTACHMENT_PRESENT)
					{
						ret = ListAttachments(raw, attachmentNames);
					}

					// Must remove <x-flowed> </x-flowed> pair
					int flowed_pos = raw.Find(X_FLOWED_PATT);
					if (flowed_pos != KNotFound)
					{
						int flowed_end_pos = raw.Find(X_FLOWED_END_PATT);
						if (flowed_end_pos != KNotFound)
						{
							raw.Delete(flowed_end_pos, op_strlen(X_FLOWED_END_PATT));
							raw.Delete(flowed_pos + 2, op_strlen(X_FLOWED_PATT) - 2);
						}
					}
					// Must remove <x-html> </x-html> pair
					// Note that we also remove everything after </x-html> as well
					// on the assumption that it only contains attachment Content-Types
					// which is handled by use of ListAttachments() and AppendAttachments()
					int html_pos = raw.Find(X_HTML_PATT);
					if (html_pos != KNotFound)
					{
						int html_end_pos = raw.Find(X_HTML_END_PATT);
						if (html_end_pos != KNotFound)
						{
							// raw.Delete(html_end_pos + 2, op_strlen(X_HTML_END_PATT) - 2);
							raw.Delete(html_end_pos + 2, KAll);
							raw.Delete(html_pos + 2, op_strlen(X_HTML_PATT) - 2);
							m2_message.SetHTML(TRUE);
						}
					}

					ret = m2_message.SetRawMessage(raw.CStr());

					if (OpStatus::IsSuccess(ret) && msg_head.flag2 & MF_ATTACHMENT_PRESENT)
					{
						ret = AppendAttachments(attachmentNames, &m2_message);
						if (OpStatus::IsSuccess(ret))
						{
							m2_message.SetFlag(Message::HAS_ATTACHMENT, TRUE);
						}
						ret = m2_message.MimeEncodeAttachments(FALSE, TRUE);	// Put attachments into body
					}

					Header::HeaderValue xpersona;

					if (OpStatus::IsSuccess(ret))
					{
						BOOL xpersona_header_found = OpStatus::IsSuccess(m2_message.GetHeaderValue("X-Persona", xpersona)) &&
													 xpersona.HasContent();

						BOOL dominant_personality = m_currPersonality.IsEmpty();

						if ((MBT_OUT == m_mailboxType) ||
							(MT_SENT == msg_head.type) ||
							(dominant_personality && !xpersona_header_found) ||
							(xpersona_header_found && xpersona.CompareI(m_currPersonality) == 0))
						{
							m2_message.SetFlag(Message::IS_READ,		(MT_SENT == msg_head.type) ||
																			 !(MT_UNREAD == msg_head.type));

							m2_message.SetFlag(Message::IS_REPLIED,	(MT_REPLIED == msg_head.type));

							m2_message.SetFlag(Message::IS_FORWARDED,	(MT_FORWARDED == msg_head.type));

							m2_message.SetFlag(Message::IS_SENT,		((MBT_OUT == m_mailboxType ||
																			 (MT_SENT == msg_head.type)) &&
																			 MT_QUEUED != msg_head.type &&
																			 MT_TIMEQUEUED != msg_head.type));

							m2_message.SetFlag(Message::IS_RESENT,		(MT_REDIRECTED == msg_head.type));

							m2_message.SetFlag(Message::IS_OUTGOING,	(MBT_OUT == m_mailboxType) ||
																			(MT_SENT == msg_head.type) ||
																			(MT_QUEUED == msg_head.type));

							m2_message.SetFlag(Message::IS_QUEUED,		(MT_QUEUED == msg_head.type));
							m2_message.SetFlag(Message::IS_TIMEQUEUED,	(MT_TIMEQUEUED == msg_head.type));

							if ((MBT_OUT == m_mailboxType) || (MT_SENT == msg_head.type))
							{
								m2_message.SetDateHeaderValue(Header::DATE, msg_head.time);	// set date using toc header
								m_moveCurrentToSent = TRUE;
							}
							else
							{
								m_moveCurrentToSent = FALSE;
							}

							if (MT_TIMEQUEUED == msg_head.type)
							{
								OpString deferredFolder;
								deferredFolder.Set(m_m2FolderPath);
								deferredFolder.Append("/Deferred");

								ret = MessageEngine::GetInstance()->ImportMessage(&m2_message, deferredFolder, m_moveCurrentToSent);
							}
							else
							{
								ret = MessageEngine::GetInstance()->ImportMessage(&m2_message, m_m2FolderPath, m_moveCurrentToSent);
							}
							if (OpStatus::IsSuccess(ret))
							{
								m_grandTotal++;
							}
						}
					}
				}
				m_count++;
				MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount);
			}
		}
	}
	else
	{
		m_tocFile->Close();
		OP_DELETE(m_tocFile);
		m_tocFile = NULL;

		m_mboxFile->Close();
		OP_DELETE(m_mboxFile);
		m_mboxFile = NULL;

		AddToResumeCache(m_m2FolderPath);

		eof = TRUE;
	}

	return ret;
}


OP_STATUS EudoraImporter::ImportSettings()
{
	OpString account;
	if (GetImportAccount(account) && account.HasContent())
	{
		return ImportAccount(account);
	}

	return OpStatus::ERR;
}


BOOL EudoraImporter::FindPersonality(const OpStringC& accountName, OpString& personality)
{
	if (!m_eudora_ini)
		return FALSE;

	OpString str;

	personality.Set(DOMINANT_PERSONALITY);

	ReadIniString(personality, UNI_L("POPAccount"), str);
	if (str.HasContent() && str.CompareI(accountName) == 0)
	{
		return TRUE;
	}

	for (int persona = 0; persona < MAX_PERSONA; persona++)
	{
		uni_char buf[5];
		OpString tmp;
		tmp.Set(UNI_L("Persona"));
		tmp.Append(uni_itoa(persona, buf, 10));

		personality.Empty();

		ReadIniString(UNI_L("Personalities"), tmp, personality);
		if (personality.HasContent())
		{
			ReadIniString(personality, UNI_L("POPAccount"), str);
			if (str.HasContent() && str.CompareI(accountName) == 0)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}


OP_STATUS EudoraImporter::ImportAccount(const OpStringC& accountName)
{
	OpString personality;

	if (!FindPersonality(accountName, personality))
	{
		return OpStatus::ERR;
	}

	Account* account = MessageEngine::GetInstance()->CreateAccount();
	if (!account)
		return OpStatus::ERR_NO_MEMORY;

	account->SetIncomingProtocol(AccountTypes::POP);
	account->SetOutgoingProtocol(AccountTypes::SMTP);
	account->SetDefaults();

	OpString str;

	ReadIniString(personality, UNI_L("RealName"), str);
	if (str.HasContent())
	{
		account->SetRealname(str);
	}

	str.Empty();
	ReadIniString(personality, UNI_L("POPAccount"), str);
	if (str.HasContent())
	{
		account->SetEmail(str);
		account->SetAccountName(str);
	}

	str.Empty();
	ReadIniString(personality, UNI_L("ReturnAddress"), str);
	if (str.HasContent())
	{
		account->SetReplyTo(str);
	}

	str.Empty();
	ReadIniString(personality, UNI_L("LoginName"), str);
	if (str.HasContent())
	{
		account->SetIncomingUsername(str);
	}

	str.Empty();
	ReadIniString(personality, UNI_L("PopServer"), str);
	if (str.HasContent())
	{
		account->SetIncomingServername(str);
	}

	account->SetIncomingPort(m_eudora_ini->ReadIntL(personality, UNI_L("POPPort"), 110));

	account->SetLeaveOnServer((BOOL)m_eudora_ini->ReadIntL(personality, UNI_L("LeaveMailOnServer"), 1));

	if (personality.CompareI(DOMINANT_PERSONALITY) == 0)
	{
		account->SetPollInterval(m_eudora_ini->ReadIntL(personality, UNI_L("CheckForMailEvery"), 0) * 60);
	}
	else
	{
		if (m_eudora_ini->ReadIntL(personality, UNI_L("CheckMailByDefault"), 0) == 1)
		{
			account->SetPollInterval(m_eudora_ini->ReadIntL(DOMINANT_PERSONALITY, UNI_L("CheckForMailEvery"), 0) * 60);
		}
	}

	str.Empty();
	ReadIniString(personality, UNI_L("SMTPServer"), str);
	if (str.HasContent())
	{
		account->SetOutgoingServername(str);
	}

	account->SetUseSecureConnectionIn(m_eudora_ini->ReadIntL(personality, UNI_L("SSLReceiveUse"), 0) == 3);

	account->SetUseSecureConnectionOut(m_eudora_ini->ReadIntL(personality, UNI_L("SSLSendUse"), 0) == 3);

	str.Empty();
	ReadIniString(personality, UNI_L("Signature"), str);
	if (str.HasContent())
	{
		OpString sigFilePath;
		sigFilePath.Set(m_folder_path.CStr());
		sigFilePath.Append(PATHSEP);
		sigFilePath.Append(UNI_L("Sigs"));
		sigFilePath.Append(PATHSEP);
		sigFilePath.Append(str);
		sigFilePath.Append(UNI_L(".txt"));

		GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
		OpFile* sig_file = glue_factory->CreateOpFile(sigFilePath);

		if (sig_file && (OpStatus::IsSuccess(sig_file->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE))))
		{
			OpFileLength length;
			sig_file->GetFileLength(length);

			if (length > 0)
			{
				char* buf = OP_NEWA(char, (int)length + 1);
				OpFileLength bytes_read = 0;
				if (buf && OpStatus::IsSuccess(sig_file->Read(buf, length, &bytes_read)))
				{
					OpString signature;
					int invalid_inp = 0;
					TRAPD(rc, invalid_inp = SetFromEncodingL(&signature, g_op_system_info->GetSystemEncodingL(), buf, length));
					if (OpStatus::IsSuccess(rc) && invalid_inp == 0)
						account->SetSignature(signature,FALSE);
					OP_DELETEA(buf);
				}
			}

			sig_file->Close();
			OP_DELETE(sig_file);
		}
	}
	RETURN_IF_ERROR(account->Init());
	return SetM2Account(account->GetAccountId());
}


OP_STATUS EudoraImporter::ImportMessages()
{
	ImporterModelItem* item = GetImportItem();
	if (!item)
		return OpStatus::ERR;

	GetModel()->MakeSequence(m_sequence, item);

	OpenPrefsFile();

	m_inProgress = FALSE;

	return StartImport();
}

OP_STATUS EudoraImporter::ImportContacts()
{
	OP_STATUS ret = ImportNickNames(m_folder_path, UNI_L("NNdbase.txt"));

	if (OpStatus::IsSuccess(ret))	// check if we have custom addressbooks
	{
		OpString nickPath;
		OpString nickname;
		RETURN_IF_ERROR(nickname.Set("Nickname"));
		RETURN_IF_ERROR(OpPathDirFileCombine(nickPath, m_folder_path, nickname));

		if (OpFolderLister* folder_lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*.txt"), nickPath.CStr()))
		{
			BOOL more = folder_lister->Next();
			while (more)
			{
				const uni_char* f_name = folder_lister->GetFileName();
				if (NULL == f_name || 0 == *f_name)
					break;

				if (*f_name != '.')
				{
					if (OpStatus::IsSuccess(ImportNickNames(nickPath, f_name)))
					{
						ret = OpStatus::OK;
					}
				}

				more = folder_lister->Next();
			}

			OP_DELETE(folder_lister);
		}
	}

	return ret;
}


OP_STATUS EudoraImporter::AddImportFile(const OpStringC& path)
{
	RETURN_IF_ERROR(Importer::AddImportFile(path));

	OP_DELETE(m_eudora_ini);
	m_eudora_ini = NULL;

	OpFile file;
	RETURN_IF_ERROR(file.Construct(path.CStr()));

	PrefsFile *prefs = OP_NEW(PrefsFile, (PREFS_STD));
	if (!prefs)
		return OpStatus::ERR_NO_MEMORY;

	TRAPD(err, prefs->ConstructL(); 
			   prefs->SetFileL(&file));

	if (OpStatus::IsError(err))
	{
		OP_DELETE(prefs);
		return err;
	}
	else
		m_eudora_ini = prefs;

	// Define folder path since it is used abundantly
	OpString dir;
	TRAP(err, SplitFilenameIntoComponentsL(path, &dir, NULL, NULL));
	RETURN_IF_ERROR(err);

	return SetImportFolderPath(dir);
}


void EudoraImporter::GetDefaultImportFolder(OpString& path)
{
	path.Empty();
#ifdef MSWIN
	unsigned long size = 1024;
	OpString eudoraPath;
	if (!eudoraPath.Reserve(size))
		return;

	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	OP_STATUS ret = bu->OpRegReadStrValue(HKEY_CURRENT_USER, UNI_L("Software\\Qualcomm\\Eudora\\CommandLine"), UNI_L("current"), eudoraPath.CStr(), &size);

	if (!OpStatus::IsSuccess(ret) || eudoraPath.IsEmpty())
	{
		return;
	}

	uni_char* pEudoraPath = eudoraPath.CStr();

	uni_char* szEudoraProgram = OP_NEWA(uni_char, MAX_PATH);
	if (szEudoraProgram)
	{
		uni_char* szEudoraDirectory = OP_NEWA(uni_char, MAX_PATH);
		if (szEudoraDirectory)
		{
			uni_char* cPtr = uni_strchr(pEudoraPath, (uni_char)' ');
			if (cPtr)
			{
				uni_char* cPtr2 = uni_strchr(cPtr+1, (uni_char)' ');
				if (cPtr2)
				{
					uni_strlcpy(szEudoraProgram, pEudoraPath, (cPtr - pEudoraPath) + 1);

					cPtr++;

					uni_strlcpy(szEudoraDirectory, cPtr, (cPtr2 - cPtr) + 1);

					cPtr++;

					cPtr = uni_strchr(cPtr2, (uni_char)' ');

					if (cPtr)
					{
						OpStringC iniName(UNI_L("eudora.ini"));
						OpStringC eudoraSettings(cPtr2);
						OpPathDirFileCombine(path, eudoraSettings, iniName);
					}
				}
			}
			OP_DELETEA(szEudoraDirectory);
		}
		OP_DELETEA(szEudoraProgram);
	}
#else
	// Here we need to select the Eudora configuration file since we can't deduce where it is.
#endif // MSWIN
}


OP_STATUS EudoraImporter::InsertMailBoxes(const OpStringC& fullPath, const OpStringC& m2FolderPath, INT32 parentIndex)
{
	OP_NEW_DBG("EudoraImporter::InsertMailBoxes", "m2");
	OpVector<DescmapEntry> descmap;

	OP_STATUS status = GetDescmap(fullPath, descmap);

	if (OpStatus::IsSuccess(status))
	{
		for (UINT32 i = 0; i < descmap.GetCount(); i++)
		{
			DescmapEntry* desc = descmap.Get(i);
			if (desc)
			{
				OP_DBG(("fullPath = %S, descmap filename %d = %S", fullPath.CStr(), i, desc->m_fileName.CStr()));

				OpString folderPath;
				OpPathDirFileCombine(folderPath, fullPath, desc->m_fileName);

				OpString virtualPath;
				virtualPath.Set(m2FolderPath),
				virtualPath.Append("/");
				virtualPath.Append(desc->m_mailboxName);

				OpTypedObject::Type type = (DescmapEntry::DM_SUBFOLDER == desc->m_type) ? OpTypedObject::IMPORT_FOLDER_TYPE : OpTypedObject::IMPORT_MAILBOX_TYPE;
				ImporterModelItem* item = OP_NEW(ImporterModelItem, (type, desc->m_mailboxName, folderPath, virtualPath, UNI_L("")));
				if (item)
				{
					INT32 branch = GetModel()->AddLast(item, parentIndex);

					if (DescmapEntry::DM_SUBFOLDER == desc->m_type)
					{
						status = InsertMailBoxes(folderPath, virtualPath, branch);	// call ourselves recursively
					}
				}
				OP_DELETE(desc);
			}
		}

		descmap.Clear();
	}

	return status;
}

OP_STATUS EudoraImporter::ListAttachments(OpString8& raw, OpVector<OpString>&	attachmentNames)
{
	char*				attNumber = '\0';
	char*				firstAttFilename = '\0';
	const char*			body = raw.CStr();
	static const char	keyword[] = "Attachment Converted: \"";

	firstAttFilename = (char *)op_strstr(body, keyword);
	attNumber = firstAttFilename;

	while (attNumber)
	{
		char* startOfAttFileName = strchr(attNumber + 1, '"') + 1;
		char* endOfAttFileName = strchr(startOfAttFileName + 1, '"');

		int lenOfFileName = endOfAttFileName - startOfAttFileName;

		OpString* fileName = OP_NEW(OpString, ());
		if (fileName)
		{
			fileName->Set(startOfAttFileName, lenOfFileName);

			attachmentNames.Add(fileName);
		}
		attNumber = strstr(attNumber + 1, keyword);
	}

	if (firstAttFilename && attachmentNames.GetCount() > 0)
	{
		*firstAttFilename = '\0';	// do not show original filenames in mail body

		for (UINT32 i = 0; i < attachmentNames.GetCount(); i++)
		{
			OpString suggested_filename;
			suggested_filename.Set(*(attachmentNames.Get(i)));

			// We only import from Windows, so absolute paths will be with backslashes.
			int pos = suggested_filename.FindLastOf('\\');	// find last path separator
			if (pos != KNotFound)
			{
				suggested_filename.Delete(0, pos + 1);	// strip path
			}
			OpFile attfile;
			attfile.Construct(attachmentNames.Get(i)->CStr());
			BOOL attexists;
			RETURN_IF_ERROR(attfile.Exists(attexists));
			if (!attexists)
			{
				// And we need to test using relative path, and try for attach/<filename>
				attachmentNames.Get(i)->Set(m_folder_path.CStr());
				attachmentNames.Get(i)->Append(m_attachments_folder_name);
				attachmentNames.Get(i)->Append(PATHSEP);
				attachmentNames.Get(i)->Append(suggested_filename);
			}
		}
	}
	else
	{
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS EudoraImporter::AppendAttachments(OpVector<OpString>&	attachmentNames, Message* message)
{
	BOOL any_attachments_added = FALSE;

	if (attachmentNames.GetCount() > 0)
	{
        OP_STATUS ret;
		for (UINT32 i = 0; i < attachmentNames.GetCount(); i++)
		{
			OpString suggested_filename;
			suggested_filename.Set(*(attachmentNames.Get(i)));

			// We only import from Windows, so absolute paths will be with backslashes.
			int pos = suggested_filename.FindLastOf('\\');	// find last path separator
			if (pos != KNotFound)
			{
				suggested_filename.Delete(0, pos + 1);	// strip path
			}

			ret = message->AddAttachment(suggested_filename, *(attachmentNames.Get(i)));	// add attachment if it exists

			if (!any_attachments_added && OpStatus::IsSuccess(ret))
				any_attachments_added = TRUE;
		}

		attachmentNames.DeleteAll();
	}
	else
	{
		return OpStatus::ERR;
	}

	if (any_attachments_added)
		return OpStatus::OK;
	else
		return OpStatus::ERR;
}

OP_STATUS EudoraImporter::ReadMessage(OpFileLength file_pos, UINT32 length, OpString8& message)
{
	OP_STATUS result = OpStatus::ERR;
	message.Empty();

	if (m_mboxFile && OpStatus::IsSuccess(result = m_mboxFile->SetFilePos(file_pos)))
	{
		if (!message.Reserve(length))
			return OpStatus::ERR_NO_MEMORY;
		OpFileLength bytes_read = 0;
		if (OpStatus::IsSuccess(result = m_mboxFile->Read(message.CStr(), length, &bytes_read)))
		{
			message.CStr()[length] = 0;
			result = m_mboxFile->Eof() ? OpStatus::ERR : OpStatus::OK;
		}
	}

	return result;
}


OP_STATUS EudoraImporter::GetDescmap(const OpStringC& basePath, OpVector<DescmapEntry>& descmap)
{
	OpString fullPath;
	OpStringC fileName(UNI_L("descmap.pce"));
	RETURN_IF_ERROR(OpPathDirFileCombine(fullPath, basePath, fileName));

	OpFile descmapFile;
	RETURN_IF_ERROR(descmapFile.Construct(fullPath.CStr()));

	RETURN_IF_ERROR(descmapFile.Open(OPFILE_READ|OPFILE_SHAREDENYWRITE));

	while (!descmapFile.Eof())
	{
		const int MAX_DESC_LEN = 256;

		OpString8 buf;
		if (OpStatus::IsError(descmapFile.ReadLine(buf)) ||
			buf.IsEmpty())
		{
			break;
		}

		buf.Strip();
		if (buf.Length() > MAX_DESC_LEN)
		{
			buf[MAX_DESC_LEN] = 0;
		}

		// Make a tokenizer
		Tokenizer tokenizer;
		tokenizer.Init(200); // set this default token size. /martin Also // FIXME: OOM
		tokenizer.SetWhitespaceChars(",\n");
		tokenizer.SetInputString(buf.CStr());

		OpString8 token;
		if (token.Reserve(MAX_DESC_LEN))
			tokenizer.GetNextToken(token.CStr(), MAX_DESC_LEN);

		if (token.HasContent())
		{
			DescmapEntry* entry = OP_NEW(DescmapEntry, ());
			if (entry)
			{
				entry->m_mailboxName.Set(token);

				tokenizer.GetNextToken(token.CStr(), MAX_DESC_LEN);
				entry->m_fileName.Set(token);

				tokenizer.GetNextToken(token.CStr(), MAX_DESC_LEN);

				switch (token[0])
				{
					case 'S':
						entry->m_type = DescmapEntry::DM_SYSMAILBOX;
						break;
					case 'M':
						entry->m_type = DescmapEntry::DM_MAILBOX;
						break;
					case 'F':
						entry->m_type = DescmapEntry::DM_SUBFOLDER;
						break;
					default:
						entry->m_type = DescmapEntry::DM_INVALID;
				}

				descmap.Add(entry);
			}
		}
	}

	descmapFile.Close();
	return OpStatus::OK;
}


OP_STATUS EudoraImporter::ImportNickNames(const OpStringC& basePath, const OpStringC& fileName)
{
	OpString fullPath;

	OpPathDirFileCombine(fullPath, basePath, fileName);
    OpFile nickFile;
    RETURN_IF_ERROR(nickFile.Construct(fullPath.CStr()));
    RETURN_IF_ERROR(nickFile.Open(OPFILE_READ));

	const uni_char* alias_keyword = UNI_L("alias ");
	const uni_char* note_keyword = UNI_L("note ");

    OpString8 line;
	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	while (!nickFile.Eof())
	{
		if (OpStatus::IsSuccess(nickFile.ReadLine(line)) && line.HasContent())
		{
			OpString buf16;
			int invalid_inp = 0;
			TRAPD(rc, invalid_inp = SetFromEncodingL(&buf16, g_op_system_info->GetSystemEncodingL(), line.CStr(), line.Length()));
			if (rc == OpStatus::OK && invalid_inp == 0)
			{
				uni_char* alias = uni_strstr(buf16.CStr(), alias_keyword);
				if (alias)
				{
					// extract nick name
					uni_char* start = alias;
					uni_char* white_space = uni_strstr(start, UNI_L(" "));

					start = ++white_space;
					uni_char* end;

					if (*start == 0x0022)	// "
					{
						start++;
						end = uni_strstr(start, UNI_L("\""));
					}
					else
					{
						end = uni_strstr(start, UNI_L(" "));
					}

					int len = end - start;

					OpString name;
					name.Set(start, len);

					// nick name finished, now extract mail address(es)
					OpString mailAddr;

					start = end + 1;

					mailAddr.Set(start);
					mailAddr.Strip();

					RETURN_IF_ERROR(bu->AddContact(mailAddr, name));
				}
				else
				{
					uni_char* note = uni_strstr(buf16.CStr(), note_keyword);
					if (note)
					{
						// jant TODO later: save real name/ address?
					}
				}
			}
		}
	}

	RETURN_IF_ERROR(nickFile.Close());
	return OpStatus::OK;
}

#endif //M2_SUPPORT
