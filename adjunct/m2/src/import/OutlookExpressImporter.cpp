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

# ifdef MSWIN

#  include "adjunct/m2/src/engine/message.h"
#  include "adjunct/m2/src/engine/engine.h"
#  include "adjunct/m2/src/engine/account.h"
#  include "adjunct/m2/src/engine/accountmgr.h"
#  include "adjunct/m2/src/util/str/m2tokenizer.h"
#  include "adjunct/m2/src/import/ImporterModel.h"
#  include "adjunct/m2/src/import/liboe.h"
#  include "adjunct/m2/src/import/MSWinUtil.h"
#  include "adjunct/m2/src/import/OutlookExpressImporter.h"
#  include "modules/locale/oplanguagemanager.h"
#  include <math.h>


const char* OEAddressBookFieldTokens[] =
{
	"First Name",
	"Last Name",
	"Middle Name",
	"Name",
	"Nickname",
	"E-mail Address",
	"Home Street",
	"Home City",
	"Home Postal Code",
	"Home State",
	"Home Country/Region",
	"Home Phone",
	"Home Fax",
	"Mobile Phone",
	"Personal Web Page",
	"Business Street",
	"Business City",
	"Business Postal Code",
	"Business State",
	"Business Country/Region",
	"Business Web Page",
	"Business Phone",
	"Business Fax",
	"Pager",
	"Company",
	"Job Title",
	"Department",
	"Office Location",
	"Notes"
};

#  define OE_IMPORT_FIRSTNAME			0x00000001
#  define OE_IMPORT_LASTNAME			0x00000002
#  define OE_IMPORT_MIDDLENAME			0x00000004
#  define OE_IMPORT_NAME				0x00000008
#  define OE_IMPORT_NICKNAME			0x00000010
#  define OE_IMPORT_EMAIL				0x00000020
#  define OE_IMPORT_HSTREET				0x00000040
#  define OE_IMPORT_HCITY				0x00000080
#  define OE_IMPORT_HPOSTALCODE			0x00000100
#  define OE_IMPORT_HSTATE				0x00000200
#  define OE_IMPORT_HCOUNTRY			0x00000400
#  define OE_IMPORT_HPHONE				0x00000800
#  define OE_IMPORT_HFAX				0x00001000
#  define OE_IMPORT_MOBILEPHONE			0x00002000
#  define OE_IMPORT_PERSONALWEB			0x00004000
#  define OE_IMPORT_BSTREET				0x00008000
#  define OE_IMPORT_BCITY				0x00010000
#  define OE_IMPORT_BPOSTALCODE			0x00020000
#  define OE_IMPORT_BSTATE				0x00040000
#  define OE_IMPORT_BCOUNTRY			0x00080000
#  define OE_IMPORT_BWEB				0x00100000
#  define OE_IMPORT_BPHONE				0x00200000
#  define OE_IMPORT_BFAX				0x00400000
#  define OE_IMPORT_PAGER				0x00800000
#  define OE_IMPORT_COMPANY				0x01000000
#  define OE_IMPORT_JOBTITLE			0x02000000
#  define OE_IMPORT_DEPARTMENT			0x04000000
#  define OE_IMPORT_OFFICELOCATION		0x08000000
#  define OE_IMPORT_NOTES				0x10000000


#ifndef CAPACITY
#  define CAPACITY 			65535
#endif
#  define MAX_REGKEY_LEN	512


FILE*	OutlookExpressImporter::m_tmpFile	= NULL;
INT32	OutlookExpressImporter::m_total		= 0;
BOOL	OutlookExpressImporter::m_blank		= TRUE;


OutlookExpressImporter::OutlookExpressImporter()
:	m_version(0),
	m_prevVersion(0),
	m_raw(NULL),
	m_raw_length(0),
	m_raw_capacity(0)
{
	m_tmp_filename.Reserve(MAX_PATH);
}


OutlookExpressImporter::~OutlookExpressImporter()
{
	OP_DELETEA(m_raw);
	m_raw = NULL;

	if (m_tmpFile)
	{
		fclose(m_tmpFile);
		m_tmpFile = NULL;
	}

	m_identities.DeleteAll();
}


