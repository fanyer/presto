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

# include "adjunct/m2/src/engine/account.h"
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/import/ImporterModel.h"
# include "adjunct/m2/src/import/jsprefs.h"
# include "adjunct/m2/src/import/NetscapeImporter.h"
# include "adjunct/m2/src/util/str/m2tokenizer.h"
# include "modules/locale/oplanguagemanager.h"
# include "modules/prefsfile/prefsfile.h"
# include "modules/util/filefun.h"
# include "modules/util/path.h"

NetscapeImporter::NetscapeImporter()
:	m_prefs_js(NULL)
{
}

NetscapeImporter::~NetscapeImporter()
{
	OP_DELETE(m_prefs_js);
}


OP_STATUS NetscapeImporter::Init()
{
	if (!m_prefs_js)
		return OpStatus::ERR_NULL_POINTER;

	GetModel()->DeleteAll();

	RETURN_IF_ERROR(InitLookAheadBuffers());

	const char* _accounts = m_prefs_js->Find("mail.accountmanager.accounts");
	if (_accounts)
	{
		Tokenizer tzer;
		tzer.Init(200);
		tzer.SetWhitespaceChars(",\n");

		tzer.SetInputString(_accounts);

		const int MAX_TOKEN_SIZE = 512;
		OpString8 tok;
		if (!tok.Reserve(MAX_TOKEN_SIZE))
			return OpStatus::ERR_NO_MEMORY;

		tzer.GetNextToken(tok.CStr(), MAX_TOKEN_SIZE);
		while (!tok.IsEmpty())
		{
			OpString8 mail_server;
			mail_server.AppendFormat("mail.account.%s.server", tok.CStr());
			char* _server = m_prefs_js->Find(mail_server.CStr());

			if (_server)
			{
				OpString8 mail_name;
				mail_name.AppendFormat("mail.server.%s.name", _server);
				char* _name = m_prefs_js->Find(mail_name.CStr());

				if (_name)
				{
					OpString account;
					account.SetFromUTF8(_name);

					OpString8 mail_dir;
					mail_dir.AppendFormat("mail.server.%s.directory-rel", _server);
					char* _directory = m_prefs_js->Find(mail_dir.CStr());
					if (_directory)
					{
						OpString path;
						TRAPD(err, SplitFilenameIntoComponentsL( *m_file_list.Get(0), &path, NULL, NULL));
						if (op_strncmp(_directory, "[ProfD]", 7) == 0)
							path.Append(_directory + 7);
						else
							path.Append(_directory);

						StripDoubleBS(path);

						OpString m2FolderPath, imported;
						OpStatus::Ignore(m2FolderPath.Set(account));
						OpStatus::Ignore(g_languageManager->GetString(Str::S_IMPORTED, imported));
						OpStatus::Ignore(imported.Insert(0, " ("));
						OpStatus::Ignore(imported.Append(")"));
						OpStatus::Ignore(m2FolderPath.Append(imported));

						OpString info;
						info.Set(tok);	// prefs file accountmanager 'accountx'

						ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_ACCOUNT_TYPE,
																		account, path, m2FolderPath, info));
						if (item)
						{
							item->SetSettingsFileName(*m_file_list.Get(0));

							INT32 branch = GetModel()->AddLast(item);

							InsertMailBoxes(path, m2FolderPath, branch);
						}
					}
				}
			}

			tzer.GetNextToken(tok.CStr(), MAX_TOKEN_SIZE);
		}
	}

	return OpStatus::OK;
}


OP_STATUS NetscapeImporter::InsertMailBoxes(const OpStringC& basePath, const OpStringC& m2FolderPath, INT32 parentIndex)
{
	// find subdirectories (*.sbd)
	if (OpFolderLister* folder_lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*.sbd"), basePath.CStr()))
	{
		BOOL more = folder_lister->Next();
		while (more)
		{
			const uni_char* sub_dir = folder_lister->GetFileName();
			if (NULL == sub_dir || 0 == *sub_dir)
				break;

			OpString name;
			name.Set(sub_dir);

			int dot = name.FindLastOf('.');
			if (dot != KNotFound)
			{
				name.CStr()[dot] = 0;
			}

			ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_FOLDER_TYPE,
															name, folder_lister->GetFullPath(), m2FolderPath, UNI_L("")));

			if (item)
			{
				INT32 branch = GetModel()->AddLast(item, parentIndex);

				if (branch > -1)
				{
					OpString m2Folder;
					m2Folder.AppendFormat(UNI_L("%s/%s"), m2FolderPath.CStr(), name.CStr());
					InsertMailBoxes(folder_lister->GetFullPath(), m2Folder, branch);
				}
			}

			more = folder_lister->Next();
		}

		OP_DELETE(folder_lister);
	}

	// find mbox files (no extension)
	const uni_char * mbx_pattern = UNI_L("*");

	if (OpFolderLister* folder_lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, mbx_pattern, basePath.CStr()))
	{
		BOOL more = folder_lister->Next();
		while (more)
		{
			const uni_char* f_name = folder_lister->GetFileName();
			if (NULL == f_name || 0 == *f_name)
				break;

			if (*f_name != '.')
			{
				OpString m2Folder;
				m2Folder.AppendFormat(UNI_L("%s/%s"), m2FolderPath.CStr(), f_name);

				ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_MAILBOX_TYPE,
																folder_lister->GetFileName(), folder_lister->GetFullPath(), m2Folder, UNI_L("")));

				if (item)
					GetModel()->AddLast(item, parentIndex);
			}

			more = folder_lister->Next();
		}

		OP_DELETE(folder_lister);
	}

	return OpStatus::OK;
}


