/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef EUDORAIMPORTER_H
#define EUDORAIMPORTER_H

#include "adjunct/m2/src/import/importer.h"

class OpFile;
class Message;
class PrefsFile;

class EudoraImporter : public Importer
{
public:
	EudoraImporter();
	~EudoraImporter();

	OP_STATUS ReadIniString(const OpStringC &section, const OpStringC &key, OpString &result);
	OP_STATUS Init();

	OP_STATUS ImportSettings();
	OP_STATUS ImportMessages();
	OP_STATUS ImportContacts();

	OP_STATUS AddImportFile(const OpStringC& path);
	void GetDefaultImportFolder(OpString& path);

protected:
	BOOL OnContinueImport();
	void OnCancelImport();

private:
	class DescmapEntry
	{
	public:
		enum MapType
		{
			DM_INVALID,
			DM_SYSMAILBOX,
			DM_MAILBOX,
			DM_SUBFOLDER
		};

		DescmapEntry() : m_type(DM_INVALID) {};
		OpString	m_mailboxName;	// name of mail box
		OpString	m_fileName;		// filename or foldername
		MapType		m_type;

	private:
		DescmapEntry(const DescmapEntry&);
		DescmapEntry& operator =(const DescmapEntry&);
	};

	OP_STATUS ReadMessage(OpFileLength file_pos, UINT32 length, OpString8& message);
	OP_STATUS GetDescmap(const OpStringC& basePath, OpVector<DescmapEntry>& descmap);

	OP_STATUS OpenMailBox(const OpStringC& fullPath);
	OP_STATUS ImportSingle(BOOL& eof);

	OP_STATUS InsertMailBoxes(const OpStringC& basePath, const OpStringC& m2FolderPath, INT32 parentIndex);
	
	OP_STATUS ListAttachments(OpString8& raw, OpVector<OpString> & attNames);
	OP_STATUS AppendAttachments(OpVector<OpString> & attNames, Message* message);

	OP_STATUS ImportNickNames(const OpStringC& basePath, const OpStringC& fileName);

	OP_STATUS ImportAccount(const OpStringC& accountName);
	BOOL	  FindPersonality(const OpStringC& accountName, OpString& personality);

	EudoraImporter(const EudoraImporter&);
    EudoraImporter& operator =(const EudoraImporter&);

private:
   	OpFile*		m_mboxFile;
	OpFile*		m_tocFile;
	PrefsFile*	m_eudora_ini;
	BOOL		m_inProgress;
	INT16		m_mailboxType;
	OpString	m_currPersonality;
	OpString	m_attachments_folder_name; // Leaf folder, not path, used for import of moved Eudora dir.
};

#endif//EUDORAIMPORTER_H
