/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef TRANSFER_MANAGER_H
#define TRANSFER_MANAGER_H

#include "modules/hardcore/timer/optimer.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/url/url2.h"
#include "modules/util/simset.h"
#ifdef WIC_USE_DOWNLOAD_CALLBACK
# include "modules/viewers/viewers.h"
#endif // WIC_USE_DOWNLOAD_CALLBACK
#include "modules/util/adt/opvector.h"
#include "modules/windowcommander/OpTransferManager.h"

// a class used as a "fake" URL for P2P uploads/downloads
// where URL imposes restrictions not easily overcome in the transfer manager
class P2P_URL
{
public:
	P2P_URL()
	:	m_content_loaded(0),
		m_content_size(0),
		m_status(URL_LOADING_WAITING)
	{
	}
	virtual ~P2P_URL() {}

public:
	enum P2PExtendedTransferStatus { P2P_CHECKING_FILES = 10, P2P_SHARING_FILES };

	void SetContentSize(OpFileLength content_size) { m_content_size = content_size; }
	void ForceStatus(URLStatus status) { m_status = (int)status; }
	void ForceStatus(P2PExtendedTransferStatus status) { m_status = (int)status; }
	OpFileLength GetContentLoaded() { return m_content_loaded; }
	int Status() { return m_status; }

private:
	OpFileLength m_content_loaded;
	OpFileLength m_content_size;
	int m_status;
};

