/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _URL2_CRCOMM_
#define _URL2_CRCOMM_

#include "modules/url/protocols/cpipe.h"

class ClientRemoteComm : public CommunicationPipe
{
public:
							ClientRemoteComm(CommunicationPipe *p = NULL);
	virtual					~ClientRemoteComm();

	void					SetPipe(CommunicationPipe *p);
	CommunicationPipe*		GetPipe(void) const;
	
	virtual unsigned		ReadData(char* buf, unsigned blen);
	virtual void			SendData(char *buf, unsigned blen);
	virtual void			RequestMoreData() = 0;
	virtual void			ProcessReceivedData() = 0;
	virtual void			HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2) = 0;
	virtual unsigned		StartToLoad();
	virtual void			StopLoading();
	virtual char*			AllocMem(unsigned size);
	virtual void			FreeMem(char* buf);
	virtual const char*		GetLocalHostName();
	virtual BOOL			PostMessage(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual unsigned int	Id() const;
	virtual BOOL			StartTLSConnection();


private:
	CommunicationPipe *pipe;
};

#endif /* _URL2_CRCOMM_ */
