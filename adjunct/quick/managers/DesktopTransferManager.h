/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** George Refseth and Patricia Aas
*/

/*
  Container class interfacing between TransferManager and TransferPanel
*/

#ifndef DESKTOP_TRANSFER_MANAGER_H
#define DESKTOP_TRANSFER_MANAGER_H

#include "adjunct/desktop_util/handlers/TransferItemContainer.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/managers/DesktopManager.h"
#include "modules/windowcommander/src/TransferManager.h"
#include "adjunct/desktop_pi/loadicons_pi.h"

class DelayedManageAction;

#define g_desktop_transfer_manager (DesktopTransferManager::GetInstance())

#ifdef DESKTOP_ASYNC_ICON_LOADER
class DesktopOpAsyncFileBitmapLoader : public OpAsyncFileBitmapLoader, public MessageObject
{
public:
	DesktopOpAsyncFileBitmapLoader() :
		m_listener(NULL)
	{

	}
	virtual ~DesktopOpAsyncFileBitmapLoader();
	virtual OP_STATUS Init(OpAsyncFileBitmapHandlerListener *listener);
	virtual void Start(OpVector<TransferItemContainer>& transferitems);

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	// load icons for transfer items, if not done within a short period of time, post a message and do the rest later. 
	// this is to prevent the UI from freezing when loading icons takes too much time
	void LoadItemIcons();

private:
	OpAutoVector<OpString>			m_filenames;		// the filenames of the files to get the icon for
	OpAsyncFileBitmapHandlerListener *m_listener;
};
#endif // DESKTOP_ASYNC_ICON_LOADER

class DesktopTransferManager :
	public DesktopManager<DesktopTransferManager>,
	public OpTreeModel,
	public OpTransferListener,
	public OpTransferManagerListener,
	public OpTimerListener,
	public OpAsyncFileBitmapLoader::OpAsyncFileBitmapHandlerListener,
	public MessageObject
{

public:

	class TransferListener
	{
	public:
		virtual ~TransferListener() {}

		/** Called when data is downloaded. */
		virtual void OnTransferProgress(TransferItemContainer* transferItem,
				OpTransferListener::TransferStatus status) = 0;

		/** Called when an item is added. */
		virtual void OnTransferAdded(TransferItemContainer* transferItem) = 0;

		/** Called when an item is removed. */
		virtual void OnTransferRemoved(TransferItemContainer* transferItem) = 0;

		/** Called when new "unread" transfers are done */
		virtual void OnNewTransfersDone() = 0;
	};

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	   Constructor
	 */
    DesktopTransferManager();


	/**
	   Destructor
	 */
    ~DesktopTransferManager();

	virtual OP_STATUS Init();

	/**
	   @param listener
	 */
	void AddTransferListener(TransferListener* listener);


	/**
	   @param listener
	 */
    void RemoveTransferListener(TransferListener* listener);


	/**
	   @return
	 */
	int GetSwapColumn() { return m_swap_column; }


	/**
	   @param remaining_transfers
	   @return
	 */
    UINT32 GetMaxTimeRemaining(UINT32* remaining_transfers = NULL);


	/**
	@param downloaded_sizes - Total bytes downloaded for all active downloads
	@param total_sizes - Total bytes for all downloads
	@param remaining_transfers
	@return
	*/
	void GetFilesSizeInformation(OpFileLength& downloaded_size, OpFileLength& total_sizes, UINT32* remaining_transfers = NULL);


	/**
	   @param id
	   @param open_folder
	   @return
	 */
    BOOL OpenItemByID(INT32 id, BOOL open_folder);


	/**
	   @param new_transfers_done
	 */
    void SetNewTransfersDone(BOOL new_transfers_done);


	/**
	   @return
	 */
    BOOL IsNewTransfersDone() {return m_new_transfers_done;}


	/**
	   @return
	 */
    BOOL HasActiveTransfer() {return GetMaxTimeRemaining() != 0;}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	/**
	   @param
	   @return
	 */
    OP_STATUS GetTypeString(OpString& type_string);
#endif

	// == OpTransferListener ======================

    virtual void OnProgress(OpTransferItem* transferItem, TransferStatus status);

    virtual void OnReset(OpTransferItem* transferItem);

    virtual void OnRedirect(OpTransferItem* transferitem, URL* redirect_from, URL* redirect_to);

	// == OpTransferManagerListener ======================

    virtual BOOL OnTransferItemAdded(OpTransferItem* transferItem, BOOL is_populating, double last_speed=0);

	virtual void OnTransferItemRemoved(OpTransferItem* transferitem);

	virtual void OnTransferItemStopped(OpTransferItem* transferItem) {}

	virtual void OnTransferItemResumed(OpTransferItem* transferItem) {}

	virtual BOOL OnTransferItemCanDeleteFiles(OpTransferItem* transferItem) { return FALSE; }

	virtual void OnTransferItemDeleteFiles(OpTransferItem* transferItem) {}

    // == TreeModel implementation ==========================

    virtual INT32 GetColumnCount() { return 7; } //indicator, name, size, progress, time and speed

    virtual OP_STATUS GetColumnData(ColumnData* column_data);

    virtual OpTreeModelItem* GetItemByPosition(INT32 position);

    virtual INT32 GetItemParent(INT32 position) {return -1;} //no parents

    virtual INT32 GetItemCount() { return m_transferitems.GetCount();}

     // == MessageObject =============
     
     virtual void     HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// == OpAsyncFileBitmapHandlerListener =================

	 virtual void OnBitmapLoaded(TransferItemContainer *item);
	 virtual void OnBitmapLoadingDone();

	 void StartLoadIcons();
	 TransferItemContainer* FindTransferItemContainerFromFilename(OpStringC filename);

 private:

//  -------------------------
//  Private member functions:
//  -------------------------

    void OnTimeOut(OpTimer* timer);

    INT32 GetModelIndex(OpTransferItem* transferItem);

//  -------------------------
//  Private member variables:
//  -------------------------

    OpAutoVector<TransferItemContainer>	m_transferitems;

    OpVector<TransferListener> m_listeners;

    OpTimer* m_timer;

    DelayedManageAction* m_delayed_manage_action;

    BOOL m_addtransferitemsontop;

    BOOL m_new_transfers_done;

    int	m_swap_column;	// this is a timer dependant flag

	OpAsyncFileBitmapLoader *m_icon_loader;	// the class that'll handle the async loading of file icons

	bool m_bitmap_loading_in_progress;		// true if bitmap loading has been started
};

#endif // DESKTOP_TRANSFER_MANAGER_H
