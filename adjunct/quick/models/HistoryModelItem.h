/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 * psmaas - Patricia Aas Oct 2005
 */

#ifndef HISTORY_MODEL_ITEM_H
#define HISTORY_MODEL_ITEM_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"

#include "modules/history/history_enums.h"
#include "modules/history/OpHistoryItem.h"
#include "modules/history/OpHistoryPage.h"
#include "modules/history/OpHistoryFolder.h"
#include "modules/history/OpHistoryTimeFolder.h"
#include "modules/history/OpHistorySiteFolder.h"
#include "modules/history/OpHistorySiteSubFolder.h"
#include "modules/history/OpHistoryPrefixFolder.h"

class HistoryModel;
class HistoryModelSiteFolder;
class HistoryModelPrefixFolder;
class HistoryModelSiteSubFolder;
class HistoryAutocompleteModelItem;
class PageBasedAutocompleteItem;
class DesktopHistoryModel;

//__________________________ HISTORY MODEL ITEM ------------------------------------

class HistoryModelItem :
	public TreeModelItem<HistoryModelItem, DesktopHistoryModel>,
	public HistoryItem::Listener
{
    friend class DesktopHistoryModel; //To allow access to protected/private constructor and get/set methods

public:

	HistoryModelItem();
	~HistoryModelItem();

	/**
	 * @param title of the item
	 *
	 * @return OpStatus::OK if no error occurred
	 */
    OP_STATUS GetTitle(OpString & title) const { return GetCoreItem()->GetTitle(title); }
	const OpStringC& GetTitle() const { return GetCoreItem()->GetTitle(); }

	/**
	 * @param address of the item
	 *
	 * @return OpStatus::OK if no error occurred
	 */
	virtual OP_STATUS GetAddress(OpString & address) const { return OpStatus::OK; }

	/**
	 * @return the image string of the item
	 */
	const char * GetImage() const { return GetCoreItem()->GetImage(); }

	/**
	 * @param create_folder if TRUE the folder will be created if it does not exist
	 *
	 * @return a pointer to the folder
	 */
	HistoryModelSiteFolder * GetSiteFolder(BOOL create_folder = TRUE);

	/**
	 * @return a pointer to the folder
	 */
	HistoryModelPrefixFolder * GetPrefixFolder();

	/**
	 * @return the HistoryAutocompleteModelItem associated with this history page (if any) or NULL.
	 */
	virtual HistoryAutocompleteModelItem * GetHistoryAutocompleteModelItem() { return NULL; }

	/**
	 * @return TRUE if this item can be deleted - either itself or its contents
	 */
	virtual BOOL IsDeletable() const { return TRUE; }

	/**
	 * @return TRUE if this item cannot be deleted
	 */
	virtual BOOL IsSticky() const { return FALSE; }

	/**
	 * @return TRUE if this item is currently being moved
	 */
	BOOL IsMoving() { return m_moving; }

	/**
	 * @return the id of the previous parent
	 */
	INT32 GetOldParentId() { return m_old_parent_id; }

	void UnMatch();

	void Match(const OpStringC & excerpt);

	/**
	 * @return TRUE if item is a folder
	 */
	BOOL IsFolder() { return GetType() == FOLDER_TYPE; }
	
	// Implementing OpTreeModelItem interface
    virtual OP_STATUS GetItemData(ItemData* item_data) = 0;
    virtual INT32 GetID() { return m_id; }
    virtual Type GetType() { return HISTORY_ELEMENT_TYPE; }

	// Implementing HistoryItem::Listener interface
	virtual Type GetListenerType() { return GetType(); }
	virtual void OnHistoryItemDestroying() { RemoveCoreItem(); }

	virtual int	GetNumLines() { return m_excerpt.HasContent() ? 2 : 1; }

private:

	void SetSiteFolder(HistoryModelSiteFolder* folder);

	HistoryModelSiteFolder*       m_siteFolder;
	HistoryModelPrefixFolder*     m_prefixFolder;
	INT32	                      m_id;
	BOOL                          m_moving;
	INT32                         m_old_parent_id;

protected:

	HistoryItem::Listener* GetListener(HistoryItem* core_item);
	virtual HistoryItem* GetCoreItem() const = 0;
	virtual void RemoveCoreItem() = 0;

	BOOL                          m_matched;
	OpString                      m_excerpt;
};

//__________________________ HISTORY MODEL PAGE ----------------------------------

class HistoryModelPage : public HistoryModelItem
{
public:

	HistoryModelPage(HistoryPage* page);

	virtual ~HistoryModelPage();

    time_t GetAverageInterval() const { return m_core_page->GetAverageInterval(); }

    time_t GetPopularity() { return m_core_page->GetPopularity(); }

	const char* GetImage() const { return m_core_page->GetImage(); }

	time_t GetAccessed() const { return m_core_page->GetAccessed(); }

