/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_TRANSFER_MANAGER_H
#define OP_TRANSFER_MANAGER_H

class OpTransferItem;
class URL;
#ifdef WIC_USE_DOWNLOAD_CALLBACK
class DownloadContext;
class URLInformation;
#endif // WIC_USE_DOWNLOAD_CALLBACK

class OpTransferManagerListener
{
public:
	virtual ~OpTransferManagerListener() {}

    /** Called on transfer item added.
		@return TRUE if you wish to keep the item. Then it is
		automatically assumed that you did GetTransferItem on it, and
		you must ReleaseTransferItem when you are no longer interested
		in it. If FALSE is returned the download will be stopped and
		the item will be destroyed.
	*/
	virtual BOOL OnTransferItemAdded(OpTransferItem* transferItem, BOOL is_populating=FALSE, double last_speed=0) = 0;

    /** Called on transfer item removed and deleted.
	    Just use for deletion of pointers that should not be used anymore.
	*/
	virtual void OnTransferItemRemoved(OpTransferItem* transferItem) = 0;

	/* Called when a transfer item is stopped
	*/
	virtual void OnTransferItemStopped(OpTransferItem* transferItem) = 0;

	/* Called when a transfer item is resumed
	*/
	virtual void OnTransferItemResumed(OpTransferItem* transferItem) = 0;

	/* Called to ask the listener if the file(s) currently in the transfer manager can be deleted.
	** The listener will typically ask the user about this, if it is relevant.
	** The TransferManager will not actually delete any files, but it will call
	** OnTransferItemDeleteFiles() before  OnTransferItemRemoved() if this method returns TRUE.
	*/
	virtual BOOL OnTransferItemCanDeleteFiles(OpTransferItem* transferItem) = 0;

	/* Called to ask the listener to delete the files associated with this download.
	** The listener must previously have replied TRUE to OnTransferItemCanDeleteFile()
	** for this method to be called
	*/
	virtual void OnTransferItemDeleteFiles(OpTransferItem* transferItem) = 0;
};

class OpTransferListener
{
public:
	enum TransferStatus { TRANSFER_INITIAL, TRANSFER_DONE, TRANSFER_PROGRESS, TRANSFER_FAILED, TRANSFER_ABORTED, TRANSFER_CHECKING_FILES, TRANSFER_SHARING_FILES };
	virtual ~OpTransferListener() {}

	/** Called when data is downloaded. */
	virtual void OnProgress(OpTransferItem* transferItem, TransferStatus status ) = 0;

	/** Called when transfer is reused/reset. */
	virtual void OnReset(OpTransferItem* transferItem) = 0;

	virtual void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to) = 0;

	/** Called if file exists already and it was saved to file_location without creating a transfer item. */
	virtual void OnSavedFromCacheDone(const OpStringC & file_location) {}

	/** Allows the listener to hold a reference to the transfer item, so that it for example remove itself as a listener when it dies.*/
	virtual void SetTransferItem(OpTransferItem * transferItem) {}
};

class OpTransferItem
{
public:
	enum TransferAction {
		ACTION_UNKNOWN           		= 0x00,
		ACTION_RUN_WHEN_FINISHED 		= 0x01,
		ACTION_SAVE    			        = 0x02,
		/** Marks this file as a descriptor-file (eg. .DD/.JAD)
		 *  meaning the file it describes should be downloaded */
		ACTION_HANDLE_DESCRIPTOR_FILE	= 0x04
	};

	virtual ~OpTransferItem() {}

	/** Stops the download. */
	virtual void Stop() = 0;

	/** Continues a stopped download. A reload is performed by calling Clear(), then Continue(). */
	virtual BOOL Continue() = 0;

	/** Resets the downloaded data and frees the disk cache space used. Will do a Stop() if transfer is in progress. */
	virtual void Clear() = 0;

	/** Sets the filename to download to. If not set, the file will go into disk cache.
		Not enabled yet.
	*/
	virtual OP_STATUS SetFileName(const uni_char* filename) = 0;

	/** Sets the transferlistener
		@return the old listener. returns NULL if no listener was set.
	*/
	virtual OpTransferListener* SetTransferListener(OpTransferListener* listener) = 0;

	/** Sets what action to perform.
		@param action the action to perform. */
	virtual void SetAction(TransferAction action) = 0;