OP_STATUS OutlookExpressImporter::Init()
{
	GetModel()->DeleteAll();

	if (!GetIdentities())
		return OpStatus::ERR;

	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	if (!bu)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = OpStatus::ERR;

	for (UINT32 i = 0; i < m_identities.GetCount(); i++)
	{
		Identity* identity = m_identities.Get(i);

		OpString oe_keyroot;
		oe_keyroot.AppendFormat(UNI_L("%s\\%s"), identity->oe_key.CStr(), identity->ver.CStr());

		ImporterModelItem* identity_item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_IDENTITY_TYPE,
																 identity->name.CStr(), identity->path.CStr(),
																 identity->key.CStr(), oe_keyroot.CStr()));
		if (!identity_item)
			continue;

		INT32 parent_index = GetModel()->AddLast(identity_item);

		OpString accounts_key;
		accounts_key.Set(identity->key.CStr());
		accounts_key.Append(UNI_L("\\Internet Account Manager\\Accounts"));

		HKEY hkey_accounts = NULL;
		DWORD res = bu->_RegOpenKeyEx(HKEY_CURRENT_USER, accounts_key, 0, KEY_READ, &hkey_accounts);

		DWORD index = 0;
		while (ERROR_SUCCESS == res || ERROR_MORE_DATA == res)
		{
			unsigned long size = MAX_REGKEY_LEN - 1;
			uni_char key[MAX_REGKEY_LEN] = {0};

			res = bu->_RegEnumKeyEx(hkey_accounts, index++, key, &size, NULL, NULL, NULL, NULL);

			if ((ERROR_SUCCESS == res || ERROR_MORE_DATA == res) && uni_strncmp(key, UNI_L("00"), 2) == 0)
			{
				HKEY hkey = NULL;

				size = MAX_REGKEY_LEN - 1;
				res = bu->_RegOpenKeyEx(hkey_accounts, key, 0, KEY_READ, &hkey);

				if (ERROR_SUCCESS == res)
				{
					uni_char value[MAX_REGKEY_LEN] = {'\0'};
					size = MAX_REGKEY_LEN - 1;
					res = bu->_RegQueryValueEx(hkey, UNI_L("Account Name"), NULL, NULL, (LPBYTE)value, &size);
					if (ERROR_SUCCESS == res)
					{
						OpString full_key;
						full_key.Set(accounts_key);
						full_key.Append(UNI_L("\\"));
						full_key.Append(key);

						ImporterModelItem* account_item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_ACCOUNT_TYPE,
																				value, identity->path.CStr(), UNI_L(""), full_key.CStr()));
						if (account_item)
						{
							INT32 parent = GetModel()->AddLast(account_item, parent_index);

							OpAutoVector<OpString> mailboxes;
							OpString path;
							path.Set(identity->path.CStr());

							ret = GetMailBoxNames(path, mailboxes);

							for (UINT32 j = 0; j < mailboxes.GetCount(); j++)
							{
								OpString* box = mailboxes.Get(j);

								OpString fullPath;
								fullPath.Set(path);

								if (fullPath.HasContent() && fullPath[fullPath.Length() - 1] != UNI_L('\\'))
								{
									fullPath.Append(UNI_L("\\"));
								}

								fullPath.Append(*box);

								int last_dot = box->FindLastOf('.');
								if (KNotFound != last_dot)
								{
									box->CStr()[last_dot] = 0;	// cut off filesuffix
								}

								OpString identity_name, account;
								identity_name.Set(identity->name.CStr());
								account.Set(value);

								OpString formatstring;
								RETURN_IF_ERROR(g_languageManager->GetString(Str::S_IMPORT_PROGRESS_STATUS, formatstring));
								OpString m2FolderPath;
								m2FolderPath.AppendFormat(formatstring.CStr(),
														 identity_name.CStr(), account.CStr(), box->CStr());

								ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_MAILBOX_TYPE,
																				*box, fullPath, m2FolderPath, UNI_L("")));

								if (item)
									GetModel()->AddLast(item, parent);
							}
						}
					}
					bu->_RegCloseKey(hkey);
				}
			}
		}
		bu->_RegCloseKey(hkey_accounts);
	}
	return ret;
}


