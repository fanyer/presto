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
# ifdef _MACINTOSH_

#include <errno.h>
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/import/ImporterModel.h"
#include "adjunct/m2/src/import/AppleMailImporter.h"
#include "adjunct/m2/src/engine/account.h"

#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/util/macutils.h"

#include "modules/locale/oplanguagemanager.h"

AppleMailImporter::AppleMailImporter()
: MboxImporter()
{
	m_dictionary = NULL;
	m_localaccount_dictionary = NULL;
	m_folder_lister = NULL;
	m_accounts_version = 0;
}

AppleMailImporter::~AppleMailImporter()
{
	if(m_dictionary)
	{
		CFRelease(m_dictionary);
		m_dictionary = NULL;
		m_localaccount_dictionary = NULL;
	}
}

OP_STATUS AppleMailImporter::Init()
{
	if(m_dictionary)
	{
		CFRelease(m_dictionary);
		m_dictionary = NULL;
		m_localaccount_dictionary = NULL;
	}

	OpString packaged_mbox_path;
	OP_STATUS status = OpStatus::OK;

	GetModel()->DeleteAll();

	if (	!m_file_list.GetCount()			// No file content in file list
	 	|| 	!m_file_list.Get(0) 				// No strings
	 	||	m_file_list.Get(0)->IsEmpty())	// Empty string
		return OpStatus::ERR;

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
			return OpStatus::ERR_NO_MEMORY;
		m_two_lines_ahead[0] = m_two_lines_ahead[LOOKAHEAD_BUF_LEN] = 0;
	}

	m_finished_reading = FALSE;

	for (unsigned int i = 0; i < m_file_list.GetCount(); i++)
	{
		uni_char* file_filename = uni_strrchr(m_file_list.Get(i)->CStr(), PATHSEPCHAR);
		if(file_filename)
			file_filename++; // skip separator

		// Decide if we got a settingsfile or a mailbox file.
		if(file_filename && uni_strcmp(file_filename, UNI_L("com.apple.mail.plist")) == 0)
		{
			// Let's parse the settingsfile and insert everything that can be imported.
			InsertAllMailBoxes();
		}
		else
		{
			// We got a mailbox package, let's hand it over to the MboxImporter as soon as we have added the correct mbox path.
			int sep = m_file_list.Get(i)->FindLastOf(PATHSEPCHAR);
			if (KNotFound == sep)
			{
				status = OpStatus::ERR;
				break;
			}
			int end = m_file_list.Get(i)->FindLastOf(UNI_L('.'));
			if(KNotFound == end)
			{
				end = KAll;
			}

			OpString name;
			name.Set(m_file_list.Get(i)->CStr() + sep + 1, end - (sep+1));

			OpString m2FolderPath;
			m2FolderPath.Set(name);

			if(!packaged_mbox_path.Reserve(m_file_list.Get(i)->Length() + 6))	// +1 pathsep, +4 "mbox", +1 str null
			{
				break;
			}

			uni_sprintf(packaged_mbox_path.CStr(), UNI_L("%s%c%s"), m_file_list.Get(i)->CStr(), UNI_L(PATHSEPCHAR), UNI_L("mbox"));
			uni_char * filename_start = packaged_mbox_path.CStr();

			if (IsValidMboxFile(filename_start))
			{
				OpString path;
				path.Set(filename_start);

				OpString imported;
				RETURN_IF_ERROR(g_languageManager->GetString(Str::S_IMPORTED, imported));
				OpStatus::Ignore(imported.Insert(0, UNI_L(" (")));
				OpStatus::Ignore(imported.Append(UNI_L(")")));
				OpStatus::Ignore(m2FolderPath.Append(imported));

				ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_MAILBOX_TYPE,
																name, path, m2FolderPath, UNI_L("")));

				(void)GetModel()->AddLast(item);
			}
		}
	}
	return status;
}

