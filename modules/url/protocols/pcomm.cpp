/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"

#ifdef _CERTICOM_SSL_SUPPORT_
# include "product/mathilda/ssl/certui_common.h"
#endif

#include "modules/libssl/ssl_api.h"

// Pcomm.cpp 

// Protocol Communication base class

ProtocolComm::ProtocolComm(MessageHandler* msg_handler, SComm* prnt, SComm* snk)
  : SComm(msg_handler, prnt)
{
	sink = snk;
	if (sink)
		sink->ChangeParent(this);
}

ProtocolComm::~ProtocolComm()
{
	SComm *tmp_sink = sink;
	sink = NULL;

	NormalCallCount blocker(this);
	SafeDestruction(tmp_sink);

	sink = NULL;
}

void ProtocolComm::SetNewSink(SComm *snk)
{
	OP_ASSERT(snk != sink || sink == NULL);
	OP_ASSERT(snk != this);

	SComm *tmp_sink = sink;
	sink = NULL;

	NormalCallCount blocker(this);
	SafeDestruction(tmp_sink);

	sink = snk;
	if (sink) 
		sink->ChangeParent(this);
}

SComm *ProtocolComm::PopSink()
{
	SComm *snk = sink;

	sink = NULL;

	if (snk)
		snk->ChangeParent(NULL);

	return snk;
}

OP_STATUS ProtocolComm::SetCallbacks(MessageObject* master, MessageObject* parent)
{
	if( sink )
	{
		return sink->SetCallbacks(master, this);
	}
	else
	{
		return OpStatus::OK;
	}
	// gcc does not like this because OP_STATUS and OpStatus are 
	// used in the test (espen)
	// return (sink ? sink->SetCallbacks(master, this) : OpStatus::OK);
}

CommState ProtocolComm::Load()
{
	return sink ? sink->Load() : COMM_LOADING;
}

void ProtocolComm::SetMaxTCPConnectionEstablishedTimeout(UINT timeout)
{
	if (sink)
		sink->SetMaxTCPConnectionEstablishedTimeout(timeout);
}

void ProtocolComm::CloseConnection()	// Controlled shutdown
{
	if (sink)
		sink->CloseConnection();
}

BOOL ProtocolComm::Closed()
{
	return sink ? sink->Closed() : TRUE;
}

BOOL ProtocolComm::Connected() const
{
	return sink ? sink->Connected() : FALSE;
}

void ProtocolComm::Stop()				// Cut connection/reset connection
{
	if (sink)
		sink->Stop();
	SComm::Stop();
}

unsigned int ProtocolComm::SignalReadData(char* buf, unsigned blen)
{
	if(is_connected || !sink)
		return ReadData(buf, blen);
	else
		return sink->SignalReadData(buf,blen);

}

unsigned int ProtocolComm::ReadData(char* buf, unsigned blen)
{
	return sink ? sink->SignalReadData(buf, blen) : 0;
}

void ProtocolComm::SendData(char *buf, unsigned blen)
{
	if (sink)
		sink->SendData(buf,blen);
	else
		if (buf)
			OP_DELETEA(buf);	// buf is always freed by the handling object
}

void ProtocolComm::SetRequestMsgMode(ProgressState parm)
{
	if (sink)
		sink->SetRequestMsgMode(parm);
}

void ProtocolComm::SetConnectMsgMode(ProgressState parm)
{
	if (sink)
		sink->SetConnectMsgMode(parm);
}

void ProtocolComm::SetAllSentMsgMode(ProgressState parm, ProgressState request_parm)
{
	if (sink)
		sink->SetAllSentMsgMode(parm, request_parm);
}

SComm	*ProtocolComm::GetSink()
{
	return sink;
}

BOOL ProtocolComm::HasId(unsigned int sid)
{
	return (Id() == sid || (sink ? sink->HasId(sid) : FALSE));
}

BOOL ProtocolComm::SafeToDelete() const
{
	return SComm::SafeToDelete() && (sink ? sink->SafeToDelete() : TRUE);
}

void ProtocolComm::SetDoNotReconnect(BOOL val)
{
	if(sink)
		sink->SetDoNotReconnect(val);
}

BOOL ProtocolComm::PendingClose() const
{
	return sink ? sink->PendingClose() : TRUE;
}

#ifdef _SSL_SUPPORT_
#ifdef URL_ENABLE_INSERT_TLS
BOOL ProtocolComm::InsertTLSConnection(ServerName *_host, unsigned short _port)
{
#ifndef _EXTERNAL_SSL_SUPPORT_
	if(_host == NULL)
		return FALSE;

	ProtocolComm *tls_conn = g_ssl_api->Generate_SSL(mh,_host,_port, TRUE, TRUE);

	if(tls_conn == NULL)
		return FALSE;

	SComm *snk = PopSink();

	if(snk)
		mh->RemoveCallBacks(this, snk->Id());

	tls_conn->SetNewSink(snk);
	SetNewSink(tls_conn);

	RETURN_VALUE_IF_ERROR(tls_conn->SetCallbacks(NULL,this), FALSE);

	CommState stat = tls_conn->SignalConnectionEstablished();
	
	if(stat == COMM_LOADING)
		tls_conn->RequestMoreData();

	return (stat == COMM_LOADING);
#else //_EXTERNAL_SSL_SUPPORT_
	if(sink)
	{
		return sink->InsertTLSConnection(_host, _port);
	}
	return SComm::InsertTLSConnection(_host, _port);
#endif //_EXTERNAL_SSL_SUPPORT_
}
#endif // URL_ENABLE_INSERT_TLS
#endif //_SSL_SUPPORT_

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
void ProtocolComm::PauseLoading()
{
	if (sink)
		sink->PauseLoading();
}

OP_STATUS ProtocolComm::ContinueLoading()
{
	if (sink)
		return sink->ContinueLoading();
	return OpStatus::ERR;
}
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION
