/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** George Refseth and Patricia Aas
*/

#ifndef __TRANSFER_ITEM_CONTAINER_H__
#define __TRANSFER_ITEM_CONTAINER_H__

#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "modules/windowcommander/src/TransferManager.h"

class DesktopTransferManager;

#define TOGGLE_COLUMN_SHOW_TRANSFERRED_SIZE_MS	5000
#define TOGGLE_COLUMN_SHOW_SPEED_MS				2000
#define TOGGLE_COLUMN_SHOW_TIMELEFT_MS			2000
#define DownloadRescueFile UNI_L("download.dat")

class TransferItemContainer :
	public OpTreeModelItem
{

public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	   Constructor

	   @param last_speed
	 */
    TransferItemContainer(double last_speed);


	/**
	   Destructor
	 */
    ~TransferItemContainer();


	/**
	   @return
	 */
    OpTransferListener::TransferStatus	GetStatus() { return m_status; }


	/**
	   @param status
	 */
    void SetStatus(OpTransferListener::TransferStatus status);


	/**

	 */
    void ResetStatus() {m_status = OpTransferListener::TRANSFER_INITIAL; }


	/**
	   @return
	 */
	TransferItem* GetAssociatedItem() { return m_transferitem; }


	/**
	   @param transfer_item
	 */
    void SetAssociatedItem(TransferItem* transfer_item) { m_transferitem = transfer_item; }


	/**
	   @param status
	 */
    void SetResumableState(URL_Resumable_Status status) { m_is_resumable = status; }


	/**
	   @return
	 */
    double GetLastKnownSpeed(){ return m_last_known_speed; }


	/**
	   @param transfers_container
	 */
    void SetParentContainer(DesktopTransferManager* transfers_container ) { m_parent_container = transfers_container; }


	/**
	   @param open_folder
	 */
    void Open(BOOL open_folder);


	/**
	   @return
	 */
	OP_STATUS SetHandlerApplication();


	/**
	   @param filename
	   @return
	 */
    OP_STATUS MakeFullPath(OpString & filename);


	// == OpTreeModelItem ======================

    virtual int GetID() { return m_id; }

    virtual Type GetType() { return TRANSFER_ELEMENT_TYPE; }

    virtual OP_STATUS GetItemData(ItemData* item_data);

	BOOL NeedToLoadIcon() { return m_need_to_load_icon; }

	void SetHasTriedToLoadIcon() { m_need_to_load_icon = false; }

	BOOL LoadIconBitmap();

	void SetIconBitmap(Image image) { m_iconbitmap = image; }

private:

//  -------------------------
//  Private member functions:
//  -------------------------

    OpString* GetHandlerApplication() { return &m_handlerapplication; }

    void SetLastKnownSpeed(float speed){ m_last_known_speed = speed; }

	void GetFilename(TransferItem * transferitem, OpString & filename);

	void SetStatusSharing(OpTransferListener::TransferStatus s);

	void SetStatusDone(OpTransferListener::TransferStatus s);

	OP_STATUS GetFilenameAndExtension(OpString & fileName, OpString & fileExtension);

	OP_STATUS SetViewer();

	// GetItemData help functions

	OP_STATUS ProcessInfoQuery(ItemData * item_data);

	OP_STATUS TranslateColumn(ItemData* item_data, INT32 & col);

	OP_STATUS SetUpSpeedColumn(ItemData* item_data);

	OP_STATUS SetUpTimeColumn(ItemData* item_data);

	OP_STATUS SetUpProgressColumn(ItemData* item_data);

	OP_STATUS SetUpSizeColumn(ItemData* item_data);

	OP_STATUS SetUpFilenameColumn(ItemData* item_data);

	OP_STATUS SetUpStatusColumn(ItemData* item_data);

//  -------------------------
//  Private member variables:
//  -------------------------

    INT32 m_id;
    TransferItem* m_transferitem;
    OpTransferListener::TransferStatus m_status;
    URL_Resumable_Status m_is_resumable;
    Image m_iconbitmap;
    OpString m_handlerapplication;
    DesktopTransferManager*	m_parent_container;// this is the parent that may have something special
    double m_last_known_speed;
    double m_last_known_speed_upload;
    bool m_need_to_load_icon;
};


#endif // __TRANSFER_ITEM_CONTAINER_H__
