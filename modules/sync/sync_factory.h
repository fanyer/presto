/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SYNC_FACTORY_H_INCLUDED_
#define _SYNC_FACTORY_H_INCLUDED_

#include "modules/sync/sync_dataqueue.h"
#include "modules/sync/sync_allocator.h"

class OpSyncDataItem;
class OpSyncParser;
class OpSyncTransportProtocol;
class OpSyncAuthentication;

class OpSyncFactory
{
public:
	OpSyncFactory();
	virtual ~OpSyncFactory();

	virtual OP_STATUS GetParser(OpSyncParser** parser, OpInternalSyncListener* listener);
	virtual OP_STATUS CreateTransportProtocol(OpSyncTransportProtocol** protocol, OpInternalSyncListener* listener);
	virtual OP_STATUS GetQueueHandler(OpSyncDataQueue** queue, OpInternalSyncListener* listener, BOOL use_disk_queue);
	virtual OP_STATUS GetAllocator(OpSyncAllocator** allocator);

private:
	OpSyncDataQueue* m_queue;
	OpSyncAllocator* m_allocator;
	OpSyncParser* m_parser;
};

#endif //_SYNC_FACTORY_H_INCLUDED_