void NetscapeImporter::StripDoubleBS(OpString& path)
{
	uni_char *pos_in, *pos_out;

	pos_in = pos_out = path.CStr();
	while ((*pos_out = *pos_in) != 0)
	{
		if (pos_in[0] == '\\' && pos_in[1] == '\\' )
			pos_in++;

		pos_out++;
		pos_in++;
	}
}


OP_STATUS NetscapeImporter::ImportSettings()
{
	if (!m_prefs_js)
		return OpStatus::ERR_NULL_POINTER;

	ImporterModelItem* root = GetRootItem();
	if (!root)
		return OpStatus::ERR_NULL_POINTER;

	OpString8 prefs_account8;
	prefs_account8.Set(root->GetInfo().CStr());

	if (prefs_account8.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS res = OpStatus::ERR;

	OpString8 mail_server;
	mail_server.AppendFormat("mail.account.%s.server", prefs_account8.CStr());
	char* _server = m_prefs_js->Find(mail_server.CStr());
	OpString8 mail_ids;
	mail_ids.AppendFormat("mail.account.%s.identities", prefs_account8.CStr());
	char* _id = m_prefs_js->Find(mail_ids.CStr());

	if (_server && _id)
	{
		Account* account = MessageEngine::GetInstance()->CreateAccount();
		if (account)
		{
			OpString8 mail_type;
			mail_type.AppendFormat("mail.server.%s.type", _server);
			char* _type = m_prefs_js->Find(mail_type.CStr());
			if (_type)
			{
				if (strcmp(_type, "pop3") == 0)
				{
					account->SetIncomingProtocol(AccountTypes::POP);
				}
				else if (strcmp(_type, "imap") == 0)
				{
					account->SetIncomingProtocol(AccountTypes::IMAP);
				}
				else if (strcmp(_type, "nntp") == 0)
				{
					account->SetIncomingProtocol(AccountTypes::NEWS);
				}
				else
				{
					return OpStatus::ERR_NOT_SUPPORTED;
				}
			}

			account->SetOutgoingProtocol(AccountTypes::SMTP);
			account->SetDefaults();

			OpString tmp;

			OpString8 mail_fullName;
			mail_fullName.AppendFormat("mail.identity.%s.fullName", _id);
			char* _fullName = m_prefs_js->Find(mail_fullName.CStr());
			if (_fullName)
			{
				tmp.SetFromUTF8(_fullName);
				account->SetRealname(tmp);
			}

			OpString8 mail_org;
			mail_org.AppendFormat("mail.identity.%s.organization", _id);
			char* _organization = m_prefs_js->Find(mail_org.CStr());
			if (_organization)
			{
				tmp.Set(_organization);
				account->SetOrganization(tmp);
			}

			OpString8 mail_replyto;
			mail_replyto.AppendFormat("mail.identity.%s.reply_to", _id);
			char* _reply_to = m_prefs_js->Find(mail_replyto.CStr());
			if (_reply_to)
			{
				tmp.Set(_reply_to);
				account->SetReplyTo(tmp);
			}

			OpString8 mail_usermail;
			mail_usermail.AppendFormat("mail.identity.%s.useremail", _id);
			char* _useremail = m_prefs_js->Find(mail_usermail.CStr());
			if (_useremail)
			{
				tmp.Set(_useremail);
				account->SetEmail(tmp);
			}

			OpString8 mail_usename;
			mail_usename.AppendFormat("mail.server.%s.userName", _server);
			char* _userName = m_prefs_js->Find(mail_usename.CStr());
			if (_userName)
			{
				tmp.Set(_userName);
				account->SetIncomingUsername(tmp);
			}

			OpString8 mail_hostname;
			mail_hostname.AppendFormat("mail.server.%s.hostname", _server);
			char* _hostname = m_prefs_js->Find(mail_hostname.CStr());
			if (_hostname)
			{
				tmp.Set(_hostname);
				account->SetIncomingServername(tmp);
			}

			OpString8 mail_leaveonserver;
			mail_leaveonserver.AppendFormat("mail.server.%s.leave_on_server", _server);
			char* _leave_on_server = m_prefs_js->Find(mail_leaveonserver.CStr());
			if (_leave_on_server && strcmp(_leave_on_server, "true") == 0)
			{
				account->SetLeaveOnServer(TRUE);
			}
			else
			{
				account->SetLeaveOnServer(FALSE);
			}

			OpString8 mail_name;
			mail_name.AppendFormat("mail.server.%s.name", _server);
			char* _name = m_prefs_js->Find(mail_name.CStr());
			if (_name)
			{
				tmp.SetFromUTF8(_name);
				account->SetAccountName(tmp);
			}

			OpString8 mail_checknewmail;
			mail_checknewmail.AppendFormat("mail.server.%s.check_new_mail", _server);
			char* _check_new_mail = m_prefs_js->Find(mail_checknewmail.CStr());
			if (_check_new_mail && strcmp(_check_new_mail, "true") == 0)
			{
				account->SetPollInterval(300);
			}
			else
			{
				account->SetPollInterval(0);
			}

			OpString8 mail_issecure;
			mail_issecure.AppendFormat("mail.server.%s.isSecure", _server);
			char* _isSecure = m_prefs_js->Find(mail_issecure.CStr());
			if (_isSecure && strcmp(_isSecure, "true") == 0)
			{
				account->SetUseSecureConnectionIn(TRUE);
			}

			OpString8 mail_smtpServer;
			mail_smtpServer.AppendFormat("mail.identity.%s.smtpServer", _id);
			char* _smtpServer = m_prefs_js->Find(mail_smtpServer.CStr());
			if (_smtpServer)
			{
				OpString8 mail_hostname;
				mail_hostname.AppendFormat("mail.smtpserver.%s.hostname", _smtpServer);
				char* _hostname = m_prefs_js->Find(mail_hostname.CStr());
				if (_hostname)
				{
					tmp.Set(_hostname);
					account->SetOutgoingServername(tmp);
				}

				OpString8 mail_username;
				mail_username.AppendFormat("mail.smtpserver.%s.username", _smtpServer);
				char* _username = m_prefs_js->Find(mail_username.CStr());
				if (_username)
				{
					tmp.Set(_username);
					account->SetOutgoingUsername(tmp);
				}

				OpString8 mail_tryssl;
				mail_tryssl.AppendFormat("mail.smtpserver.%s.try_ssl", _smtpServer);
				char* _try_ssl = m_prefs_js->Find(mail_tryssl.CStr());
				if (_try_ssl && strcmp(_try_ssl, "0") != 0)
				{
					account->SetUseSecureConnectionOut(TRUE);
				}
			}

			res = account->Init();

			if (OpStatus::IsSuccess(res))
			{
				SetM2Account(account->GetAccountId());
				// jant TODO : signature
			}
		}
	}
	else
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	return res;
}


OP_STATUS NetscapeImporter::ImportMessages()
{
	ImporterModelItem* item = GetImportItem();
	if (!item)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS res = GetModel()->MakeSequence(m_sequence, item);

	if (OpStatus::IsSuccess(res))
	{
		OpenPrefsFile();

		res = StartImport();
	}

	return res;
}


OP_STATUS NetscapeImporter::AddImportFile(const OpStringC& path)
{
	RETURN_IF_ERROR(Importer::AddImportFile(path));

	if (path.IsEmpty())
	{
		return OpStatus::OK;
	}

	OP_DELETE(m_prefs_js);

	if (0 != (m_prefs_js = OP_NEW(JsPrefsFile, ())))
	{
		m_prefs_js->SetFile(path);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}


void NetscapeImporter::GetDefaultImportFolder(OpString& path)
{
#if defined (MSWIN)
	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	if (!bu)
		return;

	HKEY hkey = NULL;
	DWORD res = bu->_RegOpenKeyEx(HKEY_CURRENT_USER, UNI_L("Volatile Environment"), 0, KEY_READ, &hkey);
	if (ERROR_SUCCESS == res)
	{
		DWORD type = 0;
		DWORD size = 511;
		uni_char value[512] = {0};

		DWORD res = bu->_RegQueryValueEx(hkey, UNI_L("APPDATA"), NULL, &type, (LPBYTE)value, &size);

		if (ERROR_SUCCESS == res && (REG_SZ == type || REG_EXPAND_SZ == type))
		{
			path.Set(value);
			OpString ini_path;
			ini_path.Set(value);
			if (EngineTypes::THUNDERBIRD == Importer::m_id)
			{
				ini_path.Append("\\Thunderbird\\profiles.ini");
				PrefsFile* profiles_ini_file = MessageEngine::GetInstance()->GetGlueFactory()->CreatePrefsFile(ini_path);
				if (profiles_ini_file)
				{
					OpString prefs_path;
					TRAPD(err, profiles_ini_file->ReadStringL("Profile0", "Path", prefs_path));
					if (prefs_path.HasContent())
					{
						int pos = prefs_path.FindLastOf('/');
						if (pos != KNotFound)
						{
							prefs_path[pos] = '\\';
						}
					}
					path.Append("\\Thunderbird\\");
					path.Append(prefs_path);
					path.Append("\\prefs.js");

					MessageEngine::GetInstance()->GetGlueFactory()->DeletePrefsFile(profiles_ini_file);
				}
			}
			else if (EngineTypes::NETSCAPE == Importer::m_id)
			{
				path.Append("\\Mozilla\\Profiles\\Default\\");
			}
		}
		bu->_RegCloseKey(hkey);
	}
#else
	// Shall be selected by user.
#endif //MSWIN
}

#endif //M2_SUPPORT