/**
* This method is called to import settings for an account.
*/
OP_STATUS AppleMailImporter::ImportSettings()
{
	OP_STATUS result = OpStatus::ERR;
	OpString account;

	if(m_accounts_version >= 2)
	{
		ImporterModelItem* item = GetImportItem();

		if (!item)
			return OpStatus::ERR;

		if (item->GetType() == OpTypedObject::IMPORT_ACCOUNT_TYPE)
		{
			account.Set(item->GetName());
			if(account.HasContent())
			{
				result = ImportAccount(account);
				return result;
			}
		}
		return OpStatus::ERR;
	}

	if (GetImportAccount(account) && account.HasContent())
	{
		return ImportAccount(account);
	}
	else
	{
		if (GetModel()->SequenceIsEmpty(m_sequence))
			return OpStatus::ERR;

		while(!GetModel()->SequenceIsEmpty(m_sequence))
		{
			ImporterModelItem* item = GetModel()->PullItem(m_sequence);

			if (!item)
				return OpStatus::ERR;

			if (item->GetType() == OpTypedObject::IMPORT_ACCOUNT_TYPE)
			{
				account.Set(item->GetName());
				if(account.HasContent())
				{
					result = ImportAccount(account);
					RETURN_IF_ERROR(result);
				}
			}
		}
	}

	return result;
}

/**
* Parses the "com.apple.mail.plist" settingsfile for importable accounts and mailboxes.
*/
OP_STATUS AppleMailImporter::InsertAllMailBoxes()
{
	CFPropertyListRef 	propertyList = NULL;
	CFStringRef       	errorString = NULL;
	CFDataRef        	resourceData = 0;
	Boolean          	status = false;
	SInt32            	errorCode = 0;
	CFURLRef			theSettingsURL = NULL;
	OP_STATUS			result = OpStatus::ERR;
	FSRef				fsref;

	if(OpFileUtils::ConvertUniPathToFSRef(m_file_list.Get(0)->CStr(), fsref))
	{
		theSettingsURL = CFURLCreateFromFSRef(NULL, &fsref);
	}

	if(theSettingsURL)
	{
		// Read the XML file.
		status = CFURLCreateDataAndPropertiesFromResource(	kCFAllocatorDefault,
															theSettingsURL,
															&resourceData,            // place to put file data
															NULL,
															NULL,
															&errorCode);

		if(resourceData && (status != false))
		{
			// Reconstitute the dictionary using the XML data.
			propertyList = CFB_CALL(CFPropertyListCreateFromXMLData)( kCFAllocatorDefault,
															resourceData,
															kCFPropertyListImmutable,
															&errorString);

			if(propertyList)
			{
				// Make sure we really got a CFDictionary
				if(CFGetTypeID(propertyList) == CFDictionaryGetTypeID())
				{
					m_dictionary = (CFDictionaryRef)propertyList;

					CFRetain(m_dictionary);

					CFStringRef outboxPath;
					if(CFDictionaryGetValueIfPresent(m_dictionary, CFSTR("OutboxMailboxPath"), (const void**)&outboxPath))
					{
						if(outboxPath)
						{
							OpString opOutboxPath;
							if(SetOpString(opOutboxPath, outboxPath))
							{
								OP_STATUS rc;
								if(opOutboxPath[0] == '~')
								{
									TRAP(rc, opOutboxPath.SetL(&opOutboxPath.CStr()[1]));
									const char* home = op_getenv("HOME");
									if(home)
									{
										OpStatus::Ignore(opOutboxPath.Insert(0, home));
									}
								}
								TRAP(rc, opOutboxPath.AppendL(PATHSEP));
								TRAP(rc, opOutboxPath.AppendL("mbox"));

								if (IsValidMboxFile(opOutboxPath))
								{
									OpString imported;
									OpString accountName;
									OpString m2FolderPath;

									OpStatus::Ignore(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_OUTBOX, accountName));
									m2FolderPath.Set(accountName);

									OpStatus::Ignore(g_languageManager->GetString(Str::S_IMPORTED, imported));
									OpStatus::Ignore(imported.Insert(0, UNI_L(" (")));
									OpStatus::Ignore(imported.Append(UNI_L(")")));
									OpStatus::Ignore(m2FolderPath.Append(imported));

									ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_MAILBOX_TYPE, accountName, opOutboxPath, m2FolderPath, UNI_L("")));
									GetModel()->AddLast(item);
								}
							}
						}
					}


					CFNumberRef accountsVersion;
					if(CFDictionaryGetValueIfPresent(m_dictionary, CFSTR("AccountsVersion"), (const void**)&accountsVersion))
					{
						if(accountsVersion)
						{
							SInt32 version;
							if(CFNumberGetValue(accountsVersion, kCFNumberSInt32Type, &version))
							{
								m_accounts_version = version;
							}
						}
					}
					
					CFArrayRef mailAccounts = (CFArrayRef) CFDictionaryGetValue(m_dictionary, CFSTR("MailAccounts"));

					if(mailAccounts)
					{
						CFDictionaryRef array_dict;
						CFIndex array_index;
						CFIndex array_count = CFArrayGetCount(mailAccounts);

						for (array_index = 0; array_index < array_count; array_index++)
						{
							array_dict = (CFDictionaryRef)CFArrayGetValueAtIndex(mailAccounts, array_index);

							InsertAccount(array_dict);
						}

						result = OpStatus::OK;
					}
				}

				CFRelease(propertyList);
			}

			CFRelease(resourceData);
		}

		CFRelease(theSettingsURL);
	}

	return result;
}

