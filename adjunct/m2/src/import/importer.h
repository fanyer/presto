/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef IMPORTER_H
#define IMPORTER_H

#include "modules/util/opstring.h"
#include "adjunct/m2/src/glue/mh.h"
#include "adjunct/desktop_util/adt/opqueue.h"

#define IMPORT_CHUNK_CAPACITY 65535
#define LOOKAHEAD_BUF_LEN 1024

class ImporterModel;
class ImporterModelItem;
class PrefsFile;

class Importer : public MessageLoop::Target
{
public:
						Importer();
						virtual ~Importer();

	virtual OP_STATUS	Init() { return OpStatus::ERR_NOT_SUPPORTED; }

	virtual OP_STATUS	ImportSettings()	{ return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS	ImportMessages()	{ return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS	ImportContacts()	{ return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS	ImportAccounts()	{ return OpStatus::ERR_NOT_SUPPORTED; }

	virtual OP_STATUS	AddImportFile(const OpStringC& path);
	virtual void		SetImportItems(const OpVector<ImporterModelItem>& items) {}

	virtual void		GetDefaultImportFolder(OpString& path) {}	// overloaded method should return most recently used mail folder
	virtual void		GetLastUsedImportFolder(EngineTypes::ImporterId id, OpString& path);
	virtual void		SetLastUsedImportFolder(EngineTypes::ImporterId id, const OpStringC& path);

	virtual BOOL		GetContactsFileName(OpString& fileName) { return FALSE; }

protected:
	virtual BOOL		OnContinueImport() { return FALSE; }
	virtual	void		OnCancelImport() { OP_ASSERT(0); }

public:
	OP_STATUS			SetImportFolderPath(const OpStringC& path) { return m_folder_path.Set(path); }
	OP_STATUS			AddImportFileList(const OpVector <OpString> * file_list);
	int					GetImportFileCount() { return m_file_list.GetCount(); }
	void				ResetImportFileList(OpVector <OpString> * file_list) { m_file_list.DeleteAll(); }
	void				SetImporterId(EngineTypes::ImporterId id) { m_id = id; }
	OP_STATUS			SetM2Account(UINT16 account_id);
	OP_STATUS			SetM2Account(const OpStringC& name);
	OP_STATUS			CreateImportAccount();

	void				SetImportItem(ImporterModelItem* item) { m_currentItem = item; }
	ImporterModelItem*	GetImportItem() const { return m_currentItem; }

	ImporterModelItem*	GetMoveToSentItem() const
						{ return m_moveToSentItem; }
						
	void				SetMoveToSentItem(ImporterModelItem* moveToSentItem)
						{ m_moveToSentItem = moveToSentItem; }

	INT32				GetGrandTotal() const { return m_grandTotal; }

	BOOL				GetImportAccount(OpString& account);

	ImporterModel*		GetModel() const { return m_model; }
	void				CancelImport();

	void				SetCSVFilePath(const OpStringC& csv_file_path) { m_csvPath.Set(csv_file_path); }

	OP_STATUS			ContinueImport() { return m_loop->Post(MSG_M2_CONTINUEIMPORT); }

protected:
	OP_STATUS			StartImport();
	OP_STATUS			OpenPrefsFile();
	OP_STATUS			AddToResumeCache(const OpStringC& mailbox);
	OP_STATUS			ClearResumeCache();
	BOOL				InResumeCache(const OpStringC& mailbox) const;
	void				EnableResumeCache(BOOL enable) { m_resumeCacheEnabled = enable; }
	BOOL				GetSettingsFileName(OpString& name);
	ImporterModelItem*	GetRootItem() const;

private:
	// from Target:
	OP_STATUS			Receive(OpMessage message);

						Importer(const Importer&);
	Importer&			operator =(const Importer&);

protected:
	UINT16						m_accountId;
	OpString					m_folder_path;
	OpAutoVector<OpString>		m_file_list;
	OpString					m_m2FolderPath;
	OpString					m_csvPath;
	OpString8					m_defaultCharset;
	BOOL						m_stopImport;
	OpFileLength				m_totalCount;
	OpFileLength				m_count;
	INT32						m_duplicates;
	INT32						m_grandTotal;
	OpString					m_currInfo;
	EngineTypes::ImporterId		m_id;
	ImporterModelItem*			m_moveToSentItem;
	BOOL						m_moveCurrentToSent;
	OpQueue<ImporterModelItem>*	m_sequence;
	OpString					m_error_message;

private:
	ImporterModel*		m_model;
	ImporterModelItem*	m_currentItem;
	MessageLoop*		m_loop;
	BOOL				m_isImporting;
	PrefsFile*			m_prefs;
	OpString			m_cacheName;
	BOOL				m_resumeCacheEnabled;

};

#endif//IMPORTER_H