	/** Gets the action that is set.
		@return the action	*/
	virtual TransferAction GetAction() = 0;

#ifdef WIC_USE_DOWNLOAD_CALLBACK
	/** Gets the DownloadContext if set
		@return context */
	virtual DownloadContext * GetDownloadContext() = 0;
	/** Creates a URLInformation object based on the OpTransferItem. */
	virtual URLInformation* CreateURLInformation() = 0;
#endif // WIC_USE_DOWNLOAD_CALLBACK

	/** Methods to get the current status of the transfer. */
	virtual unsigned long GetTimeEstimate() = 0;
	virtual OpFileLength GetSize() = 0;
	virtual OpFileLength GetHaveSize() = 0;
	virtual double GetKbps() = 0;
	virtual const uni_char* GetUrlString() = 0;
	virtual URL  *GetURL() = 0;

	virtual OpString*	GetStorageFilename() = 0;

	virtual BOOL GetIsRunning() = 0;
	virtual BOOL GetShowTransfer() = 0;
};

class OpTransferManager
{
public:
	static OpTransferManager* CreateL();
	virtual ~OpTransferManager() {}

	/** Returns an existing or new OpTransferItem instance for the specified URL.
	 *
	 * If there are multiple existing OpTransferItem instances for the specified
	 * URL, the first matching item in the internal list is returned.
	 *
	 * Note that if already_created is set to FALSE after a call to GetTransferItem(), the
	 * item returned is newly allocated and should be freed with ReleaseTransferItem() when not
	 * needed anymore.
	 *
	 * Note that if GetTransferItem() is called with a NULL already_created reference, a FALSE
	 * create_new value, and a transfer item already exists for the given URL, the item state will
	 * be reset to TRANSFER_INITIAL.
	 * In case the GetTransferItem() is called with a non-NULL already_created reference, a FALSE
	 * create_new value, and the item already exists for the given URL, the item state is not changed
	 * and therefore the call can be used to check if a transfer item already exists for the URL.
	 *
	 * @param item on success receives the pointer to the OpTransferItem.
	 * @param url is the URL-string for which to return the OpTransferItem.
	 *  This should be url-escaped and contain the username and password
	 *  fragments.
	 * @param already_created points to a BOOL which is set to FALSE on success
	 *  if the returned OpTransferItem was newly created. It is set to TRUE
	 *  if the item was already used before. The caller may pass a NULL pointer
	 *  in which case the information is not set.
	 * @param create_new If the argument is TRUE a new OpTransferItem is
	 *  returned even if there is an existing item with the same url.
	 * @retval OpStatus::OK if a new or existing OpTranferItem was returned.
	 *  If already_created is set to TRUE, the caller needs to call
	 *  ReleaseTransferItem() on the returned item when it is no longer needed.
	 * @retval An error status is returned when no OpTransferItem can be created,
	 *  e.g. because the specified url is empty, or there is not enough memory.
	 *  In this case item is not changed.
	 */
	virtual OP_STATUS GetTransferItem(OpTransferItem** item, const uni_char* url, BOOL* already_created = NULL, BOOL create_new = FALSE) = 0;

	/**
	 * Returns a new OpTransferItem instance for the specified URL. The item should be released with ReleaseTransferItem() when not needed.
	 */
	virtual OP_STATUS GetNewTransferItem(OpTransferItem** item, const uni_char* url) = 0;

	/**
	 * Release the transfer item got earlier with a call GetNewTransferItem(), a call to GetTransferItem() that
	 * indicated creating a new item by returning already_created set to FALSE, or got in a OnTransferItemAdded() callback.
	 */
	virtual void ReleaseTransferItem(OpTransferItem* item) = 0;

	/** Remove all transfers started in privacy mode */
	virtual void RemovePrivacyModeTransfers() = 0;

	/** Sets the transfer manager listener */
	virtual OP_STATUS AddTransferManagerListener(OpTransferManagerListener* listener) = 0;
	virtual OP_STATUS RemoveTransferManagerListener(OpTransferManagerListener* listener) = 0;

	virtual OpTransferItem*     GetTransferItem(int index) = 0;
	virtual int					GetTransferItemCount() = 0;

#ifdef WIC_USE_DOWNLOAD_RESCUE_FILE
	/** Set Filename for rescuefile, it will be in OPFILE_HOME_FOLDER
	 *  This will start a process of saving to rescue-file whenever a
	 *  transferItem is added or removed and at transfermanager destruction */
	virtual OP_STATUS SetRescueFileName(const uni_char * rescue_file_name) = 0;
#endif // WIC_USE_DOWNLOAD_RESCUE_FILE
};

#endif // OP_TRANSFER_MANAGER_H
