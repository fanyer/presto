/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef APPLE_MAIL_IMPORTER_H
#define APPLE_MAIL_IMPORTER_H

#if defined (_MACINTOSH_)
#include "modules/pi/system/OpFolderLister.h"
#include "adjunct/m2/src/import/MboxImporter.h"

class AppleMailImporter : public MboxImporter
{
public:
	AppleMailImporter();
	virtual ~AppleMailImporter();

	OP_STATUS		Init();

	OP_STATUS		ImportSettings();
	
	virtual void	GetDefaultImportFolder(OpString& path);
	
	INT32			GetAccountsVersion() const { return m_accounts_version; }

protected:
	virtual BOOL	OnContinueImport();

private:
	OP_STATUS		InsertAllMailBoxes();
	OP_STATUS		InsertMailBoxes(const OpStringC& accountName, const OpStringC& m2FolderPath, INT32 parentIndex, CFDictionaryRef dict);
	void			InsertMailboxIfValid(const OpStringC& mailboxPath, const OpStringC& account, const OpStringC& m2FolderPath, INT32 parentIndex = -1);
	OP_STATUS		InsertAccount(CFDictionaryRef dict);
	OP_STATUS		ImportAccount(const OpStringC& accountName);
	
	BOOL			FindAccountDictionary(const OpStringC& accountName, CFDictionaryRef &dict);
	CFDictionaryRef GetLocalAccountDictionary();
	
	void			ParseEmlxFile(const uni_char* path);
	
	CFDictionaryRef m_dictionary;
	CFDictionaryRef m_localaccount_dictionary;
	OpFolderLister*	m_folder_lister;
	INT32			m_accounts_version;
};

#endif // _MACINTOSH_ || UNIX
#endif //APPLE_MAIL_IMPORTER_H

