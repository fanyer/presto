// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_TRACKER_H
#define BT_TRACKER_H

// ----------------------------------------------------

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

#define OPERA_VERSION_STRING_BITTORRENT	"OP000B"	// 6 chars

class PrefsFile;
class OpFile;
class BTDownload;
class TransferItem;

//********************************************************************
//
//	BTTracker
//
// Multitracker extension is based on the specification found
// at http://wiki.depthstrike.com/index.php/P2P:Protocol:Specifications:Multitracker
//********************************************************************

class BTTracker :
	public OpTransferListener, public OpTransferManagerListener
{

public:
	// Constructor / destructor.
	BTTracker();
	virtual ~BTTracker();

	BOOL IsRequestDone() { return m_transfer_item == NULL; }
	BOOL IsWorking() { return m_tracker_error ? FALSE : TRUE; }
	void StartClosing();
	OP_STATUS Connect(BTDownload *download, OpString& tracker_url, const char *verb = NULL, BOOL process = TRUE, BOOL scrape_request = FALSE, BOOL shutting_down = FALSE);
	UINT32 AddRef()
	{
		return ++m_ref_count;
	}
	void Release()
	{
		if(--m_ref_count == 0)
		{
			StartClosing();
			OP_DELETE(this);
		}
	}
	enum TrackerState
	{
		TrackerNone = 0,
		TrackerStartSent,
		TrackerStart,
		TrackerUpdate
	};

private:
	// No copy or assignment.
	BTTracker(const BTTracker&);
	BTTracker& operator =(const BTTracker&);

	// OpTransferListener
	void OnProgress(OpTransferItem* transferItem, TransferStatus status);
	void OnReset(OpTransferItem* transferItem) { }
	void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to) {};

	// OpTransferManagerListener
	BOOL OnTransferItemAdded(OpTransferItem* transfer_item, BOOL is_populating = FALSE, double last_speed = 0) { return FALSE; }
	void OnTransferItemRemoved(OpTransferItem* transfer_item);
	void OnTransferItemStopped(OpTransferItem* transferItem) {}
	void OnTransferItemResumed(OpTransferItem* transferItem) {}
	BOOL OnTransferItemCanDeleteFiles(OpTransferItem* transferItem) { return FALSE; }
	void OnTransferItemDeleteFiles(OpTransferItem* transferItem) {}

	OP_STATUS Connect(const OpStringC& feed_url, INT32& feed_id);
	void Process(BOOL request, OpByteBuffer *buffer);
	BOOL Process(BENode *root);
	void Process(BOOL request, OpString& buffer);

	// Members.
	INT32			m_feed_id;
	OpString		m_tracker;
	BTDownload *	m_download;
	BOOL			m_process;	// process the reply from the tracker message sent to the tracker?
	BOOL			m_scrape_request;
	BOOL			m_shutting_down;	// set when SendStopped() is called with shutting_down = TRUE
	UINT32			m_ref_count;
	BOOL			m_tracker_error;	// TRUE when the tracker has failed
	TransferItem *	m_transfer_item;
};

class BTTrackerEntry
{
public:
	BTTrackerEntry(OpString& tracker_url);
	BTTrackerEntry(BTTrackerEntry *copy_from);
	virtual ~BTTrackerEntry();
	void SetAsCurrent(BOOL current) { m_current_tracker = current; }
	BOOL IsCurrent() { return m_current_tracker; }
	void AbortRequest()
	{
		if(m_tracker && !m_tracker->IsRequestDone())
		{
			m_tracker->StartClosing();
		}
	}
	BOOL IsRequestDone()
	{
		return (m_tracker && m_tracker->IsRequestDone());
	}
	BOOL IsWorking() { return m_tracker ? m_tracker->IsWorking() : FALSE; }
	BTTracker* GetTracker() { return m_tracker; }
	BTTracker* GetScrapeTracker() { return m_tracker_scrape; }
	BTTrackerEntry* Copy() { return OP_NEW(BTTrackerEntry, (this)); }

private:
	OpString m_tracker_url;
	BOOL m_current_tracker;		// is this the tracker currently being used?
	BTTracker *m_tracker;
	BTTracker *m_tracker_scrape;

	friend class BTTrackerGroup;
};

class BTTrackerGroup
{
public:
	BTTrackerGroup();
	virtual ~BTTrackerGroup();

	OP_STATUS CreateTrackerEntry(OpString& tracker_url);
	BTTrackerGroup* Copy();

private:
	OpVector<BTTrackerEntry> m_trackers;

	OP_STATUS SendStarted(BTDownload* pDownload);
	OP_STATUS SendUpdate(BTDownload* pDownload);
	OP_STATUS SendCompleted(BTDownload* pDownload);
	OP_STATUS SendStopped(BTDownload* pDownload, BOOL shutting_down = FALSE);
	OP_STATUS SendScrape(BTDownload *pDownload);
	BTTrackerEntry* FindActiveTracker(BOOL force = FALSE, BOOL scrape = FALSE);

	friend class BTMultiTracker;
};

class BTMultiTracker
{
public:
	BTMultiTracker();
	virtual ~BTMultiTracker();

	BTTrackerGroup* CreateTrackerGroup();
	void SendStarted(BTDownload* pDownload);
	void SendUpdate(BTDownload* pDownload);
	void SendCompleted(BTDownload* pDownload);
	void SendStopped(BTDownload* pDownload, BOOL shutting_down = FALSE);
	void SendScrape(BTDownload *pDownload);

	BTMultiTracker* Copy();

private:
	OpVector<BTTrackerGroup> m_tracker_groups;
};



#endif // BT_TRACKER_H