class TransferItem : public OpTransferItem, public Link
#ifdef WIC_USE_DOWNLOAD_CALLBACK
, private MessageObject
#endif // WIC_USE_DOWNLOAD_CALLBACK
{
	OpTransferListener* m_listener;
	URL_InUse m_url;
	P2P_URL m_p2p_url;
	BOOL m_added;
	OpFileLength m_size, m_loaded, m_lastLoaded, m_uploaded, m_lastUploaded;
	unsigned long m_timeLeft;
	double m_kibs, m_kibsUpload;
	OpString m_storage_filename;		///< stored filename
	OpTransferListener::TransferStatus m_lastStatus;
	TransferAction m_action;
	UINT32 m_connections_with_complete_file, m_connections_with_incomplete_file, m_downloadConnections, m_uploadConnections;
	OpString m_metafile;			// used for BT, and future downloads requiring a meta file
	OpString m_download_directory;	// used for BT, and future downloads wanting to save the last directory used
	BOOL	 m_calc_cps;			// when TRUE, calculate cps
	BOOL	 m_is_completed;		// when TRUE, files are complete and CRC checked (BitTorrent)
	UINT32	m_total_with_complete_file;	// clients with complete file (BitTorrent)
	UINT32	m_total_leechers;			// clients currently downloading file (BitTorrent)

	BOOL	m_show_transfer;
	BOOL    m_started_in_privacy_mode;
#ifdef WIC_USE_DOWNLOAD_CALLBACK
private:
	DownloadContext *	m_download_context;
	Viewer				m_viewer;
	OpString			m_copy_to_filename;
	BOOL				m_copy_when_downloaded;
	MessageHandler*		m_copy_when_downloaded_handler;
	static const OpMessage transferitem_messages[];
	/* =
		{
			MSG_URL_LOADING_FAILED,
			MSG_HEADER_LOADED,
			MSG_URL_DATA_LOADED,
			MSG_ALL_LOADED,
			MSG_NOT_MODIFIED,
			MSG_MULTIPART_RELOAD
		};
		*/

	friend class CopyHandlerObserver;

	class CopyHandlerObserver : public MessageHandlerObserver
	{
	public:
		CopyHandlerObserver(TransferItem* ti) : m_transfer_item(ti) {}

		void MessageHandlerDestroyed(MessageHandler *mh);

	private:
		TransferItem* m_transfer_item;
	};

	CopyHandlerObserver m_copy_when_downloaded_observer;

public:

	void SetCallbacks(MessageHandler* handler, MH_PARAM_1 id);
	void UnsetCallbacks();
	//  MessageObject API:
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	//	EO MessageObject API.
	// Defined in OpTransferItem, for use by the ui (listener)
	DownloadContext * GetDownloadContext() { return m_download_context; }
	// Defined in OpTransferItem to use as gobetween  (fake url pointer)
	URLInformation* CreateURLInformation();

	// For use by core
	void SetDownloadContext(DownloadContext * dc){ m_download_context = dc; }
	// For use by core
	void SetViewer(Viewer& viewer) { m_viewer.CopyFrom(&viewer); }
	// Used by desktop_util
	Viewer & GetViewer() { return m_viewer; }
	// Used internally in windowcommander:
	OP_STATUS SetCopyWhenDownloaded(const uni_char * filename) { m_copy_when_downloaded = TRUE; return m_copy_to_filename.Set(filename);}
#endif // WIC_USE_DOWNLOAD_CALLBACK

public:

	enum TransferItemType {
		TRANSFERTYPE_DOWNLOAD,
		TRANSFERTYPE_CHAT_DOWNLOAD,
		TRANSFERTYPE_CHAT_UPLOAD,
		TRANSFERTYPE_PEER2PEER_DOWNLOAD,
		TRANSFERTYPE_WEBFEED_DOWNLOAD
	};

	TransferItem();
	~TransferItem();
	OP_STATUS Init(URL& url);

	// from OpTransferItem
	void					Stop();
	BOOL					Continue();
	void					Clear();
	OP_STATUS				SetFileName(const uni_char* filename);
	OpTransferListener*		SetTransferListener(OpTransferListener* listener);
	void					SetAction(TransferAction action);
	void					SetType(TransferItemType type);
	TransferAction          GetAction()			{ return m_action; }
	OpString*				GetStorageFilename(){ return &m_storage_filename; }
	OP_STATUS				SetStorageFilename(const uni_char* filename) { return m_storage_filename.Set(filename); }
	unsigned long			GetTimeEstimate(){ return m_timeLeft;                }
	OpFileLength			GetSize()        { return m_size;                    }
	OpFileLength			GetHaveSize()    { return m_loaded;                  }
	double					GetKbps()               { return m_kibs;                    }
	double					GetKbpsUpload()          { return m_kibsUpload;                    }
	OpFileLength			GetUploaded()	{ return m_uploaded; }
	UINT32					GetConnectionsWithCompleteFile() { return m_connections_with_complete_file; }
	UINT32					GetConnectionsWithIncompleteFile() { return m_connections_with_incomplete_file; }
	UINT32					GetDownloadConnections() { return m_downloadConnections; }
	UINT32					GetUploadConnections() { return m_uploadConnections; }
	UINT32					GetTotalWithCompleteFile() { return m_total_with_complete_file; }
	UINT32					GetTotalDownloaders() { return m_total_leechers; }
	OP_STATUS				SetMetaFile(OpString& metafile) { return m_metafile.Set(metafile); }
	OP_STATUS				GetMetaFile(OpString& metafile) { return metafile.Set(m_metafile);	}
	OP_STATUS				SetDownloadDirectory(OpString& download_directory) { return m_download_directory.Set(download_directory);	}
	OP_STATUS				GetDownloadDirectory(OpString& download_directory) { return download_directory.Set(m_download_directory);	}
	void					SetCalculateKbps(BOOL calculate) { m_calc_cps = calculate; }
	BOOL					GetIsCompleted() { return m_is_completed; }
	void					SetCompleted(BOOL completed) { m_is_completed = completed; }

	const uni_char*			GetUrlString();
	URL*					GetURL()       { return m_url; }
	P2P_URL*				Get_P2P_URL()	   { return &m_p2p_url; }
	void					SetCurrentSize(OpFileLength p)		{m_loaded=p;}
	void					SetCompleteSize(OpFileLength p)	{m_size=p;}
	void					SetUploaded(OpFileLength p)		{m_uploaded = p; }
	void					SetConnectionsWithCompleteFile(UINT32 p) { m_connections_with_complete_file = p; }
	void					SetConnectionsWithIncompleteFile(UINT32 p) { m_connections_with_incomplete_file = p; }
	void					SetDownloadConnections(UINT32 p) { m_downloadConnections = p; }
	void					SetUploadConnections(UINT32 p) { m_uploadConnections = p; }
	void					SetTotalWithCompleteFile(UINT32 p) { m_total_with_complete_file = p; }
	void					SetTotalDownloaders(UINT32 p) { m_total_leechers = p; }
	TransferItemType		GetType()		{ return m_type; }
	void					SetShowTransfer(BOOL show_transfer) { m_show_transfer = show_transfer; }
	BOOL					GetShowTransfer() { return m_show_transfer; }
	void					SetStartedInPrivacyMode(BOOL privacy_mode) { m_started_in_privacy_mode = privacy_mode; }
	BOOL					GetStartedInPrivacyMode() { return m_started_in_privacy_mode; }

	BOOL					GetIsRunning()
	{
		return m_lastStatus == OpTransferListener::TRANSFER_PROGRESS;
	}

	BOOL					GetIsFinished()
	{
		return		m_lastStatus == OpTransferListener::TRANSFER_DONE
				||	m_lastStatus == OpTransferListener::TRANSFER_ABORTED
				||	m_lastStatus == OpTransferListener::TRANSFER_FAILED;
	}

	BOOL					HeaderLoaded();

	// own methods
	BOOL ItemAdded();
	void SetUrl(const uni_char* url);
	OpTransferListener* GetTransferListener() { return m_listener; }
	void NotifyListener();

	// PutURLData and FinishedURLPutData can be used for 'ChatTransferItems' where comm-layer is handled elsewhere
	void					PutURLData(const char *data, unsigned int len);
	void					FinishedURLPutData();

	void					UpdateStats();

	void					ResetStatus();


private:
	TransferItemType m_type;

	class NullTransferListener : public OpTransferListener
	{
	  public:
		~NullTransferListener() {}
		void OnProgress(OpTransferItem* transferItem, TransferStatus status) {   }
		void OnReset(OpTransferItem* transferItem) {   }
		BOOL OnSetFilename(OpTransferItem* transferItem, const uni_char* recommended_filename)
			{ return FALSE; }
		void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to) {   }
	} m_nullTransferListener;

	class RelTimer
	{
		long last_sec, last_msec;
	public:
		RelTimer() : last_sec(0), last_msec(0){	Get(); }
		double Get();
	} m_timer;

	// Floating average calculation code.
