/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#include "modules/util/opstring.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/adt/opqueue.h"


#ifndef IMPORTER_MODEL_H
#define IMPORTER_MODEL_H

class ImporterModel;

class ImporterModelItem : public TreeModelItem<ImporterModelItem, ImporterModel>, public OpQueueItem<ImporterModelItem>
{
	public:
									ImporterModelItem(const Type type,
													  const OpStringC& name,
													  const OpStringC& path,
													  const OpStringC& virtualPath,
													  const OpStringC& info);

									ImporterModelItem(const Type type,
													  const char* name,
													  const char* path,
													  const char* virtualPath,
													  const char* info);

									ImporterModelItem(const ImporterModelItem& val);

		virtual						~ImporterModelItem() {}
									ImporterModelItem& operator=(const ImporterModelItem& val);

		virtual OP_STATUS			GetItemData(ItemData* item_data);

		virtual Type				GetType()				{ return m_type; }
		virtual INT32				GetID()					{ return -1; }
		virtual const OpString&		GetName() const			{ return m_name; }
		virtual const OpString&		GetPath() const			{ return m_path; }
		virtual const OpString&		GetVirtualPath() const	{ return m_virtualPath; }
		virtual const OpString&		GetInfo() const			{ return m_info; }
		virtual const OpString&		GetSettingsFileName() const	{ return m_settingsFileName; }
		virtual void				SetSettingsFileName(const OpStringC& name) { m_settingsFileName.Set(name); }
		virtual const OpString8&	GetAccountNumber() const{ return m_account_number; }
		virtual void				SetAccountNumber(const OpStringC8& number) { m_account_number.Set(number); }

	private:
									ImporterModelItem();

	private:
		Type						m_type;
		OpString					m_name;			// leaf name
		OpString					m_path;			// file path
		OpString					m_virtualPath;	// folder path
		OpString					m_info;			// misc info
		OpString					m_settingsFileName;	// file path of account settings file
		OpString8					m_account_number; // Number for Opera 7/8/9 import (lookup for account)
};

// ---------------------------------------------------------------------------------

class ImporterModel : public TreeModel<ImporterModelItem>
{
	public:
									ImporterModel();
									~ImporterModel();

		virtual OP_STATUS			Init();

		virtual INT32				GetColumnCount();
		virtual OP_STATUS			GetColumnData(ColumnData* column_data);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual OP_STATUS			GetTypeString(OpString& type_string);
#endif

				INT32				GetRootBranch(ImporterModelItem* item);
				ImporterModelItem*	GetRootItem(ImporterModelItem* item);
				BOOL				ItemHasChildren(INT32 position) { return (GetChildIndex(position) != -1); }

				OP_STATUS			MakeSequence(OpQueue<ImporterModelItem>* sequence, ImporterModelItem* parent_item, BOOL add = FALSE);
				OP_STATUS			MakeRootSequence(OpQueue<ImporterModelItem>* sequence);

				OP_STATUS			PushItem(OpQueue<ImporterModelItem>* sequence, const ImporterModelItem* item) { sequence->Enqueue(const_cast<ImporterModelItem*>(item)); return OpStatus::OK; }
				ImporterModelItem*	PullItem(OpQueue<ImporterModelItem>* sequence) { return sequence->Dequeue(); }
				void				EmptySequence(OpQueue<ImporterModelItem>* sequence);
				BOOL				SequenceIsEmpty(OpQueue<ImporterModelItem>* sequence) { return (sequence->IsEmpty()); }

	private:
				OP_STATUS			FillSequence(OpQueue<ImporterModelItem>* sequence, INT32 parent);


};


#endif//IMPORTER_MODEL_H
