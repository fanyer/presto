/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Blazej Kazmierczak (bkazmierczak)
 */

#ifndef GADGET_UPDATE_CONTROLER_H
#define GADGET_UPDATE_CONTROLER_H

#include "adjunct/autoupdate/scheduler/optaskscheduler.h"
#include "adjunct/desktop_util/handlers/SimpleDownloadItem.h"

#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/util/adt/opvector.h"

class DesktopGadgetDownloadObject;
class GadgetUpdateListener;
struct GadgetInstallerContext;

/*
* Class responsible for controlling gadgets autoupdate
* 
* Flow description: 
* To start update flow this class needs to be registered as a gadgetListener somewhere, 
* and gadget->Update() funciton needs to be called on some gadget. 
* If gadget update data is available  OnGadgetUpdateReady will be called. 
* This class requieres at least one update listener (so called worker) for update to process.
* When update is available OnGadgetUpdateAvailable is called on listeners
* This needs to be responded by either AllowCurrentUpdate() or RejectCurrentUpdate().
* In case of AllowCurrentUpdate() gets called, update file is downloaded and when done 
* OnGadgetUpdateStarted() and OnGadgetUpdateDownloaded() are called on listeners, which should be
* processed by the worker according to gadget type.
* After update is made, worker should call GadgetUpdateFinished() with status information, 
* which results in brodcasting finish information to all listeners. 
* 
* Additional info: 
* class can receive multiple OnGadgetUpdateReady calls
* it stores the update data in the queue, 
*/


struct GadgetUpdateInfo
{
	OpString src;
	OpString details;
	OpGadgetClass* gadget_class;
};


typedef OpAutoVector<GadgetUpdateInfo> GadgetUpdateQueue;

class GadgetUpdateController: 
	public OpGadgetListener,
	public OpScheduledTaskListener,
	public SimpleDownloadItem::Listener


{	
public:
	GadgetUpdateController();
	~GadgetUpdateController();
    // ================  OpGadgetListener ===========================
	/**
	* Called by core, when update data is availavle
	* if "data" is correctly initialized, it will be inserted in the update queue
	*/
	virtual void OnGadgetUpdateReady(const OpGadgetUpdateData& data);
	virtual void OnGadgetUpdateError(const OpGadgetErrorData& data);

	// ================  OpGadgetListener - UNUSED ===========================
	virtual void OnGadgetDownloadPermissionNeeded(const OpGadgetDownloadData& data, GadgetDownloadPermissionCallback *callback) {}
	virtual void OnGadgetDownloaded(const OpGadgetDownloadData& data){}
	virtual void OnGadgetInstalled(const OpGadgetInstallRemoveData& data){}
	virtual void OnGadgetRemoved(const OpGadgetInstallRemoveData& data){}
	virtual void OnGadgetUpgraded(const OpGadgetStartStopUpgradeData& data) {}
	virtual void OnRequestRunGadget(const OpGadgetDownloadData& data){}
	virtual void OnGadgetStarted(const OpGadgetStartStopUpgradeData& data) {}
	virtual void OnGadgetStopped(const OpGadgetStartStopUpgradeData& data) {}
	virtual void OnGadgetDownloadError(const OpGadgetErrorData& data){}
	virtual void OnGadgetInstanceCreated(const OpGadgetInstanceData& data) {}
	virtual void OnGadgetInstanceRemoved(const OpGadgetInstanceData& data) {}
	virtual void OnGadgetSignatureVerified(const OpGadgetListener::OpGadgetSignatureVerifiedData&) { }
	virtual void OnGadgetStartFailed(const OpGadgetListener::OpGadgetStartFailedData&) { }


	
	/**
	*	Remove current update from update queue
	*	(in "before download" state)
	*/
	void RejectCurrentUpdate();

	/**
	*	Initiates download of gadget update file
	*/
	void AllowCurrentUpdate();

	/**
	*  Called when download of gadget update succeeds
	*  
	*  @param download_path - path to gadget update source
	*/
	virtual void DownloadSucceeded(const OpStringC& widget_path);
	
	/**
	* Called when download of widget update fails
	*/
	virtual void DownloadFailed();

	virtual void DownloadInProgress(const URL&) {}

	/**
	* Called by update worker, 
	* GadgetUpdateFinished should be called always after OnGadgetUpdateStarted has been received
	*  
	* It notifies listener about update finish status. 
	* -> Clean up of temporary objects and files is done here
	*/
	virtual void GadgetUpdateFinished(OP_STATUS status);

	void StartScheduler();

	OP_STATUS		    AddUpdateListener(GadgetUpdateListener* listener)		{ return m_listeners.Add(listener); }
	void				RemoveUpdateListener(GadgetUpdateListener* listener)	{ m_listeners.Remove(listener); }

	enum GadgetUpdateResult{
		UPD_SUCCEDED,				// everything succeeded
		UPD_DOWNLOAD_FAILED,		// download of update file failed
		UPD_FAILED,					// update failed
		UPD_FATAL_ERROR,				// update failed, we can't probably restart old widget
		UPD_REJECTED_BY_USER
	};


	UINT32 GetQueueLength(){return m_update_queue.GetCount();}

	 // ================  OpScheduledTaskListener ===========================
	void OnTaskTimeOut(OpScheduledTask* task);

protected:
	GadgetUpdateQueue		m_update_queue;

private:

	OP_STATUS OnUpdatePossibilityConfirmed();


	/**
	* notify listeners about update availability
	*/
	void				BroadcastUpdateAvailable(GadgetUpdateInfo* data,BOOL available);

	/**
	* notify listeners about the update finish and the result of update
	*/
	void				BroadcastGadgetUpdateFinish(GadgetUpdateInfo* data,GadgetUpdateController::GadgetUpdateResult result);

	/**
	* notify listeners to prepare for update
	*/
	void				BroadcastPreGadgetUpdate(GadgetUpdateInfo* data);

	/**
	* notify listeners to process update file
	*/
	void				BroadcastUpdateDownloaded(GadgetUpdateInfo* data,OpStringC &path);

	/**
	* get next item from the update queue and process it if possible
	*/
	void				ProcessNextUpdate();

	OpScheduledTask		m_update_sheduler;
	OpString			m_update_file_path; 
	SimpleDownloadItem* m_download_obj;
	OpListeners<GadgetUpdateListener>	m_listeners;
	GadgetUpdateResult	m_update_result;
	BOOL				m_is_unite_update;

};


/**
* Interface for gadget update listeneres. Using that update controller 
* will inform about auto update progress and status changes
*/
class GadgetUpdateListener
{
public: 

	virtual ~GadgetUpdateListener(){};

	/**
	*	When update is available
	*/
	virtual void OnGadgetUpdateAvailable(GadgetUpdateInfo* data,BOOL visible){}

	/**
	*	When update file has been dowloaded - preparation to real update phase
	*/
	virtual void OnGadgetUpdateStarted(GadgetUpdateInfo* data){} 

	/**
	*	When update file has been dowloaded and update is ready to be processed
	*/
	virtual void OnGadgetUpdateDownloaded(GadgetUpdateInfo* data, OpStringC& update_source ){}

	/**
	*   When update has been finihed
	*/
	virtual void OnGadgetUpdateFinish(GadgetUpdateInfo* data,GadgetUpdateController::GadgetUpdateResult result){}
};


#endif  //GADGET_UPDATE_CONTROLER_H
