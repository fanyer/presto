/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/url/protocols/crcomm.h"

ClientRemoteComm::ClientRemoteComm(CommunicationPipe *p)
{
	pipe = p;
}

ClientRemoteComm::~ClientRemoteComm()
{
	pipe = NULL;
}

void ClientRemoteComm::SetPipe(CommunicationPipe *p)
{
	pipe = p;
}

CommunicationPipe*	ClientRemoteComm::GetPipe(void) const
{
	return pipe;
}

unsigned ClientRemoteComm::ReadData(char *buf, unsigned blen)
{
	return pipe->ReadData(buf, blen);
}

void ClientRemoteComm::SendData(char *buf, unsigned blen)
{
	pipe->SendData(buf, blen);
}

unsigned ClientRemoteComm::StartToLoad()
{
	return pipe->StartToLoad();
}

void ClientRemoteComm::StopLoading()
{
	pipe->StopLoading();
}

char* ClientRemoteComm::AllocMem(unsigned size)
{
	return pipe->AllocMem(size);
}

void ClientRemoteComm::FreeMem(char *buf)
{
	pipe->FreeMem(buf);
}

const char* ClientRemoteComm::GetLocalHostName()
{
	return pipe->GetLocalHostName();
}

BOOL ClientRemoteComm::PostMessage(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	return pipe->PostMessage(msg, par1, par2);
}

unsigned int ClientRemoteComm::Id() const
{
	return pipe->Id();
}

BOOL ClientRemoteComm::StartTLSConnection()
{
	return pipe->StartTLSConnection();
}