/**
* Inserts a mailbox into the list of things to be imported (if valid).
* The parentIndex parameter is optional, can be specified to insert the mailbox under an account for example.
*/
void AppleMailImporter::InsertMailboxIfValid(const OpStringC& mailboxPath, const OpStringC& account, const OpStringC& m2FolderPath, INT32 parentIndex /* = -1 */)
{
	OpString newM2FolderPath;
	OpString imported;
	OpString fullMailboxPath;

	OpStatus::Ignore(g_languageManager->GetString(Str::S_IMPORTED, imported));
	OpStatus::Ignore(imported.Insert(0, UNI_L(" (")));
	OpStatus::Ignore(imported.Append(UNI_L(")")));

#if 0
	if(m2FolderPath.HasContent())
	{
		OpStatus::Ignore(newM2FolderPath.Set(m2FolderPath));
		OpStatus::Ignore(newM2FolderPath.Append("/"));
	}
#endif
	OpStatus::Ignore(newM2FolderPath.Append(account));
	OpStatus::Ignore(newM2FolderPath.Append(imported));

	OP_STATUS rc;
	if(mailboxPath[0] == '~')
	{
		const char* home = op_getenv("HOME");
		if(home)
		{
			TRAP(rc, fullMailboxPath.SetL(home));
			TRAP(rc, fullMailboxPath.AppendL( &(mailboxPath.CStr()[1]) ));
		}
		else
		{
			return;
		}
	}
	else
	{
		TRAP(rc, fullMailboxPath.SetL(mailboxPath));
	}

	TRAP(rc, fullMailboxPath.AppendL(PATHSEP));
	TRAP(rc, fullMailboxPath.AppendL(account));
	TRAP(rc, fullMailboxPath.AppendL(".mbox"));
	TRAP(rc, fullMailboxPath.AppendL(PATHSEP));
	TRAP(rc, fullMailboxPath.AppendL("mbox"));

	if(IsValidMboxFile(fullMailboxPath))
	{
		ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_MAILBOX_TYPE, account, fullMailboxPath, newM2FolderPath, UNI_L("")));
		if(parentIndex == -1)
		{
			GetModel()->AddLast(item);
		}
		else
		{
			GetModel()->AddLast(item, parentIndex);
		}
	}
}

