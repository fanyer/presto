// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_PEER_H
#define BT_PEER_H

// ----------------------------------------------------

#include "adjunct/m2/src/util/buffer.h"

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

#include "bt-util.h"

enum PeerState
{
	PeerStateChoking,
	PeerStateInterested,

};

enum ClientState
{
	ClientStateInterested,
	ClientStateChoking,

};

class BTPeer
{
// Construction
public:
	BTPeer();
	BTPeer(SHAStruct* pGUID, struct in_addr* pAddress, WORD nPort, BTPeer *previous);
	virtual ~BTPeer();

	Clear();
	const WORD GetPort() { return m_port; }
	const SHAStruct *GetGUID() { return &m_guid; }
	const in_addr *GetAddr() { return &m_address; }
	const PeerState GetPeerState() { return m_peerstate; }
	const ClientState GetClientState() { return m_clientstate; }

	inline BOOL Equals(BTPeer* pSource) const
	{
		return &m_guid == pSource->GetGUID();
	}
	BTPeer *NextPeer() { return m_nextpeer; }
	BTPeer *PrevPeer() { return m_prevpeer; }

private:
	BTPeer *m_nextpeer;
	BTPeer *m_prevpeer;
	struct in_addr m_address;
	WORD m_port;
	SHAStruct m_guid;
	PeerState m_peerstate;
	ClientState m_clientstate;
};



#endif // BT_PEER_H
