/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/include/mailfiles.h"

OP_STATUS MailFiles::GetLexiconPath (OpString& lexicon_path) 
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return lexicon_path.AppendFormat(UNI_L("%slexicon"), mailpath.CStr());
}

OP_STATUS MailFiles::GetIndexerPath (OpString& indexer_path)
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return indexer_path.AppendFormat(UNI_L("%sindexer"), mailpath.CStr());
}

OP_STATUS MailFiles::GetIMAPUIDFilename (int account_id, OpString& filename) 
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return filename.AppendFormat(UNI_L("%simap%cuid_account%d"), mailpath.CStr(), PATHSEPCHAR, account_id);
}

OP_STATUS MailFiles::GetPOPUIDLFilename (int account_id, OpString& filename) 
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return filename.AppendFormat(UNI_L("%spop3%cuidl_account%d_ver8"), mailpath.CStr(), PATHSEPCHAR,account_id);
}

OP_STATUS MailFiles::GetNewsfeedFilename (UINT32 index_id, OpString& filename) 
{
	RETURN_IF_ERROR(GetNewsfeedfolder(filename));
	return filename.AppendFormat(UNI_L("feed_%u"), index_id); 
}

OP_STATUS MailFiles::GetNewsfeedfolder (OpString& foldername)
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return foldername.AppendFormat(UNI_L("%snewsfeed%c"), mailpath.CStr(), PATHSEPCHAR);
}

OP_STATUS MailFiles::GetMailBaseFilename (OpString& filename) 
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return filename.AppendFormat(UNI_L("%s%s"), mailpath.CStr(), MAILBASE_FILENAME);
}

OP_STATUS MailFiles::GetMessageIDFilename (OpString& filename) 
{ 
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return filename.AppendFormat(UNI_L("%sindexer%c%s"), mailpath.CStr(), PATHSEPCHAR, MESSAGE_ID_FILENAME);
}

OP_STATUS MailFiles::GetAccountsINIFilename (OpString& filename)
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return filename.AppendFormat(UNI_L("%s%s"),mailpath.CStr(), ACCOUNTS_INI_FILENAME);
}

OP_STATUS MailFiles::GetAccountsSettingsFilename (const uni_char* setting, UINT16 account_id, OpString& filename)
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return filename.AppendFormat(UNI_L("%s%s%u.txt"),mailpath.CStr(), setting, account_id);
}

OP_STATUS MailFiles::GetRecoveryLogFilename (OpString& filename)
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return filename.AppendFormat(UNI_L("%srecovery.log"), mailpath.CStr());
}

OP_STATUS MailFiles::GetDraftsFolder (OpString& foldername)
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return foldername.AppendFormat(UNI_L("%sstore%cdrafts"), mailpath.CStr(), PATHSEPCHAR);
}

OP_STATUS MailFiles::GetIndexIniFileName (OpString& filename)
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return filename.AppendFormat(UNI_L("%s%s"), mailpath.CStr(), INDEX_INI_FILENAME);
}

OP_STATUS MailFiles::GetNewsrcFileName(OpString& servername, OpString& filename)
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return filename.AppendFormat(UNI_L("%s%s%s"), mailpath.CStr(), servername.CStr(), UNI_L(".newsrc"));
}

OP_STATUS MailFiles::GetAutofilterFileName(UINT32 index_id, OpString& filename)
{
	OpString mailpath;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailpath));
	return filename.AppendFormat(UNI_L("%sautofilter%cfilter_%u.ini"), mailpath.CStr(), PATHSEPCHAR, index_id);
}

#endif // M2_SUPPORT