/**
* This method inserts an account into the list of things to be imported by m2.
* It inserts both mailboxes (mbox-files) and settings for all importable accounts.
*/
OP_STATUS AppleMailImporter::InsertAccount(CFDictionaryRef dict)
{
	OP_STATUS			status = OpStatus::ERR;
	OpString 			account;
	OpString 			imported;
	OpString 			m2FolderPath;
	BOOL				addLocalMailboxes = TRUE;
	BOOL				addMailboxes = FALSE;
	BOOL				addSettings = FALSE;
	ImporterModelItem*	item = NULL;
	INT32 				branch = 0;

	CFStringRef accountName = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("AccountName"));
	if(accountName)
	{
		SetOpString(account, accountName);
	}

	CFStringRef accountType = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("AccountType"));
	if(accountType)
	{
		if(CFStringCompare(accountType, CFSTR("POPAccount"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
			addSettings = TRUE;
			addMailboxes = TRUE;
		}
		else if(CFStringCompare(accountType, CFSTR("IMAPAccount"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
			addSettings = TRUE;
		}
		else if(CFStringCompare(accountType, CFSTR("LocalAccount"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
			OpString emptyM2folderpath;
			addLocalMailboxes = FALSE;

			// Ok, we found the localaccount dictionary, let's keep that reference if we don't already have it.
			if(!m_localaccount_dictionary)
			{
				m_localaccount_dictionary = dict;
			}

			OpString opMailboxPath;

			CFStringRef mailboxPath = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("AccountPath"));
			if(mailboxPath)
			{
				SetOpString(opMailboxPath, mailboxPath);
			}

			CFStringRef draftsName = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("DraftsMailboxName"));
			if(draftsName)
			{
				SetOpString(account, draftsName);
				InsertMailboxIfValid(opMailboxPath, account, emptyM2folderpath);
			}

			CFStringRef junkName = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("JunkMailboxName"));
			if(junkName)
			{
				SetOpString(account, junkName);
				InsertMailboxIfValid(opMailboxPath, account, emptyM2folderpath);
			}

			CFStringRef sentName = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("SentMessagesMailboxName"));
			if(sentName)
			{
				SetOpString(account, sentName);
				InsertMailboxIfValid(opMailboxPath, account, emptyM2folderpath);
			}

			CFStringRef trashName = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("TrashMailboxName"));
			if(trashName)
			{
				SetOpString(account, trashName);
				InsertMailboxIfValid(opMailboxPath, account, emptyM2folderpath);
			}
		}
	}

	if(addSettings)
	{
		OpStatus::Ignore(g_languageManager->GetString(Str::S_IMPORTED, imported));
		OpStatus::Ignore(imported.Insert(0, UNI_L(" (")));
		OpStatus::Ignore(imported.Append(UNI_L(")")));

		m2FolderPath.Set(account);
		OpStatus::Ignore(m2FolderPath.Append(imported));

		item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_ACCOUNT_TYPE, account, *m_file_list.Get(0), m2FolderPath, UNI_L("")));
		if(!item)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		branch = GetModel()->AddLast(item);

		if(addMailboxes)
		{
			status = InsertMailBoxes(account, m2FolderPath, branch, dict);
		}
	}

	if(addLocalMailboxes)
	{
		InsertMailBoxes(account, m2FolderPath, branch, GetLocalAccountDictionary());
	}

	return status;
}