OP_STATUS OutlookExpressImporter::ImportAccountSettings(const OpStringC& identityName,
														const OpStringC& account_reg_key,
														const OpStringC& signature)
{
	AccountManager* account_manager = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();

	if (!account_manager || !bu)
		return OpStatus::ERR;

	Account* account = MessageEngine::GetInstance()->CreateAccount();
	if (!account)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	account->SetOutgoingProtocol(AccountTypes::SMTP);

	HKEY hkey = NULL;
	DWORD res = bu->_RegOpenKeyEx(HKEY_CURRENT_USER, account_reg_key.CStr(), 0, KEY_READ, &hkey);

	if (res != ERROR_SUCCESS)
		return OpStatus::ERR;

	uni_char value[MAX_REGKEY_LEN] = {0};
	DWORD int_value = 0;

	if (GetRegistryValue(hkey, UNI_L("Account Name"), value))
	{
		OpString accountName;
		accountName.AppendFormat(UNI_L("%s [%s]"), identityName.CStr(), value);
		account->SetAccountName(accountName);
	}

	BOOL isNewsAccount = FALSE;

	if (GetRegistryValue(hkey, UNI_L("POP3 Server"), value))
	{
		account->SetIncomingProtocol(AccountTypes::POP);
		account->SetDefaults();

		account->SetIncomingServername(value);

		if (GetRegistryValue(hkey, UNI_L("POP3 User Name"), value))
		{
			account->SetIncomingUsername(value);
		}

		if (GetRegistryValue(hkey, UNI_L("POP3 Port"), &int_value))
		{
			account->SetIncomingPort((const UINT16)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("POP3 Secure Connection"), &int_value))
		{
			account->SetUseSecureConnectionIn((const BOOL)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("POP3 Use Sicily"), &int_value))
		{
			account->SetUseSecureAuthentication((const BOOL)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("POP3 Timeout"), &int_value))
		{
			account->SetIncomingTimeout((const UINT16)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("Leave Mail On Server"), &int_value))
		{
			account->SetLeaveOnServer((const BOOL)int_value);
		}
	}
	else if (GetRegistryValue(hkey, UNI_L("IMAP Server"), value))
	{
		account->SetIncomingProtocol(AccountTypes::IMAP);
		account->SetDefaults();

		account->SetIncomingServername(value);

		if (GetRegistryValue(hkey, UNI_L("IMAP User Name"), value))
		{
			account->SetIncomingUsername(value);
		}

		if (GetRegistryValue(hkey, UNI_L("IMAP Port"), &int_value))
		{
			account->SetIncomingPort((const UINT16)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("IMAP Secure Connection"), &int_value))
		{
			account->SetUseSecureConnectionIn((const BOOL)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("IMAP Use Sicily"), &int_value))
		{
			account->SetUseSecureConnectionIn((const BOOL)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("IMAP Timeout"), &int_value))
		{
			account->SetIncomingTimeout((const UINT16)int_value);
		}
	}
	else if (GetRegistryValue(hkey, UNI_L("NNTP Server"), value))
	{
		isNewsAccount = TRUE;

		account->SetIncomingProtocol(AccountTypes::NEWS);
		account->SetOutgoingProtocol(AccountTypes::UNDEFINED);
		account->SetDefaults();

		account->SetIncomingServername(value);

		if (GetRegistryValue(hkey, UNI_L("NNTP Display Name"), value))
		{
			account->SetRealname(value);
		}

		if (GetRegistryValue(hkey, UNI_L("NNTP User Name"), value))
		{
			account->SetIncomingUsername(value);
		}

		if (GetRegistryValue(hkey, UNI_L("NNTP Port"), &int_value))
		{
			account->SetIncomingPort((const UINT16)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("NNTP Secure Connection"), &int_value))
		{
			account->SetUseSecureConnectionIn((const BOOL)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("NNTP Email Address"), value))
		{
			account->SetEmail(value);
		}

		if (GetRegistryValue(hkey, UNI_L("NNTP Organization Name"), value))
		{
			account->SetOrganization(value);
		}

		if (GetRegistryValue(hkey, UNI_L("NNTP Reply To Email Address"), value))
		{
			account->SetReplyTo(value);
		}

		if (GetRegistryValue(hkey, UNI_L("NNTP Use Sicily"), &int_value))
		{
			account->SetUseSecureAuthentication((const BOOL)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("NNTP Timeout"), &int_value))
		{
			account->SetIncomingTimeout((const UINT16)int_value);
		}
	}

	if (!isNewsAccount)
	{
		if (GetRegistryValue(hkey, UNI_L("SMTP Server"), value))
		{
			account->SetOutgoingServername(value);
		}

		if (GetRegistryValue(hkey, UNI_L("SMTP Port"), &int_value))
		{
			account->SetOutgoingPort((const UINT16)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("SMTP Secure Connection"), &int_value))
		{
			account->SetUseSecureConnectionOut((const BOOL)int_value);
		}

		if (GetRegistryValue(hkey, UNI_L("SMTP Use Sicily"), &int_value))
		{
			account->SetOutgoingAuthenticationMethod(int_value==0 ? AccountTypes::NONE : AccountTypes::AUTOSELECT);
		}

		if (GetRegistryValue(hkey, UNI_L("SMTP Display Name"), value))
		{
			account->SetRealname(value);
		}

		if (GetRegistryValue(hkey, UNI_L("SMTP Email Address"), value))
		{
			account->SetEmail(value);
		}

		if (GetRegistryValue(hkey, UNI_L("SMTP Organization Name"), value))
		{
			account->SetOrganization(value);
		}

		if (GetRegistryValue(hkey, UNI_L("SMTP Reply To Email Address"), value))
		{
			account->SetReplyTo(value);
		}

		if (GetRegistryValue(hkey, UNI_L("SMTP Timeout"), &int_value))
		{
			account->SetOutgoingTimeout((const UINT16)int_value);
		}
	}

	RETURN_IF_ERROR(account->Init());

	if (signature.HasContent())
	{
		RETURN_IF_ERROR(account->SetSignature(signature,FALSE));
		return account->SaveSettings();
	}

	return OpStatus::OK;
}


