/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#ifndef MAILFILES_H
#define MAILFILES_H

#define LEXICON_FILENAME					UNI_L("lexicon")
#define INDEXER_FILENAME					UNI_L("indexer")
#define INDEX_INI_FILENAME					UNI_L("index.ini")
#define ACCOUNTS_INI_FILENAME				UNI_L("accounts.ini")
#define MAILBASE_FILENAME					UNI_L("omailbase.dat")
#define	MESSAGE_ID_FILENAME					UNI_L("message_id")
#define	WELCOME_MESSAGE_FILENAME			UNI_L("m2_welcome_message.mbs")
#define	UPGRADE_1160_MESSAGE_FILENAME		UNI_L("m2_upgrade_1160.mbs")
#define	OPERA_MAIL_CSS_FILENAME				UNI_L("mail.css")
#define	OPERA_FEED_CSS_FILENAME				UNI_L("feed.css")

namespace MailFiles
{
	OP_STATUS GetLexiconPath (OpString& lexicon_path);

	OP_STATUS GetIndexerPath (OpString& indexer_path);

	OP_STATUS GetIMAPUIDFilename (int account_id, OpString& filename);

	OP_STATUS GetPOPUIDLFilename (int account_id, OpString& filename);

	OP_STATUS GetNewsfeedFilename (UINT32 index_id, OpString& filename);

	OP_STATUS GetNewsfeedfolder (OpString& foldername);

	OP_STATUS GetMailBaseFilename (OpString& filename); 

	OP_STATUS GetMessageIDFilename (OpString& filename);

	OP_STATUS GetAccountsINIFilename (OpString& filename);
	
	OP_STATUS GetAccountsSettingsFilename (const uni_char* setting, UINT16 account_id, OpString& filename);

	OP_STATUS GetRecoveryLogFilename (OpString& filename);

	OP_STATUS GetDraftsFolder (OpString& foldername);
	
	OP_STATUS GetIndexIniFileName (OpString& filename);

	OP_STATUS GetNewsrcFileName(OpString& servername, OpString& filename);

	OP_STATUS GetAutofilterFileName(UINT32 index_id, OpString& filename);

};
#endif // MAILFILES_H