/**
* Insert mailboxes for the account named accountName, at specfied m2folderpath + parentIndex.
* The CFDictionaryRef can be either the dictionary for the account or for the LocalAccount (contains Drafts, Sent Messages etc).
* This is necessary because some mailboxes are kept in the actual account path, and others are in the LocalAccounts path
* only with the real account name suffixed to the filename.
*/
OP_STATUS AppleMailImporter::InsertMailBoxes(const OpStringC& accountName, const OpStringC& m2FolderPath, INT32 parentIndex, CFDictionaryRef dict)
{
	if(!dict)
		return OpStatus::ERR;

	CFStringRef accountPath = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("AccountPath"));
	CFStringRef accountType = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("AccountType"));
	if(accountPath)
	{
		OpString opAccountPath;
		if(SetOpString(opAccountPath, accountPath))
		{
			OP_STATUS rc;
			if(opAccountPath[0] == '~')
			{
				TRAP(rc, opAccountPath.SetL(&opAccountPath.CStr()[1]));
				const char* home = op_getenv("HOME");
				if(home)
				{
					OpStatus::Ignore(opAccountPath.Insert(0, home));
				}
			}

			if(accountType && (CFStringCompare(accountType, CFSTR("LocalAccount"), kCFCompareCaseInsensitive) == kCFCompareEqualTo))
			{
				OpString account;
				CFStringRef draftsName = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("DraftsMailboxName"));
				if(draftsName)
				{

					SetOpString(account, draftsName);
					TRAP(rc, account.AppendL(" ("));
					TRAP(rc, account.AppendL(accountName));
					TRAP(rc, account.AppendL(")"));
					InsertMailboxIfValid(opAccountPath, account, m2FolderPath, parentIndex);
				}

				CFStringRef junkName = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("JunkMailboxName"));
				if(junkName)
				{
					SetOpString(account, junkName);
					TRAP(rc, account.AppendL(" ("));
					TRAP(rc, account.AppendL(accountName));
					TRAP(rc, account.AppendL(")"));
					InsertMailboxIfValid(opAccountPath, account, m2FolderPath, parentIndex);
				}

				CFStringRef sentName = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("SentMessagesMailboxName"));
				if(sentName)
				{
					SetOpString(account, sentName);
					TRAP(rc, account.AppendL(" ("));
					TRAP(rc, account.AppendL(accountName));
					TRAP(rc, account.AppendL(")"));
					InsertMailboxIfValid(opAccountPath, account, m2FolderPath, parentIndex);
				}

				CFStringRef trashName = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("TrashMailboxName"));
				if(trashName)
				{
					SetOpString(account, trashName);
					TRAP(rc, account.AppendL(" ("));
					TRAP(rc, account.AppendL(accountName));
					TRAP(rc, account.AppendL(")"));
					InsertMailboxIfValid(opAccountPath, account, m2FolderPath, parentIndex);
				}
			}
			else
			{
				TRAP(rc, opAccountPath.AppendL(PATHSEP));
				TRAP(rc, opAccountPath.AppendL(UNI_L("INBOX.mbox")));
				TRAP(rc, opAccountPath.AppendL(PATHSEP));
				if(m_accounts_version < 2)
				{
					TRAP(rc, opAccountPath.AppendL("mbox"));
				}
				else
				{
					opAccountPath.Append("Messages");
					opAccountPath.Append(PATHSEP);

					ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_FOLDER_TYPE, UNI_L("INBOX.mbox"), opAccountPath, m2FolderPath, UNI_L("")));
					GetModel()->AddLast(item, parentIndex);
					return OpStatus::OK;
				}
			}

			if(IsValidMboxFile(opAccountPath))
			{
				ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_MAILBOX_TYPE, accountName, opAccountPath, m2FolderPath, UNI_L("")));
				GetModel()->AddLast(item, parentIndex);
			}
		}
	}

	return OpStatus::OK;
}

