/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
 */

#ifndef __ST_SYNC_SPEEDDIAL_ENTRIES_H__
#define __ST_SYNC_SPEEDDIAL_ENTRIES_H__

#if defined(SUPPORT_DATA_SYNC) && defined(SUPPORT_SYNC_SPEED_DIAL)

#include "adjunct/quick/sync/SyncSpeedDialEntries.h"

/*************************************************************************
 **
 ** ST_SyncSpeedDialEntries
 **
 **
 **************************************************************************/

class ST_SyncSpeedDialEntries : 
public SyncSpeedDialEntries
{
public:
	ST_SyncSpeedDialEntries() : SyncSpeedDialEntries(), m_received_notification(FALSE), m_sent_to_server(FALSE), m_listening(FALSE) {}
	// virtual ~ST_SyncSpeedDialEntries();

	OP_STATUS ProcessSyncItem(OpSyncItem *item, BOOL& dirty) { return SyncSpeedDialEntries::ProcessSyncItem(item, dirty);}

	BOOL ItemsSentToServer() { return m_sent_to_server; }
	void ResetSentToServer() { m_sent_to_server = FALSE; }
	BOOL ReceivedNotification() { return m_received_notification; }
	void ResetNotification() { m_received_notification = FALSE;}

	void EnableSpeedDialListening(BOOL enable)
	{
		m_listening = TRUE;
		SyncSpeedDialEntries::EnableSpeedDialListening(enable, TRUE);
	}

	// SpeedDialListener. 
	virtual void OnSpeedDialAdded(const DesktopSpeedDial &sd)
	{
		m_received_notification = TRUE;
		SyncSpeedDialEntries::OnSpeedDialAdded(sd);
	    if (!IsProcessingIncomingItems())
	      m_sent_to_server = TRUE;

	}
	virtual void OnSpeedDialReplaced(const DesktopSpeedDial &old_sd, const DesktopSpeedDial &new_sd)
	{
		m_received_notification = TRUE;
		SyncSpeedDialEntries::OnSpeedDialReplaced(old_sd, new_sd);
	    if (!IsProcessingIncomingItems())
	      m_sent_to_server = TRUE;
	}
	virtual void OnSpeedDialRemoving(const DesktopSpeedDial &sd)
	{
		m_received_notification = TRUE;
		SyncSpeedDialEntries::OnSpeedDialRemoving(sd);
	    if (!IsProcessingIncomingItems())
	      m_sent_to_server = TRUE;
	}

	BOOL IsListening() { return m_listening; }


private:

	BOOL m_received_notification;
	BOOL m_sent_to_server;
	BOOL m_listening;
};

#endif // SUPPORT_DATA_SYNC && SUPPORT_SYNC_SPEED_DIAL
#endif // __ST_SYNC_SPEEDDIAL_ENTRIES_H__
