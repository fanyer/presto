/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef OPERAIMPORTER_H
#define OPERAIMPORTER_H

#include "adjunct/m2/src/import/importer.h"

class OpFile;
class M1_Preferences;

class OperaImporter : public Importer
{
public:
	OperaImporter();
	~OperaImporter();

	OP_STATUS Init();

	OP_STATUS ImportSettings();
	OP_STATUS ImportMessages();

	OP_STATUS AddImportFile(const OpStringC& path);
	BOOL GetContactsFileName(OpString& fileName);

	void GetDefaultImportFolder(OpString& path);

protected:
	BOOL OnContinueImport();
	void OnCancelImport();

private:
	BOOL GetOperaIni(OpString& fileName);

	OP_STATUS ReadMessage(OpFileLength file_pos, UINT32 length, OpString8& message);
	OP_STATUS OpenMailBox(const OpStringC& fullPath);

	OP_STATUS ImportSingle(BOOL& eof);

	OP_STATUS InsertMailBoxes(const OpStringC& basePath, const OpStringC& m2FolderPath, INT32 parentIndex);
	OP_STATUS InsertSubFolder(const OpStringC& basePath, const OpStringC& folderName, const OpStringC& m2FolderPath, INT32 parentIndex);

	void QPDecode(const OpStringC8& src, OpString& decoded); 

	OperaImporter(const OperaImporter&);
	OperaImporter& operator =(const OperaImporter&);

private:
	OpFile*			m_mboxFile;
	OpFile*			m_idxFile;
	M1_Preferences* m_accounts_ini;
	INT32			m_treePos;
	BOOL			m_inProgress;
	OpString		m_m2FolderPath;
	BOOL			m_version4;
	BOOL			m_isTrashFolder;
};



#endif//OPERAIMPORTER_H
