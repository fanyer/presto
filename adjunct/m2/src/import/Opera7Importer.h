/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef OPERA7IMPORTER_H
#define OPERA7IMPORTER_H

#include "adjunct/m2/src/import/importer.h"
#include "adjunct/m2/src/util/blockfile.h"

class OpFile;
class Account;
class StoreReader;

class Opera7Importer : public Importer
{
public:
	Opera7Importer();
	~Opera7Importer();

	OP_STATUS Init();

	OP_STATUS ImportAccounts();

	OP_STATUS AddImportFile(const OpStringC& path);
	BOOL GetContactsFileName(OpString& fileName);

	void GetDefaultImportFolder(OpString& path);
	
	virtual OP_STATUS	ImportMessages()	{ m_should_import_messages = TRUE; return OpStatus::OK; }

protected:
	BOOL OnContinueImport();
	void OnCancelImport();
	void SetImportItems(const OpVector<ImporterModelItem>& items);
private:
	BOOL GetOperaIni(OpString& fileName);
	
	OP_STATUS ImportAccount(const ImporterModelItem * item);
	OP_STATUS ImportAccountMessages(const ImporterModelItem * item, Account * account);

	OP_STATUS ImportSingle();

	Opera7Importer(const Opera7Importer&);
	Opera7Importer& operator =(const Opera7Importer&);

private:
	int				m_current_block;
	PrefsFile*		m_accounts_ini;
	INT32			m_treePos;
	BOOL			m_inProgress;
	OpString		m_m2FolderPath;
	Account*		m_account;
	StoreReader *	m_store_reader;
	int				m_import_accountID;
	
	// The Opera7Importer by default doesn't import messages, 
	// but will be done if ImportMessages() is called before ImportAccounts() is called
	BOOL			m_should_import_messages;
};



#endif//OPERA7IMPORTER_H