/**
* Finds the CFDictionary for the localaccount (contains mailboxes: Drafts, Sent Messages etc).
* Returns NULL if not found.
*/
CFDictionaryRef AppleMailImporter::GetLocalAccountDictionary()
{
	if(m_localaccount_dictionary)
	{
		return m_localaccount_dictionary;
	}
	if(!m_dictionary)
	{
		return NULL;
	}

	CFArrayRef mailAccounts = (CFArrayRef) CFDictionaryGetValue(m_dictionary, CFSTR("MailAccounts"));
	if(mailAccounts)
	{
		CFDictionaryRef array_dict = NULL;
		CFIndex array_index;
		CFIndex array_count = CFArrayGetCount(mailAccounts);

		for (array_index = 0; array_index < array_count; array_index++)
		{
			array_dict = (CFDictionaryRef)CFArrayGetValueAtIndex(mailAccounts, array_index);

			CFStringRef cfAccountType = NULL;
			if(CFDictionaryGetValueIfPresent(array_dict, CFSTR("AccountType"), (const void**)&cfAccountType))
			{
				if(cfAccountType)
				{
					if(CFStringCompare(cfAccountType, CFSTR("LocalAccount"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
					{
						return array_dict;
					}
				}
			}
		}
	}

	return NULL;
}

/**
* Given an account name try to find the dictionary for it.
* Returns TRUE if found, FALSE otherwise.
*/
BOOL AppleMailImporter::FindAccountDictionary(const OpStringC& accountName, CFDictionaryRef &dict)
{
	if(!m_dictionary)
	{
		return FALSE;
	}

	BOOL result = FALSE;
	CFArrayRef mailAccounts = (CFArrayRef) CFDictionaryGetValue(m_dictionary, CFSTR("MailAccounts"));
	if(mailAccounts)
	{
		CFDictionaryRef array_dict = NULL;
		CFIndex array_index;
		CFIndex array_count = CFArrayGetCount(mailAccounts);

		for (array_index = 0; array_index < array_count; array_index++)
		{
			array_dict = (CFDictionaryRef)CFArrayGetValueAtIndex(mailAccounts, array_index);

			CFStringRef cfAccountName = NULL;
			OpString	opAccountName;
			if(CFDictionaryGetValueIfPresent(array_dict, CFSTR("AccountName"), (const void**)&cfAccountName))
			{
				if(cfAccountName)
				{
					if(SetOpString(opAccountName, cfAccountName))
					{
						if(opAccountName.Compare(accountName) == 0)
						{
							dict = array_dict;
							result = TRUE;
							break;
						}
					}
				}
			}

		}
	}

	return result;
}

/**
* This method imports the settings for the specified account (if found).
* Returns OpStatus::OK if successful.
*/
OP_STATUS AppleMailImporter::ImportAccount(const OpStringC& accountName)
{
	BOOL isPop = FALSE;
	BOOL isImap = FALSE;
	CFStringRef tmp;
	OpString optmp;
	OP_STATUS res = OpStatus::ERR;
	CFDictionaryRef dict = NULL;
	BOOL found = FindAccountDictionary(accountName, dict);

	if (!found || !dict)
	{
		return OpStatus::ERR;
	}

	CFStringRef accountType = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("AccountType"));
	if(accountType)
	{
		if(CFStringCompare(accountType, CFSTR("IMAPAccount"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
			isImap = true;
		}
		else if(CFStringCompare(accountType, CFSTR("POPAccount"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		{
			isPop = true;
		}
	}

	if(isPop || isImap)
	{
		Account* account = MessageEngine::GetInstance()->CreateAccount();
		if (account)
		{
			if(isPop)
			{
				account->SetIncomingProtocol(AccountTypes::POP);
				account->SetLeaveOnServer(FALSE);
			}
			else if(isImap)
			{
				account->SetIncomingProtocol(AccountTypes::IMAP);
				account->SetLeaveOnServer(TRUE);
			}

			account->SetOutgoingProtocol(AccountTypes::SMTP);
			account->SetDefaults();

			tmp = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("FullUserName"));
			if(tmp)
			{
				if(SetOpString(optmp, tmp))
					account->SetRealname(optmp);
			}
			tmp = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("SMTPIdentifier"));
			if(tmp)
			{
				if(SetOpString(optmp, tmp))
				{
					int colon = optmp.FindLastOf(':');
					if(KNotFound == colon)
					{
						account->SetOutgoingServername(optmp);
					}
					else
					{
						OpString servername;
						RETURN_IF_ERROR(servername.Set(optmp.CStr(), colon));
						account->SetOutgoingServername(servername);
						account->SetOutgoingUsername(optmp.SubString(colon+1));
					}
				}

			}
			tmp = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("AccountName"));
			if(tmp)
			{
				if(SetOpString(optmp, tmp))
					account->SetAccountName(optmp);
			}
			tmp = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("Hostname"));
			if(tmp)
			{
				if(SetOpString(optmp, tmp))
					account->SetIncomingServername(optmp);
			}
			tmp = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("Username"));
			if(tmp)
			{
				if(SetOpString(optmp, tmp))
					account->SetIncomingUsername(optmp);
			}

			CFArrayRef emails = (CFArrayRef) CFDictionaryGetValue(dict, CFSTR("EmailAddresses"));
			if(emails)
			{
				tmp = (CFStringRef) CFArrayGetValueAtIndex(emails, 0);
				if(tmp)
				{
					if(SetOpString(optmp, tmp))
						account->SetEmail(optmp);
				}
			}

			res = account->Init();
			if(res == OpStatus::OK)
				m_accountId = account->GetAccountId();
		}
	}

	return res;
}

/**
* This is an attempt to be nice to the user and get the default path for the Mail.app settingsfile.
*/
void AppleMailImporter::GetDefaultImportFolder(OpString& path)
{
	path.Empty();

	FSRef fsref;
	OSErr err = FSFindFolder(kUserDomain, kPreferencesFolderType, kDontCreateFolder, &fsref);
	if(err == noErr)
	{
		if(OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&fsref, &path)))
		{
			TRAPD(rc, path.AppendL(PATHSEP));
			TRAP(rc, path.AppendL("com.apple.mail.plist"));
			TRAP(rc, path.Insert(0, "\""));
			TRAP(rc, path.AppendL("\""));
		}
	}
}

