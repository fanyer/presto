/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef MBOXIMPORTER_H
# define MBOXIMPORTER_H
# include "adjunct/m2/src/import/importer.h"
# include "adjunct/desktop_util/treemodel/optreemodel.h"

class FolderRecursor;

class MboxImporter: public Importer,
					public OpTreeModel::SortListener
{
public:
	MboxImporter();
	virtual ~MboxImporter();

	OP_STATUS Init();
	OP_STATUS ImportMessages();

protected:
	virtual BOOL OnContinueImport();
	virtual void OnCancelImport();
	virtual void ImportMboxAsync();
	virtual void SetImportItems(const OpVector<ImporterModelItem>& items);

	BOOL		IsValidMboxFile(const uni_char* file_name);

	OP_STATUS	InitLookAheadBuffers();

	OP_STATUS	InitMboxFile();
	void		InitSingleMbox(const OpString& mbox, OpString& virtual_path, INT32 index = -1);
	OP_STATUS	SetRecursorRoot(const OpString& root);
	INT32		GetOrCreateFolder(const OpString& root, OpFile& mbox, OpString& virtual_path);
	
	// Implementing OpTreeModel::SortListener API
	INT32		OnCompareItems(OpTreeModel* tree_model, OpTreeModelItem* item0, OpTreeModelItem* item1);
protected:
	OpFile*				m_mboxFile;
	char*				m_raw;
	INT32				m_raw_length;
	INT32				m_raw_capacity;

	char*				m_one_line_ahead;
	char*				m_two_lines_ahead;
	BOOL				m_finished_reading;
	
	BOOL				m_found_start_of_message;
	BOOL				m_found_start_of_next_message;
	BOOL				m_found_valid_message;

	FolderRecursor *	m_folder_recursor;
};

#endif //MBOXIMPORTER_H
