/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _URL2_SRCOMM_
#define _URL2_SRCOMM_

#include "modules/url/protocols/cpipe.h"
#include "modules/url/protocols/pcomm.h"

class ServerRemoteComm : public CommunicationPipe, public ProtocolComm
{
private:
	CommunicationPipe *pipe;
	ServerName_Pointer host;
	unsigned short port;

public:
							ServerRemoteComm(ServerName *_host, unsigned short _port, CommunicationPipe *p);
	virtual					~ServerRemoteComm();

	virtual unsigned		ReadData(char *buf, unsigned blen);
	virtual void			SendData(char *buf, unsigned blen);
	virtual void			RequestMoreData();
	virtual void			ProcessReceivedData();

	virtual unsigned		StartToLoad(); // Use this instead of Load()
	virtual void			StopLoading();
	virtual char*			AllocMem(unsigned size);
	virtual void			FreeMem(char* buf);
	virtual const char*		GetLocalHostName();
	virtual BOOL			PostMessage(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual unsigned int	Id() const;

  	virtual void			HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual BOOL			StartTLSConnection();

};

#endif /* _URL2_SRCOMM_ */