BOOL OutlookExpressImporter::ImportSignature(const OpStringC& signature_reg_key, OpString& signature)
{
	OpString keyRoot;
	keyRoot.AppendFormat(UNI_L("%s\\signatures"), signature_reg_key);

	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();

	HKEY hkey = NULL;
	if (bu->_RegOpenKeyEx(HKEY_CURRENT_USER, keyRoot.CStr(), 0, KEY_READ, &hkey) != ERROR_SUCCESS)
		return FALSE;

	BOOL res = FALSE;
	uni_char value[MAX_REGKEY_LEN] = {0};
	if (GetRegistryValue(hkey, UNI_L("Default Signature"), value))
	{
		bu->_RegCloseKey(hkey);
		OpString key;
		key.AppendFormat(UNI_L("%s\\%s"), keyRoot.CStr(), value);

		if (ERROR_SUCCESS == bu->_RegOpenKeyEx(HKEY_CURRENT_USER, key.CStr(), 0, KEY_READ, &hkey))
		{
			DWORD val = 0;
			if (GetRegistryValue(hkey, UNI_L("type"), &val))
			{
				switch (val)
				{
				case 1:
					if (GetRegistryValue(hkey, UNI_L("text"), value))
					{
						signature.Set(value);
						res = TRUE;
					}
					break;

				case 2:
				{
					if (!GetRegistryValue(hkey, UNI_L("file"), value))
						break;

					if (uni_strlen(value) == 0)
						break;

					GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
					OpFile* sig_file = glue_factory->CreateOpFile(value);

					if (sig_file)
					{
						if (OpStatus::IsSuccess(sig_file->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE)))
						{
							OpFileLength length;
							sig_file->GetFileLength(length);

							if (length > 0)
							{
								char* buf = OP_NEWA(char, (int)length + 1);
								OpFileLength bytes_read = 0;
								if (buf && sig_file->Read(buf, length, &bytes_read)==OpStatus::OK)
								{
									int nWideChars = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
																		 buf, (int)length, NULL, 0);
									if (nWideChars > 0)
									{
										OpString signature;
										if (signature.Reserve(nWideChars + 1))
										{
											MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
																buf, (int)length, signature.CStr(), nWideChars);

											signature.CStr()[nWideChars] = 0;
											res = TRUE;
										}
									}
								}

								OP_DELETEA(buf);
							}

							sig_file->Close();
						}
						glue_factory->DeleteOpFile(sig_file);
					}
				}
				}
			}
		}
	}
	bu->_RegCloseKey(hkey);

	return res;
}


