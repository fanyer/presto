/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef OEIMPORTER_H
#define OEIMPORTER_H
#ifdef MSWIN

#include "adjunct/m2/src/import/importer.h"

#include "modules/util/adt/opvector.h"

struct Identity
{
	BOOL	 isDefault;		// TRUE = default identity
	BOOL	 isMain;		// TRUE = main identity
	OpString key;			// key root
	OpString name;			// the username
	OpString ver;			// the version of Outlook Express, e.g. "5.0"
	OpString path;			// the path to the store root, e.g. "c:\\store"
	OpString oe_key;		// key path to oe
};

class OutlookExpressImporter : public Importer
{
public:
	OutlookExpressImporter();
	~OutlookExpressImporter();

	OP_STATUS Init();

	virtual OP_STATUS ImportSettings();
	virtual OP_STATUS ImportMessages();
	virtual OP_STATUS ImportContacts();


protected:
	BOOL OnContinueImport();
	void OnCancelImport();

private:
	BOOL		GetIdentities();
	OP_STATUS	ImportAccountSettings(const OpStringC& identityName,
									  const OpStringC& account_reg_key,
									  const OpStringC& signature);

	BOOL		ImportSignature(const OpStringC& signature_reg_key, OpString& signature);

	OP_STATUS	GetMailBoxNames(const OpStringC& basePath, OpVector<OpString>& mailboxes);
	OP_STATUS	ImportSingle();
	void		ContinueImport2();

	BOOL		GetRegistryValue(HKEY hKey, const uni_char* value_name, uni_char* value);
	BOOL		GetRegistryValue(HKEY hKey, const uni_char* value_name, DWORD* value);

	static void	getmboxstream(char*);

	// Disable copy constructor and assignment operator
	OutlookExpressImporter(const OutlookExpressImporter&);
	OutlookExpressImporter& operator =(const OutlookExpressImporter&);

private:
	UINT16				m_version;
	UINT16				m_prevVersion;
	OpString8			m_tmp_filename;
	char*				m_raw;
	INT32				m_raw_length;
	INT32				m_raw_capacity;
	static FILE*		m_tmpFile;
	OpVector<Identity>	m_identities;
	static INT32		m_total;
	static BOOL			m_blank;
};

#endif//MSWIN

#endif//OEIMPORTER_H