void AppleMailImporter::ParseEmlxFile(const uni_char* path)
{
	OpFile* file = OP_NEW(OpFile, ());
	if(OpStatus::IsSuccess(file->Construct(path)))
	{
		if(OpStatus::IsSuccess(file->Open(OPFILE_READ)))
		{
			OpString8 buf;
			OpString8 line;
			int i=0;
			while(file->ReadLine(line) == OpStatus::OK)
			{
				// Read until xml section
				if(strstr(line, "<?xml"))
					break;

				if(i) // Skip first line
				{
					buf.Append(line);
					buf.Append("\n");
				}
				i++;
			}
			file->Close();

			if(buf.HasContent())
			{
				Message m2_message;
				if(m2_message.Init(m_accountId) == OpStatus::OK)
				{
					m2_message.SetRawMessage(buf);
					if(MessageEngine::GetInstance()->ImportMessage(&m2_message, m_m2FolderPath, m_moveCurrentToSent) == OpStatus::OK)
					{
						m_grandTotal++;
						MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount, TRUE);
					}				
				}
			}
		}
	}
	OP_DELETE(file);
}

BOOL AppleMailImporter::OnContinueImport()
{
	if(m_accounts_version < 2)
	{
		return MboxImporter::OnContinueImport();
	}

	if(m_folder_lister)
	{
		if(m_folder_lister->Next())
		{
			const uni_char* path = m_folder_lister->GetFullPath();
			if(path)
			{
				ParseEmlxFile(path);
				return TRUE;
			}
		}
		else
		{
			OP_DELETE(m_folder_lister);
			m_folder_lister = NULL;
			return TRUE;
		}
	}

	if (GetModel()->SequenceIsEmpty(m_sequence))
		return FALSE;

	ImporterModelItem* item = GetModel()->PullItem(m_sequence);

	if (!item)
		return FALSE;

	if (item->GetType() != OpTypedObject::IMPORT_FOLDER_TYPE)
		return TRUE;

	if(!OpStatus::IsSuccess(OpFolderLister::Create(&m_folder_lister)))
		return FALSE;
		
	if(!OpStatus::IsSuccess(m_folder_lister->Construct(item->GetPath(), UNI_L("*.emlx"))))
		return FALSE;

	return TRUE;
}

# endif // _MACINTOSH_

#endif // M2_SUPPORT
