/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef _SYNC_ST_FACTORY_H_INCLUDED_
#define _SYNC_ST_FACTORY_H_INCLUDED_

#ifdef SELFTEST

#include "modules/sync/sync_factory.h"

class OpSyncTransportProtocol;

class ST_SyncFactory : public OpSyncFactory
{
public:
	ST_SyncFactory();
	virtual ~ST_SyncFactory();

	virtual OP_STATUS CreateTransportProtocol(OpSyncTransportProtocol** protocol, OpInternalSyncListener* listener);
};
#endif // SELFTEST
#endif //_SYNC_ST_FACTORY_H_INCLUDED_
