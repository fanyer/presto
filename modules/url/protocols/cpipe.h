/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _URL2_CPIPE_
#define _URL2_CPIPE_

class CommunicationPipe
{
public:
	virtual unsigned		ReadData(char* buf, unsigned blen) = 0;
	virtual void			SendData(char *buf, unsigned blen) = 0;
	virtual void			RequestMoreData() = 0;
	virtual void			ProcessReceivedData() = 0;
	virtual void			HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2) = 0;
	virtual unsigned		StartToLoad() = 0;
	virtual void			StopLoading() = 0;
	virtual char*			AllocMem(unsigned size) = 0;
	virtual void			FreeMem(char* buf) = 0;
	virtual const char*		GetLocalHostName() = 0;
	virtual BOOL			PostMessage(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2) = 0;
	virtual unsigned int	Id() const = 0;
	virtual BOOL			StartTLSConnection()=0;
};

#endif /* _URL2_CPIPE_ */