OP_STATUS OutlookExpressImporter::ImportSettings()
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


OP_STATUS OutlookExpressImporter::ImportMessages()
{
	ImporterModelItem* item = GetImportItem();
	if (!item)
		return OpStatus::ERR;

	RETURN_IF_ERROR(GetModel()->MakeSequence(m_sequence, item));

	OpenPrefsFile();

	return StartImport();
}


OP_STATUS OutlookExpressImporter::ImportContacts()
{
	OP_STATUS ret = OpStatus::OK;

	if (m_csvPath.IsEmpty())
		return OpStatus::ERR;

	UINT32 ctTokenFlags = 0;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	OpFile* contactsFile = glue_factory->CreateOpFile(m_csvPath);
	if (NULL == contactsFile)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsSuccess(ret = contactsFile->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE)))
	{
		const size_t MAX_READ_LEN = 1024;
		OpString8 buf;
		OpString8 token;

		Tokenizer tokenizer;
		if (tokenizer.Init(200) == OpStatus::ERR_NO_MEMORY || // Default max tokensize is 200 /martin
			!token.Reserve(MAX_READ_LEN))
		{
			glue_factory->DeleteOpFile(contactsFile);
			return OpStatus::ERR_NO_MEMORY;
		}

		tokenizer.SetWhitespaceChars(";");
		tokenizer.SetSpecialChars(";");

		// Read address book token fields
		if (contactsFile->ReadLine(buf) == OpStatus::OK &&
			buf.HasContent())
		{
			buf.Strip();
			if (buf.Length() > MAX_READ_LEN)
			{
				*(buf.CStr()+MAX_READ_LEN) = 0;
			}

			if(OpStatus::IsSuccess(tokenizer.SetInputString(buf)))
			{
				int i = 0;
				// Flag address book token fields found
				while(tokenizer.GetNextToken(token, MAX_READ_LEN))
					if (!token.IsEmpty())
						for(; i<29/*ARRAY_SIZE(OEAddressBookFieldTokens)*/; i++)
							if(strcmp(token.CStr(),OEAddressBookFieldTokens[i]) == 0)
							{
								ctTokenFlags |= ((UINT32)1) << i;
								i++;
								break;
							}
			}
		}

		if(OpStatus::IsSuccess(ret))
		{
			OpString name;
			OpString address;
			OpString notes;

			while(!contactsFile->Eof())
			{
				if (contactsFile->ReadLine(buf) == OpStatus::OK &&
					buf.HasContent())
				{
					buf.Strip();
					if (buf.Length() > MAX_READ_LEN)
					{
						*(buf.CStr()+MAX_READ_LEN) = 0;
					}

					if(OpStatus::IsSuccess(tokenizer.SetInputString(buf)))
					{
						UINT32 counter = 1;
						while(tokenizer.GetNextToken(token, MAX_READ_LEN) && (counter <= OE_IMPORT_NOTES))
						{
							switch(counter & ctTokenFlags)
							{
							case OE_IMPORT_FIRSTNAME:
							case OE_IMPORT_LASTNAME:
							case OE_IMPORT_MIDDLENAME:
							case OE_IMPORT_NAME:
							case OE_IMPORT_NICKNAME:
								if(name.IsEmpty())
									name.Set(token);		// FIXME: use other means to transfer token to actual contact <no>
								else
								{
									name.Append(UNI_L("; "));
									name.Append(token);
								}
								break;
							case OE_IMPORT_EMAIL:
								address.SetFromUTF8(token);	// FIXME: use other means to transfer token to actual contact <no>
								break;
							case OE_IMPORT_HSTREET:
							case OE_IMPORT_HCITY:
							case OE_IMPORT_HPOSTALCODE:
							case OE_IMPORT_HSTATE:
							case OE_IMPORT_HCOUNTRY:
							case OE_IMPORT_HPHONE:
							case OE_IMPORT_HFAX:
							case OE_IMPORT_MOBILEPHONE:
							case OE_IMPORT_PERSONALWEB:
							case OE_IMPORT_BSTREET:
							case OE_IMPORT_BCITY:
							case OE_IMPORT_BPOSTALCODE:
							case OE_IMPORT_BSTATE:
							case OE_IMPORT_BCOUNTRY:
							case OE_IMPORT_BWEB:
							case OE_IMPORT_BPHONE:
							case OE_IMPORT_BFAX:
							case OE_IMPORT_PAGER:
							case OE_IMPORT_COMPANY:
							case OE_IMPORT_JOBTITLE:
							case OE_IMPORT_DEPARTMENT:
							case OE_IMPORT_OFFICELOCATION:
							case OE_IMPORT_NOTES:
								if(notes.IsEmpty())
									notes.Set(token);	// FIXME: use other means to transfer token to actual contact <no>
								else
								{
									notes.Append(UNI_L("\n"));
									notes.Append(token);
								}
								break;
							default:
								break;
							}
							do{
								counter = counter << 1;
							}while(tokenizer.IsNextSpecialChar() && tokenizer.Skip(1) && counter <= OE_IMPORT_NOTES);
						}
						glue_factory->GetBrowserUtils()->AddContact(address, name, notes);
						name.Empty();
						address.Empty();
						notes.Empty();
					}
				}
			}
		}
		contactsFile->Close();
	}

	glue_factory->DeleteOpFile(contactsFile);
	return ret;
}