	BOOL IsInHistory() const { return m_core_page ? m_core_page->IsInHistory() : FALSE; }

	bool IsInBookmarks() const { return m_core_page ? !!m_core_page->IsBookmarked() : false; }

	PageBasedAutocompleteItem * GetSimpleItem() { return m_simple_item; }

	/**
	 * @return the HistoryAutocompleteModelItem associated with this history page (if any) or NULL.
	 */
	virtual HistoryAutocompleteModelItem * GetHistoryAutocompleteModelItem();

	void UpdateSimpleItem();

	void SetOpen(BOOL value) { if(m_open_in_a_tab != value) { m_open_in_a_tab = value; Change(); } }
	BOOL IsOpen() { return m_open_in_a_tab; }

	// Implementing HistoryModelItem interface
	OP_STATUS GetAddress(OpString & address) const { return m_core_page->GetAddress(address); }
	virtual HistoryItem* GetCoreItem() const { return m_core_page; }
	HistoryPage* GetCoreHistoryPage() const { return m_core_page; }
	virtual void RemoveCoreItem();

	OP_STATUS GetStrippedAddress(OpString & address) const { return address.Append(m_core_page->GetStrippedAddress()); }
	// Implementing OpTreeModelItem interface
    virtual OP_STATUS GetItemData(ItemData* item_data);

private:

	void RemoveCorePage();

	HistoryPage* m_core_page;
	BOOL m_open_in_a_tab;
	PageBasedAutocompleteItem * m_simple_item;
};

//__________________________ HISTORY MODEL FOLDER ----------------------------------

class HistoryModelFolder : public HistoryModelItem
{
public:

	virtual ~HistoryModelFolder();

	HistoryModelFolder(HistoryFolder* folder);

    virtual HistoryFolderType GetFolderType() = 0;

	// Implementing HistoryModelItem interface
	virtual HistoryItem* GetCoreItem() const { return m_core_folder; }
	virtual void RemoveCoreItem();
	virtual BOOL IsEmpty() const { return FALSE; }
    virtual Type GetType() { return FOLDER_TYPE; }

	// Implementing OpTreeModelItem interface
    virtual OP_STATUS GetItemData(ItemData* item_data);

private:

	void RemoveCoreFolder();
	HistoryFolder* m_core_folder;
};

//__________________________ HISTORY MODEL SITE FOLDER -----------------------------

class HistoryModelSiteFolder : public HistoryModelFolder
{
    friend class HistoryModelItem;

public:

	HistoryModelSiteFolder(HistorySiteFolder* folder);

	~HistoryModelSiteFolder();

	HistoryModelSiteSubFolder * GetSubFolder(TimePeriod period);

	// Implementing HistoryModelItem interface
    virtual BOOL IsEmpty() const { return m_num_children == 0; }
	virtual BOOL IsDeletable() const { return IsEmpty(); }

	// Implementing HistoryModelFolder interface
    virtual HistoryFolderType GetFolderType() { return SITE_FOLDER; }

private:

    void RemoveChild(){m_num_children--;}

    void AddChild() {m_num_children++;}

	INT32  m_num_children;

	HistoryModelSiteSubFolder* m_subfolders[NUM_TIME_PERIODS];
};

//__________________________ HISTORY MODEL SITE SUB FOLDER -------------------------

class HistoryModelSiteSubFolder : public HistoryModelFolder
{
public:

	HistoryModelSiteSubFolder(HistorySiteSubFolder* folder)
		: HistoryModelFolder(folder){}

	// Implementing HistoryModelItem interface
	virtual BOOL IsDeletable() const { return FALSE; }

	// Implementing HistoryModelFolder interface
    virtual HistoryFolderType GetFolderType() { return SITE_SUB_FOLDER; }
};

//__________________________ HISTORY MODEL TIME FOLDER -----------------------------

class HistoryModelTimeFolder : public HistoryModelFolder
{
public:

    HistoryModelTimeFolder(HistoryTimeFolder* folder):
		HistoryModelFolder(folder){}

	// Implementing HistoryModelItem interface
	virtual BOOL IsDeletable() const { return FALSE; }
	virtual BOOL IsSticky() const { return TRUE; }

	// Implementing HistoryModelFolder interface
    virtual HistoryFolderType GetFolderType() { return TIME_FOLDER; }
};

//__________________________ HISTORY MODEL PREFIX FOLDER ---------------------------

class HistoryModelPrefixFolder : public HistoryModelFolder
{
public:

    HistoryModelPrefixFolder(HistoryPrefixFolder* folder):
		HistoryModelFolder(folder){}

	// Implementing HistoryModelItem interface
	virtual BOOL IsDeletable() const { return FALSE; }
	virtual BOOL IsSticky() const { return TRUE; }

	// Implementing HistoryModelFolder interface
	virtual HistoryFolderType GetFolderType() { return PREFIX_FOLDER; }
};

#endif // HISTORY_MODEL_H