#define FLOATING_AVG_SLOTS 200
	int m_pointpos;
	void AddPoint( double elapsed, OpFileLength bytes );
	double AverageSpeed( );

	struct Checkpoint {
		double elapsed;
		OpFileLength bytes;
		BOOL used;
	} m_points[ FLOATING_AVG_SLOTS ], m_pointsUpload[ FLOATING_AVG_SLOTS ];

	int m_pointposUpload;
	void AddPointUpload( double elapsed, OpFileLength bytes);
	double AverageSpeedUpload();
};

class TransferManager : public OpTransferManager, public OpTimerListener
{
public:
	TransferManager();
	~TransferManager();
	void InitL();

	/**
	 * Implementation of OpTransferManager
	 */
	OP_STATUS GetTransferItem(OpTransferItem** item, const uni_char* url, BOOL* already_created = NULL, BOOL create_new = FALSE);
	OP_STATUS GetNewTransferItem(OpTransferItem** item, const uni_char* url);
	void ReleaseTransferItem(OpTransferItem* item);
	void RemovePrivacyModeTransfers();
	OP_STATUS AddTransferManagerListener(OpTransferManagerListener* listener);
	OP_STATUS RemoveTransferManagerListener(OpTransferManagerListener* listener);
	OpTransferItem* GetTransferItem(int index);
	int GetTransferItemCount();
#ifdef WIC_USE_DOWNLOAD_RESCUE_FILE
	OP_STATUS SetRescueFileName(const uni_char * rescue_file_name);
#endif // WIC_USE_DOWNLOAD_RESCUE_FILE

	/**
	 * Implementation of OpTimerListener
	 */
	void OnTimeOut(OpTimer* timer);

	// other
	OP_STATUS AddTransferItem(	URL& url,
								const uni_char* filename=NULL,
								OpTransferItem::TransferAction action=OpTransferItem::ACTION_UNKNOWN,
								BOOL is_populating=FALSE,
								double last_speed=0,
								TransferItem::TransferItemType type = TransferItem::TRANSFERTYPE_DOWNLOAD,
								OpString *meta_file = NULL,
								OpString *download_directory = NULL,
								BOOL completed = FALSE,
								BOOL show_transfer = TRUE,
								BOOL started_in_privacy_mode = FALSE,
								TransferItem** out_item = NULL);
	BOOL HaveTransferItem(const uni_char* storage_filename);

	void BroadcastResume(OpTransferItem *item);
	void BroadcastStop(OpTransferItem *item);

private:
	TransferItem* GetTransferItemByFilename(const uni_char* filename);

#ifdef WIC_USE_DOWNLOAD_RESCUE_FILE
	OP_STATUS ReadRescueFile();
	OP_STATUS WriteRescueFile();
	OpString m_rescue_filename;
#endif // WIC_USE_DOWNLOAD_RESCUE_FILE

	Head itemList;

	class NullTransferManagerListener : public OpTransferManagerListener
	{
		BOOL OnTransferItemAdded(OpTransferItem* transferItem, BOOL is_populating, double last_speed) {
			OP_NEW_DBG("transfermanagerlistener::ontransferitemadded", "transfer");
			OP_DBG(("item: %p", transferItem));
			return FALSE;
		}
		void OnTransferItemRemoved(OpTransferItem* transferitem)        {}
		void OnTransferItemStopped(OpTransferItem* transferItem)        {}
		void OnTransferItemResumed(OpTransferItem* transferItem)        {}
		BOOL OnTransferItemCanDeleteFiles(OpTransferItem* transferItem) { return FALSE; }
		void OnTransferItemDeleteFiles(OpTransferItem* transferItem)    {}
	} m_nullTransferManagerListener;

	OpVector<OpTransferManagerListener> m_listeners;
	OpTimer* m_timer;
};

#endif // TRANSFER_MANAGER_H