void OutlookExpressImporter::ContinueImport2()
{
	OP_NEW_DBG("OutlookExpressImporter::ContinueImport2", "m2");

	const int BUF_LEN = 1024;
	static char buf[BUF_LEN];
	static INT32 old = 0;

	char* ret = fgets(buf, BUF_LEN, m_tmpFile);

	if ( m_raw_length > 14 && ('F' == buf[0] && strncmp(ret, "From importer@", 14) == 0 || feof(m_tmpFile)))
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

		OP_DBG(("OutlookExpressImporter::ContinueImport2() REALLOCATION, m_raw_capacity=%u", m_raw_capacity));
	}

	memcpy(m_raw + m_raw_length, buf, buflen);
	m_raw_length += buflen;

	if (feof(m_tmpFile))
	{
		OP_DELETEA(m_raw);
		m_raw = NULL;

		fclose(m_tmpFile);
		m_tmpFile = NULL;
		::DeleteFileA(m_tmp_filename.CStr());

		AddToResumeCache(m_m2FolderPath);

		OP_DBG(("OutlookExpressImporter::ContinueImport2() m_count=%u, m_totalCount=%u", m_count, m_totalCount));
	}
}


BOOL OutlookExpressImporter::OnContinueImport()
{
	OP_NEW_DBG("OutlookExpressImporter::OnContinueImport", "m2");

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

	OP_DBG(("OutlookExpressImporter::OnContinueImport() box = %S", item->GetName()));
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
			::DeleteFileA(m_tmp_filename.CStr());
		}

		OpString already_imported;

		g_languageManager->GetString(Str::S_IMPORT_ALREADY, already_imported);

		progressInfo.Append(already_imported);
		MessageEngine::GetInstance()->OnImporterProgressChanged(this, progressInfo, 0, 0);

		return TRUE;
	}

	char tmp_path[MAX_PATH];

	::GetTempPathA(MAX_PATH, tmp_path);
	::GetTempFileNameA(tmp_path, "imp", 0, m_tmp_filename.CStr());

	m_tmpFile = fopen(m_tmp_filename, "w+");
	if (!m_tmpFile)
		return TRUE;

	OP_DBG(("OutlookExpressImporter::OnContinueImport() m_tmp_filename=%s", m_tmp_filename));

	m_total = 0;
	m_blank = TRUE;

	OpString converting;
	g_languageManager->GetString(Str::S_IMPORT_CONVERTING, converting);

	progressInfo.Append(converting);

	MessageEngine::GetInstance()->OnImporterProgressChanged(this, progressInfo, 0, 0);

	const uni_char* filename = item->GetPath().CStr();

