/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/url/url_sn.h"

#include "modules/url/protocols/srcomm.h"
#include "modules/url/protocols/comm.h"

ServerRemoteComm::ServerRemoteComm(ServerName *_host, unsigned short _port, CommunicationPipe *p)
: ProtocolComm(g_main_message_handler, NULL, NULL)
{
	pipe = p;
	host = _host;
	port = _port;
}

ServerRemoteComm::~ServerRemoteComm()
{
	pipe = NULL;
}

unsigned ServerRemoteComm::ReadData(char *buf, unsigned blen)
{
	return ProtocolComm::ReadData(buf, blen);
}

void ServerRemoteComm::SendData(char *buf, unsigned blen)
{
	ProtocolComm::SendData(buf, blen);
}

void ServerRemoteComm::RequestMoreData()
{
	SetDoNotReconnect(TRUE);
	pipe->RequestMoreData();
}

void ServerRemoteComm::ProcessReceivedData()
{
	pipe->ProcessReceivedData();
}

void ServerRemoteComm::HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	pipe->HandleCallback(msg, par1, par2);
}

unsigned ServerRemoteComm::StartToLoad()
{
	return (unsigned) ProtocolComm::Load();
}

void ServerRemoteComm::StopLoading()
{
	ProtocolComm::Stop();
}

char* ServerRemoteComm::AllocMem(unsigned size)
{
	return OP_NEWA(char, size);
}

void ServerRemoteComm::FreeMem(char* buf)
{
	OP_DELETEA(buf);
}

const char* ServerRemoteComm::GetLocalHostName()
{
	// I might be doing some nasty assumptions here?
	return  (GetSink() ? ((Comm*)GetSink())->GetLocalHostName() : NULL);
}

BOOL ServerRemoteComm::PostMessage(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	return mh->PostMessage(msg, par1, par2);
}

unsigned int ServerRemoteComm::Id() const
{
	return ProtocolComm::Id();
}


BOOL ServerRemoteComm::StartTLSConnection()
{
#ifdef _SSL_SUPPORT_
	return ProtocolComm::InsertTLSConnection(host, port);
#else
	return FALSE;
#endif // _SSL_SUPPORT_
}


