// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#include "adjunct/m2/src/engine/engine.h"
//#include "irc-message.h"
#include "BT-module.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
# include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"

#include <cassert>

#include "bt-peer.h"

#if defined(_DEBUG) && defined (MSWIN)
#define DEBUGTRACE(x, y) {	uni_char tmp[100]; wsprintf(tmp, x, y);	OutputDebugStringW(tmp); }
#else
#define DEBUGTRACE(x, y) ((void)0)
#endif


//////////////////////////////////////////////////////////////////////
// BTInfo construction

BTPeer::BTPeer()
{
}

BTPeer::~BTPeer()
{
}

BTPeer::Clear()
{
	memset(&m_address, 0, sizeof(struct in_addr));
	m_port = 0;
	memset(&m_guid, 0, sizeof(SHAStruct));
	m_peerstate = PeerStateChoking;
	m_clientstate = ClientStateChoking;
	m_nextpeer = NULL;
	m_prevpeer = NULL;
}

BTPeer::BTPeer(SHAStruct* pGUID, struct in_addr* pAddress, WORD nPort, BTPeer *previous)
{
	Clear();

	if(pAddress != NULL)
	{
		memcpy(&m_address, pAddress, sizeof(struct in_addr));
	}
	if(pGUID != NULL)
	{
		memcpy(&m_guid, pGUID, sizeof(SHAStruct));
	}
	m_port = nPort;
	m_prevpeer = previous;
}