#ifdef _DEBUG
	unsigned long start = ::GetTickCount();
	OP_DBG(("conversion start: filename=%S", filename));
#endif

	if (filename)
	{
		oe_data* data = oe_readbox(filename, getmboxstream);
		OP_DBG(("worker thread: data->errcode = %u", data->errcode));
		OP_DELETE(data);
	}

#ifdef _DEBUG
	unsigned long end = ::GetTickCount();
	OP_DBG(("conversion end, time used: %lu ms, m_total=%li", end - start, m_total));
#endif

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

	m_raw_capacity = min(IMPORT_CHUNK_CAPACITY, size + 1);
	if ((m_raw = OP_NEWA(char, m_raw_capacity)) == NULL)
	{
		m_raw_capacity = NULL;
	}
	else
		m_raw[0] = '\0';

	m_raw_length = 0;

	MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount);

	return TRUE;
}


void OutlookExpressImporter::OnCancelImport()
{
	OP_NEW_DBG("OutlookExpressImporter::OnCancelImport", "m2");
	OP_DBG(("OutlookExpressImporter::OnCancelImport()"));

	OP_DELETEA(m_raw);
	m_raw = NULL;

	if (m_tmpFile)
	{
		fclose(m_tmpFile);
		m_tmpFile = NULL;
		::DeleteFileA(m_tmp_filename.CStr());
	}
}


// ****************** Private ********************

