/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "modules/util/str.h"
#include "modules/util/opstring.h"

#include "modules/sync/sync_factory.h"
#include "modules/sync/sync_dataitem.h"
#include "modules/sync/sync_dataqueue.h"
#include "modules/sync/sync_parser.h"
#include "modules/sync/sync_parser_myopera.h"
#include "modules/sync/sync_transport.h"
#include "modules/sync/sync_allocator.h"

OpSyncFactory::OpSyncFactory()
	: m_queue(0)
	, m_allocator(0)
	, m_parser(0)
{
	OP_NEW_DBG("OpSyncFactory::OpSyncFactory()", "sync");
}

OpSyncFactory::~OpSyncFactory()
{
	OP_NEW_DBG("OpSyncFactory::~OpSyncFactory()", "sync");
}

// TODO: fix so the parser created is based on dynamic properties
OP_STATUS OpSyncFactory::GetParser(OpSyncParser** parser, OpInternalSyncListener* listener)
{
	OP_ASSERT(parser);
	if (!parser)
		return OpStatus::ERR_NULL_POINTER;

	if (!m_parser)
		m_parser = OP_NEW(MyOperaSyncParser, (this, listener));

	*parser = m_parser;
	return m_parser ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS OpSyncFactory::CreateTransportProtocol(OpSyncTransportProtocol** protocol, OpInternalSyncListener* listener)
{
	OP_NEW_DBG("OpSyncFactory::CreateTransportProtocol()", "sync");
	OP_ASSERT(protocol);
	if (!protocol)
		return OpStatus::ERR_NULL_POINTER;

	*protocol = OP_NEW(OpSyncTransportProtocol, (this, listener));
	return *protocol ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS OpSyncFactory::GetQueueHandler(OpSyncDataQueue** queue, OpInternalSyncListener* listener, BOOL use_disk_queue)
{
	OP_ASSERT(queue);
	if (!queue)
		return OpStatus::ERR_NULL_POINTER;

	if (!m_queue)
	{
		m_queue = OP_NEW(OpSyncDataQueue, (this, listener));
		if (m_queue)
			RETURN_IF_MEMORY_ERROR(m_queue->Init(use_disk_queue));
		else
			return OpStatus::ERR_NO_MEMORY;
	}
	*queue = m_queue;
	return OpStatus::OK;
}

OP_STATUS OpSyncFactory::GetAllocator(OpSyncAllocator** allocator)
{
	OP_ASSERT(allocator);
	if (!allocator)
		return OpStatus::ERR_NULL_POINTER;

	if (!m_allocator)
		m_allocator = OP_NEW(OpSyncAllocator, (this));
	*allocator = m_allocator;
	return m_allocator ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#endif // SUPPORT_DATA_SYNC
