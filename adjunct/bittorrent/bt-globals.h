// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_GLOBALS_H
#define BT_GLOBALS_H

// ----------------------------------------------------

#include "adjunct/m2/src/engine/module.h"
#include "adjunct/m2/src/util/buffer.h"
#include "adjunct/desktop_util/adt/oplist.h"

#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"

#include <openssl/evp.h>
#include <openssl/sha.h>
//#include "bt-info.h"

//#define BT_FILELOGGING_ENABLED		// only enabled if set in the opera6.ini file
#define BT_UPLOAD_DESTRICTION		// when defined, will enforce a dynamic upload speed restriction (if enabled in prefs)
#define BT_UPLOAD_FIXED_RESTRICTION
//#define BT_UPLOAD_DISABLED			// when defined, will not create a listening socket

//#define DEBUG_BT_TRANSFERPANEL			// used in retail builds to add information to the transfer panel

#ifndef _MACINTOSH_
#ifdef _DEBUG
#define DEBUG_BT_TORRENT
#define DEBUG_BT_RESOURCE_TRACKING
//#define DEBUG_BT_RESOURCES
//#define DEBUG_BT_UPLOAD
//#define DEBUG_BT_DOWNLOAD
//#define DEBUG_BT_PROFILING
//#define DEBUG_BT_FILEPROFILING
#define DEBUG_BT_FILEACCESS
//#define DEBUG_BT_HAVE
//#define DEBUG_BT_CHOKING
#define DEBUG_BT_TRACKER
//#define DEBUG_BT_SOCKETS
//#define DEBUG_BT_THROTTLE
//#define DEBUG_BT_BLOCKSELECTION
//#define DEBUG_BT_PEERSPEED
//#define DEBUG_BT_CONNECT
//#define DEBUG_BT_DISKCACHE
#endif
#endif

class P2PFilePartPool;
class Transfers;
class BTServerConnector;
class BTPacketPool;
class Downloads;
class BTFileLogging;
class P2PFiles;
class BTInfo;
class BTResourceTracker;

class P2PGlobalData
{
public:
	OP_STATUS Init();
	void Destroy();

	P2PFilePartPool		*m_P2PFilePartPool;
	BTPacketPool		*m_PacketPool;

	Transfers			*m_Transfers;
	BTServerConnector	*m_BTServerConnector;
	Downloads			*m_Downloads;
	P2PFiles			*m_P2PFiles;
	OpVector<BTInfo>	m_PendingInfos;

#if defined(_DEBUG) && defined (MSWIN) && defined(DEBUG_BT_RESOURCE_TRACKING)
	BTResourceTracker	*m_BTResourceTracker;
#endif
#if defined(BT_FILELOGGING_ENABLED)
	BTFileLogging		*m_BTFileLogging;
#endif
};

#define g_BTResourceTracker		g_P2PGlobalData.m_BTResourceTracker
#define g_Transfers				g_P2PGlobalData.m_Transfers
#define g_BTServerConnector		g_P2PGlobalData.m_BTServerConnector
#define g_Downloads				g_P2PGlobalData.m_Downloads
#define g_PacketPool			g_P2PGlobalData.m_PacketPool
#define g_P2PFilePartPool		g_P2PGlobalData.m_P2PFilePartPool
#define g_BTFileLogging			g_P2PGlobalData.m_BTFileLogging
#define g_P2PFiles				g_P2PGlobalData.m_P2PFiles
#define g_PendingInfos			g_P2PGlobalData.m_PendingInfos

#if defined(_DEBUG) && defined (MSWIN) && defined(DEBUG_BT_RESOURCE_TRACKING)
#define BT_RESOURCE_ADD(x, y) if(g_BTResourceTracker) g_BTResourceTracker->Add(UNI_L(x), (void *)y, __FILE__, __LINE__)
#define BT_RESOURCE_REMOVE(x) if(g_BTResourceTracker) g_BTResourceTracker->Remove((void *)x)
#define BT_RESOURCE_CHECKPOINT(x) if(g_BTResourceTracker) g_BTResourceTracker->Checkpoint(x)
#else
#define BT_RESOURCE_ADD(x, y)
#define BT_RESOURCE_REMOVE(x)
#define BT_RESOURCE_CHECKPOINT(x)
#endif

#endif // BT_GLOBALS_H