BOOL OutlookExpressImporter::GetIdentities()
{
	m_identities.DeleteAll();

	HKEY hkey_identities	= NULL;
	DWORD index				= 0;
	uni_char key[MAX_REGKEY_LEN]	= {0};

	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	if (!bu)
		return FALSE;

	DWORD res = bu->_RegOpenKeyEx(HKEY_CURRENT_USER, UNI_L("Identities"), 0, KEY_READ, &hkey_identities);

	if (res != ERROR_SUCCESS)
		return FALSE;

	uni_char def_id[MAX_REGKEY_LEN] = {0};
	unsigned long size = MAX_REGKEY_LEN - 1;

	res = bu->_RegQueryValueEx(hkey_identities, UNI_L("Default User ID"), NULL, NULL, (LPBYTE)def_id, &size);

	while (ERROR_SUCCESS == res || ERROR_MORE_DATA == res)
	{
		size = MAX_REGKEY_LEN - 1;
		res = bu->_RegEnumKeyEx(hkey_identities, index++, key, &size, NULL, NULL, NULL, NULL);

		if (ERROR_SUCCESS != res && ERROR_MORE_DATA != res)
			break;

		HKEY hkey = NULL;
		res = bu->_RegOpenKeyEx(hkey_identities, key, 0, KEY_READ, &hkey);

		if (ERROR_SUCCESS != res)
			break;

		uni_char value[MAX_REGKEY_LEN] = {0};

		Identity* identity = OP_NEW(Identity, ());
		if (!identity)
			continue;

		identity->isDefault = FALSE;

		HKEY hkey_iam = NULL;
		res = bu->_RegOpenKeyEx(hkey, UNI_L("Software\\Microsoft\\Internet Account Manager"), 0, KEY_READ, &hkey_iam);

		if (ERROR_SUCCESS == res)
		{
			identity->isMain = FALSE;
			bu->_RegCloseKey(hkey_iam);
		}
		else
		{
			identity->isMain = TRUE;	// this is the main identity; there is no IAM here
		}

		size = MAX_REGKEY_LEN - 1;
		res = bu->_RegQueryValueEx(hkey, UNI_L("User ID"), NULL, NULL, (LPBYTE)value, &size);

		if (ERROR_SUCCESS == res)
		{
			identity->isDefault = (uni_strcmp(def_id, value) == 0);

			OpString pre;
			pre.AppendFormat(UNI_L("Identities\\%s\\"), value);
			identity->key.Set(UNI_L("Software\\Microsoft"));

			if (!identity->isMain)
			{
				identity->key.Insert(0, pre);
			}

			identity->oe_key.Set(UNI_L("Software\\Microsoft"));
			identity->oe_key.Insert(0, pre);
			identity->oe_key.Append("\\Outlook Express");
		}

		size = MAX_REGKEY_LEN - 1;
		res = bu->_RegQueryValueEx(hkey, UNI_L("Username"), NULL, NULL, (LPBYTE)value, &size);

		if (ERROR_SUCCESS == res)
		{
			identity->name.Set(value);
		}

		HKEY hkey_oe = NULL;
		res = bu->_RegOpenKeyEx(hkey, UNI_L("Software\\Microsoft\\Outlook Express"), 0, KEY_READ, &hkey_oe);
		bu->_RegCloseKey(hkey);

		if (ERROR_SUCCESS == res)
		{
			DWORD ix = 0;
			size = MAX_REGKEY_LEN - 1;
			res = bu->_RegEnumKeyEx(hkey_oe, ix, key, &size, NULL, NULL, NULL, NULL);

			if (ERROR_SUCCESS == res)	// there should be only one key here: "5.0" or "4.0"
			{
				identity->ver.Set(key);

				HKEY hkey_ver = NULL;
				res = bu->_RegOpenKeyEx(hkey_oe, key, 0, KEY_READ, &hkey_ver);
				bu->_RegCloseKey(hkey_oe);

				if (ERROR_SUCCESS == res)
				{
					size = MAX_REGKEY_LEN - 1;
					res = bu->_RegQueryValueEx(hkey_ver, UNI_L("Store Root"), NULL, NULL, (LPBYTE)value, &size);
					bu->_RegCloseKey(hkey_ver);

					if (ERROR_SUCCESS == res)
					{
						if (identity->path.Reserve(MAX_PATH))
							bu->_ExpandEnvironmentStrings(value, identity->path.CStr(), MAX_PATH);
					}
				}
			}
		}

		if (identity->key.HasContent() && identity->path.HasContent())
		{
			m_identities.Add(identity);
		}
	}

	bu->_RegCloseKey(hkey_identities);
	return (m_identities.GetCount() > 0);
}


OP_STATUS OutlookExpressImporter::GetMailBoxNames(const OpStringC& basePath, OpVector<OpString>& mailboxes)
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


void OutlookExpressImporter::getmboxstream(char* s)
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


BOOL OutlookExpressImporter::GetRegistryValue(HKEY hKey, const uni_char* value_name, uni_char* value)
{
	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	if (!bu)
		return FALSE;

	DWORD type = 0;
	DWORD size = MAX_REGKEY_LEN - 1;

	DWORD res = bu->_RegQueryValueEx(hKey, value_name, NULL, &type, (LPBYTE)value, &size);

	return (ERROR_SUCCESS == res && (REG_SZ == type || REG_EXPAND_SZ == type));
}


BOOL OutlookExpressImporter::GetRegistryValue(HKEY hKey, const uni_char* value_name, DWORD* value)
{
	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	if (!bu)
		return FALSE;

	DWORD type = 0;
	DWORD size = sizeof(DWORD);

	DWORD res = bu->_RegQueryValueEx(hKey, value_name, NULL, &type, (LPBYTE)value, &size);

	return (ERROR_SUCCESS == res && REG_DWORD == type);
}

# endif //MSWIN
#endif //M2_SUPPORT
